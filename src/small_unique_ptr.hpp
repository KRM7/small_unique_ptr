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

namespace detail
{
    template<typename T>
    constexpr const T& min(const T& left, const T& right) { return (left < right) ? left : right; }

    template<typename T>
    constexpr const T& max(const T& left, const T& right) { return (left < right) ? right : left; }


    template<typename T>
    void move_buffer(void* src, void* dst) noexcept(std::is_nothrow_move_constructible_v<T>)
    {
        static_assert(!std::is_const_v<T> && !std::is_volatile_v<T>);
        std::construct_at(static_cast<T*>(dst), std::move(*static_cast<T*>(src)));
    }

    template<typename T>
    concept has_virtual_move = requires(std::remove_cv_t<T> object, void* const dst)
    {
        requires std::is_polymorphic_v<T>;
        { object.small_unique_ptr_move(dst) } noexcept -> std::same_as<void>;
    };


    template<typename T>
    struct is_nothrow_dereferenceable : std::bool_constant<noexcept(*std::declval<T>())> {};

    template<typename T>
    inline constexpr bool is_nothrow_dereferenceable_v = is_nothrow_dereferenceable<T>::value;


    template<typename Base, typename Derived>
    struct is_proper_base_of :
        std::conjunction<std::is_base_of<std::remove_cv_t<Base>, std::remove_cv_t<Derived>>,
                         std::negation<std::is_same<std::remove_cv_t<Base>, std::remove_cv_t<Derived>>>>
    {};

    template<typename Base, typename Derived>
    inline constexpr bool is_proper_base_of_v = is_proper_base_of<Base, Derived>::value;


    template<typename T, typename = void>
    struct is_complete : std::false_type {};

    template<typename T>
    struct is_complete<T, std::void_t<decltype(sizeof(T))>> : std::true_type {};

    template<typename T>
    inline constexpr bool is_complete_v = is_complete<T>::value;


    inline constexpr std::size_t small_ptr_size = 64;


    template<typename T>
    struct buffer_size
    {
    private:
        static constexpr std::size_t dynamic_buffer_size = small_ptr_size - sizeof(T*) - !has_virtual_move<T> * sizeof(decltype(&move_buffer<T>));
        static constexpr std::size_t static_buffer_size  = detail::min(sizeof(T), small_ptr_size - sizeof(T*));
    public:
        static constexpr std::size_t value = std::has_virtual_destructor_v<T> ? dynamic_buffer_size : static_buffer_size;
    };

    template<typename T>
    inline constexpr std::size_t buffer_size_v = buffer_size<T>::value;


    template<typename T>
    struct buffer_alignment
    {
    private:
        static constexpr std::size_t dynamic_buffer_alignment = small_ptr_size;
        static constexpr std::size_t static_buffer_alignment  = detail::min(alignof(T), small_ptr_size);
    public:
        static constexpr std::size_t value = std::has_virtual_destructor_v<T> ? dynamic_buffer_alignment : static_buffer_alignment;
    };

    template<typename T>
    inline constexpr std::size_t buffer_alignment_v = buffer_alignment<T>::value;


    template<typename T>
    struct is_always_heap_allocated
    {
        static constexpr bool value = (sizeof(T) > buffer_size_v<T>) || (alignof(T) > buffer_alignment_v<T>) ||
                                      (!std::is_abstract_v<T> && !std::is_nothrow_move_constructible_v<std::remove_cv_t<T>>);
    };

    template<typename T>
    inline constexpr bool is_always_heap_allocated_v = is_always_heap_allocated<T>::value;


    template<typename T>
    struct small_unique_ptr_base
    {
        using pointer  = std::remove_cv_t<T>*;
        using buffer_t = unsigned char[buffer_size_v<T>];
        using move_fn  = void(*)(void* src, void* dst) noexcept;

        pointer buffer(std::ptrdiff_t offset = 0) const noexcept
        {
            return std::launder(reinterpret_cast<pointer>(static_cast<unsigned char*>(buffer_) + offset));
        }

        template<typename U>
        void move_buffer_to(small_unique_ptr_base<U>& dst) noexcept
        {
            move_(buffer(), dst.buffer());
            dst.move_ = move_;
        }

        constexpr bool is_stack_allocated() const noexcept
        {
            return static_cast<bool>(move_);
        }

        alignas(buffer_alignment_v<T>) mutable buffer_t buffer_;
        T* data_      = nullptr;
        move_fn move_ = nullptr;
    };

    template<typename T>
    requires(is_always_heap_allocated_v<T>)
    struct small_unique_ptr_base<T>
    {
        static constexpr bool is_stack_allocated() noexcept { return false; }

        T* data_ = nullptr;
    };

    template<typename T>
    requires(!is_always_heap_allocated_v<T> && !std::is_polymorphic_v<T>)
    struct small_unique_ptr_base<T>
    {
        using pointer  = std::remove_cv_t<T>*;
        using buffer_t = unsigned char[buffer_size_v<T>];

        pointer buffer(std::ptrdiff_t offset = 0) const noexcept
        {
            return std::launder(reinterpret_cast<pointer>(static_cast<unsigned char*>(buffer_) + offset));
        }

        template<typename U>
        void move_buffer_to(small_unique_ptr_base<U>& dst) noexcept
        {
            std::construct_at(dst.buffer(), std::move(*buffer()));
        }

        constexpr bool is_stack_allocated() const noexcept
        {
            return !std::is_constant_evaluated() && (data_ == buffer());
        }

        alignas(buffer_alignment_v<T>) mutable buffer_t buffer_;
        T* data_ = nullptr;
    };

    template<typename T>
    requires(!is_always_heap_allocated_v<T> && has_virtual_move<T>)
    struct small_unique_ptr_base<T>
    {
        using pointer  = std::remove_cv_t<T>*;
        using buffer_t = unsigned char[buffer_size_v<T>];

        pointer buffer(std::ptrdiff_t offset = 0) const noexcept
        {
            return std::launder(reinterpret_cast<pointer>(static_cast<unsigned char*>(buffer_) + offset));
        }

        template<typename U>
        requires(has_virtual_move<U>)
        void move_buffer_to(small_unique_ptr_base<U>& dst) noexcept
        {
            const pointer data = const_cast<pointer>(data_);
            data->small_unique_ptr_move(dst.buffer());
        }

        constexpr bool is_stack_allocated() const noexcept
        {
            if (std::is_constant_evaluated()) return false;

            const volatile unsigned char* data = reinterpret_cast<const volatile unsigned char*>(data_);
            const volatile unsigned char* buffer_first = static_cast<unsigned char*>(buffer_);
            const volatile unsigned char* buffer_last  = buffer_first + buffer_size_v<T>;

            assert(reinterpret_cast<std::uintptr_t>(buffer_last) - reinterpret_cast<std::uintptr_t>(buffer_first) == buffer_size_v<T>);

            return std::less_equal{}(buffer_first, data) && std::less{}(data, buffer_last);
        }

        alignas(buffer_alignment_v<T>) mutable buffer_t buffer_;
        T* data_ = nullptr;
    };

    struct make_unique_small_impl;

} // namespace detail


template<typename T>
class small_unique_ptr : private detail::small_unique_ptr_base<T>
{
public:
    static_assert(detail::is_complete_v<T> && !std::is_array_v<T>);

    using element_type = T;
    using pointer      = T*;
    using reference    = T&;

    struct constructor_tag_t {};

    constexpr small_unique_ptr() noexcept = default;
    constexpr small_unique_ptr(std::nullptr_t) noexcept {}

    constexpr small_unique_ptr(small_unique_ptr&& other) noexcept :
        small_unique_ptr(std::move(other), constructor_tag_t{})
    {}

    template<typename U>
    constexpr small_unique_ptr(small_unique_ptr<U>&& other, constructor_tag_t = {}) noexcept
    {
        static_assert(!detail::is_proper_base_of_v<T, U> || std::has_virtual_destructor_v<T>);

        if (!other.is_stack_allocated())
        {
            this->data_ = std::exchange(other.data_, nullptr);
            return;
        }
        if constexpr (!detail::is_always_heap_allocated_v<U>) // other.is_stack_allocated()
        {
            other.move_buffer_to(*this);
            this->data_ = this->buffer(other.template offsetof_base<T>());
            other.reset();
        }
    }

    constexpr small_unique_ptr& operator=(small_unique_ptr&& other) noexcept
    {
        if (std::addressof(other) == this) [[unlikely]] return *this;

        return operator=<T>(std::move(other));
    }

    template<typename U>
    constexpr small_unique_ptr& operator=(small_unique_ptr<U>&& other) noexcept
    {
        static_assert(!detail::is_proper_base_of_v<T, U> || std::has_virtual_destructor_v<T>);

        if (!other.is_stack_allocated())
        {
            reset(std::exchange(other.data_, nullptr));
            return *this;
        }
        if constexpr (!detail::is_always_heap_allocated_v<U>) // other.is_stack_allocated()
        {
            reset();
            other.move_buffer_to(*this);
            this->data_ = this->buffer(other.template offsetof_base<T>());
            other.reset();
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
        is_stack_allocated() ? std::destroy_at(this->data_) : delete this->data_;
    }

    constexpr void reset(pointer new_data = pointer{}) noexcept
    {
        is_stack_allocated() ? std::destroy_at(this->data_) : delete this->data_;
        if constexpr (requires { small_unique_ptr::move_; }) this->move_ = nullptr;
        this->data_ = new_data;
    }

    constexpr void swap(small_unique_ptr& other) noexcept
    {
        if constexpr (detail::is_always_heap_allocated_v<T>)
        {
            std::swap(this->data_, other.data_);
        }
        else if (!is_stack_allocated() && !other.is_stack_allocated())
        {
            std::swap(this->data_, other.data_);
        }
        else if (is_stack_allocated() && other.is_stack_allocated())
        {
            const std::ptrdiff_t other_offset = other.offsetof_base();
            const std::ptrdiff_t this_offset  = this->offsetof_base();

            detail::small_unique_ptr_base<T> temp;

            other.move_buffer_to(temp);
            temp.data_ = temp.buffer(other_offset);
            std::destroy_at(other.data_);

            this->move_buffer_to(other);
            other.data_ = other.buffer(this_offset);
            std::destroy_at(this->data_);

            temp.move_buffer_to(*this);
            this->data_ = this->buffer(other_offset);
            std::destroy_at(temp.data_);
        }
        else if (!is_stack_allocated() && other.is_stack_allocated())
        {
            const pointer new_data = this->buffer(other.offsetof_base());
            other.move_buffer_to(*this);
            other.reset(std::exchange(this->data_, new_data));
        }
        else /* if (is_stack_allocated() && !other.is_stack_allocated()) */
        {
            const pointer new_data = other.buffer(this->offsetof_base());
            this->move_buffer_to(other);
            this->reset(std::exchange(other.data_, new_data));
        }
    }

    [[nodiscard]]
    pointer release() noexcept = delete;

    [[nodiscard]]
    static constexpr bool is_always_heap_allocated() noexcept
    {
        return detail::is_always_heap_allocated_v<T>;
    }

    [[nodiscard]]
    constexpr bool is_stack_allocated() const noexcept
    {
        return small_unique_ptr::small_unique_ptr_base::is_stack_allocated();
    }

    [[nodiscard]]
    static constexpr std::size_t stack_buffer_size() noexcept
    {
        if constexpr (detail::is_always_heap_allocated_v<T>) return 0;
        else return detail::buffer_size_v<T>;
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
    constexpr reference operator*() const noexcept(detail::is_nothrow_dereferenceable_v<pointer>)
    {
        assert(this->data_);
        return *this->data_;
    }

    [[nodiscard]]
    constexpr pointer operator->() const noexcept
    {
        assert(this->data_);
        return this->data_;
    }

    constexpr bool operator==(std::nullptr_t) const noexcept
    {
        return this->data_ == pointer{ nullptr };
    }

    constexpr std::strong_ordering operator<=>(std::nullptr_t) const noexcept
    {
        return this->data_ <=> pointer{ nullptr };
    }

    template<typename U>
    constexpr bool operator==(const small_unique_ptr<U>& rhs) const noexcept
    {
        return this->data_ == rhs.data_;
    }

    template<typename U>
    constexpr std::strong_ordering operator<=>(const small_unique_ptr<U>& rhs) const noexcept
    {
        return this->data_ <=> rhs.data_;
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
    constexpr std::ptrdiff_t offsetof_base() const noexcept
    {
        if (!is_stack_allocated()) return 0;

        const auto derived_ptr = reinterpret_cast<const volatile unsigned char*>(this->buffer());
        const auto base_ptr    = reinterpret_cast<const volatile unsigned char*>(static_cast<const volatile Base*>(this->data_));

        return base_ptr - derived_ptr;
    }

    template<typename U>
    friend class small_unique_ptr;

    friend struct detail::make_unique_small_impl;
};

namespace std
{
    template<typename T>
    struct hash<small_unique_ptr<T>>
    {
        std::size_t operator()(const small_unique_ptr<T>& p) const noexcept
        {
            return std::hash<typename small_unique_ptr<T>::pointer>{}(p.get());
        }
    };

} // namespace std

namespace detail
{
    struct make_unique_small_impl
    {
        template<typename T, typename... Args>
        static constexpr small_unique_ptr<T> invoke(Args&&... args)
        noexcept(std::is_nothrow_constructible_v<T, Args...> && !detail::is_always_heap_allocated_v<T>)
        {
            small_unique_ptr<T> ptr;

            if (detail::is_always_heap_allocated_v<T> || std::is_constant_evaluated())
            {
                ptr.data_ = new T(std::forward<Args>(args)...);
            }
            else if constexpr (!detail::is_always_heap_allocated_v<T> && std::is_polymorphic_v<T> && !detail::has_virtual_move<T>)
            {
                ptr.data_ = std::construct_at(ptr.buffer(), std::forward<Args>(args)...);
                ptr.move_ = detail::move_buffer<std::remove_cv_t<T>>;
            }
            else if constexpr (!detail::is_always_heap_allocated_v<T>)
            {
                ptr.data_ = std::construct_at(ptr.buffer(), std::forward<Args>(args)...);
            }

            return ptr;
        }
    };

} // namespace detail

template<typename T, typename... Args>
[[nodiscard]] constexpr small_unique_ptr<T> make_unique_small(Args&&... args)
noexcept(std::is_nothrow_constructible_v<T, Args...> && !detail::is_always_heap_allocated_v<T>)
{
    return detail::make_unique_small_impl::invoke<T>(std::forward<Args>(args)...);
}

#endif // !SMALL_UNIQUE_PTR_HPP
