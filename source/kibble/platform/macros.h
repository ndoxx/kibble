#pragma once

#include "kibble/platform/platform.h"

#if defined(K_COMPILER_MSVC)
#define __PRETTY_FUNCTION__ __FUNCSIG__
#endif