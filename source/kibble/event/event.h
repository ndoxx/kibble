#pragma once

#include "../ctti/ctti.h"
#include <cstdint>
#include <string_view>

namespace kb
{
namespace event
{

using EventID = hash_t;

#define EVENT_DECLARATIONS(EVENT_NAME)                                                                                 \
    static constexpr std::string_view NAME = #EVENT_NAME;                                                              \
    static constexpr EventID ID = kb::ctti::type_id<EVENT_NAME>()

} // namespace event
} // namespace kb