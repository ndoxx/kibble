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
    using signature = R(Args...);
    static constexpr std::size_t arity = sizeof...(Args);

    template <std::size_t N>
    using arg_type = std::tuple_element_t<N, args_tuple>;
};

// Specialization for noexcept function types
template <typename R, typename... Args>
struct function_traits<R(Args...) noexcept> : public function_traits<R(Args...)>
{
};

// Specialization for function pointers
template <typename R, typename... Args>
struct function_traits<R (*)(Args...)> : public function_traits<R(Args...)>
{
};

// Specialization for noexcept function pointers
template <typename R, typename... Args>
struct function_traits<R (*)(Args...) noexcept> : public function_traits<R(Args...)>
{
};

// Specialization for member function pointers
template <typename C, typename R, typename... Args>
struct function_traits<R (C::*)(Args...)> : public function_traits<R(Args...)>
{
    using class_type = C;
};

// Specialization for noexcept member function pointers
template <typename C, typename R, typename... Args>
struct function_traits<R (C::*)(Args...) noexcept> : public function_traits<R(Args...)>
{
    using class_type = C;
};

// Specialization for const member function pointers
template <typename C, typename R, typename... Args>
struct function_traits<R (C::*)(Args...) const> : public function_traits<R(Args...)>
{
    using class_type = C;
};

// Specialization for const noexcept member function pointers
template <typename C, typename R, typename... Args>
struct function_traits<R (C::*)(Args...) const noexcept> : public function_traits<R(Args...)>
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
 * @warning A free function assigned via operator= is stored by round-tripping its address through
 * a `const void*` (see free_stub() and operator=(F&&)). Converting a function pointer to an object
 * pointer and back is only *conditionally-supported* by the C++ standard, not strictly portable
 * ISO C++. In practice this works on every mainstream ABI (POSIX explicitly requires it, e.g. for
 * dlsym), but it could fail on an exotic target with a Harvard-style architecture or a platform
 * where code and data pointers differ in size/representation.
 *
 * @warning Comparison (operator==/operator!=) relies on the *address identity* of the internal
 * stub functions, one distinct static lambda per create<Function>() instantiation. Under normal
 * linking this correctly tracks "same binding". However, if a linker performs aggressive Identical
 * Code Folding (e.g. `lld --icf=all`, gold `--icf=all`), two stubs that happen to compile down to
 * byte-identical machine code (typically trivial bodies) may be folded into a single symbol, which
 * would make two otherwise-unrelated delegates spuriously compare equal. This is an accepted
 * limitation of function-pointer-identity-based delegates in general, not specific to this
 * implementation. Avoid safe/aggressive ICF for translation units using this class if strict
 * equality semantics matter.
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
     * @brief Check whether this delegate holds a callable target
     *
     * @return true if the delegate was bound to a function or lambda
     * @return false if the delegate is default-constructed or unbound
     */
    explicit operator bool() const noexcept
    {
        return stub_ != &null_stub;
    }

    /**
     * @brief Assign a capture-less lambda or a free function pointer to this delegate
     *
     * A capture-less lambda implicitly converts to a plain function pointer, so this
     * also covers direct assignment of free functions, eg d = my_free_function;
     *
     * @tparam F Type of the callable, must convert to a plain function pointer
     * @param f The callable to attach
     */
    template <typename F, typename = std::enable_if_t<std::is_convertible_v<std::decay_t<F>, R (*)(Args...)>>>
    auto operator=(F&& f) -> Delegate&
    {
        /*
            NOTE(ndx):
            The conversion to FreeFunction happens here, once, regardless of how f was spelled
            (a lambda, a named function, or an explicit function pointer). The stub itself is a
            plain non-template static member, so its address is the same in every case, which
            keeps equality comparisons meaningful: two delegates bound to the same function this
            way always compare equal, no matter which of the three forms was used to assign them.
        */
        auto fn = static_cast<FreeFunction>(f);
        instance_ = reinterpret_cast<const void*>(fn);
        stub_ = &free_stub;
        return *this;
    }

    /**
     * @brief Reset the delegate to the unbound state
     *
     * Without this overload, `d = nullptr;` would be accepted by the templated operator=(F&&)
     * above (std::nullptr_t is implicitly convertible to any function pointer type), which would
     * leave the delegate bound to free_stub with a null underlying function pointer: operator
     * bool() would then report the delegate as truthy, while actually invoking it would call
     * through a null function pointer (undefined behavior). This non-template overload is a
     * better match for a literal nullptr argument, so it is preferred by overload resolution and
     * correctly restores the same state as a default-constructed delegate.
     */
    auto operator=(std::nullptr_t) -> Delegate&
    {
        instance_ = nullptr;
        stub_ = &null_stub;
        return *this;
    }

    /**
     * @brief Call the registered function
     *
     * @tparam UArgs Types of the arguments
     * @param args Arguments to forward to the function
     * @return R
     *
     * @note The enable_if condition checks is_invocable_v against the *function type* R(Args...)
     * itself rather than against stub_ or some other callable. This is a compact way to ask
     * "can R(Args...) be called with UArgs&&...?", which is true exactly when each UArgs is
     * usable to construct/convert to the corresponding Args (including implicit conversions at
     * the call site, e.g. passing an int where a `const size_t&` is expected). It does not
     * actually invoke anything at compile time; it only piggybacks on std::is_invocable's
     * handling of function types to get argument-compatibility checking "for free" without
     * writing a bespoke type trait.
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
    bool operator==(const Delegate& rhs) const noexcept
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
    bool operator!=(const Delegate& rhs) const noexcept
    {
        return (instance_ != rhs.instance_ || stub_ != rhs.stub_);
    }

private:
    using stub_function = R (*)(const void*, Args...);
    using FreeFunction = R (*)(Args...);

    [[noreturn]] static auto null_stub(const void*, Args...) -> R
    {
        throw BadDelegateCallException{};
    }

    static auto free_stub(const void* p, Args... args) -> R
    {
        auto fn = reinterpret_cast<FreeFunction>(const_cast<void*>(p));
        return std::invoke(fn, std::forward<Args>(args)...);
    }

    const void* instance_ = nullptr;
    stub_function stub_ = &null_stub;
};

/**
 * @brief Create a delegate bound to a free function, deducing the signature
 *
 * Shorter than spelling out Delegate<Signature>::create<&Function>()
 *
 * @tparam Function Pointer to the free function to attach
 */
template <auto Function>
auto make_delegate()
{
    using Signature = typename function_traits<decltype(Function)>::signature;
    return Delegate<Signature>::template create<Function>();
}

/**
 * @brief Create a delegate bound to a member function and an instance, deducing the signature
 *
 * @tparam MemberFunction Member function pointer to attach
 * @tparam Class Class holding the member function
 * @param instance Instance pointer
 */
template <auto MemberFunction, typename Class>
auto make_delegate(Class* instance)
{
    using Signature = typename function_traits<decltype(MemberFunction)>::signature;
    return Delegate<Signature>::template create<MemberFunction>(instance);
}

/**
 * @brief Create a delegate bound to a const member function and an instance, deducing the signature
 *
 * @tparam MemberFunction Member function pointer to attach
 * @tparam Class Class holding the member function
 * @param instance Instance pointer
 */
template <auto MemberFunction, typename Class>
auto make_delegate(const Class* instance)
{
    using Signature = typename function_traits<decltype(MemberFunction)>::signature;
    return Delegate<Signature>::template create<MemberFunction>(instance);
}

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