[![sanitizers](https://github.com/KRM7/small_unique_ptr/actions/workflows/sanitizers.yml/badge.svg)](https://github.com/KRM7/small_unique_ptr/actions/workflows/sanitizers.yml)

### `small_unique_ptr<T>`

A constexpr `unique_ptr` implementation in C++20 with small object optimization.

```cpp
smp::small_unique_ptr<Base> p = smp::make_unique_small<Derived>();
```

Objects created with `make_unique_small<T>` will be stored directly within the
`small_unique_ptr` object, avoiding a dynamic allocation, if:

 - The size of `T` is not greater than the size of the internal buffer
 - The required alignment of `T` is not greater than the alignment of the internal buffer
 - `T` is nothrow move constructible

If any of these conditions is not met, the object will be allocated dynamically, and the
`small_unique_ptr` object will only hold a pointer to the object.

--------------------------------------------------------------------------------------------------

#### Interface

The interface matches `std::unique_ptr<T>`, with the following differences:

 - There is no `Deleter` template parameter and the associated methods are not implemented
   (i.e. `get_deleter` and the constructors with a deleter parameter). The `small_unique_ptr`
   class is only intended to be used for memory management.

 - There is a `Size` template parameter that specifies the size of the `small_unique_ptr` object,
   and implicitly, the size of the internal buffer.

   The value of `Size` may be any positive multiple of `sizeof(T*)`, but it is recommended
   to choose a power-of-2 value, in order to guarantee that objects will never be dynamically
   allocated because of alignment issues.

 - `T` can't be an incomplete type, as the layout of `T` is used to determine the layout
   of `small_unique_ptr<T>`.

 - Constructors from pointers are not provided except for the `nullptr` constructor, as these
   would not be able to make use of the internal buffer. The `make_unique_small` functions
   should be used instead to create objects.

 - `release()` is not implemented due to the possible internal allocation. This means that
   `small_unique_ptr` can't release ownership of the managed object.

 - There are some additional methods that don't exist for `std::unique_ptr`. These can be used
   to query the properties of the internal buffer, and to check where the objects are allocated
   (`is_stack_allocated`, `is_always_heap_allocated`, `stack_buffer_size`, `stack_array_size`).

Everything is constexpr, but the internal buffer is not used in constant evaluated contexts,
so any constexpr usage is subject to the same transient allocation requirements that a constexpr
`std::unique_ptr` object would be.

Unlike `std::unique_ptr`, `small_unique_ptr` objects are not guaranteed to be null after being
moved. When the pointed-to object is allocated in the internal buffer, the `small_unique_ptr`
object will point to the moved-from object after a move instead of being set to null.

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
        constexpr void reset(pointer new_data = pointer()) noexcept;
        constexpr void swap(small_unique_ptr& other) noexcept;

        // observers
        constexpr pointer get() const noexcept;
        constexpr explicit operator bool() const noexcept;

        // single-object version, small_unique_ptr<T>
        constexpr reference operator*() const noexcept(/*...*/) requires(!std::is_array_v<T>);
        constexpr pointer operator->() const noexcept requires(!std::is_array_v<T>);

        // array version, small_unique_ptr<T[]>
        constexpr reference operator[](std::size_t idx) const requires(std::is_array_v<T>);

        // internal buffer details
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

#### Benchmarks

The below benchmark results compare various operations using `std::unique_ptr` and `small_unique_ptr`.
The benchmarks were compiled using clang-20.

```
-----------------------------------------------------------------------------------------------
Benchmark                                                     Time             CPU   Iterations
-----------------------------------------------------------------------------------------------
// make_unique
bm_make_unique<SmallDerived>                               9.26 ns         8.80 ns     79786082
bm_make_unique_small<SmallDerived>                         1.20 ns         1.16 ns    602953438
bm_make_unique_small_cast<SmallDerived>                    1.00 ns        0.965 ns    725237541

bm_make_unique<LargeDerived>                               9.90 ns         9.14 ns     76926796
bm_make_unique_small<LargeDerived>                         9.84 ns         9.08 ns     77247857
bm_make_unique_small_cast<LargeDerived>                    10.0 ns         9.25 ns     75674939

// move construct twice
bm_move_construct2_unique_ptr<SmallDerived>               0.696 ns        0.666 ns   1069598793
bm_move_construct2_unique_ptr<LargeDerived>               0.684 ns        0.655 ns   1070704753

bm_move_construct2_small_unique_ptr<SmallDerived>          4.66 ns         4.37 ns    160723716
bm_move_construct2_small_unique_ptr<LargeDerived>          1.49 ns         1.40 ns    500367413

// move assign twice
bm_move_assign2_unique_ptr<SmallDerived>                  0.856 ns        0.802 ns    886261055
bm_move_assign2_unique_ptr<LargeDerived>                  0.823 ns        0.779 ns    887713162

bm_move_assign2_small_unique_ptr<SmallDerived>             4.31 ns         4.16 ns    168566775
bm_move_assign2_small_unique_ptr<LargeDerived>             1.14 ns         1.10 ns    621560330

// swap
bm_swap_unique_ptr<SmallDerived>                          0.702 ns        0.652 ns   1070302024
bm_swap_unique_ptr<LargeDerived>                          0.648 ns        0.598 ns   1154519200

bm_swap_small_unique_ptr<SmallDerived, SmallDerived>       5.01 ns         4.63 ns    151582740
bm_swap_small_unique_ptr<LargeDerived, LargeDerived>       1.69 ns         1.62 ns    429720185
bm_swap_small_unique_ptr<SmallDerived, LargeDerived>       2.76 ns         2.64 ns    262193398
```

In the above benchmarks, `SmallDerived` is a type which fits in the inline buffer of
`small_unique_ptr`, while `LargeDerived` is a type which does not.

The overhead of the move and swap operations comes from having to perform an indirect
function call to move the objects when they are allocated in the inline buffer. This could
be mostly eliminated by only allocating trivially relocatable objects in the inline buffer
and using memcpy, but the library currently doesn't implement this, as it would require C++26
and reflection to detect trivially relocatable types.

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
    requires(!std::is_same_v<std::remove_reference_t<F>, move_only_function> && std::is_invocable_r_v<Ret, F&, Args...>)
    constexpr move_only_function(F&& f) :
        fptr_(smp::make_unique_small<Impl<std::decay_t<F>>>(std::forward<F>(f)))
    {}

    template<typename F>
    requires(!std::is_same_v<std::remove_reference_t<F>, move_only_function> && std::is_invocable_r_v<Ret, F&, Args...>)
    constexpr move_only_function& operator=(F&& f)
    {
        fptr_ = smp::make_unique_small<Impl<std::decay_t<F>>>(std::forward<F>(f));
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
