/**
 * @file uri.h
 * @brief Generic URI parser (scheme://path?query#fragment).
 *
 * Example: scn://path/to/scene.scn.hades?cam=pos:10,5,3;rot:0,45,0&play=1#id={uuid}
 *
 * This parser does NOT interpret query or fragment keys. It only splits the
 * URI into scheme / path / query entries / fragment entries, each exposed as
 * a std::string_view into the original buffer. Adding a new query key (e.g.
 * tool=translate) never touches this file, interpreting known keys is a
 * separate, later stage that can ignore any key it doesn't recognize.
 *
 * No percent-decoding or escaping is performed, this is a lightweight parser
 * for app-internal URI schemes, not a full web URL parser.
 *
 * All views in URIView stay valid only as long as the buffer passed to
 * parse_uri() stays alive and unmodified.
 */
#pragma once

#include <expected>
#include <string_view>
#include <vector>

namespace kb::uri
{

/**
 * @brief One key=value entry from a query or fragment block.
 *
 * value is empty (not absent) for value-less flags such as ?play.
 */
struct KeyValueView
{
    std::string_view key;   ///< Entry key, never empty.
    std::string_view value; ///< Entry value, empty for value-less flags.
};

/// @brief Why parsing failed.
enum class UriParseError
{
    EmptyURI,
    MissingSchemeSeparator, /// < no "://" found
    EmptyScheme,            /// < "://path" with nothing before "://"
    EmptyPath,              /// < "scn://" with nothing after "://"
};

/**
 * @brief Parsed, non-owning view over a URI.
 *
 * Every std::string_view here points into the buffer originally passed to
 * parse_uri(). Query and fragment entries are kept in left-to-right order as
 * they appeared in the source URI.
 */
struct URIView
{
    std::string_view scheme; ///< e.g. "scn"
    std::string_view path;   ///< e.g. "path/to/scene.scn.hades"

    std::vector<KeyValueView> query;    ///< Entries after '?', empty if none.
    std::vector<KeyValueView> fragment; ///< Entries after '#', empty if none.

    /// @brief True if the URI had a query block.
    [[nodiscard]] bool has_query() const noexcept
    {
        return !query.empty();
    }

    /// @brief True if the URI had a fragment block.
    [[nodiscard]] bool has_fragment() const noexcept
    {
        return !fragment.empty();
    }
};

/// @brief Result of parsing a URI, holds either a view or an error.
using UriParseResult = std::expected<URIView, UriParseError>;

/**
 * @brief Parses text into a URIView.
 *
 * Grammar: uri := scheme "://" path [ "?" query ] [ "#" fragment ]
 * query/fragment := entry ( "&" entry )*, entry := key [ "=" value ]
 *
 * text must outlive the returned view.
 */
[[nodiscard]] UriParseResult parse_uri(std::string_view text) noexcept;

/**
 * @brief Finds the first entry in entries whose key equals key.
 * @return Pointer to the matching entry, or nullptr if none match.
 */
[[nodiscard]] const KeyValueView* find_key(const std::vector<KeyValueView>& entries, std::string_view key) noexcept;

} // namespace kb::uri