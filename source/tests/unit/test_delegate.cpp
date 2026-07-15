#include "kibble/util/delegate.h"

#include <catch2/catch_all.hpp>
#include <string>
#include <type_traits>

using namespace kb;

auto square(int x) -> int
{
    return x * x;
}

auto cube(int x) -> int
{
    return x * x * x;
}

auto negate(int x) noexcept -> int
{
    return -x;
}

float foo(float a, int* b, const size_t& c)
{
    float res = 0.f;
    for (size_t ii = 0; ii < c; ++ii)
    {
        res += a * float(*b) / float(ii + 1);
    }
    return res;
}

void increment_ref(int& x)
{
    ++x;
}

struct Counter
{
    int value = 0;

    void increment() noexcept
    {
        ++value;
    }

    int get() const noexcept
    {
        return value;
    }

    void add(int amount)
    {
        value += amount;
    }
};

// --- Free function delegation -----------------------------------------------------------------

TEST_CASE("It is possible to delegate a free function", "[delegate]")
{
    auto d = Delegate<int(int)>::create<&square>();
    REQUIRE(d(2) == 4);
    REQUIRE(d(5) == 25);
}

TEST_CASE("A delegate correctly forwards multiple arguments of mixed value, pointer and reference type",
          "[delegate]")
{
    auto d = Delegate<float(float, int*, const size_t&)>::create<&foo>();

    int b = 2;
    size_t c = 3;
    REQUIRE(d(0.1f, &b, c) == foo(0.1f, &b, c));
}

TEST_CASE("A delegate can bind a function taking a mutable reference and observe the mutation", "[delegate]")
{
    auto d = Delegate<void(int&)>::create<&increment_ref>();
    int x = 41;
    d(x);
    REQUIRE(x == 42);
}

// --- Member function delegation ---------------------------------------------------------------

TEST_CASE("It is possible to delegate a non-mutating member function", "[delegate]")
{
    auto str = std::string{"Hello"};
    auto d = Delegate<size_t()>::create<&std::string::size>(&str);
    REQUIRE(d() == 5);
}

TEST_CASE("It is possible to delegate a mutating member function", "[delegate]")
{
    auto str = std::string{"Hello"};
    auto d = Delegate<void(int)>::create<&std::string::push_back>(&str);
    d('!');
    REQUIRE(str.compare("Hello!") == 0);
}

TEST_CASE("A member delegate can be bound through a const instance pointer", "[delegate]")
{
    const auto str = std::string{"Hello"};
    auto d = Delegate<size_t()>::create<&std::string::size>(&str);
    REQUIRE(d() == 5);
}

// --- Unbound delegate / exception behavior ------------------------------------------------------

TEST_CASE("Calling a default constructed delegate throws BadDelegateCallException", "[delegate]")
{
    Delegate<int(int)> d;
    REQUIRE_THROWS_AS(d(1), BadDelegateCallException);
}

TEST_CASE("BadDelegateCallException reports a meaningful message", "[delegate]")
{
    Delegate<void()> d;
    try
    {
        d();
        FAIL("Expected BadDelegateCallException to be thrown");
    }
    catch (const BadDelegateCallException& e)
    {
        REQUIRE(std::string(e.what()).size() > 0);
    }
}

// --- nullptr assignment ---------------------------------------------------------------------------

TEST_CASE("Assigning nullptr to a bound delegate resets it to the unbound state", "[delegate][nullptr]")
{
    auto d = Delegate<int(int)>::create<&square>();
    REQUIRE(bool(d));

    d = nullptr;
    REQUIRE_FALSE(bool(d));
}

TEST_CASE("Calling a delegate after assigning nullptr throws BadDelegateCallException, not UB", "[delegate][nullptr]")
{
    Delegate<int(int)> d;
    d = square;
    d = nullptr;

    REQUIRE_THROWS_AS(d(1), BadDelegateCallException);
}

TEST_CASE("Assigning nullptr to an already-unbound delegate leaves it unbound", "[delegate][nullptr]")
{
    Delegate<int(int)> d;
    REQUIRE_FALSE(bool(d));

    d = nullptr;
    REQUIRE_FALSE(bool(d));
}

TEST_CASE("A delegate reset via nullptr compares equal to a default constructed delegate", "[delegate][nullptr][comparison]")
{
    auto d1 = Delegate<int(int)>::create<&square>();
    d1 = nullptr;

    Delegate<int(int)> d2;
    REQUIRE(d1 == d2);
    REQUIRE_FALSE(d1 != d2);
}

TEST_CASE("A delegate reset via nullptr can be reassigned to a new target afterwards", "[delegate][nullptr]")
{
    auto d = Delegate<int(int)>::create<&square>();
    d = nullptr;
    REQUIRE_FALSE(bool(d));

    d = cube;
    REQUIRE(bool(d));
    REQUIRE(d(3) == 27);
}

// --- operator bool -------------------------------------------------------------------------------

TEST_CASE("A default constructed delegate should be falsy", "[delegate][bool]")
{
    Delegate<int(int)> d;
    REQUIRE_FALSE(bool(d));
}

TEST_CASE("A delegate bound to a free function should be truthy", "[delegate][bool]")
{
    auto d = Delegate<int(int)>::create<&square>();
    REQUIRE(bool(d));
}

TEST_CASE("A delegate bound to a member function should be truthy", "[delegate][bool]")
{
    auto str = std::string{"Hello"};
    auto d = Delegate<size_t()>::create<&std::string::size>(&str);
    REQUIRE(bool(d));
}

TEST_CASE("A delegate assigned a lambda should be truthy", "[delegate][bool]")
{
    Delegate<int(int)> d;
    d = [](int x) -> int { return x * x; };
    REQUIRE(bool(d));
}

TEST_CASE("A copy of a default constructed delegate is still falsy", "[delegate][bool]")
{
    Delegate<int(int)> d1;
    Delegate<int(int)> d2 = d1;
    REQUIRE_FALSE(bool(d2));
}

TEST_CASE("A copy of a bound delegate is truthy and independently callable", "[delegate][bool]")
{
    auto d1 = Delegate<int(int)>::create<&square>();
    Delegate<int(int)> d2 = d1;
    REQUIRE(bool(d2));
    REQUIRE(d2(3) == 9);
}

// --- Comparisons -----------------------------------------------------------------------------

TEST_CASE("Free delegate comparison should be reflexive", "[delegate][comparison]")
{
    auto d = Delegate<int(int)>::create<&square>();
    REQUIRE(d == d);
    REQUIRE_FALSE(d != d);
}

TEST_CASE("A delegate should be equal to another delegate pointing to the same free function",
          "[delegate][comparison]")
{
    auto d1 = Delegate<int(int)>::create<&square>();
    [[maybe_unused]] auto d3 = Delegate<int(int)>::create<&cube>();
    auto d2 = Delegate<int(int)>::create<&square>();
    REQUIRE(d1 == d2);
    REQUIRE_FALSE(d1 != d2);
}

TEST_CASE("A delegate should not be equal to another delegate pointing to a different free function",
          "[delegate][comparison]")
{
    auto d1 = Delegate<int(int)>::create<&square>();
    [[maybe_unused]] auto d3 = Delegate<int(int)>::create<&square>();
    auto d2 = Delegate<int(int)>::create<&cube>();
    REQUIRE_FALSE(d1 == d2);
    REQUIRE(d1 != d2);
}

TEST_CASE("Member delegate comparison should be reflexive", "[delegate][comparison]")
{
    auto str = std::string{"Hello"};
    auto d1 = Delegate<size_t()>::create<&std::string::size>(&str);
    auto d2 = Delegate<size_t()>::create<&std::string::size>(&str);

    REQUIRE(d1 == d2);
    REQUIRE_FALSE(d1 != d2);
}

TEST_CASE("Member delegates with different instances should be different", "[delegate][comparison]")
{
    auto str1 = std::string{"Hello"};
    auto str2 = std::string{"World"};
    auto d1 = Delegate<size_t()>::create<&std::string::size>(&str1);
    auto d2 = Delegate<size_t()>::create<&std::string::size>(&str2);

    REQUIRE_FALSE(d1 == d2);
    REQUIRE(d1 != d2);
}

TEST_CASE("Member delegates with different member pointers should be different", "[delegate][comparison]")
{
    auto str = std::string{"Hello"};
    // Here we need to have the same signature otherwise it wouldn't even compile (and we would
    // know at compile-time that they are different)
    auto d1 = Delegate<size_t()>::create<&std::string::size>(&str);
    auto d2 = Delegate<size_t()>::create<&std::string::length>(&str);

    REQUIRE_FALSE(d1 == d2);
    REQUIRE(d1 != d2);
}

TEST_CASE("Two default constructed delegates compare equal", "[delegate][comparison]")
{
    Delegate<int(int)> d1;
    Delegate<int(int)> d2;
    REQUIRE(d1 == d2);
    REQUIRE_FALSE(d1 != d2);
}

TEST_CASE("A default constructed delegate is not equal to a bound one", "[delegate][comparison]")
{
    Delegate<int(int)> d1;
    auto d2 = Delegate<int(int)>::create<&square>();
    REQUIRE_FALSE(d1 == d2);
    REQUIRE(d1 != d2);
}

TEST_CASE("Reassigning a delegate to a different free function changes what it compares equal to",
          "[delegate][comparison]")
{
    auto d1 = Delegate<int(int)>::create<&square>();
    auto d2 = Delegate<int(int)>::create<&cube>();
    REQUIRE(d1 != d2);

    d1 = Delegate<int(int)>::create<&cube>();
    REQUIRE(d1 == d2);
}

// --- Assignment from lambdas and free function pointers --------------------------------------

TEST_CASE("It is possible to assign a capture-less lambda to a delegate", "[delegate][assignment]")
{
    Delegate<int(int)> d;
    REQUIRE_FALSE(bool(d));

    d = [](int x) -> int { return x * x; };
    REQUIRE(bool(d));
    REQUIRE(d(2) == 4);
    REQUIRE(d(5) == 25);
}

TEST_CASE("It is possible to assign a free function directly to a delegate", "[delegate][assignment]")
{
    Delegate<int(int)> d;
    d = square;
    REQUIRE(d(4) == 16);

    d = cube;
    REQUIRE(d(3) == 27);
}

TEST_CASE("It is possible to assign an explicit free function pointer to a delegate", "[delegate][assignment]")
{
    Delegate<int(int)> d;
    d = &square;
    REQUIRE(d(4) == 16);
}

TEST_CASE("A lambda assignment can be reassigned to a different lambda", "[delegate][assignment]")
{
    Delegate<int(int)> d;
    d = [](int x) -> int { return x + 1; };
    REQUIRE(d(1) == 2);

    d = [](int x) -> int { return x - 1; };
    REQUIRE(d(1) == 0);
}

TEST_CASE("Assigning a lambda to a delegate previously bound to a member function overwrites the instance",
          "[delegate][assignment]")
{
    auto str = std::string{"Hello"};
    auto d = Delegate<size_t()>::create<&std::string::size>(&str);
    REQUIRE(d() == 5);

    d = []() -> size_t { return 42; };
    REQUIRE(d() == 42);
}

TEST_CASE("A mutating capture-less lambda can be assigned and observed through a reference argument",
          "[delegate][assignment]")
{
    Delegate<void(int&)> d;
    d = [](int& x) { x *= 2; };
    int v = 21;
    d(v);
    REQUIRE(v == 42);
}

TEST_CASE("Delegates assigned from the same free function compare equal whether it was spelled as a bare name or "
          "an explicit function pointer",
          "[delegate][assignment][comparison]")
{
    // This locks in a subtle invariant: naming a function directly (d = square;) and taking its
    // address explicitly (d = &square;) both resolve to the exact same function, so the stub
    // used by operator= must not depend on which spelling was used at the call site. Before this
    // was fixed, the stub was generated inside the templated operator= itself, so its identity
    // (and therefore the comparison result) accidentally depended on how the argument was typed.
    Delegate<int(int)> named, pointer;
    named = square;
    pointer = &square;

    REQUIRE(named == pointer);
}

TEST_CASE("A lambda does not compare equal to a free function with identical behavior", "[delegate][assignment][comparison]")
{
    // A lambda, even a capture-less one with an identical body, compiles down to its own distinct
    // function. It is a different callable from square(), not an alias for it, so the delegates
    // must not compare equal just because they happen to produce the same results.
    Delegate<int(int)> named;
    Delegate<int(int)> lambda;
    named = square;
    lambda = [](int x) -> int { return x * x; };

    REQUIRE(named(4) == lambda(4));
    REQUIRE(named != lambda);
}

TEST_CASE("Two delegates assigned from the same lambda object compare equal", "[delegate][assignment][comparison]")
{
    auto multiply_by_three = [](int x) -> int { return x * 3; };

    Delegate<int(int)> d1;
    Delegate<int(int)> d2;
    d1 = multiply_by_three;
    d2 = multiply_by_three;

    REQUIRE(d1 == d2);
    REQUIRE(d1(4) == 12);
}

TEST_CASE("Delegates assigned from a lambda compare equal when bound to the same function",
          "[delegate][assignment][comparison]")
{
    Delegate<int(int)> d1;
    Delegate<int(int)> d2;
    d1 = square;
    d2 = square;

    REQUIRE(d1 == d2);
    REQUIRE_FALSE(d1 != d2);
}

TEST_CASE("Delegates assigned from a lambda compare different when bound to different functions",
          "[delegate][assignment][comparison]")
{
    Delegate<int(int)> d1;
    Delegate<int(int)> d2;
    d1 = square;
    d2 = cube;

    REQUIRE_FALSE(d1 == d2);
    REQUIRE(d1 != d2);
}

TEST_CASE("A delegate built with create<> and one assigned via operator= for the same function are distinct "
          "bindings",
          "[delegate][assignment][comparison]")
{
    // This is documented, expected behavior rather than a bug: create<>() bakes the function
    // into a dedicated stub at compile time, while operator= stores a runtime function pointer
    // and dispatches through a shared stub. They are two different binding mechanisms, so their
    // stubs never compare equal even when they end up calling the same function.
    auto d1 = Delegate<int(int)>::create<&square>();
    Delegate<int(int)> d2;
    d2 = square;

    REQUIRE(d1(6) == d2(6));
    REQUIRE(d1 != d2);
}

TEST_CASE("A capturing lambda cannot be assigned to a delegate", "[delegate][assignment]")
{
    int offset = 1;
    auto capturing = [offset](int x) -> int { return x + offset; };
    STATIC_REQUIRE_FALSE(std::is_assignable_v<Delegate<int(int)>&, decltype(capturing)>);
    (void)capturing;
}

TEST_CASE("A lambda with an incompatible signature cannot be assigned to a delegate", "[delegate][assignment]")
{
    STATIC_REQUIRE_FALSE(std::is_assignable_v<Delegate<int(int)>&, double (*)(double)>);
}

// --- Copy semantics --------------------------------------------------------------------------

TEST_CASE("Copy constructing a delegate preserves its binding", "[delegate]")
{
    auto d1 = Delegate<int(int)>::create<&square>();
    Delegate<int(int)> d2(d1);
    REQUIRE(d1 == d2);
    REQUIRE(d2(3) == 9);
}

TEST_CASE("Copy assigning a delegate preserves its binding and does not get shadowed by operator=(F&&)",
          "[delegate]")
{
    auto d1 = Delegate<int(int)>::create<&square>();
    Delegate<int(int)> d2;
    d2 = d1;
    REQUIRE(d1 == d2);
    REQUIRE(d2(4) == 16);
}

// Will not compile with paranoid warnings
// TEST_CASE("Self-assignment leaves a delegate in a valid, callable state", "[delegate]")
// {
//     auto d = Delegate<int(int)>::create<&square>();
//     d = d;
//     REQUIRE(d(5) == 25);
// }

// --- make_delegate -----------------------------------------------------------------------------

TEST_CASE("make_delegate can bind a free function without spelling out the signature", "[delegate][make_delegate]")
{
    auto d = make_delegate<&square>();
    REQUIRE(d(2) == 4);
    REQUIRE(d(5) == 25);
}

TEST_CASE("make_delegate can bind a non-mutating member function without spelling out the signature",
          "[delegate][make_delegate]")
{
    auto str = std::string{"Hello"};
    auto d = make_delegate<&std::string::size>(&str);
    REQUIRE(d() == 5);
}

TEST_CASE("make_delegate can bind a mutating member function without spelling out the signature",
          "[delegate][make_delegate]")
{
    auto str = std::string{"Hello"};
    auto d = make_delegate<&std::string::push_back>(&str);
    d('!');
    REQUIRE(str.compare("Hello!") == 0);
}

TEST_CASE("make_delegate can bind a member function through a const instance pointer", "[delegate][make_delegate]")
{
    const auto str = std::string{"Hello"};
    auto d = make_delegate<&std::string::size>(&str);
    REQUIRE(d() == 5);
}

TEST_CASE("make_delegate produces delegates that compare equal under the same rules as create<>",
          "[delegate][make_delegate][comparison]")
{
    auto d1 = make_delegate<&square>();
    auto d2 = make_delegate<&square>();
    auto d3 = make_delegate<&cube>();

    REQUIRE(d1 == d2);
    REQUIRE(d1 != d3);
}

TEST_CASE("make_delegate result is truthy", "[delegate][make_delegate][bool]")
{
    auto d = make_delegate<&square>();
    REQUIRE(bool(d));
}

// --- noexcept support ------------------------------------------------------------------------

TEST_CASE("A delegate can bind a noexcept free function through create<>", "[delegate][noexcept]")
{
    auto d = Delegate<int(int)>::create<&negate>();
    REQUIRE(d(5) == -5);
}

TEST_CASE("make_delegate can bind a noexcept free function", "[delegate][noexcept][make_delegate]")
{
    auto d = make_delegate<&negate>();
    REQUIRE(d(5) == -5);
}

TEST_CASE("make_delegate can bind a noexcept, non-const member function", "[delegate][noexcept][make_delegate]")
{
    Counter c;
    auto d = make_delegate<&Counter::increment>(&c);
    d();
    d();
    REQUIRE(c.value == 2);
}

TEST_CASE("make_delegate can bind a noexcept, const member function", "[delegate][noexcept][make_delegate]")
{
    Counter c;
    c.value = 7;
    auto d = make_delegate<&Counter::get>(&c);
    REQUIRE(d() == 7);
}

TEST_CASE("make_delegate can bind a noexcept, const member function through a const instance",
          "[delegate][noexcept][make_delegate]")
{
    Counter c;
    c.value = 9;
    const Counter* cp = &c;
    auto d = make_delegate<&Counter::get>(cp);
    REQUIRE(d() == 9);
}

TEST_CASE("A regular non-noexcept member function is unaffected by the noexcept function_traits specializations",
          "[delegate][noexcept][make_delegate]")
{
    Counter c;
    auto d = make_delegate<&Counter::add>(&c);
    d(5);
    REQUIRE(c.value == 5);
}

// --- PackagedDelegate (unaffected by the additions above, sanity-checked for regressions) ----

TEST_CASE("It is possible to delegate a free function", "[packaged-delegate]")
{
    auto d = Delegate<int(int)>::create<&square>();
    PackagedDelegate pd(d);
    pd.prepare(2);
    REQUIRE(pd.execute<int>() == 4);
    pd.prepare(5);
    REQUIRE(pd.execute<int>() == 25);
}

TEST_CASE("Packaged delegates can store multiple arguments", "[packaged-delegate]")
{
    auto d = Delegate<float(float, int*, const size_t&)>::create<&foo>();

    int b = 2;
    size_t c = 3;
    PackagedDelegate pd(d);
    pd.prepare(0.1f, &b, c);

    REQUIRE(pd.execute<float>() == foo(0.1f, &b, c));
}

TEST_CASE("It is possible to delegate a non-mutating member function", "[packaged-delegate]")
{
    auto str = std::string{"Hello"};
    auto d = Delegate<size_t()>::create<&std::string::size>(&str);
    PackagedDelegate pd(d);
    REQUIRE(pd.execute<size_t>() == 5);
}

TEST_CASE("It is possible to delegate a mutating member function", "[packaged-delegate]")
{
    auto str = std::string{"Hello"};
    auto d = Delegate<void(int)>::create<&std::string::push_back>(&str);
    PackagedDelegate pd(d);
    pd.prepare('!');
    pd();
    REQUIRE(str.compare("Hello!") == 0);
}