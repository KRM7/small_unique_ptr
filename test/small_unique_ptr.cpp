/* Copyright (c) 2024 Krisztián Rugási. Subject to the MIT License. */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_template_test_macros.hpp>
#include <small_unique_ptr.hpp>
#include <functional>
#include <bit>
#include <utility>
#include <cstdint>

using namespace smp;

struct SmallPOD { char dummy_; };
struct LargePOD { char dummy_[128]; };

struct Base
{
    constexpr virtual int value() const { return 0; }
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
    constexpr int padding() const override { return Padding; }

private:
    unsigned char padding_[Padding] = {};
    int value_ = Padding;
};

using SmallDerived = Derived<32>;
using LargeDerived = Derived<64>;

struct BaseIntrusive
{
    constexpr virtual int value() const { return 0; }
    constexpr virtual int padding() const { return 0; }
    constexpr virtual ~BaseIntrusive() noexcept {};

    virtual void small_unique_ptr_move(void* dst) noexcept
    {
        std::construct_at(static_cast<BaseIntrusive*>(dst), std::move(*this));
    }
};

template<size_t Padding>
struct DerivedIntrusive : BaseIntrusive
{
public:
    constexpr DerivedIntrusive() = default;
    constexpr DerivedIntrusive(int n) noexcept : value_(n) {}
    constexpr ~DerivedIntrusive() noexcept override {}
    constexpr int value() const override { return value_; }
    constexpr int padding() const override { return Padding; }

    void small_unique_ptr_move(void* dst) noexcept override
    {
        std::construct_at(static_cast<DerivedIntrusive*>(dst), std::move(*this));
    }

private:
    unsigned char padding_[Padding] = {};
    int value_ = Padding;
};

using SmallIntrusive = DerivedIntrusive<32>;
using LargeIntrusive = DerivedIntrusive<64>;


TEST_CASE("object_layout", "[small_unique_ptr]")
{
    STATIC_REQUIRE(std::is_standard_layout_v<small_unique_ptr<SmallPOD>>);
    STATIC_REQUIRE(std::is_standard_layout_v<small_unique_ptr<LargePOD>>);

    STATIC_REQUIRE(std::is_standard_layout_v<small_unique_ptr<SmallPOD[]>>);
    STATIC_REQUIRE(std::is_standard_layout_v<small_unique_ptr<LargePOD[]>>);

    STATIC_REQUIRE(std::is_standard_layout_v<small_unique_ptr<Base>>);
    STATIC_REQUIRE(std::is_standard_layout_v<small_unique_ptr<SmallDerived>>);
    STATIC_REQUIRE(std::is_standard_layout_v<small_unique_ptr<LargeDerived>>);

    STATIC_REQUIRE(std::is_standard_layout_v<small_unique_ptr<BaseIntrusive>>);
    STATIC_REQUIRE(std::is_standard_layout_v<small_unique_ptr<SmallIntrusive>>);
    STATIC_REQUIRE(std::is_standard_layout_v<small_unique_ptr<LargeIntrusive>>);
}

TEST_CASE("object_size_default", "[small_unique_ptr]")
{
    STATIC_REQUIRE(sizeof(small_unique_ptr<SmallPOD>) <= 2 * sizeof(void*));
    STATIC_REQUIRE(sizeof(small_unique_ptr<LargePOD>) == sizeof(void*));

    STATIC_REQUIRE(alignof(small_unique_ptr<SmallPOD>) == alignof(void*));
    STATIC_REQUIRE(alignof(small_unique_ptr<LargePOD>) == alignof(void*));


    STATIC_REQUIRE(sizeof(small_unique_ptr<SmallPOD[]>) == detail::default_small_ptr_size);
    STATIC_REQUIRE(sizeof(small_unique_ptr<LargePOD[]>) == sizeof(void*));

    STATIC_REQUIRE(alignof(small_unique_ptr<SmallPOD[]>) == alignof(void*));
    STATIC_REQUIRE(alignof(small_unique_ptr<LargePOD[]>) == alignof(void*));


    STATIC_REQUIRE(sizeof(small_unique_ptr<SmallDerived>) == detail::default_small_ptr_size);
    STATIC_REQUIRE(sizeof(small_unique_ptr<LargeDerived>) == sizeof(void*));

    STATIC_REQUIRE(alignof(small_unique_ptr<SmallDerived>) == detail::default_small_ptr_size);
    STATIC_REQUIRE(alignof(small_unique_ptr<LargeDerived>) == alignof(void*));


    STATIC_REQUIRE(sizeof(small_unique_ptr<SmallIntrusive>) == detail::default_small_ptr_size);
    STATIC_REQUIRE(sizeof(small_unique_ptr<LargeIntrusive>) == sizeof(void*));

    STATIC_REQUIRE(alignof(small_unique_ptr<SmallIntrusive>) == detail::default_small_ptr_size);
    STATIC_REQUIRE(alignof(small_unique_ptr<LargeIntrusive>) == alignof(void*));
}

TEMPLATE_TEST_CASE("object_size_custom", "[small_unique_ptr]", Base, BaseIntrusive)
{
    STATIC_REQUIRE(sizeof(small_unique_ptr<TestType, 8>) == 8);
    STATIC_REQUIRE(sizeof(small_unique_ptr<TestType, 16>) <= 16); // Base will be always heap allocated on 64 bit arch
    STATIC_REQUIRE(sizeof(small_unique_ptr<TestType, 24>) == 24);
    STATIC_REQUIRE(sizeof(small_unique_ptr<TestType, 32>) == 32);
    STATIC_REQUIRE(sizeof(small_unique_ptr<TestType, 40>) == 40);
    STATIC_REQUIRE(sizeof(small_unique_ptr<TestType, 48>) == 48);
    STATIC_REQUIRE(sizeof(small_unique_ptr<TestType, 56>) == 56);
    STATIC_REQUIRE(sizeof(small_unique_ptr<TestType, 64>) == 64);
    STATIC_REQUIRE(sizeof(small_unique_ptr<TestType, 128>) == 128);
}

TEMPLATE_TEST_CASE("object_align_custom", "[small_unique_ptr]", Base, BaseIntrusive)
{
    STATIC_REQUIRE(alignof(small_unique_ptr<TestType, 8>) == 8);
    STATIC_REQUIRE(alignof(small_unique_ptr<TestType, 16>) <= 16); // Base will be always heap allocated on 64 bit arch
    STATIC_REQUIRE(alignof(small_unique_ptr<TestType, 24>) == 8);
    STATIC_REQUIRE(alignof(small_unique_ptr<TestType, 32>) == 32);
    STATIC_REQUIRE(alignof(small_unique_ptr<TestType, 40>) == 8);
    STATIC_REQUIRE(alignof(small_unique_ptr<TestType, 48>) == 16);
    STATIC_REQUIRE(alignof(small_unique_ptr<TestType, 56>) == 8);
    STATIC_REQUIRE(alignof(small_unique_ptr<TestType, 64>) == 64);
    STATIC_REQUIRE(alignof(small_unique_ptr<TestType, 128>) == 128);
}

TEST_CASE("object_align_custom_pod", "[small_unique_ptr]")
{
    static_assert(alignof(SmallPOD) <= alignof(SmallPOD*));

    STATIC_REQUIRE(alignof(small_unique_ptr<SmallPOD, 8>) == alignof(SmallPOD*));
    STATIC_REQUIRE(alignof(small_unique_ptr<SmallPOD, 16>) == alignof(SmallPOD*));
    STATIC_REQUIRE(alignof(small_unique_ptr<SmallPOD, 32>) == alignof(SmallPOD*));
    STATIC_REQUIRE(alignof(small_unique_ptr<SmallPOD, 64>) == alignof(SmallPOD*));
    STATIC_REQUIRE(alignof(small_unique_ptr<SmallPOD, 128>) == alignof(SmallPOD*));

    STATIC_REQUIRE(alignof(small_unique_ptr<SmallPOD[], 8>) == alignof(SmallPOD*));
    STATIC_REQUIRE(alignof(small_unique_ptr<SmallPOD[], 16>) == alignof(SmallPOD*));
    STATIC_REQUIRE(alignof(small_unique_ptr<SmallPOD[], 32>) == alignof(SmallPOD*));
    STATIC_REQUIRE(alignof(small_unique_ptr<SmallPOD[], 64>) == alignof(SmallPOD*));
    STATIC_REQUIRE(alignof(small_unique_ptr<SmallPOD[], 128>) == alignof(SmallPOD*));
}

TEST_CASE("stack_buffer_size", "[small_unique_ptr]")
{
    STATIC_REQUIRE(small_unique_ptr<SmallPOD>::stack_buffer_size() == sizeof(SmallPOD));
    STATIC_REQUIRE(small_unique_ptr<LargePOD>::stack_buffer_size() == 0);

    STATIC_REQUIRE(small_unique_ptr<SmallPOD[]>::stack_buffer_size() != 0);
    STATIC_REQUIRE(small_unique_ptr<LargePOD[]>::stack_buffer_size() == 0);

    STATIC_REQUIRE(small_unique_ptr<Base>::stack_buffer_size()          != 0);
    STATIC_REQUIRE(small_unique_ptr<BaseIntrusive>::stack_buffer_size() != 0);

    STATIC_REQUIRE(small_unique_ptr<LargeDerived>::stack_buffer_size()   == 0);
    STATIC_REQUIRE(small_unique_ptr<LargeIntrusive>::stack_buffer_size() == 0);

    STATIC_REQUIRE(small_unique_ptr<BaseIntrusive>::stack_buffer_size() > small_unique_ptr<Base>::stack_buffer_size());
    STATIC_REQUIRE(small_unique_ptr<SmallIntrusive>::stack_buffer_size() > small_unique_ptr<SmallDerived>::stack_buffer_size());

    CHECKED_IF(sizeof(void*) == 8)
    {
        REQUIRE(small_unique_ptr<Base>::stack_buffer_size()          == 48);
        REQUIRE(small_unique_ptr<BaseIntrusive>::stack_buffer_size() == 56);

        REQUIRE(small_unique_ptr<SmallDerived>::stack_buffer_size()   == 48);
        REQUIRE(small_unique_ptr<SmallIntrusive>::stack_buffer_size() == 56);

        REQUIRE(small_unique_ptr<SmallPOD[]>::stack_buffer_size() == 56);

        REQUIRE(small_unique_ptr<SmallPOD[]>::stack_array_size() == 56);
        REQUIRE(small_unique_ptr<LargePOD[]>::stack_array_size() == 0);

        REQUIRE(small_unique_ptr<SmallDerived[]>::stack_array_size() > 0);
    }
}

TEMPLATE_TEST_CASE("construct_scalar", "[small_unique_ptr]", SmallPOD, LargePOD, Base, SmallDerived, LargeDerived, BaseIntrusive, SmallIntrusive, LargeIntrusive)
{
    STATIC_REQUIRE( std::invoke([]{ (void) small_unique_ptr<TestType>();              return true; }) );
    STATIC_REQUIRE( std::invoke([]{ (void) small_unique_ptr<const TestType>();        return true; }) );

    STATIC_REQUIRE( std::invoke([]{ (void) small_unique_ptr<TestType>(nullptr);       return true; }) );
    STATIC_REQUIRE( std::invoke([]{ (void) small_unique_ptr<const TestType>(nullptr); return true; }) );
}

TEMPLATE_TEST_CASE("make_unique_scalar", "[small_unique_ptr]", SmallPOD, LargePOD, Base, SmallDerived, LargeDerived, BaseIntrusive, SmallIntrusive, LargeIntrusive)
{
    STATIC_REQUIRE( std::invoke([]{ (void) make_unique_small<TestType>();       return true; }) );
    STATIC_REQUIRE( std::invoke([]{ (void) make_unique_small<const TestType>(); return true; }) );

    (void) make_unique_small<TestType>();
    (void) make_unique_small<const TestType>();

    SUCCEED();
}

TEMPLATE_TEST_CASE("make_unique_for_overwrite_scalar", "[small_unique_ptr]", SmallPOD, LargePOD, Base, SmallDerived, LargeDerived, BaseIntrusive, SmallIntrusive, LargeIntrusive)
{
    STATIC_REQUIRE( std::invoke([]{ (void) make_unique_small_for_overwrite<TestType>(); return true; }) );

    (void) make_unique_small_for_overwrite<TestType>();

    SUCCEED();
}

TEMPLATE_TEST_CASE("construction_array", "[small_unique_ptr]", SmallPOD, LargePOD)
{
    STATIC_REQUIRE( std::invoke([]{ (void) small_unique_ptr<TestType[]>();              return true; }) );
    STATIC_REQUIRE( std::invoke([]{ (void) small_unique_ptr<const TestType[]>();        return true; }) );

    STATIC_REQUIRE( std::invoke([]{ (void) small_unique_ptr<TestType[]>(nullptr);       return true; }) );
    STATIC_REQUIRE( std::invoke([]{ (void) small_unique_ptr<const TestType[]>(nullptr); return true; }) );
}

TEMPLATE_TEST_CASE("make_unique_array", "[small_unique_ptr]", SmallPOD, LargePOD)
{
    STATIC_REQUIRE( std::invoke([]{ (void) make_unique_small<TestType[]>(2);       return true; }) );
    STATIC_REQUIRE( std::invoke([]{ (void) make_unique_small<const TestType[]>(2); return true; }) );

    (void) make_unique_small<TestType[]>(2);
    (void) make_unique_small<const TestType[]>(2);

    (void) make_unique_small<TestType[]>(0);

    SUCCEED();
}

TEST_CASE("make_unique_for_overwrite_array", "[small_unique_ptr]")
{
    STATIC_REQUIRE( std::invoke([]{ (void) make_unique_small_for_overwrite<SmallPOD[]>(2); return true; }) );
    STATIC_REQUIRE( std::invoke([]{ (void) make_unique_small_for_overwrite<LargePOD[]>(2); return true; }) );

    (void) make_unique_small_for_overwrite<SmallPOD[]>(2);
    (void) make_unique_small_for_overwrite<LargePOD[]>(2);

    (void) make_unique_small_for_overwrite<SmallPOD[]>(0);
    (void) make_unique_small_for_overwrite<LargePOD[]>(0);

    SUCCEED();
}

TEST_CASE("noexcept_construction", "[small_unique_ptr]")
{
    STATIC_REQUIRE(noexcept(make_unique_small<SmallDerived>()));
    STATIC_REQUIRE(!noexcept(make_unique_small<LargeDerived>()));

    STATIC_REQUIRE(noexcept(make_unique_small_for_overwrite<SmallDerived>()));
    STATIC_REQUIRE(!noexcept(make_unique_small_for_overwrite<LargeDerived>()));
}

TEST_CASE("is_always_heap_allocated", "[small_unique_ptr]")
{
    STATIC_REQUIRE(!small_unique_ptr<SmallPOD>::is_always_heap_allocated());
    STATIC_REQUIRE(small_unique_ptr<LargePOD>::is_always_heap_allocated());

    STATIC_REQUIRE(!small_unique_ptr<SmallPOD[]>::is_always_heap_allocated());
    STATIC_REQUIRE(small_unique_ptr<LargePOD[]>::is_always_heap_allocated());

    STATIC_REQUIRE(!small_unique_ptr<SmallDerived>::is_always_heap_allocated());
    STATIC_REQUIRE(small_unique_ptr<LargeDerived>::is_always_heap_allocated());

    STATIC_REQUIRE(!small_unique_ptr<SmallIntrusive>::is_always_heap_allocated());
    STATIC_REQUIRE(small_unique_ptr<LargeIntrusive>::is_always_heap_allocated());
}

TEST_CASE("is_stack_allocated", "[small_unique_ptr]")
{
    STATIC_REQUIRE( !std::invoke([]{ return make_unique_small<SmallDerived>().is_stack_allocated(); }) );
    STATIC_REQUIRE( !std::invoke([]{ return make_unique_small<LargeDerived>().is_stack_allocated(); }) );

    STATIC_REQUIRE( !std::invoke([]{ return make_unique_small<SmallIntrusive>().is_stack_allocated(); }) );
    STATIC_REQUIRE( !std::invoke([]{ return make_unique_small<LargeIntrusive>().is_stack_allocated(); }) );

    STATIC_REQUIRE( !std::invoke([]{ return make_unique_small<SmallPOD>().is_stack_allocated(); }) );
    STATIC_REQUIRE( !std::invoke([]{ return make_unique_small<LargePOD>().is_stack_allocated(); }) );

    STATIC_REQUIRE( !std::invoke([] { return make_unique_small<SmallPOD[]>(2).is_stack_allocated(); }) );
    STATIC_REQUIRE( !std::invoke([] { return make_unique_small<LargePOD[]>(2).is_stack_allocated(); }) );

    small_unique_ptr<Base> p1 = make_unique_small<SmallDerived>();
    small_unique_ptr<Base> p2 = make_unique_small<LargeDerived>();
    REQUIRE(p1.is_stack_allocated());
    REQUIRE(!p2.is_stack_allocated());

    small_unique_ptr<BaseIntrusive> p3 = make_unique_small<SmallIntrusive>();
    small_unique_ptr<BaseIntrusive> p4 = make_unique_small<LargeIntrusive>();
    REQUIRE(p3.is_stack_allocated());
    REQUIRE(!p4.is_stack_allocated());

    small_unique_ptr<SmallPOD> p5 = make_unique_small<SmallPOD>();
    small_unique_ptr<LargePOD> p6 = make_unique_small<LargePOD>();
    REQUIRE(p5.is_stack_allocated());
    REQUIRE(!p6.is_stack_allocated());

    small_unique_ptr<SmallPOD[]> p7 = make_unique_small<SmallPOD[]>(3);
    small_unique_ptr<LargePOD[]> p8 = make_unique_small<LargePOD[]>(1);
    REQUIRE(p7.is_stack_allocated());
    REQUIRE(!p8.is_stack_allocated());

    small_unique_ptr<SmallPOD> np(nullptr);
    REQUIRE(!np.is_stack_allocated());
}

TEST_CASE("comparisons", "[small_unique_ptr]")
{
    STATIC_REQUIRE(small_unique_ptr<SmallPOD>(nullptr) == nullptr);
    STATIC_REQUIRE(small_unique_ptr<LargePOD>(nullptr) == nullptr);

    STATIC_REQUIRE(make_unique_small<SmallDerived>() != nullptr);
    STATIC_REQUIRE(make_unique_small<LargeDerived>() != nullptr);

    STATIC_REQUIRE(make_unique_small<LargeDerived>() != make_unique_small<Base>());
    STATIC_REQUIRE(make_unique_small<Base>() != make_unique_small<SmallDerived>());
}

TEMPLATE_TEST_CASE("bool_conversion", "[small_unique_ptr]", Base, SmallDerived, LargeDerived, SmallPOD, LargePOD)
{
    STATIC_REQUIRE(!small_unique_ptr<TestType>());
    STATIC_REQUIRE(make_unique_small<TestType>());

    REQUIRE(!small_unique_ptr<TestType>());
    REQUIRE(make_unique_small<TestType>());
}

TEMPLATE_TEST_CASE("get", "[small_unique_ptr]", Base, SmallDerived, LargeDerived, SmallPOD, LargePOD)
{
    STATIC_REQUIRE(small_unique_ptr<TestType>().get() == nullptr);
    STATIC_REQUIRE(make_unique_small<TestType>().get() != nullptr);

    REQUIRE(small_unique_ptr<TestType>() == nullptr);
    REQUIRE(make_unique_small<TestType>().get() != nullptr);
}

TEST_CASE("dereference", "[small_unique_ptr]")
{
    STATIC_REQUIRE((*make_unique_small<SmallDerived>()).padding() == 32);
    STATIC_REQUIRE((*make_unique_small<LargeDerived>()).padding() == 64);

    STATIC_REQUIRE(make_unique_small<SmallDerived>()->padding() == 32);
    STATIC_REQUIRE(make_unique_small<LargeDerived>()->padding() == 64);

    const auto p0 = make_unique_small<const Base>();
    const auto p1 = make_unique_small<SmallDerived>();
    const auto p2 = make_unique_small<LargeDerived>();

    REQUIRE((*p0).value() == 0);
    REQUIRE((*p1).value() == 32);
    REQUIRE((*p2).value() == 64);

    REQUIRE(p0->value() == 0);
    REQUIRE(p1->value() == 32);
    REQUIRE(p2->value() == 64);
}

TEST_CASE("move_construct_plain", "[small_unique_ptr]")
{
    STATIC_REQUIRE(32 == std::invoke([] { small_unique_ptr<Base> p = make_unique_small<SmallDerived>(); return p->padding(); }));
    STATIC_REQUIRE(64 == std::invoke([] { small_unique_ptr<Base> p = make_unique_small<LargeDerived>(); return p->padding(); }));

    STATIC_REQUIRE(32 == std::invoke([] { small_unique_ptr<BaseIntrusive> p = make_unique_small<SmallIntrusive>(); return p->padding(); }));
    STATIC_REQUIRE(64 == std::invoke([] { small_unique_ptr<BaseIntrusive> p = make_unique_small<LargeIntrusive>(); return p->padding(); }));

    STATIC_REQUIRE( std::invoke([] { small_unique_ptr<const SmallPOD> p = make_unique_small<SmallPOD>(); return true; }) );
    STATIC_REQUIRE( std::invoke([] { small_unique_ptr<volatile LargePOD> p = make_unique_small<LargePOD>(); return true; }) );


    small_unique_ptr<Base> base1 = make_unique_small<SmallDerived>();
    small_unique_ptr<Base> base2 = make_unique_small<LargeDerived>();
    REQUIRE(base1->value() == 32);
    REQUIRE(base2->value() == 64);

    small_unique_ptr<const Base> cbase = std::move(base1);
    REQUIRE(cbase->value() == 32);
    REQUIRE(base1 == nullptr);


    small_unique_ptr<BaseIntrusive> ibase1 = make_unique_small<SmallIntrusive>();
    small_unique_ptr<BaseIntrusive> ibase2 = make_unique_small<LargeIntrusive>();
    REQUIRE(ibase1->value() == 32);
    REQUIRE(ibase2->value() == 64);

    small_unique_ptr<const BaseIntrusive> icbase = std::move(ibase1);
    REQUIRE(icbase->value() == 32);
    REQUIRE(ibase1 == nullptr);

    small_unique_ptr<const volatile SmallPOD> cpod1 = make_unique_small<SmallPOD>();
    small_unique_ptr<const volatile LargePOD> cpod2 = make_unique_small<LargePOD>();
    SUCCEED();
}

TEST_CASE("move_construct_array", "[small_unique_ptr]")
{
    STATIC_REQUIRE( std::invoke([] { small_unique_ptr<const SmallPOD[]> p = make_unique_small<SmallPOD[]>(4); return true; }) );
    STATIC_REQUIRE( std::invoke([] { small_unique_ptr<volatile LargePOD[]> p = make_unique_small<LargePOD[]>(2); return true; }) );

    small_unique_ptr<const volatile SmallPOD[]> cpod1 = make_unique_small<SmallPOD[]>(4);
    small_unique_ptr<const volatile LargePOD[]> cpod2 = make_unique_small<LargePOD[]>(2);

    small_unique_ptr<SmallPOD[]> cpod3 = make_unique_small<SmallPOD[]>(0);
    small_unique_ptr<LargePOD[]> cpod4 = make_unique_small<LargePOD[]>(0);
    SUCCEED();
}

TEST_CASE("move_assignment_plain", "[small_unique_ptr]")
{
    STATIC_REQUIRE(32 == std::invoke([] { small_unique_ptr<Base> p; p = make_unique_small<SmallDerived>(); return p->padding(); }));
    STATIC_REQUIRE(64 == std::invoke([] { small_unique_ptr<Base> p; p = make_unique_small<LargeDerived>(); return p->padding(); }));

    STATIC_REQUIRE(32 == std::invoke([] { small_unique_ptr<BaseIntrusive> p; p = make_unique_small<SmallIntrusive>(); return p->padding(); }));
    STATIC_REQUIRE(64 == std::invoke([] { small_unique_ptr<BaseIntrusive> p; p = make_unique_small<LargeIntrusive>(); return p->padding(); }));

    STATIC_REQUIRE( std::invoke([] { small_unique_ptr<const SmallPOD> p; p = make_unique_small<SmallPOD>(); return true; }) );
    STATIC_REQUIRE( std::invoke([] { small_unique_ptr<const LargePOD> p; p = make_unique_small<LargePOD>(); return true; }) );


    small_unique_ptr<const Base> base;

    base = make_unique_small<SmallDerived>();
    REQUIRE(base->value() == 32);

    base = make_unique_small<LargeDerived>();
    REQUIRE(base->value() == 64);

    base = nullptr;
    REQUIRE(!base);


    small_unique_ptr<const BaseIntrusive> ibase;

    ibase = make_unique_small<SmallIntrusive>();
    REQUIRE(ibase->value() == 32);

    ibase = make_unique_small<LargeIntrusive>();
    REQUIRE(ibase->value() == 64);

    ibase = nullptr;
    REQUIRE(!ibase);


    small_unique_ptr<const volatile SmallPOD> cpod1; cpod1 = make_unique_small<SmallPOD>();
    small_unique_ptr<const volatile LargePOD> cpod2; cpod2 = make_unique_small<LargePOD>();
    SUCCEED();
}

TEST_CASE("move_assignment_array", "[small_unique_ptr]")
{
    STATIC_REQUIRE( std::invoke([] { small_unique_ptr<const SmallPOD[]> p; p = make_unique_small<SmallPOD[]>(4); return true; }) );
    STATIC_REQUIRE( std::invoke([] { small_unique_ptr<const LargePOD[]> p; p = make_unique_small<LargePOD[]>(4); return true; }) );

    small_unique_ptr<const volatile SmallPOD[]> cpod1; cpod1 = make_unique_small<SmallPOD[]>(4);
    small_unique_ptr<const volatile LargePOD[]> cpod2; cpod2 = make_unique_small<LargePOD[]>(4);
    SUCCEED();
}

TEST_CASE("swap_pod", "[small_unique_ptr]")
{
    small_unique_ptr<const SmallPOD> p1 = nullptr;
    small_unique_ptr<const SmallPOD> p2 = make_unique_small<const SmallPOD>();

    using std::swap;
    swap(p1, p2);

    REQUIRE(p2 == nullptr);
    REQUIRE(p1 != nullptr);
}

TEST_CASE("swap_array", "[small_unique_ptr]")
{
    small_unique_ptr<SmallDerived[]> p1 = nullptr;
    small_unique_ptr<SmallDerived[]> p2 = make_unique_small<SmallDerived[]>(3);

    using std::swap;
    swap(p1, p2);

    REQUIRE(p2 == nullptr);
    REQUIRE(p1[2].value() == 32);

    swap(p1, p2);

    REQUIRE(p1 == nullptr);
    REQUIRE(p2[1].value() == 32);
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

TEST_CASE("swap_large_intrusive", "[small_unique_ptr]")
{
    small_unique_ptr<BaseIntrusive> p1 = nullptr;
    small_unique_ptr<BaseIntrusive> p2 = make_unique_small<LargeIntrusive>();

    using std::swap;
    swap(p1, p2);

    REQUIRE(p2 == nullptr);
    REQUIRE(p1->value() == 64);
}

TEST_CASE("swap_small_intrusive", "[small_unique_ptr]")
{
    small_unique_ptr<BaseIntrusive> p1 = make_unique_small<SmallIntrusive>(1);
    small_unique_ptr<BaseIntrusive> p2 = make_unique_small<SmallIntrusive>(2);

    using std::swap;
    swap(p1, p2);

    REQUIRE(p1->value() == 2);
    REQUIRE(p2->value() == 1);

    swap(p1, p2);

    REQUIRE(p1->value() == 1);
    REQUIRE(p2->value() == 2);
}

TEST_CASE("swap_mixed_intrusive", "[small_unique_ptr]")
{
    using std::swap;

    small_unique_ptr<BaseIntrusive> p1 = make_unique_small<SmallIntrusive>();
    small_unique_ptr<BaseIntrusive> p2 = make_unique_small<LargeIntrusive>();

    REQUIRE(p1->value() == 32);
    REQUIRE(p2->value() == 64);

    swap(p1, p2);

    REQUIRE(p1->value() == 64);
    REQUIRE(p2->value() == 32);

    swap(p1, p2);

    REQUIRE(p1->value() == 32);
    REQUIRE(p2->value() == 64);
}

TEST_CASE("constexpr_swap", "[small_unique_ptr]")
{
    using std::swap;

    STATIC_REQUIRE(std::invoke([]
    {
        small_unique_ptr<const SmallPOD> p1 = make_unique_small<const SmallPOD>();
        small_unique_ptr<const SmallPOD> p2 = make_unique_small<const SmallPOD>();

        swap(p1, p2);

        return true;
    }));


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


    STATIC_REQUIRE(32 == std::invoke([]
    {
        small_unique_ptr<BaseIntrusive> p1 = make_unique_small<SmallIntrusive>();
        small_unique_ptr<BaseIntrusive> p2 = make_unique_small<LargeIntrusive>();

        swap(p1, p2);

        return p2->padding();
    }));

    STATIC_REQUIRE(32 == std::invoke([]
    {
        small_unique_ptr<BaseIntrusive> p1 = make_unique_small<SmallIntrusive>();
        small_unique_ptr<BaseIntrusive> p2 = make_unique_small<SmallIntrusive>();

        swap(p1, p2);

        return p2->padding();
    }));

    STATIC_REQUIRE(64 == std::invoke([]
    {
        small_unique_ptr<BaseIntrusive> p1 = make_unique_small<LargeIntrusive>();
        small_unique_ptr<BaseIntrusive> p2 = make_unique_small<LargeIntrusive>();

        swap(p1, p2);

        return p2->padding();
    }));

    STATIC_REQUIRE(32 == std::invoke([]
    {
        small_unique_ptr<SmallDerived[]> p1 = make_unique_small<SmallDerived[]>(2);
        small_unique_ptr<SmallDerived[]> p2 = make_unique_small<SmallDerived[]>(4);

        swap(p1, p2);

        return p1[3].padding();
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

    small_unique_ptr<const ABase> pc = std::move(p);
    REQUIRE(pc->value() == 12);
    REQUIRE(p == nullptr);
}

TEST_CASE("alignment_simple", "[small_unique_ptr]")
{
    small_unique_ptr<SmallPOD> ps = make_unique_small<SmallPOD>();
    small_unique_ptr<LargePOD> pl = make_unique_small<LargePOD>();

    REQUIRE((std::bit_cast<std::uintptr_t>(std::addressof(*ps)) % alignof(SmallPOD)) == 0);
    REQUIRE((std::bit_cast<std::uintptr_t>(std::addressof(*pl)) % alignof(LargePOD)) == 0);
}

TEST_CASE("alignment_array", "[small_unique_ptr]")
{
    small_unique_ptr<SmallPOD[]> ps = make_unique_small<SmallPOD[]>(4);
    small_unique_ptr<LargePOD[]> pl = make_unique_small<LargePOD[]>(2);

    REQUIRE((std::bit_cast<std::uintptr_t>(std::addressof(ps[0])) % alignof(SmallPOD)) == 0);
    REQUIRE((std::bit_cast<std::uintptr_t>(std::addressof(pl[0])) % alignof(LargePOD)) == 0);
}

TEST_CASE("alignment_poly", "[small_unique_ptr]")
{
    struct alignas(16) SmallAlign { virtual ~SmallAlign() = default; };
    struct alignas(128) LargeAlign : SmallAlign {};

    small_unique_ptr<SmallAlign> p = make_unique_small<LargeAlign>();

    REQUIRE((std::bit_cast<std::uintptr_t>(std::addressof(*p)) % alignof(LargeAlign)) == 0);
}

TEST_CASE("const_unique_ptr", "[small_unique_ptr]")
{
    const small_unique_ptr<int> p = make_unique_small<int>(3);
    *p = 2;

    REQUIRE(*p == 2);
}
