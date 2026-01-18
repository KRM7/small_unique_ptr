#ifndef MOVE_ONLY_FUNCTION_HPP
#define MOVE_ONLY_FUNCTION_HPP

#include <functional>
#include <memory>
#include <type_traits>
#include <utility>
#include <cstddef>
#include "small_unique_ptr.hpp"

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

#endif // !MOVE_ONLY_FUNCTION_HPP
