#include "ctti/ctti.h"

#include <iostream>
#include <string>
#include <vector>

using namespace kb;

#define D_(A) #A << " -> " << A

struct Foo
{
    int i;
};

class Bar
{
public:
    Bar(){};
};

template <typename T>
struct Baz
{
public:
    using type = T;
};

template <typename T>
consteval hash_t consteval_type_id()
{
    return ctti::type_id<T>();
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    std::cout << "Working with classes directly" << std::endl;
    std::cout << D_(ctti::type_name<std::string>()) << std::endl;
    std::cout << D_(ctti::type_name<Foo>()) << std::endl;
    std::cout << D_(ctti::type_name<const Bar &>()) << std::endl;
    std::cout << D_(ctti::type_name<const Baz<Bar> &>()) << std::endl;
    std::cout << D_(ctti::type_name<const Baz<Bar>::type &>()) << std::endl;
    std::cout << D_(ctti::type_name<std::vector<Foo>>()) << std::endl;

    std::cout << D_(ctti::type_id<Bar>()) << " == " << D_("Bar"_h) << std::endl;
    std::cout << D_(ctti::type_id<Foo>()) << " == " << D_("Foo"_h) << std::endl;
    std::cout << D_(ctti::type_id<const Foo &>()) << " == " << D_("const Foo &"_h) << std::endl;
    std::cout << D_(ctti::type_id<const Foo *>()) << " == " << D_("const Foo *"_h) << std::endl;

    std::cout << "It really is compile-time: " << D_(consteval_type_id<Foo>()) << std::endl;

    std::cout << "Working with instances" << std::endl;
    Foo foo;
    Bar bar;
    Baz<Foo> baz_foo;
    Baz<Bar> baz_bar;

    std::cout << D_(ctti::type_name(foo)) << std::endl;
    std::cout << D_(ctti::type_name(bar)) << std::endl;
    std::cout << D_(ctti::type_name(baz_foo)) << std::endl;
    std::cout << D_(ctti::type_name(baz_bar)) << std::endl;
    std::cout << D_(ctti::type_name(&baz_bar)) << std::endl;
    std::cout << D_(ctti::type_name(std::cref(baz_foo).get())) << std::endl;
    std::cout << D_(ctti::type_name<decltype(std::cref(baz_foo).get())>()) << std::endl;

    std::cout << D_(ctti::type_id(foo)) << std::endl;
    std::cout << D_(ctti::type_id(bar)) << std::endl;
    std::cout << D_(ctti::type_id(baz_foo)) << std::endl;
    std::cout << D_(ctti::type_id(baz_bar)) << std::endl;
    std::cout << D_(ctti::type_id(&baz_bar)) << std::endl;
    std::cout << D_(ctti::type_id(std::cref(baz_foo).get())) << std::endl;
    std::cout << D_(ctti::type_id<decltype(std::cref(baz_foo).get())>()) << std::endl;

    return 0;
}
