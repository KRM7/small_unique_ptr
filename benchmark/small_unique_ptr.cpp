#include <small_unique_ptr.hpp>
#include <benchmark/benchmark.h>
#include <memory>


struct Base { virtual ~Base() = default; };

struct SmallDerived : public Base
{
    int n_;
};

struct LargeDerived : public Base
{
    int n_[16];
};

/* --------------------------------------------------------------------------------------------- */

template<typename T>
void bm_make_unique(benchmark::State& state)
{
    for (auto _ : state)
    {
        std::unique_ptr<Base> ptr = std::make_unique<T>();
        benchmark::DoNotOptimize(ptr);
    }
}


template<typename T>
void bm_make_unique_small(benchmark::State& state)
{
    for (auto _ : state)
    {
        smp::small_unique_ptr<Base> ptr = smp::make_unique_small<T, Base>();
        benchmark::DoNotOptimize(ptr);
    }
}

template<typename T>
void bm_make_unique_small_cast(benchmark::State& state)
{
    for (auto _ : state)
    {
        smp::small_unique_ptr<Base> ptr = smp::make_unique_small<T>();
        benchmark::DoNotOptimize(ptr);
    }
}

BENCHMARK(bm_make_unique<SmallDerived>);
BENCHMARK(bm_make_unique_small<SmallDerived>);
BENCHMARK(bm_make_unique_small_cast<SmallDerived>);

BENCHMARK(bm_make_unique<LargeDerived>);
BENCHMARK(bm_make_unique_small<LargeDerived>);
BENCHMARK(bm_make_unique_small_cast<LargeDerived>);

/* --------------------------------------------------------------------------------------------- */

template<typename T>
void bm_move_construct2_unique_ptr(benchmark::State& state)
{
    std::unique_ptr<Base> p1 = std::make_unique<T>();
    std::unique_ptr<Base> p2;

    for (auto _ : state)
    {
        std::construct_at(&p2, std::move(p1));
        std::destroy_at(&p1);

        benchmark::DoNotOptimize(p1);
        benchmark::DoNotOptimize(p2);

        std::construct_at(&p1, std::move(p2));
        std::destroy_at(&p2);

        benchmark::DoNotOptimize(p1);
        benchmark::DoNotOptimize(p2);
    }
}

template<typename T>
void bm_move_construct2_small_unique_ptr(benchmark::State& state)
{
    smp::small_unique_ptr<Base> p1 = smp::make_unique_small<T>();
    smp::small_unique_ptr<Base> p2;

    for (auto _ : state)
    {
        std::construct_at(&p2, std::move(p1));
        std::destroy_at(&p1);

        benchmark::DoNotOptimize(p1);
        benchmark::DoNotOptimize(p2);

        std::construct_at(&p1, std::move(p2));
        std::destroy_at(&p2);

        benchmark::DoNotOptimize(p1);
        benchmark::DoNotOptimize(p2);
    }
}

BENCHMARK(bm_move_construct2_unique_ptr<SmallDerived>);
BENCHMARK(bm_move_construct2_unique_ptr<LargeDerived>);

BENCHMARK(bm_move_construct2_small_unique_ptr<SmallDerived>);
BENCHMARK(bm_move_construct2_small_unique_ptr<LargeDerived>);

/* --------------------------------------------------------------------------------------------- */

template<typename T>
void bm_move_assign2_unique_ptr(benchmark::State& state)
{
    std::unique_ptr<Base> left = std::make_unique<T>();
    std::unique_ptr<Base> right = std::make_unique<T>();

    for (auto _ : state)
    {
        right = std::move(left);

        benchmark::DoNotOptimize(left);
        benchmark::DoNotOptimize(right);

        left = std::move(right);

        benchmark::DoNotOptimize(left);
        benchmark::DoNotOptimize(right);
    }
}

template<typename T>
void bm_move_assign2_small_unique_ptr(benchmark::State& state)
{
    smp::small_unique_ptr<Base> left = smp::make_unique_small<T>();
    smp::small_unique_ptr<Base> right = smp::make_unique_small<T>();

    for (auto _ : state)
    {
        right = std::move(left);

        benchmark::DoNotOptimize(left);
        benchmark::DoNotOptimize(right);

        left = std::move(right);

        benchmark::DoNotOptimize(left);
        benchmark::DoNotOptimize(right);
    }
}

BENCHMARK(bm_move_assign2_unique_ptr<SmallDerived>);
BENCHMARK(bm_move_assign2_unique_ptr<LargeDerived>);

BENCHMARK(bm_move_assign2_small_unique_ptr<SmallDerived>);
BENCHMARK(bm_move_assign2_small_unique_ptr<LargeDerived>);

/* --------------------------------------------------------------------------------------------- */

template<typename T>
void bm_swap_unique_ptr(benchmark::State& state)
{
    std::unique_ptr<T> lhs = std::make_unique<T>();
    std::unique_ptr<T> rhs = std::make_unique<T>();

    for (auto _ : state)
    {
        benchmark::DoNotOptimize(lhs);
        benchmark::DoNotOptimize(rhs);

        using std::swap;
        swap(lhs, rhs);
    }
}


template<typename T, typename U = T>
void bm_swap_small_unique_ptr(benchmark::State& state)
{
    smp::small_unique_ptr<Base> lhs = smp::make_unique_small<T>();
    smp::small_unique_ptr<Base> rhs = smp::make_unique_small<U>();

    for (auto _ : state)
    {
        benchmark::DoNotOptimize(lhs);
        benchmark::DoNotOptimize(rhs);

        using std::swap;
        swap(lhs, rhs);
    }
}

BENCHMARK(bm_swap_unique_ptr<SmallDerived>);
BENCHMARK(bm_swap_unique_ptr<LargeDerived>);

BENCHMARK(bm_swap_small_unique_ptr<SmallDerived, SmallDerived>);
BENCHMARK(bm_swap_small_unique_ptr<LargeDerived, LargeDerived>);
BENCHMARK(bm_swap_small_unique_ptr<SmallDerived, LargeDerived>);

/* --------------------------------------------------------------------------------------------- */
