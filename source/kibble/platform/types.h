#pragma once

#include "kibble/platform/platform.h"

#if defined(K_PLATFORM_WINDOWS)
#include <cstddef>
#define ssize_t std::ptrdiff_t
#endif