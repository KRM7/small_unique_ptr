/* Copyright (c) 2024 Krisztián Rugási. Subject to the MIT License. */

#include <catch2/catch_test_macros.hpp>
#include <small_unique_ptr.hpp>
#include <functional>
#include <bit>
#include <utility>
#include <cstdint>

struct Base
{
    constexpr virtual int value() const { return 0; }
    constexpr virtual void value(int) {}
    constexpr virtual int padding() const { return 0; }
    constexpr virtual ~Base() noexcept {};
};

template<size_t Padding>
struct Derived : Base
{
public:
    constexpr Derived() = default;
    constexpr Derived(int n) noexcept : value_(n) {}
    constexpr ~Derived() noexcept override {}
    constexpr int value() const override { return value_; }
    constexpr void value(int n) override { value_ = n; }
    constexpr int padding() const override { return Padding; }
private:
    unsigned char padding_[Padding] = {};
    int value_ = Padding;
};

using SmallDerived = Derived<32>;
using LargeDerived = Derived<64>;


TEST_CASE("object_size", "[small_unique_ptr]")
{
    STATIC_REQUIRE(sizeof(small_unique_ptr<SmallDerived>) == detail::cache_line_size);
    STATIC_REQUIRE(sizeof(small_unique_ptr<LargeDerived>) == sizeof(void*));

    STATIC_REQUIRE(alignof(small_unique_ptr<SmallDerived>) == detail::cache_line_size);
    STATIC_REQUIRE(alignof(small_unique_ptr<LargeDerived>) == alignof(void*));
}

TEST_CASE("construction", "[small_unique_ptr]")
{
    STATIC_REQUIRE( std::invoke([]{ (void) small_unique_ptr<Base>(nullptr);  return true; }) );
    STATIC_REQUIRE( std::invoke([]{ (void) small_unique_ptr<SmallDerived>(); return true; }) );
    STATIC_REQUIRE( std::invoke([]{ (void) small_unique_ptr<LargeDerived>(); return true; }) );

    STATIC_REQUIRE( std::invoke([]{ (void) make_unique_small<double>(3.0);    return true; }) );
    STATIC_REQUIRE( std::invoke([]{ (void) make_unique_small<Base>();         return true; }) );
    STATIC_REQUIRE( std::invoke([]{ (void) make_unique_small<SmallDerived>(); return true; }) );
    STATIC_REQUIRE( std::invoke([]{ (void) make_unique_small<LargeDerived>(); return true; }) );

    REQUIRE_NOTHROW(make_unique_small<Base>());
    REQUIRE_NOTHROW(make_unique_small<SmallDerived>());
    REQUIRE_NOTHROW(make_unique_small<LargeDerived>());
}

TEST_CASE("comparisons", "[small_unique_ptr]")
{
    STATIC_REQUIRE(small_unique_ptr<int>(nullptr) == nullptr);
    STATIC_REQUIRE(small_unique_ptr<int>(nullptr) <= nullptr);

    STATIC_REQUIRE(make_unique_small<SmallDerived>() != nullptr);
    STATIC_REQUIRE(make_unique_small<LargeDerived>() != nullptr);

    STATIC_REQUIRE(make_unique_small<LargeDerived>() != make_unique_small<Base>());
    STATIC_REQUIRE(make_unique_small<Base>() != make_unique_small<SmallDerived>());
}

TEST_CASE("bool_conversion", "[small_unique_ptr]")
{
    STATIC_REQUIRE(!small_unique_ptr<SmallDerived>());
    STATIC_REQUIRE(!small_unique_ptr<LargeDerived>(nullptr));

    STATIC_REQUIRE(make_unique_small<SmallDerived>());
    STATIC_REQUIRE(make_unique_small<LargeDerived>());

    REQUIRE(make_unique_small<SmallDerived>());
    REQUIRE(make_unique_small<LargeDerived>());
}

TEST_CASE("get", "[small_unique_ptr]")
{
    STATIC_REQUIRE(small_unique_ptr<SmallDerived>(nullptr).get() == nullptr);
    STATIC_REQUIRE(small_unique_ptr<LargeDerived>().get() == nullptr);

    STATIC_REQUIRE(make_unique_small<SmallDerived>().get() != nullptr);
    STATIC_REQUIRE(make_unique_small<LargeDerived>().get() != nullptr);

    REQUIRE(make_unique_small<SmallDerived>().get() != nullptr);
    REQUIRE(make_unique_small<LargeDerived>().get() != nullptr);
}

TEST_CASE("dereference", "[small_unique_ptr]")
{
    STATIC_REQUIRE((*make_unique_small<SmallDerived>()).padding() == 32);
    STATIC_REQUIRE((*make_unique_small<LargeDerived>()).padding() == 64);

    STATIC_REQUIRE(make_unique_small<SmallDerived>()->padding() == 32);
    STATIC_REQUIRE(make_unique_small<LargeDerived>()->padding() == 64);

    const auto p1 = make_unique_small<SmallDerived>();
    const auto p2 = make_unique_small<LargeDerived>();

    REQUIRE((*p1).value() == 32);
    REQUIRE((*p2).value() == 64);

    REQUIRE(p1->value() == 32);
    REQUIRE(p2->value() == 64);
}

TEST_CASE("move_construct_plain", "[small_unique_ptr]")
{
    STATIC_REQUIRE(32 == std::invoke([] { small_unique_ptr<Base> p = make_unique_small<SmallDerived>(); return p->padding(); }));
    STATIC_REQUIRE(64 == std::invoke([] { small_unique_ptr<Base> p = make_unique_small<LargeDerived>(); return p->padding(); }));

    small_unique_ptr<Base> base1 = make_unique_small<SmallDerived>();
    small_unique_ptr<Base> base2 = make_unique_small<LargeDerived>();

    REQUIRE(base1->value() == 32);
    REQUIRE(base2->value() == 64);

    small_unique_ptr<const Base> cbase = std::move(base1);

    REQUIRE(cbase->value() == 32);
}

TEST_CASE("move_assignment_plain", "[small_unique_ptr]")
{
    STATIC_REQUIRE(32 == std::invoke([] { small_unique_ptr<Base> p; p = make_unique_small<SmallDerived>(); return p->padding(); }));
    STATIC_REQUIRE(64 == std::invoke([] { small_unique_ptr<Base> p; p = make_unique_small<LargeDerived>(); return p->padding(); }));

    small_unique_ptr<const Base> base;

    base = make_unique_small<SmallDerived>();
    REQUIRE(base->value() == 32);

    base = make_unique_small<LargeDerived>();
    REQUIRE(base->value() == 64);

    base = nullptr;
    REQUIRE(!base);
}

TEST_CASE("swap_large", "[small_unique_ptr]")
{
    small_unique_ptr<Base> p1 = nullptr;
    small_unique_ptr<Base> p2 = make_unique_small<LargeDerived>();

    using std::swap;
    swap(p1, p2);

    REQUIRE(p2 == nullptr);
    REQUIRE(p1->value() == 64);
}

TEST_CASE("swap_mixed", "[small_unique_ptr]")
{
    using std::swap;

    small_unique_ptr<Base> p1 = make_unique_small<SmallDerived>();
    small_unique_ptr<Base> p2 = make_unique_small<LargeDerived>();

    REQUIRE(p1->value() == 32);
    REQUIRE(p2->value() == 64);

    swap(p1, p2);

    REQUIRE(p1->value() == 64);
    REQUIRE(p2->value() == 32);

    swap(p1, p2);

    REQUIRE(p1->value() == 32);
    REQUIRE(p2->value() == 64);
}

TEST_CASE("swap_small", "[small_unique_ptr]")
{
    small_unique_ptr<Base> p1 = make_unique_small<SmallDerived>(1);
    small_unique_ptr<Base> p2 = make_unique_small<SmallDerived>(2);

    using std::swap;
    swap(p1, p2);

    REQUIRE(p1->value() == 2);
    REQUIRE(p2->value() == 1);

    swap(p1, p2);

    REQUIRE(p1->value() == 1);
    REQUIRE(p2->value() == 2);
}

TEST_CASE("swap_constexpr", "[small_unique_ptr]")
{
    using std::swap;

    STATIC_REQUIRE(32 == std::invoke([]
    {
        small_unique_ptr<Base> p1 = make_unique_small<SmallDerived>();
        small_unique_ptr<Base> p2 = make_unique_small<LargeDerived>();

        swap(p1, p2);

        return p2->padding();
    }));

    STATIC_REQUIRE(32 == std::invoke([]
    {
        small_unique_ptr<Base> p1 = make_unique_small<SmallDerived>();
        small_unique_ptr<Base> p2 = make_unique_small<SmallDerived>();

        swap(p1, p2);

        return p2->padding();
    }));

    STATIC_REQUIRE(64 == std::invoke([]
    {
        small_unique_ptr<Base> p1 = make_unique_small<LargeDerived>();
        small_unique_ptr<Base> p2 = make_unique_small<LargeDerived>();

        swap(p1, p2);

        return p2->padding();
    }));
}

struct A { virtual ~A() = default; };
struct C { virtual ~C() = default; };
struct B { char b = 2; virtual char value() const { return b; } virtual ~B() = default; };
struct D : A, B { char d = 5; char value() const override { return d; } };
struct E : C, D { char e = 6; char value() const override { return e; } };

TEST_CASE("move_construct_multiple_inheritance", "[small_unique_ptr]")
{
    small_unique_ptr<D> derived = make_unique_small<E>();
    REQUIRE(derived->b == 2);
    REQUIRE(derived->value() == 6);

    small_unique_ptr<B> base = std::move(derived);
    REQUIRE(base->b == 2);
    REQUIRE(base->value() == 6);

    small_unique_ptr<const B> cbase = std::move(base);
    REQUIRE(cbase->b == 2);
    REQUIRE(cbase->value() == 6);
}

TEST_CASE("move_assignment_multiple_inheritance", "[small_unique_ptr]")
{
    small_unique_ptr<B> p1;

    p1 = make_unique_small<B>();
    REQUIRE(p1->value() == 2);

    p1 = make_unique_small<D>();
    REQUIRE(p1->value() == 5);

    p1 = make_unique_small<B>();
    REQUIRE(p1->value() == 2);

    p1 = make_unique_small<E>();
    REQUIRE(p1->value() == 6);
}

TEST_CASE("swap_multiple_inheritance", "[small_unique_ptr]")
{
    using std::swap;

    small_unique_ptr<B> p1 = make_unique_small<D>();
    small_unique_ptr<B> p2 = make_unique_small<E>();

    REQUIRE(p1->value() == 5);
    REQUIRE(p2->value() == 6);

    swap(p1, p2);

    REQUIRE(p1->value() == 6);
    REQUIRE(p2->value() == 5);

    swap(p1, p2);

    REQUIRE(p1->value() == 5);
    REQUIRE(p2->value() == 6);

    p1 = nullptr;
    swap(p1, p2);

    REQUIRE(p1->value() == 6);
    REQUIRE(p2 == nullptr);

    swap(p1, p2);

    REQUIRE(p1 == nullptr);
    REQUIRE(p2->value() == 6);

    swap(p1, p2);

    REQUIRE(p1->value() == 6);
    REQUIRE(p2 == nullptr);
}

TEST_CASE("virtual_inheritance", "[small_unique_ptr]")
{
    struct VBase { char a = 1; virtual char value() const { return a; } virtual ~VBase() = default; };
    struct VMiddle1 : virtual VBase { char value() const override { return 2; } };
    struct VMiddle2 : virtual VBase { char value() const override { return 3; } };
    struct VDerived : VMiddle1, VMiddle2 { char d = 4; char value() const override { return d; } };

    small_unique_ptr<VMiddle1> middle1 = make_unique_small<VDerived>();
    REQUIRE(middle1->a == 1);
    REQUIRE(middle1->value() == 4);

    small_unique_ptr<VBase> base1 = std::move(middle1);
    REQUIRE(base1->a == 1);
    REQUIRE(base1->value() == 4);


    small_unique_ptr<VMiddle1> middle2; middle2 = make_unique_small<VDerived>();
    REQUIRE(middle2->a == 1);
    REQUIRE(middle2->value() == 4);

    small_unique_ptr<VBase> base2 = std::move(middle2);
    REQUIRE(base2->a == 1);
    REQUIRE(base2->value() == 4);
}

TEST_CASE("abstract_base", "[small_unique_ptr]")
{
    struct ABase { virtual int value() const = 0; virtual ~ABase() = default; };
    struct ADerived : ABase { int value() const override { return 12; } };

    small_unique_ptr<ABase> p = make_unique_small<ADerived>();

    REQUIRE(p->value() == 12);
}

TEST_CASE("poly_alignment", "[small_unique_ptr]")
{
    struct alignas(16) SmallAlign { virtual ~SmallAlign() = default; };
    struct alignas(128) LargeAlign : SmallAlign {};

    small_unique_ptr<SmallAlign> p = make_unique_small<LargeAlign>();

    REQUIRE(!(std::bit_cast<std::uintptr_t>(std::addressof(*p)) % alignof(LargeAlign)));
}

TEST_CASE("const_unique_ptr", "[small_unique_ptr]")
{
    const small_unique_ptr<int> p = make_unique_small<int>(3);

    *p = 2;

    REQUIRE(*p == 2);
}
