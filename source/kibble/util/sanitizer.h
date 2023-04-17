#pragma once

#if defined(__has_feature) && __has_feature(thread_sanitizer)
#define NO_SANITIZE_THREAD __attribute__((no_sanitize("thread")))
#define ANNOTATE_HAPPENS_BEFORE(addr) AnnotateHappensBefore(__FILE__, __LINE__, static_cast<void*>(addr))
#define ANNOTATE_HAPPENS_AFTER(addr) AnnotateHappensAfter(__FILE__, __LINE__, static_cast<void*>(addr))
extern "C" void AnnotateHappensBefore(const char* f, int l, void* addr);
extern "C" void AnnotateHappensAfter(const char* f, int l, void* addr);
#else
#define NO_SANITIZE_THREAD
#define ANNOTATE_HAPPENS_BEFORE(addr)
#define ANNOTATE_HAPPENS_AFTER(addr)
#endif

#if defined(__has_feature) && __has_feature(memory_sanitizer)
#define NO_SANITIZE_MEMORY __attribute__((no_sanitize("memory")))
#else
#define NO_SANITIZE_MEMORY
#endif