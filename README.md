
### `small_unique_ptr<T>`

A constexpr `unique_ptr` implementation in C++20 with small object optimization.

```cpp
smp::small_unique_ptr<Base> p = smp::make_unique_small<Derived>();
```

Objects created with `make_unique_small<T>` are allocated on the stack if:

 - Their size is not greater than the size of the internal stack buffer
 - Their alignment is not greater than the alignment of the stack buffer
 - Their move constructor is `noexcept`

The size of the stack buffer depends on the architecture and the overall size of the
`small_unique_ptr` object, but the default values on 64 bit architectures will typically be:

 - 48 bytes for polymorphic types
 - 56 bytes for polymorphic types that implement a virtual `small_unique_ptr_move` method
 - `sizeof(T)` for non-polymophic types, with an upper limit of 56 bytes
 - 56 bytes for array types (rounded down to a multiple of the element size)

The overall size of a `small_unique_ptr<T, Size>` object for a polymorphic type is:

 - `Size` if `T` may be allocated in the stack buffer (64 bytes by default)
 - `sizeof(T*)` otherwise

The interface matches `std::unique_ptr`, except for:

 - There is no `Deleter` template parameter or any of the associated methods
 - There is a `Size` template parameter that specifies the (maximum) size of the `small_unique_ptr` object
 - Constructors from pointers are not provided except for the nullptr constructor
 - `release()` is not implemented
 - `T` can't be an incomplete type
 - There are a couple of extra methods for checking where objects are allocated

Everything is constexpr, but the stack buffer is not used in constant evaluated contexts,
so any constexpr usage is subject to the same transient allocation requirements that a constexpr
`std::unique_ptr` would be.

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

