[![sanitizers](https://github.com/KRM7/small_unique_ptr/actions/workflows/sanitizers.yml/badge.svg)](https://github.com/KRM7/small_unique_ptr/actions/workflows/sanitizers.yml)

### `small_unique_ptr<T>`

A constexpr `unique_ptr` implementation in C++20 with small object optimization.

```cpp
smp::small_unique_ptr<Base> p = smp::make_unique_small<Derived>();
```

Objects created with `make_unique_small<T>` will be allocated on the stack if:

 - The size of `T` is not greater than the size of the internal stack buffer
 - The alignment of `T` is not greater than the alignment of the stack buffer
 - Their move constructor is `noexcept`

If any of these conditions is not met, the object will be allocated dynamically.

--------------------------------------------------------------------------------------------------

#### Interface

The interface matches `std::unique_ptr<T>`, except for:

 - There is no `Deleter` template parameter and the associated methods are not implemented
   (i.e. `get_deleter` and the constructors with a deleter parameter).

 - There is a `Size` template parameter that specifies the size of the `small_unique_ptr` object,
   and implicitly, the size of the internal stack buffer. The actual size of the object may be
   smaller than the size specified, for example if the object is always dynamically allocated.

   The value of `Size` may be any positive multiple of `sizeof(T*)`, but it is recommended
   to choose a power of 2, in order to guarantee that objects will never be dynamically
   allocated just because of alignment issues.

- `T` can't be an incomplete type, as details of `T` are used to determine the layout
   of `small_unique_ptr<T>`.

 - Constructors from pointers are not provided except for the `nullptr` constructor, as these
   would not be able to make use of the internal stack buffer. The `make_unique_small` functions
   should be used instead to create objects.

 - `release()` is not implemented due to the stack allocated case. This means that
   `small_unique_ptr` can't release ownership of the managed object.

 - There are some additional methods that don't exist for `std::unique_ptr`. These can be used
   to query details about the internal stack buffer and to check where objects are allocated
   (`is_stack_allocated`, `is_always_heap_allocated`, `stack_buffer_size`, `stack_array_size`).

Everything is constexpr, but the stack buffer is not used in constant evaluated contexts,
so any constexpr usage is subject to the same transient allocation requirements that a constexpr
`std::unique_ptr` would be.

--------------------------------------------------------------------------------------------------

#### Layout details

As mentioned above, the `Size` template parameter is used to specify to size of the entire
`small_unique_ptr` object, not the size of the internal stack buffer. The buffer size will
be calculated from this as the size specified minus some implementation overhead. This will
be the size of either 1 or 2 pointers depending on `T`.

Using the default values, the size of the buffer on a 64 bit architecture will typically be:

 - 48 bytes for polymorphic types
 - 56 bytes for polymorphic types that implement a virtual `small_unique_ptr_move` method
 - `sizeof(T)` for non-polymophic types, with an upper limit of 56 bytes
 - 56 bytes for array types (rounded down to a multiple of the element size)

The size of `small_unique_ptr` objects may in some cases be smaller than the size specified 
as the template parameter in order to avoid wasting space.
Specifically, this will happen in 3 cases:

 - If `T` is larger than the calculated stack buffer size, there will be no buffer and
   `sizeof(small_unique_ptr<T>)` will be equal to `sizeof(T*)`.
 - If `T` is a non-polymorphic type smaller than the calculated stack buffer size, the buffer
   size will be `sizeof(T)`.
 - If `T` is an array type, the size of the stack buffer will be rounded down to a multiple
   of `sizeof(T)`.

The size of `small_unique_ptr<T, Size>` will never be greater than `Size`.

Polymorphic classes that are used with `small_unique_ptr` may optionally implement a virtual
`small_unique_ptr_move` method in order to increase the available buffer size. If some class
in an object hierarchy declares this function, it must be properly implemented in every
non-abstract derived class in the hierarchy. The signature of the function must be:

`virtual void small_unique_ptr_move(void* dst) noexcept;`

The implementation must move construct an object at the memory location pointed to by
`dst` from the object on which the function is invoked (i.e. from `this`).
The typical implementation would look like:

```cpp
struct Widget
{
    virtual void small_unique_ptr_move(void* dst) noexcept
    {
        std::construct_at(static_cast<Widget*>(dst), std::move(*this));
    }
};
```

--------------------------------------------------------------------------------------------------

#### Usage example

Example of a simplified `move_only_function` implementation with small object
optimization using `small_unique_ptr`:

```cpp
template<typename...>
class move_only_function;

template<typename Ret, typename... Args>
class move_only_function<Ret(Args...)>
{
public:
    constexpr move_only_function() noexcept = default;
    constexpr move_only_function(std::nullptr_t) noexcept {}

    template<typename F>
    requires(!std::is_same_v<F, move_only_function> && std::is_invocable_r_v<Ret, F&, Args...>)
    constexpr move_only_function(F f) noexcept(noexcept(smp::make_unique_small<Impl<F>>(std::move(f)))) :
        fptr_(smp::make_unique_small<Impl<F>>(std::move(f)))
    {}

    template<typename F>
    requires(!std::is_same_v<F, move_only_function> && std::is_invocable_r_v<Ret, F&, Args...>)
    constexpr move_only_function& operator=(F f) noexcept(noexcept(smp::make_unique_small<Impl<F>>(std::move(f))))
    {
        fptr_ = smp::make_unique_small<Impl<F>>(std::move(f));
        return *this;
    }

    constexpr move_only_function(move_only_function&&)            = default;
    constexpr move_only_function& operator=(move_only_function&&) = default;

    constexpr Ret operator()(Args... args) const
    {
        return fptr_->invoke(std::forward<Args>(args)...);
    }

    constexpr void swap(move_only_function& other) noexcept
    {
        fptr_.swap(other.fptr_);
    }

    constexpr explicit operator bool() const noexcept { return static_cast<bool>(fptr_); }

private:
    struct ImplBase
    {
        constexpr virtual Ret invoke(Args...) = 0;
        constexpr virtual void small_unique_ptr_move(void* dst) noexcept = 0;
        constexpr virtual ~ImplBase() = default;
    };

    template<typename Callable>
    struct Impl : public ImplBase
    {
        constexpr Impl(Callable func) noexcept(std::is_nothrow_move_constructible_v<Callable>) :
            func_(std::move(func))
        {}

        constexpr void small_unique_ptr_move(void* dst) noexcept override
        {
            std::construct_at(static_cast<Impl*>(dst), std::move(*this));
        }

        constexpr Ret invoke(Args... args) override
        {
            return std::invoke(func_, std::forward<Args>(args)...);
        }

        Callable func_;
    };

    smp::small_unique_ptr<ImplBase> fptr_ = nullptr;
};
```

--------------------------------------------------------------------------------------------------

#### Synopsis

```cpp
#include <small_unique_ptr.hpp>

namespace smp
{
    template<typename T, std::size_t Size = /*...*/>
    class small_unique_ptr
    {
        using element_type = std::remove_extent_t<T>;
        using pointer      = std::remove_extent_t<T>*;
        using reference    = std::remove_extent_t<T>&;

        // constructors
        constexpr small_unique_ptr() noexcept;
        constexpr small_unique_ptr(std::nullptr_t) noexcept;
        constexpr small_unique_ptr(small_unique_ptr&& other) noexcept;

        template<typename U>
        constexpr small_unique_ptr(small_unique_ptr<U, Size>&& other) noexcept;

        // assignment operators
        constexpr small_unique_ptr& operator=(small_unique_ptr&& other) noexcept;
        constexpr small_unique_ptr& operator=(std::nullptr_t) noexcept;

        template<typename U>
        constexpr small_unique_ptr& operator=(small_unique_ptr<U, Size>&& other) noexcept;

        // destructor
        constexpr ~small_unique_ptr() noexcept;

        // modifiers
        constexpr pointer release() noexcept = delete;
        constexpr void reset(pointer new_data = pointer{}) noexcept;
        constexpr void swap(small_unique_ptr& other) noexcept;

        // observers
        constexpr pointer get() const noexcept;
        constexpr explicit operator bool() const noexcept;

        // non-array version, small_unique_ptr<T>
        constexpr reference operator*() const noexcept(/*...*/) requires(!std::is_array_v<T>);
        constexpr pointer operator->() const noexcept requires(!std::is_array_v<T>);

        // array version, small_unique_ptr<T[]>
        constexpr reference operator[](std::size_t idx) const requires(std::is_array_v<T>);

        // stack buffer details
        constexpr bool is_stack_allocated() const noexcept;
        static constexpr bool is_always_heap_allocated() noexcept;
        static constexpr std::size_t stack_buffer_size() noexcept;
        static constexpr std::size_t stack_array_size() noexcept requires(std::is_array_v<T>);

        // comparisons
        constexpr bool operator==(std::nullptr_t) const noexcept;
        constexpr std::strong_ordering operator<=>(std::nullptr_t) const noexcept;

        template<typename U>
        constexpr bool operator==(const small_unique_ptr<U>& rhs) const noexcept;

        template<typename U>
        constexpr std::strong_ordering operator<=>(const small_unique_ptr<U, Size>& rhs) const noexcept;

        // streams support
        template<typename CharT, typename Traits>
        friend std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, const small_unique_ptr& p);

        // swap
        constexpr friend void swap(small_unique_ptr& lhs, small_unique_ptr& rhs) noexcept;
    };

    // make_unique factory functions
    template<typename T, std::size_t Size = /*...*/, typename... Args>
    constexpr small_unique_ptr<T, Size> make_unique_small(Args&&... args) noexcept(/*...*/) requires(!std::is_array_v<T>);

    template<typename T, std::size_t Size = /*...*/>
    constexpr small_unique_ptr<T, Size> make_unique_small(std::size_t count) requires(std::is_unbounded_array_v<T>);

    template<typename T, std::size_t Size = /*...*/, typename... Args>
    constexpr small_unique_ptr<T, Size> make_unique_small(Args&&...) requires(std::is_bounded_array_v<T>) = delete;

    // make_unique_for_overwrite factory functions
    template<typename T, std::size_t Size = /*...*/>
    constexpr small_unique_ptr<T, Size> make_unique_small_for_overwrite() noexcept(/*...*/) requires(!std::is_array_v<T>);

    template<typename T, std::size_t Size = /*...*/>
    constexpr small_unique_ptr<T, Size> make_unique_small_for_overwrite(std::size_t count) requires(std::is_unbounded_array_v<T>);

    template<typename T, std::size_t Size = /*...*/, typename... Args>
    constexpr small_unique_ptr<T, Size> make_unique_small_for_overwrite(Args&&...) requires(std::is_bounded_array_v<T>) = delete;

} // namespace smp

namespace std
{
    // hash support
    template<typename T, std::size_t Size>
    struct hash<smp::small_unique_ptr<T, Size>>;

} // namespace std
```
