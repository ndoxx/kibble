#pragma once

// OS
#if defined(_WIN32)
#define K_PLATFORM_NAME "windows" // Windows
#define K_PLATFORM_WINDOWS
#define K_PLATFORM_WINDOWS_32
#elif defined(_WIN64)
#define K_PLATFORM_NAME "windows" // Windows
#define K_PLATFORM_WINDOWS
#define K_PLATFORM_WINDOWS_64
#elif defined(__CYGWIN__) && !defined(_WIN32)
#define K_PLATFORM_NAME "windows" // Windows (Cygwin POSIX under Microsoft Window)
#define K_PLATFORM_WINDOWS
#elif defined(__ANDROID__)
#define K_PLATFORM_NAME "android" // Android (implies Linux, so it must come first)
#define K_PLATFORM_ANDROID
#elif defined(__linux__)
#define K_PLATFORM_NAME "linux" // Debian, Ubuntu, Gentoo, Fedora, openSUSE, RedHat, Centos and other
#define K_PLATFORM_LINUX
#elif defined(__unix__) || !defined(__APPLE__) && defined(__MACH__)
#include <sys/param.h>
#if defined(BSD)
#define K_PLATFORM_NAME "bsd" // FreeBSD, NetBSD, OpenBSD, DragonFly BSD
#define K_PLATFORM_BSD
#endif
#elif defined(__hpux)
#define K_PLATFORM_NAME "hp-ux" // HP-UX
#define K_PLATFORM_HPUX
#elif defined(_AIX)
#define K_PLATFORM_NAME "aix" // IBM AIX
#define K_PLATFORM_AIX
#elif defined(__APPLE__) && defined(__MACH__) // Apple OSX and iOS (Darwin)
#include <TargetConditionals.h>
#if TARGET_IPHONE_SIMULATOR == 1
#define K_PLATFORM_NAME "ios" // Apple iOS
#define K_PLATFORM_IOS
#define K_PLATFORM_APPLE
#elif TARGET_OS_IPHONE == 1
#define K_PLATFORM_NAME "ios" // Apple iOS
#define K_PLATFORM_IOS
#define K_PLATFORM_APPLE
#elif TARGET_OS_MAC == 1
#define K_PLATFORM_NAME "osx" // Apple OSX
#define K_PLATFORM_OSX
#define K_PLATFORM_APPLE
#endif
#elif defined(__sun) && defined(__SVR4)
#define K_PLATFORM_NAME "solaris" // Oracle Solaris, Open Indiana
#define K_PLATFORM_SOLARIS
#else
#define K_PLATFORM_NAME NULL
#define K_PLATFORM_UNKNOWN
#endif

// Compiler
#if defined(_MSC_VER)
#define K_COMPILER_MSVC
#define K_COMPILER_NAME "msvc"
#if defined(__clang__)
#define K_COMPILER_CLANG_CL // clang-cl acting as MSVC (ABI/API compatible)
#endif
#elif defined(__clang__)
#define K_COMPILER_CLANG
#define K_COMPILER_NAME "clang"
#elif defined(__GNUC__) || defined(__GNUG__)
#define K_COMPILER_GCC
#define K_COMPILER_NAME "gcc"
#endif