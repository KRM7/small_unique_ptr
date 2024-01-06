/* Copyright (c) 2024 Krisztián Rugási. Subject to the MIT License. */

#ifndef SMALL_UNIQUE_PTR_SMALL_UNIQUE_PTR_HPP
#define SMALL_UNIQUE_PTR_SMALL_UNIQUE_PTR_HPP

#include <compare>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>
#include <cstddef>
#include <cassert>

namespace detail
{
    struct empty_t {};


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


    inline constexpr std::size_t cache_line_size = 64;


    template<typename T>
    struct buffer_size
    {
    private:
        inline static constexpr std::size_t dynamic_buffer_size = cache_line_size - sizeof(void*) - sizeof(void(*)());
        inline static constexpr std::size_t static_buffer_size  = (sizeof(T) < dynamic_buffer_size) ? sizeof(T) : dynamic_buffer_size;
    public:
        inline static constexpr std::size_t value = std::has_virtual_destructor_v<T> ? dynamic_buffer_size : static_buffer_size;
    };

    template<typename T>
    inline constexpr std::size_t buffer_size_v = buffer_size<T>::value;


    template<typename T>
    struct buffer_alignment
    {
        inline static constexpr std::size_t value = cache_line_size;
    };

    template<typename T>
    inline constexpr std::size_t buffer_alignment_v = buffer_alignment<T>::value;


    template<typename T>
    struct always_heap_allocated
    {
        inline static constexpr bool value = (sizeof(T) > buffer_size_v<T>) || (alignof(T) > buffer_alignment_v<T>) ||
                                             (!std::is_abstract_v<T> && !std::is_nothrow_move_constructible_v<T>);
    };

    template<typename T>
    inline constexpr bool always_heap_allocated_v = always_heap_allocated<T>::value;


    template<typename T>
    struct polymorphic_ptr_storage
    {
        using pointer       = std::remove_cv_t<T>*;
        using const_pointer = const std::remove_cv_t<T>*;

        constexpr polymorphic_ptr_storage()  = default;
        constexpr ~polymorphic_ptr_storage() = default;

        polymorphic_ptr_storage(const polymorphic_ptr_storage&) = delete;
        polymorphic_ptr_storage(polymorphic_ptr_storage&&)      = delete;

        polymorphic_ptr_storage& operator=(const polymorphic_ptr_storage&) = delete;
        polymorphic_ptr_storage& operator=(polymorphic_ptr_storage&&)      = delete;

        pointer buffer(std::ptrdiff_t offset = 0) noexcept
        {
            return std::launder(reinterpret_cast<pointer>(std::addressof(buffer_[0]) + offset));
        }

        const_pointer buffer(std::ptrdiff_t offset = 0) const noexcept
        {
            return std::launder(reinterpret_cast<const_pointer>(std::addressof(buffer_[0]) + offset));
        }

        template<typename U>
        void move_buffer_to(polymorphic_ptr_storage<U>& dst) noexcept
        {
            move_(buffer(), dst.buffer());
            dst.move_ = move_;
        }

        alignas(buffer_alignment_v<T>)
        mutable unsigned char buffer_[buffer_size_v<T>] = {};

        T* data_ = nullptr;

        using move_fn = void(*)(void* src, void* dst) noexcept;
        move_fn move_ = nullptr;
    };

    template<typename T>
    requires(always_heap_allocated_v<T>)
    struct polymorphic_ptr_storage<T> { T* data_ = nullptr; };


    template<typename T>
    void move_buffer(void* src, void* dst) noexcept
    {
        std::construct_at(static_cast<T*>(dst), std::move(*static_cast<T*>(src)));
    }

} // namespace detail

template<typename T>
class small_unique_ptr : private detail::polymorphic_ptr_storage<T>
{
public:
    static_assert(!std::is_array_v<T> && !std::is_void_v<T>);

    using element_type = T;
    using pointer      = T*;
    using reference    = T&;

    constexpr small_unique_ptr() noexcept = default;
    constexpr small_unique_ptr(std::nullptr_t) noexcept {}

    constexpr small_unique_ptr(small_unique_ptr&& other) noexcept :
        small_unique_ptr<T>(std::move(other), detail::empty_t{})
    {}

    template<typename U>
    constexpr small_unique_ptr(small_unique_ptr<U>&& other, detail::empty_t = {}) noexcept
    {
        static_assert(!detail::is_proper_base_of_v<T, U> || std::has_virtual_destructor_v<T>);

        if (!other.is_local() || std::is_constant_evaluated())
        {
            this->data_ = std::exchange(other.data_, nullptr);
            return;
        }
        if constexpr (!detail::always_heap_allocated_v<U>) // other.is_local()
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

        if (!other.is_local() || std::is_constant_evaluated())
        {
            reset(std::exchange(other.data_, nullptr));
            return *this;
        }
        if constexpr (!detail::always_heap_allocated_v<U>) // other.is_local()
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
        reset();
    }

    constexpr void reset(pointer new_data = pointer{}) noexcept
    {
        is_local() ? std::destroy_at(this->data_) : delete this->data_;
        this->data_ = new_data;
        if constexpr (!detail::always_heap_allocated_v<T>) this->move_ = nullptr;
    }

    constexpr void swap(small_unique_ptr& other) noexcept requires(detail::always_heap_allocated_v<T>)
    {
        std::swap(this->data_, other.data_);
    }

    constexpr void swap(small_unique_ptr& other) noexcept requires(!detail::always_heap_allocated_v<T>)
    {
        if (is_local() && other.is_local())
        {
            const std::ptrdiff_t other_offset = other.offsetof_base();
            const std::ptrdiff_t this_offset  = this->offsetof_base();

            detail::polymorphic_ptr_storage<T> temp;

            other.move_buffer_to(temp);
            std::destroy_at(other.data_);
            this->move_buffer_to(other);
            std::destroy_at(this->data_);
            temp.move_buffer_to(*this);
            std::destroy_at(temp.buffer(other_offset));

            this->data_ = this->buffer(other_offset);
            other.data_ = other.buffer(this_offset);
        }
        else if (!is_local() && !other.is_local())
        {
            std::swap(this->data_, other.data_);
        }
        else if (!is_local() && other.is_local())
        {
            T* new_data = this->buffer(other.offsetof_base());
            other.move_buffer_to(*this);
            other.reset(std::exchange(this->data_, new_data));
        }
        else /* if (is_local() && !other.is_local()) */
        {
            T* new_data = other.buffer(this->offsetof_base());
            this->move_buffer_to(other);
            reset(std::exchange(other.data_, new_data));
        }
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

private:
    constexpr bool is_local() const noexcept requires(!detail::always_heap_allocated_v<T>) { return this->move_; }
    constexpr bool is_local() const noexcept requires(detail::always_heap_allocated_v<T>)  { return false; }

    template<typename B = T>
    constexpr std::ptrdiff_t offsetof_base() const noexcept
    {
        if (!is_local()) return 0;

        const auto derived_ptr = reinterpret_cast<const volatile unsigned char*>(this->buffer());
        const auto base_ptr    = reinterpret_cast<const volatile unsigned char*>(static_cast<const volatile B*>(this->data_));

        return base_ptr - derived_ptr;
    }

    template<typename U>
    friend class small_unique_ptr;

    template<typename U, typename... Args>
    friend constexpr small_unique_ptr<U> make_unique_small(Args&&...);
};

template<typename T>
constexpr void swap(small_unique_ptr<T>& lhs, small_unique_ptr<T>& rhs) noexcept
{
    lhs.swap(rhs);
}

template<typename T, typename... Args>
[[nodiscard]] constexpr small_unique_ptr<T> make_unique_small(Args&&... args)
{
    small_unique_ptr<T> ptr;

    if (detail::always_heap_allocated_v<T> || std::is_constant_evaluated())
    {
        ptr.data_ = new T(std::forward<Args>(args)...);
    }
    else if constexpr (!detail::always_heap_allocated_v<T>)
    {
        ptr.data_ = std::construct_at(ptr.buffer(), std::forward<Args>(args)...);
        ptr.move_ = detail::move_buffer<T>;
    }

    return ptr;
}

#endif // !SMALL_UNIQUE_PTR_SMALL_UNIQUE_PTR_HPP