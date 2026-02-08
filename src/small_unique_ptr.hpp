/* Copyright (c) 2024 Krisztián Rugási. Subject to the MIT License. */

#ifndef SMALL_UNIQUE_PTR_HPP
#define SMALL_UNIQUE_PTR_HPP

#include <compare>
#include <memory>
#include <iosfwd>
#include <functional>
#include <new>
#include <type_traits>
#include <concepts>
#include <utility>
#include <cstddef>
#include <cassert>

namespace smp::detail
{
    template<typename T>
    constexpr const T& min(const T& left, const T& right) noexcept { return (left < right) ? left : right; }

    template<typename T>
    constexpr const T& max(const T& left, const T& right) noexcept { return (left < right) ? right : left; }

    template<std::integral T>
    constexpr T max_pow2_factor(T n) noexcept { return n & (~n + 1); }

    template<typename T, typename U, std::size_t N>
    constexpr const void fill(T(&arr)[N], U value)
    {
        for (std::size_t i = 0; i < N; i++) 
            arr[i] = value;
    }

    struct ignore_t
    {
        constexpr const ignore_t& operator=(const auto&) const noexcept { return *this; }
    };


    using move_fn = void(*)(void* src, void* dst) noexcept;

    template<typename T>
    void move_buffer(void* src, void* dst) noexcept(std::is_nothrow_move_constructible_v<T>)
    {
        static_assert(!std::is_const_v<T> && !std::is_volatile_v<T> && !std::is_array_v<T>);
        std::construct_at(static_cast<T*>(dst), std::move(*static_cast<T*>(src)));
    }

    template<typename T>
    concept has_virtual_move = requires(std::remove_cv_t<T>& object, void* const dst)
    {
        requires std::is_polymorphic_v<T>;
        { object.small_unique_ptr_move(dst) } noexcept -> std::same_as<void>;
    };


    template<typename T>
    struct is_nothrow_dereferenceable : std::bool_constant<noexcept(*std::declval<T>())> {};

    template<typename T>
    inline constexpr bool is_nothrow_dereferenceable_v = is_nothrow_dereferenceable<T>::value;


    template<typename T, typename U>
    struct is_same_unqualified : std::is_same<std::remove_cv_t<T>, std::remove_cv_t<U>> {};

    template<typename T, typename U>
    inline constexpr bool is_same_unqualified_v = is_same_unqualified<T, U>::value;


    template<typename T>
    struct cv_qual_rank : std::integral_constant<std::size_t, std::is_const_v<T> + std::is_volatile_v<T>> {};

    template<typename T>
    inline constexpr std::size_t cv_qual_rank_v = cv_qual_rank<T>::value;


    template<typename T, typename U>
    struct is_less_cv_qualified : std::bool_constant<cv_qual_rank_v<T> < cv_qual_rank_v<U>> {};

    template<typename T, typename U>
    inline constexpr bool is_less_cv_qualified_v = is_less_cv_qualified<T, U>::value;


    template<typename Base, typename Derived>
    struct is_proper_base_of :
        std::conjunction<std::is_base_of<std::remove_cv_t<Base>, std::remove_cv_t<Derived>>,
                         std::negation<detail::is_same_unqualified<Base, Derived>>>
    {};

    template<typename Base, typename Derived>
    inline constexpr bool is_proper_base_of_v = is_proper_base_of<Base, Derived>::value;


    template<typename From, typename To>
    struct is_pointer_convertible : std::disjunction<
        std::conjunction<detail::is_proper_base_of<To, From>, std::has_virtual_destructor<To>, std::negation<detail::is_less_cv_qualified<To, From>>>,
        std::conjunction<detail::is_same_unqualified<From, To>, std::negation<detail::is_less_cv_qualified<To, From>>>
    > {};

    template<typename From, typename To>
    inline constexpr bool is_pointer_convertible_v = is_pointer_convertible<From, To>::value;


    template<typename T, std::size_t small_ptr_size>
    struct buffer_size
    {
    private:
        static constexpr std::size_t dynamic_buffer_size = small_ptr_size - sizeof(T*) - !has_virtual_move<T> * sizeof(move_fn);
        static constexpr std::size_t static_buffer_size  = detail::min(sizeof(T), small_ptr_size - sizeof(T*));
    public:
        static constexpr std::size_t value = std::has_virtual_destructor_v<T> ? dynamic_buffer_size : static_buffer_size;
    };

    template<typename T>
    struct buffer_size<T, sizeof(void*)>
    {
        static constexpr std::size_t value = 0;
    };

    template<typename T, std::size_t small_ptr_size>
    struct buffer_size<T[], small_ptr_size>
    {
        static constexpr std::size_t value = small_ptr_size - sizeof(T*);
    };

    template<typename T>
    struct buffer_size<T[], sizeof(void*)>
    {
        static constexpr std::size_t value = 0;
    };

    template<typename T, std::size_t small_ptr_size>
    inline constexpr std::size_t buffer_size_v = buffer_size<T, small_ptr_size>::value;


    template<typename T, std::size_t small_ptr_size>
    struct buffer_alignment
    {
    private:
        static constexpr std::size_t dynamic_buffer_alignment = detail::max_pow2_factor(small_ptr_size);
        static constexpr std::size_t static_buffer_alignment  = detail::max_pow2_factor(detail::min(alignof(T), small_ptr_size));
    public:
        static constexpr std::size_t value = std::has_virtual_destructor_v<T> ? dynamic_buffer_alignment : static_buffer_alignment;
    };

    template<typename T, std::size_t small_ptr_size>
    inline constexpr std::size_t buffer_alignment_v = buffer_alignment<T, small_ptr_size>::value;


    template<typename T, std::size_t small_ptr_size>
    struct is_always_heap_allocated
    {
        using U = std::remove_cv_t<std::remove_extent_t<T>>;

        static constexpr bool value = (sizeof(U)  > buffer_size_v<T, small_ptr_size>) ||
                                      (alignof(U) > buffer_alignment_v<T, small_ptr_size>) ||
                                      (!std::is_nothrow_move_constructible_v<U> && !std::is_abstract_v<U>);
    };

    template<typename T, std::size_t small_ptr_size>
    inline constexpr bool is_always_heap_allocated_v = is_always_heap_allocated<T, small_ptr_size>::value;


    template<typename T, std::size_t Size>
    struct small_unique_ptr_base
    {
        using pointer  = std::remove_cv_t<T>*;
        using buffer_t = unsigned char[buffer_size_v<T, Size>];

        constexpr small_unique_ptr_base() noexcept
        {
            if (std::is_constant_evaluated())
            {
                detail::fill(buffer_, 0);
            }
        }

        pointer buffer(std::ptrdiff_t offset = 0) const noexcept
        {
            assert(offset <= sizeof(buffer_t));
            return reinterpret_cast<pointer>(static_cast<unsigned char*>(buffer_) + offset);
        }

        template<typename U>
        void move_buffer_to(small_unique_ptr_base<U, Size>& dst) noexcept
        {
            move_(std::launder(buffer()), dst.buffer());
            dst.move_ = move_;
        }

        constexpr bool is_stack_allocated() const noexcept
        {
            return static_cast<bool>(move_);
        }

        alignas(buffer_alignment_v<T, Size>) mutable buffer_t buffer_;
        T* data_;
        move_fn move_;
    };

    template<typename T, std::size_t Size>
    requires(is_always_heap_allocated_v<T, Size>)
    struct small_unique_ptr_base<T, Size>
    {
        static constexpr bool is_stack_allocated() noexcept { return false; }

        std::remove_extent_t<T>* data_;
        static constexpr ignore_t move_ = {};
    };

    template<typename T, std::size_t Size>
    requires(!is_always_heap_allocated_v<T, Size> && !std::is_polymorphic_v<T> && !std::is_array_v<T>)
    struct small_unique_ptr_base<T, Size>
    {
        using pointer  = std::remove_cv_t<T>*;
        using buffer_t = std::remove_cv_t<T>;

        constexpr pointer buffer(std::ptrdiff_t = 0) const noexcept
        {
            return std::addressof(buffer_);
        }

        template<typename U>
        constexpr void move_buffer_to(small_unique_ptr_base<U, Size>& dst) noexcept
        {
            std::construct_at(dst.buffer(), std::move(*buffer()));
        }

        constexpr bool is_stack_allocated() const noexcept
        {
            return !std::is_constant_evaluated() && data_;
        }

        constexpr small_unique_ptr_base() noexcept {}
        constexpr ~small_unique_ptr_base() noexcept {}

        union { mutable buffer_t buffer_; };
        T* data_;
        static constexpr ignore_t move_ = {};
    };

    template<typename T, std::size_t Size>
    requires(!is_always_heap_allocated_v<T, Size> && has_virtual_move<T>)
    struct small_unique_ptr_base<T, Size>
    {
        using pointer  = std::remove_cv_t<T>*;
        using buffer_t = unsigned char[buffer_size_v<T, Size>];

        constexpr small_unique_ptr_base() noexcept
        {
            if (std::is_constant_evaluated())
            {
                detail::fill(buffer_, 0);
            }
        }

        pointer buffer(std::ptrdiff_t offset = 0) const noexcept
        {
            assert(offset <= sizeof(buffer_t));
            return reinterpret_cast<pointer>(static_cast<unsigned char*>(buffer_) + offset);
        }

        template<typename U>
        requires(has_virtual_move<U>)
        void move_buffer_to(small_unique_ptr_base<U, Size>& dst) noexcept
        {
            const pointer data = const_cast<pointer>(data_);
            data->small_unique_ptr_move(dst.buffer());
        }

        constexpr bool is_stack_allocated() const noexcept
        {
            if (std::is_constant_evaluated()) return false;

            auto* data = reinterpret_cast<const volatile unsigned char*>(data_);
            auto* buffer_first = static_cast<const volatile unsigned char*>(buffer_);
            auto* buffer_last  = buffer_first + buffer_size_v<T, Size>;

            assert(reinterpret_cast<std::uintptr_t>(buffer_last) - reinterpret_cast<std::uintptr_t>(buffer_first) == (buffer_size_v<T, Size>) &&
                   "Linear address space assumed for the stack buffer.");

            return std::less_equal{}(buffer_first, data) && std::less{}(data, buffer_last);
        }

        alignas(buffer_alignment_v<T, Size>) mutable buffer_t buffer_;
        T* data_;
        static constexpr ignore_t move_ = {};
    };

    template<typename T, std::size_t Size>
    requires(!is_always_heap_allocated_v<T, Size> && std::is_array_v<T>)
    struct small_unique_ptr_base<T, Size>
    {
        static constexpr std::size_t array_size = buffer_size_v<T, Size> / sizeof(std::remove_extent_t<T>);

        using pointer  = std::remove_cv_t<std::remove_extent_t<T>>*;
        using buffer_t = std::remove_cv_t<std::remove_extent_t<T>>[array_size];

        constexpr pointer buffer(std::ptrdiff_t = 0) const noexcept
        {
            return static_cast<pointer>(buffer_);
        }

        template<typename U>
        void move_buffer_to(small_unique_ptr_base<U, Size>& dst) noexcept
        {
            std::uninitialized_move(buffer(), buffer() + array_size, dst.buffer());
        }

        constexpr bool is_stack_allocated() const noexcept
        {
            return !std::is_constant_evaluated() && (data_ == buffer());
        }

        constexpr small_unique_ptr_base() noexcept {}
        constexpr ~small_unique_ptr_base() noexcept {}

        union { mutable buffer_t buffer_; };
        std::remove_extent_t<T>* data_;
        static constexpr ignore_t move_ = {};
    };

    struct make_unique_small_impl;

} // namespace smp::detail

namespace smp
{
    inline constexpr std::size_t default_small_ptr_size = 64;

    template<typename T, std::size_t Size = default_small_ptr_size>
    class small_unique_ptr : private detail::small_unique_ptr_base<T, Size>
    {
    public:
        static_assert(!std::is_bounded_array_v<T>, "Bounded array types are not supported.");
        static_assert(Size >= sizeof(T*),   "Size must be at least the size of a pointer.");
        static_assert(!(Size % sizeof(T*)), "Size must be a multiple of the size of a pointer.");

        using element_type = std::remove_extent_t<T>;
        using pointer      = std::remove_extent_t<T>*;
        using reference    = std::remove_extent_t<T>&;

        struct constructor_tag_t {};

        constexpr small_unique_ptr() noexcept
        {
            this->data_ = nullptr;
            this->move_ = nullptr;
        }

        constexpr small_unique_ptr(std::nullptr_t) noexcept :
            small_unique_ptr()
        {}

        constexpr small_unique_ptr(small_unique_ptr&& other) noexcept :
            small_unique_ptr(std::move(other), constructor_tag_t{})
        {}

        template<typename U>
        constexpr small_unique_ptr(small_unique_ptr<U, Size>&& other) noexcept :
            small_unique_ptr(std::move(other), constructor_tag_t{})
        {}

        template<typename U>
        constexpr small_unique_ptr(small_unique_ptr<U, Size>&& other, constructor_tag_t) noexcept
        {
            static_assert(detail::is_pointer_convertible_v<U, T>);

            if (!other.is_stack_allocated())
            {
                this->data_ = other.data_;
                this->move_ = nullptr;
                other.data_ = nullptr;
                return;
            }

            if constexpr (!detail::is_always_heap_allocated_v<U, Size>) // other.is_stack_allocated()
            {
                const ptrdiff_t base_offset = other.template offsetof_base<T>();
                const pointer base_pointer = this->buffer(base_offset);
                other.move_buffer_to(*this);
                this->data_ = std::launder(base_pointer);
            }
        }

        constexpr small_unique_ptr& operator=(small_unique_ptr&& other) noexcept
        {
            if (std::addressof(other) == this) return *this;

            return operator=<T>(std::move(other));
        }

        template<typename U>
        constexpr small_unique_ptr& operator=(small_unique_ptr<U, Size>&& other) noexcept
        {
            static_assert(detail::is_pointer_convertible_v<U, T>);

            if (!other.is_stack_allocated())
            {
                reset(std::exchange(other.data_, nullptr));
                return *this;
            }

            if constexpr (!detail::is_always_heap_allocated_v<U, Size>) // other.is_stack_allocated()
            {
                destroy();
                const ptrdiff_t base_offset = other.template offsetof_base<T>();
                const pointer base_pointer = this->buffer(base_offset);
                other.move_buffer_to(*this);
                this->data_ = std::launder(base_pointer);
            }

            return *this;
        }

        constexpr small_unique_ptr& operator=(std::nullptr_t) noexcept
        {
            reset();
            return *this;
        }

        constexpr ~small_unique_ptr() noexcept
        {
            destroy();
        }

        constexpr void reset(pointer new_data = pointer()) noexcept
        {
            is_stack_allocated() ? reset_internal(new_data) : reset_allocated(new_data);
        }

        constexpr void swap(small_unique_ptr& other) noexcept
        {
            if constexpr (small_unique_ptr::is_always_heap_allocated())
            {
                std::swap(this->data_, other.data_);
            }
            else if (!is_stack_allocated() && !other.is_stack_allocated())
            {
                std::swap(this->data_, other.data_);
            }
            else if (is_stack_allocated() && other.is_stack_allocated())
            {
                const auto other_offset = other.template offsetof_base<>();

                detail::small_unique_ptr_base<T, Size> temp;
                if constexpr (detail::has_virtual_move<T>) temp.data_ = std::launder(temp.buffer(other_offset));
                other.move_buffer_to(temp);

                other.destroy_buffer();
                other.data_ = std::launder(other.buffer(this->template offsetof_base<>()));
                this->move_buffer_to(other);

                this->destroy_buffer();
                temp.move_buffer_to(*this);
                this->data_ = std::launder(this->buffer(other_offset));
            }
            else if (!is_stack_allocated()/* && other.is_stack_allocated() */)
            {
                const pointer new_data = this->buffer(other.offsetof_base());
                other.move_buffer_to(*this);
                other.reset_internal(this->data_);
                this->data_ = std::launder(new_data);
            }
            else /* if (is_stack_allocated() && !other.is_stack_allocated()) */
            {
                const pointer new_data = other.buffer(this->offsetof_base());
                this->move_buffer_to(other);
                this->reset_internal(other.data_);
                other.data_ = std::launder(new_data);
            }
        }

        [[nodiscard]]
        constexpr pointer release() noexcept = delete;

        [[nodiscard]]
        constexpr bool is_stack_allocated() const noexcept
        {
            return small_unique_ptr::small_unique_ptr_base::is_stack_allocated();
        }

        [[nodiscard]]
        static constexpr bool is_always_heap_allocated() noexcept
        {
            return detail::is_always_heap_allocated_v<T, Size>;
        }

        [[nodiscard]]
        static constexpr std::size_t stack_buffer_size() noexcept
        {
            if constexpr (is_always_heap_allocated()) return 0;
            else return sizeof(typename small_unique_ptr::buffer_t);
        }

        [[nodiscard]]
        static constexpr std::size_t stack_array_size() noexcept requires(std::is_array_v<T>)
        {
            return stack_buffer_size() / sizeof(std::remove_extent_t<T>);
        }

        [[nodiscard]]
        constexpr pointer get() const noexcept
        {
            return this->data_;
        }

        [[nodiscard]]
        constexpr explicit operator bool() const noexcept
        {
            return static_cast<bool>(this->data_);
        }

        [[nodiscard]]
        constexpr reference operator*() const noexcept(detail::is_nothrow_dereferenceable_v<pointer>) requires(!std::is_array_v<T>)
        {
            assert(this->data_);
            return *this->data_;
        }

        [[nodiscard]]
        constexpr pointer operator->() const noexcept requires(!std::is_array_v<T>)
        {
            assert(this->data_);
            return this->data_;
        }

        [[nodiscard]]
        constexpr reference operator[](std::size_t idx) const requires(std::is_array_v<T>)
        {
            assert(this->data_);
            return this->data_[idx];
        }

        constexpr bool operator==(std::nullptr_t) const noexcept
        {
            return this->data_ == pointer{ nullptr };
        }

        constexpr std::strong_ordering operator<=>(std::nullptr_t) const noexcept
        {
            return std::compare_three_way{}(this->data_, pointer{ nullptr });
        }

        template<typename U>
        constexpr bool operator==(const small_unique_ptr<U>& rhs) const noexcept
        {
            return this->data_ == rhs.data_;
        }

        template<typename U>
        constexpr std::strong_ordering operator<=>(const small_unique_ptr<U, Size>& rhs) const noexcept
        {
            return std::compare_three_way{}(this->data_, rhs.data_);
        }

        template<typename CharT, typename Traits>
        friend std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, const small_unique_ptr& p)
        {
            return os << p.get();
        }

        constexpr friend void swap(small_unique_ptr& lhs, small_unique_ptr& rhs) noexcept
        {
            lhs.swap(rhs);
        }

    private:
        template<typename Base = T>
        constexpr std::ptrdiff_t offsetof_base() const noexcept requires(std::is_polymorphic_v<T>)
        {
            assert(is_stack_allocated());

            const auto derived_ptr = reinterpret_cast<const volatile unsigned char*>(this->buffer());
            const auto base_ptr    = reinterpret_cast<const volatile unsigned char*>(static_cast<const volatile Base*>(this->data_));

            return base_ptr - derived_ptr;
        }

        template<typename = T>
        constexpr std::ptrdiff_t offsetof_base() const noexcept requires(!std::is_polymorphic_v<T>)
        {
            return 0;
        }

        constexpr void destroy_buffer() noexcept requires(!std::is_array_v<T>)
        {
            assert(is_stack_allocated());
            std::destroy_at(this->data_);
        }

        constexpr void destroy_buffer() noexcept requires(std::is_array_v<T>)
        {
            assert(is_stack_allocated());
            std::destroy(this->data_, this->data_ + stack_array_size());
        }

        constexpr void destroy_allocated() noexcept requires(!std::is_array_v<T>)
        {
            assert(!is_stack_allocated());
            delete this->data_;
        }

        constexpr void destroy_allocated() noexcept requires(std::is_array_v<T>)
        {
            assert(!is_stack_allocated());
            delete[] this->data_;
        }

        constexpr void destroy() noexcept
        {
            is_stack_allocated() ? destroy_buffer() : destroy_allocated();
        }

        constexpr void reset_internal(pointer new_data = pointer()) noexcept
        {
            assert(is_stack_allocated());

            destroy_buffer();
            this->data_ = new_data;
            this->move_ = nullptr;
        }

        constexpr void reset_allocated(pointer new_data = pointer()) noexcept
        {
            assert(!is_stack_allocated());

            destroy_allocated();
            this->data_ = new_data;
        }

        template<typename U, std::size_t>
        friend class small_unique_ptr;

        friend struct detail::make_unique_small_impl;
    };

} // namespace smp

namespace std
{
    template<typename T, std::size_t Size>
    struct hash<smp::small_unique_ptr<T, Size>>
    {
        std::size_t operator()(const smp::small_unique_ptr<T, Size>& p) const noexcept
        {
            return std::hash<typename smp::small_unique_ptr<T, Size>::pointer>{}(p.get());
        }
    };

} // namespace std

namespace smp::detail
{
    struct make_unique_small_impl
    {
        template<typename T, typename B, std::size_t S, typename... Args>
        static constexpr small_unique_ptr<B, S> invoke_scalar(Args&&... args)
        noexcept(std::is_nothrow_constructible_v<T, Args...> && !detail::is_always_heap_allocated_v<T, S>)
        {
            small_unique_ptr<B, S> ptr;

            if (detail::is_always_heap_allocated_v<T, S> || std::is_constant_evaluated())
            {
                ptr.data_ = new T(std::forward<Args>(args)...);
            }
            else if constexpr (!detail::is_always_heap_allocated_v<T, S> && std::is_polymorphic_v<T> && !detail::has_virtual_move<T>)
            {
                ptr.data_ = std::construct_at(reinterpret_cast<std::remove_cv_t<T>*>(ptr.buffer()), std::forward<Args>(args)...);
                ptr.move_ = detail::move_buffer<std::remove_cv_t<T>>;
            }
            else if constexpr (!detail::is_always_heap_allocated_v<T, S>)
            {
                ptr.data_ = std::construct_at(reinterpret_cast<std::remove_cv_t<T>*>(ptr.buffer()), std::forward<Args>(args)...);
            }

            return ptr;
        }

        template<typename T, std::size_t S>
        static constexpr small_unique_ptr<T, S> invoke_array(std::size_t count)
        {
            small_unique_ptr<T, S> ptr;

            if (detail::is_always_heap_allocated_v<T, S> || (ptr.stack_array_size() < count) || std::is_constant_evaluated())
            {
                ptr.data_ = new std::remove_extent_t<T>[count]{};
            }
            else if constexpr (!detail::is_always_heap_allocated_v<T, S>)
            {
                std::uninitialized_value_construct_n(ptr.buffer(), ptr.stack_array_size());
                ptr.data_ = std::launder(ptr.buffer());
            }

            return ptr;
        }

        template<typename T, typename B, std::size_t S>
        static constexpr small_unique_ptr<B, S> invoke_for_overwrite_scalar()
        noexcept(std::is_nothrow_default_constructible_v<T> && !detail::is_always_heap_allocated_v<T, S>)
        {
            small_unique_ptr<B, S> ptr;

            if (detail::is_always_heap_allocated_v<T, S> || std::is_constant_evaluated())
            {
                ptr.data_ = new T;
            }
            else if constexpr (!detail::is_always_heap_allocated_v<T, S> && std::is_polymorphic_v<T> && !detail::has_virtual_move<T>)
            {
                ptr.data_ = ::new(static_cast<void*>(ptr.buffer())) std::remove_cv_t<T>;
                ptr.move_ = detail::move_buffer<std::remove_cv_t<T>>;
            }
            else if constexpr (!detail::is_always_heap_allocated_v<T, S>)
            {
                ptr.data_ = ::new(static_cast<void*>(ptr.buffer())) std::remove_cv_t<T>;
            }

            return ptr;
        }

        template<typename T, std::size_t S>
        static constexpr small_unique_ptr<T, S> invoke_for_overwrite_array(std::size_t count)
        {
            small_unique_ptr<T, S> ptr;

            if (detail::is_always_heap_allocated_v<T, S> || (ptr.stack_array_size() < count) || std::is_constant_evaluated())
            {
                ptr.data_ = new std::remove_extent_t<T>[count];
            }
            else if constexpr (!detail::is_always_heap_allocated_v<T, S>)
            {
                std::uninitialized_default_construct_n(ptr.buffer(), ptr.stack_array_size());
                ptr.data_ = std::launder(ptr.buffer());
            }

            return ptr;
        }
    };

} // namespace smp::detail

namespace smp
{
    template<typename T, std::size_t Size = default_small_ptr_size, typename... Args>
    [[nodiscard]] constexpr small_unique_ptr<T, Size> make_unique_small(Args&&... args)
    noexcept(std::is_nothrow_constructible_v<T, Args...> && !detail::is_always_heap_allocated_v<T, Size>)
    requires(!std::is_array_v<T>)
    {
        return detail::make_unique_small_impl::invoke_scalar<T, T, Size>(std::forward<Args>(args)...);
    }

    template<typename T, typename Base, std::size_t Size = default_small_ptr_size, typename... Args>
    [[nodiscard]] constexpr small_unique_ptr<Base, Size> make_unique_small(Args&&... args)
    noexcept(std::is_nothrow_constructible_v<T, Args...> && !detail::is_always_heap_allocated_v<T, Size>)
    requires(!std::is_array_v<T> && detail::is_pointer_convertible_v<T, Base>)
    {
        return detail::make_unique_small_impl::invoke_scalar<T, Base, Size>(std::forward<Args>(args)...);
    }

    template<typename T, std::size_t Size = default_small_ptr_size>
    [[nodiscard]] constexpr small_unique_ptr<T, Size> make_unique_small(std::size_t count) requires(std::is_unbounded_array_v<T>)
    {
        return detail::make_unique_small_impl::invoke_array<T, Size>(count);
    }

    template<typename T, std::size_t Size = default_small_ptr_size, typename... Args>
    [[nodiscard]] constexpr small_unique_ptr<T, Size> make_unique_small(Args&&...) requires(std::is_bounded_array_v<T>) = delete;


    template<typename T, std::size_t Size = default_small_ptr_size>
    [[nodiscard]] constexpr small_unique_ptr<T, Size> make_unique_small_for_overwrite()
    noexcept(std::is_nothrow_default_constructible_v<T> && !detail::is_always_heap_allocated_v<T, Size>)
    requires(!std::is_array_v<T>)
    {
        return detail::make_unique_small_impl::invoke_for_overwrite_scalar<T, T, Size>();
    }

    template<typename T, typename Base, std::size_t Size = default_small_ptr_size>
    [[nodiscard]] constexpr small_unique_ptr<Base, Size> make_unique_small_for_overwrite()
    noexcept(std::is_nothrow_default_constructible_v<T> && !detail::is_always_heap_allocated_v<T, Size>)
    requires(!std::is_array_v<T> && detail::is_pointer_convertible_v<T, Base>)
    {
        return detail::make_unique_small_impl::invoke_for_overwrite_scalar<T, Base, Size>();
    }

    template<typename T, std::size_t Size = default_small_ptr_size>
    [[nodiscard]] constexpr small_unique_ptr<T, Size> make_unique_small_for_overwrite(std::size_t count) requires(std::is_unbounded_array_v<T>)
    {
        return detail::make_unique_small_impl::invoke_for_overwrite_array<T, Size>(count);
    }

    template<typename T, std::size_t Size = default_small_ptr_size, typename... Args>
    [[nodiscard]] constexpr small_unique_ptr<T, Size> make_unique_small_for_overwrite(Args&&...) requires(std::is_bounded_array_v<T>) = delete;

} // namespace smp

#endif // !SMALL_UNIQUE_PTR_HPP
