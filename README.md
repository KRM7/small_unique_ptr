A fully constexpr unique_ptr implementation in C++20 with small object optimization.

Objects created with `make_unique_small<T>` are allocated on the stack if:

 - Their size is not greater than `64 - 2 * sizeof(void*)`
 - Their required alignment is not greater than `64`
 - Their move constructor is declared as `noexcept`

The size of a `small_unique_ptr<T>` object is:

 - `64` if T can be allocated in the stack buffer
 - `sizeof(T*)` otherwise

The interface matches `std::unique_ptr<T>`, except for:

 - There is no deleter template parameter and associated methods
 - Constructors from T* are not provided
 - `release()` is not implemented
 - The type of T can't be an array type or `void`

The stack buffer is not used in constant evaluated contexts, so any constexpr usage
is subject to the same transient allocation requirements that `std::unique_ptr<T>` would be.
