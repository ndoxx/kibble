#pragma once

/*
 * Based on the excellent series of articles by Matthew Rodusek:
 * https://bitwizeshift.github.io/posts/2021/02/24/creating-a-fast-and-efficient-delegate-type-part-1/
 * https://bitwizeshift.github.io/posts/2021/02/24/creating-a-fast-and-efficient-delegate-type-part-2/
 * https://bitwizeshift.github.io/posts/2021/02/24/creating-a-fast-and-efficient-delegate-type-part-3/
 */

#include <exception>
#include <type_traits>

namespace kb
{
/**
 * @brief This exception is thrown when a member delegate call happens
 * with no initialized instance
 *
 */
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
    // Make delegates copyable
    Delegate() = default;
    Delegate(const Delegate &other) = default;
    auto operator=(const Delegate &other) -> Delegate & = default;

    /**
     * @brief Call the registered function
     *
     * @tparam UArgs Types of the arguments
     * @param args Arguments to forward to the function
     * @return R
     */
    template <typename... UArgs, typename = std::enable_if_t<std::is_invocable_v<R(Args...), UArgs...>>>
    auto operator()(UArgs &&...args) const -> R
    {
        // Call the stub function, passing the instance pointer and forwarding each argument
        return (*stub_)(instance_, std::forward<UArgs>(args)...);
    }

    /**
     * @brief Attach a free function to this delegate
     *
     * @tparam Function Pointer to the function to attach
     */
    template <auto Function, typename = std::enable_if_t<std::is_invocable_r_v<R, decltype(Function), Args...>>>
    auto bind() -> void
    {
        // A free function does not use any instance pointer, we leave it null
        // A lambda without capture is implicitly convertible to a function pointer
        stub_ = [](const void *, Args... args) -> R { return std::invoke(Function, std::forward<Args>(args)...); };
    }

    /**
     * @brief Attach a const member function to this delegate, as well as an instance.
     *
     * @tparam MemberFunction Member function pointer to attach
     * @tparam Class Class holding the member function
     * @param instance Instance pointer
     */
    template <auto MemberFunction, typename Class,
              typename = std::enable_if_t<std::is_invocable_r_v<R, decltype(MemberFunction), const Class *, Args...>>>
    auto bind(const Class *instance) -> void
    {
        // This time we need to provide an instance pointer
        instance_ = instance;
        stub_ = [](const void *p, Args... args) -> R {
            const auto *c = static_cast<const Class *>(p);
            return std::invoke(MemberFunction, c, std::forward<Args>(args)...);
        };
    }

    /**
     * @brief Attach a non-const member function to this delegate, as well as an instance.
     *
     * @tparam MemberFunction Member function pointer to attach
     * @tparam Class Class holding the member function
     * @param instance Instance pointer
     */
    template <auto MemberFunction, typename Class,
              typename = std::enable_if_t<std::is_invocable_r_v<R, decltype(MemberFunction), Class *, Args...>>>
    auto bind(Class *instance) -> void
    {
        instance_ = instance;
        stub_ = [](const void *p, Args... args) -> R {
            // I don't like const_cast but can't find an easy alternative
            auto *c = const_cast<Class *>(static_cast<const Class *>(p));
            return std::invoke(MemberFunction, c, std::forward<Args>(args)...);
        };
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