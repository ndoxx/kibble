#pragma once

#include <exception>
#include <type_traits>

namespace kb
{
struct BadDelegateCallException : public std::exception
{
    const char *what() const throw()
    {
        return "Cannot invoke a member function without a class instance";
    }
};

template <typename Signature> class Delegate;

template <typename R, typename... Args> class Delegate<R(Args...)>
{
public:
    Delegate() = default;

    Delegate(const Delegate &other) = default;
    auto operator=(const Delegate &other) -> Delegate & = default;

    template <typename... UArgs, typename = std::enable_if_t<std::is_invocable_v<R(Args...), UArgs...>>>
    auto operator()(UArgs &&...args) -> R
    {
        return (*stub_)(instance_, std::forward<UArgs>(args)...);
    }

    template <typename... UArgs, typename = std::enable_if_t<std::is_invocable_v<R(Args...), UArgs...>>>
    auto operator()(UArgs &&...args) const -> R
    {
        return (*stub_)(instance_, std::forward<UArgs>(args)...);
    }

    template <std::invocable<Args...> auto Function>
    auto bind() -> void
    {
        instance_ = nullptr;
        stub_ = static_cast<stub_function>(
            [](const void *, Args... args) -> R { return std::invoke(Function, std::forward<Args>(args)...); });
    }

    // template <typename Class, std::invocable<const Class *, Args...> auto MemberFunction>
    template <auto MemberFunction, typename Class,
              typename = std::enable_if_t<std::is_invocable_r_v<R, decltype(MemberFunction), const Class *, Args...>>>
    auto bind(const Class *cls) -> void
    {
        instance_ = cls;
        stub_ = static_cast<stub_function>([](const void *p, Args... args) -> R {
            const auto *c = static_cast<const Class *>(p);
            return std::invoke(MemberFunction, c, std::forward<Args>(args)...);
        });
    }

    // template <typename Class, std::invocable<Class *, Args...> auto MemberFunction>
    template <auto MemberFunction, typename Class,
              typename = std::enable_if_t<std::is_invocable_r_v<R, decltype(MemberFunction), Class *, Args...>>>
    auto bind(Class *cls) -> void
    {
        instance_ = cls;
        stub_ = static_cast<stub_function>([](const void *p, Args... args) -> R {
            auto *c = const_cast<Class *>(static_cast<const Class *>(p));
            return std::invoke(MemberFunction, c, std::forward<Args>(args)...);
        });
    }

private:
    using stub_function = R (*)(const void *, Args...);

    [[noreturn]] static auto stub_null(const void *, Args...) -> R
    {
        throw BadDelegateCallException{};
    }

    const void *instance_ = nullptr;
    stub_function stub_ = &stub_null;
};
} // namespace kb