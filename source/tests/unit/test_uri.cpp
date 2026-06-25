#include "kibble/uri/uri.h"

#include <catch2/catch_test_macros.hpp>

using namespace kb::uri;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static const KeyValueView* qkey(const URIView& uri, std::string_view k)
{
    return find_key(uri.query, k);
}

static const KeyValueView* fkey(const URIView& uri, std::string_view k)
{
    return find_key(uri.fragment, k);
}

// ---------------------------------------------------------------------------
// Error cases
// ---------------------------------------------------------------------------

TEST_CASE("parse_uri - error cases", "[uri][errors]")
{
    SECTION("empty input")
    {
        const auto r = parse_uri("");
        REQUIRE_FALSE(r.has_value());
        CHECK(r.error() == UriParseError::EmptyURI);
    }

    SECTION("no scheme separator")
    {
        const auto r = parse_uri("not_a_uri_at_all");
        REQUIRE_FALSE(r.has_value());
        CHECK(r.error() == UriParseError::MissingSchemeSeparator);
    }

    SECTION("empty scheme")
    {
        const auto r = parse_uri("://path");
        REQUIRE_FALSE(r.has_value());
        CHECK(r.error() == UriParseError::EmptyScheme);
    }

    SECTION("empty path - scheme only")
    {
        const auto r = parse_uri("scn://");
        REQUIRE_FALSE(r.has_value());
        CHECK(r.error() == UriParseError::EmptyPath);
    }

    SECTION("empty path - query present but no path")
    {
        // "scn://?key=val" - the path segment between "://" and "?" is empty.
        const auto r = parse_uri("scn://?key=val");
        REQUIRE_FALSE(r.has_value());
        CHECK(r.error() == UriParseError::EmptyPath);
    }
}

// ---------------------------------------------------------------------------
// Scheme and path extraction
// ---------------------------------------------------------------------------

TEST_CASE("parse_uri - scheme and path", "[uri][basic]")
{
    SECTION("simple path, no query or fragment")
    {
        const auto r = parse_uri("scn://path/to/scene.scn.hades");
        REQUIRE(r.has_value());
        CHECK(r->scheme == "scn");
        CHECK(r->path == "path/to/scene.scn.hades");
        CHECK(r->query.empty());
        CHECK(r->fragment.empty());
    }

    SECTION("scheme is captured correctly for a different scheme")
    {
        const auto r = parse_uri("bp://path/to/blueprint.bp.hades");
        REQUIRE(r.has_value());
        CHECK(r->scheme == "bp");
        CHECK(r->path == "path/to/blueprint.bp.hades");
    }
}

// ---------------------------------------------------------------------------
// Query parsing
// ---------------------------------------------------------------------------

TEST_CASE("parse_uri - query block", "[uri][query]")
{
    SECTION("single key=value")
    {
        const auto r = parse_uri("bp://path/to/blueprint.bp.hades?action=select");
        REQUIRE(r.has_value());
        REQUIRE(r->query.size() == 1);
        CHECK(r->query[0].key == "action");
        CHECK(r->query[0].value == "select");
    }

    SECTION("multiple entries preserved in order")
    {
        const auto r = parse_uri("scn://scene?cam=pos:10,5,3;rot:0,45,0&play=1&tool=translate&action=frame");
        REQUIRE(r.has_value());
        REQUIRE(r->query.size() == 4);
        CHECK(r->query[0].key == "cam");
        CHECK(r->query[0].value == "pos:10,5,3;rot:0,45,0");
        CHECK(r->query[1].key == "play");
        CHECK(r->query[1].value == "1");
        CHECK(r->query[2].key == "tool");
        CHECK(r->query[2].value == "translate");
        CHECK(r->query[3].key == "action");
        CHECK(r->query[3].value == "frame");
    }

    SECTION("value-less flag in query")
    {
        const auto r = parse_uri("scn://scene?play");
        REQUIRE(r.has_value());
        REQUIRE(r->query.size() == 1);
        CHECK(r->query[0].key == "play");
        CHECK(r->query[0].value == "");
    }

    SECTION("stray double-ampersand is skipped")
    {
        const auto r = parse_uri("scn://scene?a=1&&b=2");
        REQUIRE(r.has_value());
        CHECK(r->query.size() == 2);
        CHECK(r->query[0].key == "a");
        CHECK(r->query[1].key == "b");
    }
}

// ---------------------------------------------------------------------------
// Fragment parsing
// ---------------------------------------------------------------------------

TEST_CASE("parse_uri - fragment block", "[uri][fragment]")
{
    SECTION("single fragment entry")
    {
        const auto r = parse_uri("scn://scene#id={b0e1206a-d7b9-48ff-a57e-4cdb5be6497f}");
        REQUIRE(r.has_value());
        CHECK(r->query.empty());
        REQUIRE(r->fragment.size() == 1);
        CHECK(r->fragment[0].key == "id");
        CHECK(r->fragment[0].value == "{b0e1206a-d7b9-48ff-a57e-4cdb5be6497f}");
    }

    SECTION("fragment with quoted value")
    {
        const auto r = parse_uri("scn://scene#name=\"light_2\"");
        REQUIRE(r.has_value());
        REQUIRE(r->fragment.size() == 1);
        CHECK(r->fragment[0].key == "name");
        CHECK(r->fragment[0].value == "\"light_2\"");
    }
}

// ---------------------------------------------------------------------------
// Query + fragment together
// ---------------------------------------------------------------------------

TEST_CASE("parse_uri - query and fragment combined", "[uri][combined]")
{
    SECTION("full kitchen-sink URI")
    {
        const auto r = parse_uri("scn://path/to/scene.scn.hades"
                                 "?cam=pos:10,5,3;rot:0,45,0&play=1&tool=translate&action=frame"
                                 "#id={b0e1206a-d7b9-48ff-a57e-4cdb5be6497f}");
        REQUIRE(r.has_value());
        CHECK(r->scheme == "scn");
        CHECK(r->path == "path/to/scene.scn.hades");
        CHECK(r->query.size() == 4);
        CHECK(r->fragment.size() == 1);
    }

    SECTION("fragment before query: '?' inside fragment is not a query separator")
    {
        // The '#' comes before '?', so everything after '#' is fragment content.
        const auto r = parse_uri("scn://scene#frag?not_a_query=1");
        REQUIRE(r.has_value());
        CHECK(r->query.empty());
        REQUIRE(r->fragment.size() == 1);
        CHECK(r->fragment[0].key == "frag?not_a_query");
        CHECK(r->fragment[0].value == "1");
    }
}

// ---------------------------------------------------------------------------
// find_key
// ---------------------------------------------------------------------------

TEST_CASE("find_key", "[uri][find_key]")
{
    const auto r = parse_uri("scn://scene?cam=pos:10,5,3&play=1#id=abc");
    REQUIRE(r.has_value());

    SECTION("finds existing key")
    {
        const auto* e = qkey(*r, "cam");
        REQUIRE(e != nullptr);
        CHECK(e->value == "pos:10,5,3");
    }

    SECTION("returns nullptr for missing key")
    {
        CHECK(qkey(*r, "nonexistent") == nullptr);
    }

    SECTION("searches only the supplied vector - query miss in fragment")
    {
        // "id" is in the fragment, not the query.
        CHECK(qkey(*r, "id") == nullptr);
        CHECK(fkey(*r, "id") != nullptr);
    }
}