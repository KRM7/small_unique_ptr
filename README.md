A fully constexpr unique_ptr implementation in C++20 with small object optimization.

Objects created with `make_unique_small<T>` are allocated on the stack if:

 - Their size is not greater than `64 - 2 * sizeof(void*)`
 - Their required alignment is not greater than `64`
 - Their move constructor is declared as `noexcept`

The size of a `small_unique_ptr<T>` object is:

 - `64` if T can be allocated in the stack buffer
 - `sizeof(T*)` otherwise

The interface matches `std::unique_ptr<T>`, except for:

 - There is no `Deleter` template parameter or any of the associated methods
 - Constructors from pointers are not provided except for the `nullptr` constructor
 - `release()` is not implemented
 - `T` can't be an incomplete type or an array type
 - There are a couple of extra methods for checking where objects are allocated

The stack buffer is not used in constant evaluated contexts, so any constexpr usage
is subject to the same transient allocation requirements that `std::unique_ptr<T>` would be.
