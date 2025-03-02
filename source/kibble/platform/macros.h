#pragma once

#include "kibble/platform/platform.h"

#if defined(K_COMPILER_MSVC)
#define KB_PRETTY_FUNCTION __FUNCSIG__
#else
#define KB_PRETTY_FUNCTION __PRETTY_FUNCTION__
#endif