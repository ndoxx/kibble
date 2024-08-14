#include "kibble/memory/util/debug.h"
#include "kibble/string/string.h"

#include "fmt/color.h"
#include "fmt/core.h"
#include <unordered_map>

namespace kb::memory::util
{

static std::unordered_map<uint32_t, fmt::rgb> s_highlights;

void hex_dump_highlight(uint32_t word, math::argb32_t color)
{
    s_highlights[word] = fmt::rgb(uint8_t(color.r()), uint8_t(color.g()), uint8_t(color.b()));
}

void hex_dump_clear_highlights()
{
    s_highlights.clear();
}

void hex_dump(const void* ptr, std::size_t size, const std::string& title)
{
    const uint32_t* begin = static_cast<const uint32_t*>(ptr);
    const uint32_t* end = begin + size / 4;
    uint32_t* current = const_cast<uint32_t*>(begin);

    // Find 32 bytes aligned address before current if current not 32 bytes aligned itself
    std::size_t begin_offset = std::size_t(current) % 32;
    current -= begin_offset / 4;
    // Find offset of 32 bytes aligned address after end if end not 32 bytes aligned itself
    std::size_t end_offset = 32 - std::size_t(end) % 32;

    std::string dump_title = title.size() ? title : "HEX DUMP";
    kb::su::center(dump_title, 12);
    fmt::print("/-{}-\\\n", fmt::styled(dump_title, fmt::fg(fmt::rgb(102, 153, 0))));

    while (current < end + end_offset / 4)
    {
        // Show 32 bytes aligned addresses
        if (std::size_t(current) % 32 == 0)
        {
            fmt::print("[0x{:016x}] ", std::size_t(current));
        }

        // Separator after 16 bytes aligned addresses
        else if (std::size_t(current) % 16 == 0)
        {
            fmt::print(" ");
        }

        // Out-of-scope data in dark gray dots
        if (current < begin || current >= end)
        {
            fmt::print("{}", fmt::styled("........", fmt::fg(fmt::rgb(100, 100, 100))));
        }
        else
        {
            // Get current value
            uint32_t value = *current;
            // Display
            auto it = s_highlights.find(value);
            // Highlight recognized words
            if (it != s_highlights.end())
            {
                fmt::print("{:08x}", fmt::styled(value, fmt::bg(it->second)));
            }
            // Basic display
            else
            {
                fmt::print("{:08x}", value);
            }
        }

        // Jump lines before 32 bytes aligned addresses
        if (std::size_t(current) % 32 == 28)
        {
            fmt::print("\n");
        }
        else
        {
            fmt::print(" ");
        }

        ++current;
    }
}

} // namespace kb::memory::util