#include "uri/uri.h"

#include <algorithm>

namespace kb::uri
{

namespace
{

constexpr std::string_view k_scheme_separator = "://";

/**
 * @internal
 * @brief Splits a query or fragment body into KeyValueView entries.
 */
void split_entries(std::string_view block, std::vector<KeyValueView>& out)
{
    /*
        Entries are separated by '&', each split into key/value at the first
        '=' it contains. No '=' means a value-less flag. Stray separators
        (e.g. "a=1&&b=2") are skipped, an empty entry carries no info.
    */
    while (!block.empty())
    {
        const auto amp_pos = block.find('&');
        const std::string_view entry = block.substr(0, amp_pos);

        if (!entry.empty())
        {
            const auto eq_pos = entry.find('=');
            if (eq_pos == std::string_view::npos)
            {
                out.push_back(KeyValueView{entry, std::string_view{}});
            }
            else
            {
                out.push_back(KeyValueView{entry.substr(0, eq_pos), entry.substr(eq_pos + 1)});
            }
        }

        if (amp_pos == std::string_view::npos)
        {
            break;
        }
        block.remove_prefix(amp_pos + 1);
    }
}

} // namespace

UriParseResult parse_uri(std::string_view text) noexcept
{
    if (text.empty())
    {
        return std::unexpected(UriParseError::EmptyURI);
    }

    const auto scheme_end = text.find(k_scheme_separator);
    if (scheme_end == std::string_view::npos)
    {
        return std::unexpected(UriParseError::MissingSchemeSeparator);
    }
    if (scheme_end == 0)
    {
        return std::unexpected(UriParseError::EmptyScheme);
    }

    const std::string_view scheme = text.substr(0, scheme_end);
    std::string_view rest = text.substr(scheme_end + k_scheme_separator.size());

    // Rest is now: path[?query][#fragment]
    const auto query_start = rest.find('?');
    const auto fragment_start = rest.find('#');

    /*
        Path ends at whichever of '?' / '#' comes first, either may be absent.
        A fragment can appear without a query (path#frag) and vice versa, so
        just take the min of the two positions, npos acts as infinity here.
    */
    const auto path_end = std::min(query_start, fragment_start);
    const std::string_view path = rest.substr(0, path_end);

    if (path.empty())
    {
        return std::unexpected(UriParseError::EmptyPath);
    }

    URIView uri;
    uri.scheme = scheme;
    uri.path = path;

    /*
        Query block only counts if '?' shows up before any '#' (or there's no
        '#' at all). If '#' comes first, a later '?' is just fragment content,
        not a query separator, so don't split on it.
    */
    if (query_start != std::string_view::npos &&
        (fragment_start == std::string_view::npos || query_start < fragment_start))
    {
        const auto query_body_start = query_start + 1;
        const auto query_body_end = (fragment_start == std::string_view::npos) ? rest.size() : fragment_start;
        const std::string_view query_block = rest.substr(query_body_start, query_body_end - query_body_start);
        split_entries(query_block, uri.query);
    }

    // Fragment is everything after the first '#', there's only ever one block.
    if (fragment_start != std::string_view::npos)
    {
        const std::string_view fragment_block = rest.substr(fragment_start + 1);
        split_entries(fragment_block, uri.fragment);
    }

    return uri;
}

const KeyValueView* find_key(const std::vector<KeyValueView>& entries, std::string_view key) noexcept
{
    for (const KeyValueView& entry : entries)
    {
        if (entry.key == key)
        {
            return &entry;
        }
    }
    return nullptr;
}

} // namespace kb::uri