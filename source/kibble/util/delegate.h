#pragma once

#include <cstddef>
#include <exception>
#include <functional>
#include <tuple>
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
    const char* what() const throw()
    {
        return "Cannot invoke a member function without a class instance";
    }
};

// Primary template
template <typename T, typename = void>
struct function_traits;

// Specialization for function types
template <typename R, typename... Args>
struct function_traits<R(Args...)>
{
    using return_type = R;
    using args_tuple = std::tuple<std::decay_t<Args>...>;
    static constexpr std::size_t arity = sizeof...(Args);

    template <std::size_t N>
    using arg_type = std::tuple_element_t<N, args_tuple>;
};

// Specialization for function pointers
template <typename R, typename... Args>
struct function_traits<R (*)(Args...)> : public function_traits<R(Args...)>
{
};

// Specialization for member function pointers
template <typename C, typename R, typename... Args>
struct function_traits<R (C::*)(Args...)> : public function_traits<R(Args...)>
{
    using class_type = C;
};

// Specialization for const member function pointers
template <typename C, typename R, typename... Args>
struct function_traits<R (C::*)(Args...) const> : public function_traits<R(Args...)>
{
    using class_type = C;
};

// Specialization for member object pointers
template <typename C, typename R>
struct function_traits<R C::*>
{
    using return_type = R;
    using class_type = C;
};

// Specialization for functors and lambdas
template <typename T>
struct function_traits<T, std::void_t<decltype(&T::operator())>> : public function_traits<decltype(&T::operator())>
{
};

/**
 * @brief A Delegate encapsulates a free or a member function.
 * This implementation is based on the excellent series of articles by Matthew Rodusek:
 * - https://bitwizeshift.github.io/posts/2021/02/24/creating-a-fast-and-efficient-delegate-type-part-1/
 * - https://bitwizeshift.github.io/posts/2021/02/24/creating-a-fast-and-efficient-delegate-type-part-2/
 * - https://bitwizeshift.github.io/posts/2021/02/24/creating-a-fast-and-efficient-delegate-type-part-3/
 *
 * Differences are:
 *     - Instead of the bind functions I wrote factories, so it is possible to create a delegate with a one-liner
 *       See: https://www.codeproject.com/articles/11015/the-impossibly-fast-c-delegates
 *     - Equal and not-equal comparison operators. Comparison is done indirectly by comparing the stubs and
 *       instances, but it works. See:
 *       https://www.codeproject.com/Articles/1170503/The-Impossibly-Fast-Cplusplus-Delegates-Fixed
 *
 * @tparam Signature
 */
template <typename Signature>
class Delegate;

/**
 * @brief Specialization of the Delegate class for function signatures with a return type and multiple arguments.
 *
 * @tparam R
 * @tparam Args
 */
template <typename R, typename... Args>
class Delegate<R(Args...)>
{
public:
    // Make delegates copyable
    Delegate() = default;
    Delegate(const Delegate& other) = default;
    auto operator=(const Delegate& other) -> Delegate& = default;

    /**
     * @brief Call the registered function
     *
     * @tparam UArgs Types of the arguments
     * @param args Arguments to forward to the function
     * @return R
     */
    template <typename... UArgs, typename = std::enable_if_t<std::is_invocable_v<R(Args...), UArgs...>>>
    auto operator()(UArgs&&... args) const -> R
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
    static auto create()
    {
        Delegate d;
        // A free function does not use any instance pointer, we leave it null
        // A lambda without capture is implicitly convertible to a function pointer
        d.stub_ = [](const void*, Args... args) -> R { return std::invoke(Function, std::forward<Args>(args)...); };
        return d;
    }

    /**
     * @brief Attach a const member function to this delegate, as well as an instance.
     *
     * @tparam MemberFunction Member function pointer to attach
     * @tparam Class Class holding the member function
     * @param instance Instance pointer
     */
    template <auto MemberFunction, typename Class,
              typename = std::enable_if_t<std::is_invocable_r_v<R, decltype(MemberFunction), const Class*, Args...>>>
    static auto create(const Class* instance)
    {
        Delegate d;
        // This time we need to provide an instance pointer
        d.instance_ = instance;
        d.stub_ = [](const void* p, Args... args) -> R {
            const auto* c = static_cast<const Class*>(p);
            return std::invoke(MemberFunction, c, std::forward<Args>(args)...);
        };
        return d;
    }

    /**
     * @brief Attach a non-const member function to this delegate, as well as an instance.
     *
     * @tparam MemberFunction Member function pointer to attach
     * @tparam Class Class holding the member function
     * @param instance Instance pointer
     */
    template <auto MemberFunction, typename Class,
              typename = std::enable_if_t<std::is_invocable_r_v<R, decltype(MemberFunction), Class*, Args...>>>
    static auto create(Class* instance)
    {
        Delegate d;
        d.instance_ = instance;
        d.stub_ = [](const void* p, Args... args) -> R {
            // I don't like const_cast but can't find an easy alternative.
            // However it's safe, because we know the instance pointer was
            // bound to a non-const instance.
            auto* c = const_cast<Class*>(static_cast<const Class*>(p));
            return std::invoke(MemberFunction, c, std::forward<Args>(args)...);
        };
        return d;
    }

    /**
     * @brief Check if two delegates are the same
     *
     * @param rhs the other delegate
     * @return true if they point to the same function and the same instance in the
     * case of member delegates
     * @return false otherwise
     */
    bool operator==(const Delegate& rhs) const
    {
        // If two stubs are different, it will always mean that the underlying function
        // pointers are different and vice versa.
        return (instance_ == rhs.instance_ && stub_ == rhs.stub_);
    }

    /**
     * @brief Check if two delegates are different
     *
     * @param rhs the other delegate
     * @return true if they point to a different member function or a different
     * instance in the case of member delegates
     * @return false otherwise
     */
    bool operator!=(const Delegate& rhs) const
    {
        return (instance_ != rhs.instance_ || stub_ != rhs.stub_);
    }

private:
    using stub_function = R (*)(const void*, Args...);

    [[noreturn]] static auto stub_null(const void*, Args...) -> R
    {
        throw BadDelegateCallException{};
    }

    const void* instance_ = nullptr;
    stub_function stub_ = &stub_null;
};

template <size_t ArgSize = 64>
class PackagedDelegate
{
public:
    struct alignas(std::max_align_t) ArgStorage
    {
        std::byte data[ArgSize];
    };

    template <typename Signature>
    PackagedDelegate(const Delegate<Signature>& delegate) : instance_(&delegate)
    {
        using ReturnType = typename function_traits<Signature>::return_type;
        using ArgsTuple = typename function_traits<Signature>::args_tuple;
        static_assert(sizeof(ArgsTuple) <= sizeof(ArgStorage), "Arguments too large for ArgStorage");

        execute_ = [](const void* instance, const ArgStorage* args, void* result) {
            auto& del = *static_cast<const Delegate<Signature>*>(instance);
            if constexpr (std::is_void_v<ReturnType>)
            {
                std::apply(del, *reinterpret_cast<const ArgsTuple*>(args));
            }
            else
            {
                *static_cast<ReturnType*>(result) = std::apply(del, *reinterpret_cast<const ArgsTuple*>(args));
            }
        };
    }

    /**
     * @brief Store argument values for future execution
     *
     * @note It is the caller's responsibility to ensure that the argument types
     * match those expected by the stored delegate. Mismatched types will lead
     * to undefined behavior.
     *
     * @tparam Args
     * @param args
     */
    template <typename... Args>
    inline void prepare(Args&&... args)
    {
        using ArgsTuple = std::tuple<std::decay_t<Args>...>;
        new (&arg_storage_) ArgsTuple(std::forward<Args>(args)...);
    }

    template <typename ReturnType>
    ReturnType execute() const
    {
        if constexpr (std::is_void_v<ReturnType>)
        {
            execute_(instance_, &arg_storage_, nullptr);
        }
        else
        {
            ReturnType result;
            execute_(instance_, &arg_storage_, &result);
            return result;
        }
    }

    inline void operator()() const
    {
        execute<void>();
    }

private:
    using ExecuteFunc = void (*)(const void*, const ArgStorage*, void*);
    ArgStorage arg_storage_;
    ExecuteFunc execute_;
    const void* instance_;
};

} // namespace kb