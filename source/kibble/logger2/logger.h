#include "entry_builder.h"

// Constructs an EntryBuilder in place, and feeds it contextual information
#define klog(CHANNEL) kb::log::EntryBuilder(CHANNEL, __LINE__, __FILE__, __PRETTY_FUNCTION__)
// For printf-debugging. It's lame, but everybody does it.
#define kbang(CHANNEL) kb::log::EntryBuilder(CHANNEL, __LINE__, __FILE__, __PRETTY_FUNCTION__).warn("  \u0489")