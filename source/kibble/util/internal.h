#pragma once

#include <memory>

namespace kb
{

/*
 * I like to use std::unique_ptr to implement the Pimpl idiom. However, if a unique pointer can
 * be constructed with an incomplete type T and the default deleter, T must be complete where
 * the deleter is invoked. This is typically not the case when using an API.
 * This utility stuff is a workaround. Usage:
 *
 * // header
 * class API
 * {
 *     // ...
 * private:
 *     struct Internal;
 *     friend struct internal_deleter::Deleter<Internal>;
 *     internal_ptr<Internal> internal_ = nullptr;
 * };
 *
 * // impl
 * namespace internal_deleter
 * {
 * template <> void Deleter<API::Internal>::operator()(API::Internal* p) { delete p; }
 * }
 *
 * // possible call inside API::method()
 * internal_ = make_internal<Internal>(create_info);
 */

namespace internal_deleter
{
template <typename T>
struct Deleter
{
    void operator()(T* p);
};
} // namespace internal_deleter

template <typename T>
using internal_ptr = std::unique_ptr<T, internal_deleter::Deleter<T>>;

template <typename T, typename... ArgsT>
inline internal_ptr<T> make_internal(ArgsT&&... args)
{
    return internal_ptr<T>(new T(std::forward<ArgsT>(args)...));
}

} // namespace kb