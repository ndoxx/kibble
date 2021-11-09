#include "logger/dispatcher.h"
#include "logger/logger.h"
#include "logger/sink.h"

#include "ctti/ctti.h"

#include <string>
#include <vector>

using namespace kb;

#define D_(A) #A << " -> " << KF_(kb::col::lightorange) << A << KC_

struct Foo
{
    int i;
};

class Bar
{
public:
    Bar(){};
};

template <typename T> struct Baz
{
public:
    using type = T;
};

template <typename T> consteval hash_t consteval_type_id()
{
    return ctti::type_id<T>();
}

void init_logger()
{
    KLOGGER_START();

    KLOGGER(create_channel("ctti", 3));
    KLOGGER(attach_all("console_sink", std::make_unique<klog::ConsoleSink>()));
    KLOGGER(set_backtrace_on_error(false));
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    init_logger();

    KLOGN("ctti") << "Working with classes directly" << std::endl;
    KLOG("ctti", 1) << D_(ctti::type_name<std::string>()) << std::endl;
    KLOG("ctti", 1) << D_(ctti::type_name<Foo>()) << std::endl;
    KLOG("ctti", 1) << D_(ctti::type_name<const Bar &>()) << std::endl;
    KLOG("ctti", 1) << D_(ctti::type_name<const Baz<Bar> &>()) << std::endl;
    KLOG("ctti", 1) << D_(ctti::type_name<const Baz<Bar>::type &>()) << std::endl;
    KLOG("ctti", 1) << D_(ctti::type_name<std::vector<Foo>>()) << std::endl;

    KLOG("ctti", 1) << D_(ctti::type_id<Bar>()) << " == " << D_("Bar"_h) << std::endl;
    KLOG("ctti", 1) << D_(ctti::type_id<Foo>()) << " == " << D_("Foo"_h) << std::endl;
    KLOG("ctti", 1) << D_(ctti::type_id<const Foo &>()) << " == " << D_("const Foo &"_h) << std::endl;
    KLOG("ctti", 1) << D_(ctti::type_id<const Foo *>()) << " == " << D_("const Foo *"_h) << std::endl;

    KLOG("ctti", 1) << "It really is compile-time: " << D_(consteval_type_id<Foo>()) << std::endl;

    KLOGN("ctti") << "Working with instances" << std::endl;
    Foo foo;
    Bar bar;
    Baz<Foo> baz_foo;
    Baz<Bar> baz_bar;

    KLOG("ctti", 1) << D_(ctti::type_name(foo)) << std::endl;
    KLOG("ctti", 1) << D_(ctti::type_name(bar)) << std::endl;
    KLOG("ctti", 1) << D_(ctti::type_name(baz_foo)) << std::endl;
    KLOG("ctti", 1) << D_(ctti::type_name(baz_bar)) << std::endl;
    KLOG("ctti", 1) << D_(ctti::type_name(&baz_bar)) << std::endl;
    KLOG("ctti", 1) << D_(ctti::type_name(std::cref(baz_foo).get())) << std::endl;
    KLOG("ctti", 1) << D_(ctti::type_name<decltype(std::cref(baz_foo).get())>()) << std::endl;

    KLOG("ctti", 1) << D_(ctti::type_id(foo)) << std::endl;
    KLOG("ctti", 1) << D_(ctti::type_id(bar)) << std::endl;
    KLOG("ctti", 1) << D_(ctti::type_id(baz_foo)) << std::endl;
    KLOG("ctti", 1) << D_(ctti::type_id(baz_bar)) << std::endl;
    KLOG("ctti", 1) << D_(ctti::type_id(&baz_bar)) << std::endl;
    KLOG("ctti", 1) << D_(ctti::type_id(std::cref(baz_foo).get())) << std::endl;
    KLOG("ctti", 1) << D_(ctti::type_id<decltype(std::cref(baz_foo).get())>()) << std::endl;

    return 0;
}
