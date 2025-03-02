#include "kibble/logger/entry_builder.h"
#include "kibble/platform/macros.h"

/// Constructs an EntryBuilder in place, and feeds it contextual information
#define klog(CHANNEL) kb::log::EntryBuilder(CHANNEL, __LINE__, __FILE__, KB_PRETTY_FUNCTION)
/// For printf-debugging. It's lame, but everybody does it.
#define kbang(CHANNEL) kb::log::EntryBuilder(CHANNEL, __LINE__, __FILE__, KB_PRETTY_FUNCTION).warn("  \u0489")