#include "kibble/uri/uri.h"
#include "kibble/uri/formatters/uri.h" // IWYU pragma: keep

#include "fmt/core.h"

namespace
{

void print_uri(std::string_view text)
{
    const auto result = kb::uri::parse_uri(text);

    fmt::print("input:  {}\n", text);
    fmt::print("result: {}\n", result);

    if (result.has_value())
    {
        const auto& uri = result.value();
        fmt::print("  scheme:   {}\n", uri.scheme);
        fmt::print("  path:     {}\n", uri.path);

        if (uri.has_query())
        {
            for (const auto& entry : uri.query)
            {
                fmt::print("  query:    {}\n", entry);
            }
        }
        if (uri.has_fragment())
        {
            for (const auto& entry : uri.fragment)
            {
                fmt::print("  fragment: {}\n", entry);
            }
        }
    }

    fmt::print("\n");
}

} // namespace

int main()
{
    // A scn:// URI from the game engine editor's navigation bar, the first
    // real consumer of this parser, but the parser itself knows nothing
    // about scenes or entities, any scheme works the same way.
    print_uri("scn://path/to/scene.scn.hades");
    print_uri("scn://path/to/scene.scn.hades#id={b0e1206a-d7b9-48ff-a57e-4cdb5be6497f}");
    print_uri("scn://path/to/scene.scn.hades#name=\"light_2\"");
    print_uri("scn://path/to/scene.scn.hades"
              "?cam=pos:10,5,3;rot:0,45,0&play=1&tool=translate&action=frame"
              "#id={b0e1206a-d7b9-48ff-a57e-4cdb5be6497f}");

    // A different scheme entirely, just to show it's not scn-specific.
    print_uri("bp://path/to/blueprint.bp.hades?action=select");

    // A couple of malformed ones, just to see the error path print cleanly.
    print_uri("not_a_uri_at_all");
    print_uri("scn://");

    return 0;
}