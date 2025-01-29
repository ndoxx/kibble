#pragma once

#include "kibble/platform/platform.h"

#if defined(K_COMPILER_CLANG)
#if defined(__has_feature) && __has_feature(thread_sanitizer)
#define NO_SANITIZE_THREAD __attribute__((no_sanitize("thread")))
#define ANNOTATE_HAPPENS_BEFORE(addr) AnnotateHappensBefore(__FILE__, __LINE__, static_cast<void*>(addr))
#define ANNOTATE_HAPPENS_AFTER(addr) AnnotateHappensAfter(__FILE__, __LINE__, static_cast<void*>(addr))
#define ANNOTATE_IGNORE_WRITES_BEGIN() AnnotateIgnoreWritesBegin(__FILE__, __LINE__)
#define ANNOTATE_IGNORE_WRITES_END() AnnotateIgnoreWritesEnd(__FILE__, __LINE__)
extern "C"
{
    void AnnotateHappensAfter(const char* file, int line, const volatile void* cv);
    void AnnotateHappensBefore(const char* file, int line, const volatile void* cv);
    void AnnotateIgnoreWritesBegin(const char* file, int line);
    void AnnotateIgnoreWritesEnd(const char* file, int line);
}
#else
#define NO_SANITIZE_THREAD
#define ANNOTATE_HAPPENS_BEFORE(addr)
#define ANNOTATE_HAPPENS_AFTER(addr)
#define ANNOTATE_IGNORE_WRITES_BEGIN()
#define ANNOTATE_IGNORE_WRITES_END()
#endif

#if defined(__has_feature) && __has_feature(memory_sanitizer)
#define NO_SANITIZE_MEMORY __attribute__((no_sanitize("memory")))
#else
#define NO_SANITIZE_MEMORY
#endif

#else
#define NO_SANITIZE_THREAD
#define NO_SANITIZE_MEMORY
#define ANNOTATE_HAPPENS_BEFORE(addr)
#define ANNOTATE_HAPPENS_AFTER(addr)
#define ANNOTATE_IGNORE_WRITES_BEGIN()
#define ANNOTATE_IGNORE_WRITES_END()
#endif