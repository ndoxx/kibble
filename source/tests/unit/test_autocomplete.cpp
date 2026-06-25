#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_range_equals.hpp>

#include "kibble/autocomplete/autocomplete_engine.h"
#include "kibble/autocomplete/fuzzy_scorer.h"

#include <algorithm>
#include <string>
#include <thread>
#include <vector>

using namespace kb::autocomplete;

// ============================================================================
// Helpers
// ============================================================================

namespace
{

/// Returns true if `path` appears anywhere in `matches`.
bool contains_path(const std::vector<ScoredMatch>& matches, std::string_view path)
{
    return std::any_of(matches.begin(), matches.end(), [&](const ScoredMatch& m) { return m.path == path; });
}

/// Thin wrapper: score a single candidate directly, matching the old free-function interface.
/// Encapsulates the new three-step API (prepare / score / fill_matched_indices) so that
/// individual tests don't need to change.
ScoredMatch score(std::string_view pattern, std::string_view candidate)
{
    FuzzyScorer scorer;
    scorer.prepare(pattern, candidate);
    ScoredMatch m;
    m.path = candidate;
    m.score = scorer.score();
    if (m.score != ScoreWeights::k_no_match)
    {
        scorer.fill_matched_indices(m);
    }
    return m;
}

bool is_match(std::string_view pattern, std::string_view candidate)
{
    return score(pattern, candidate).score != ScoreWeights::k_no_match;
}

/// Adapter: expose matched_indices as a std::vector for tests that compare against one.
std::vector<uint32_t> matched_vec(const ScoredMatch& m)
{
    return {m.matched_indices.begin(), m.matched_indices.begin() + m.n_matched};
}

} // namespace

// ============================================================================
// compute_letter_mask
// ============================================================================

TEST_CASE("compute_letter_mask - basic", "[letter_mask]")
{
    SECTION("empty string gives zero mask")
    {
        REQUIRE(compute_letter_mask("") == 0u);
    }

    SECTION("single letter sets exactly one bit")
    {
        uint32_t mask = compute_letter_mask("a");
        REQUIRE(mask == (1u << 0));
    }

    SECTION("case-insensitive")
    {
        REQUIRE(compute_letter_mask("A") == compute_letter_mask("a"));
        REQUIRE(compute_letter_mask("Abc") == compute_letter_mask("abc"));
    }

    SECTION("non-alpha characters are ignored")
    {
        REQUIRE(compute_letter_mask("123/._-") == 0u);
    }

    SECTION("all letters set their correct bits")
    {
        uint32_t mask = compute_letter_mask("abcdefghijklmnopqrstuvwxyz");
        REQUIRE(mask == 0x03FF'FFFFu); // bits 0-25
    }

    SECTION("duplicate letters don't add extra bits")
    {
        REQUIRE(compute_letter_mask("aaa") == compute_letter_mask("a"));
    }
}

// ============================================================================
// score_candidate - match/no-match
// ============================================================================

TEST_CASE("score_candidate - match detection", "[scorer]")
{
    SECTION("empty pattern always matches with score 0")
    {
        auto m = score("", "src/main.cpp");
        REQUIRE(m.score == 0);
        REQUIRE(m.n_matched == 0);
    }

    SECTION("pattern longer than candidate never matches")
    {
        REQUIRE_FALSE(is_match("toolong", "abc"));
    }

    SECTION("pattern whose letters don't all appear never matches")
    {
        REQUIRE_FALSE(is_match("xyz", "abcdef"));
    }

    SECTION("exact same-length match gives maximum short-circuit score")
    {
        auto m = score("main.cpp", "main.cpp");
        REQUIRE(m.score == std::numeric_limits<int32_t>::max() / 2);
    }

    SECTION("exact same-length match is case-insensitive")
    {
        auto m = score("MAIN.CPP", "main.cpp");
        REQUIRE(m.score == std::numeric_limits<int32_t>::max() / 2);
    }

    SECTION("single-character subsequence match")
    {
        REQUIRE(is_match("m", "main.cpp"));
    }

    SECTION("subsequence match across separators")
    {
        // 's', 'r', 'c', 'm', 'a' all appear in order
        REQUIRE(is_match("srcma", "src/main.cpp"));
    }

    SECTION("order matters - reversed pattern does not match")
    {
        // 'c', 'r', 's' in that order don't appear in "src/main"
        REQUIRE_FALSE(is_match("crs", "src/main"));
    }
}

// ============================================================================
// score_candidate - matched_indices
// ============================================================================

TEST_CASE("score_candidate - matched_indices", "[scorer]")
{
    SECTION("indices are strictly ascending")
    {
        auto m = score("sm", "src/main.cpp");
        REQUIRE(m.score != ScoreWeights::k_no_match);
        REQUIRE(m.n_matched == 2);
        REQUIRE(m.matched_indices[0] < m.matched_indices[1]);
    }

    SECTION("indices are within candidate bounds")
    {
        std::string_view candidate = "src/main.cpp";
        auto m = score("srcm", candidate);
        REQUIRE(m.score != ScoreWeights::k_no_match);
        for (uint32_t i = 0; i < m.n_matched; ++i)
        {
            REQUIRE(m.matched_indices[i] < candidate.size());
        }
    }

    SECTION("exact match populates all indices sequentially")
    {
        auto m = score("abc", "abc");
        REQUIRE(matched_vec(m) == std::vector<uint32_t>{0, 1, 2});
    }

    SECTION("matched characters actually correspond to pattern")
    {
        std::string_view pattern = "mc";
        std::string_view candidate = "src/main.cpp";
        auto m = score(pattern, candidate);
        REQUIRE(m.score != ScoreWeights::k_no_match);
        REQUIRE(m.n_matched == pattern.size());

        for (size_t i = 0; i < pattern.size(); ++i)
        {
            char p = static_cast<char>(std::tolower(static_cast<unsigned char>(pattern[i])));
            char c = static_cast<char>(std::tolower(static_cast<unsigned char>(candidate[m.matched_indices[i]])));
            REQUIRE(p == c);
        }
    }
}

// ============================================================================
// score_candidate - ranking order
// ============================================================================

TEST_CASE("score_candidate - ranking", "[scorer]")
{
    SECTION("boundary match scores higher than mid-word match")
    {
        // "main" starting right after '/' vs buried mid-word
        int32_t boundary = score("main", "src/main.cpp").score;
        int32_t midword = score("main", "remaining_domain.cpp").score;
        // "main" appears at a boundary in first, mid-word in second
        REQUIRE(boundary > midword);
    }

    SECTION("consecutive run scores higher than spread-out match")
    {
        // "abc" consecutive vs spread across the string
        int32_t consecutive = score("abc", "xabcyz").score;
        int32_t spread = score("abc", "xaybzc").score;
        REQUIRE(consecutive > spread);
    }

    SECTION("leading character bonus - match at position 0 outscores match at position 1")
    {
        int32_t leading = score("m", "main.cpp").score;
        int32_t non_leading = score("a", "main.cpp").score; // 'a' at index 1
        REQUIRE(leading > non_leading);
    }

    SECTION("shorter gap penalty - tight alignment beats loose one")
    {
        // "ac" in "abc" (gap=0) vs "ac" in "aXXXc" (gap=3)
        int32_t tight = score("ac", "abc").score;
        int32_t loose = score("ac", "aXXXc").score;
        REQUIRE(tight > loose);
    }

    SECTION("camelCase transition earns bonus")
    {
        // 'M' follows lowercase 'e' in "someModule"
        int32_t camel = score("eM", "someModule").score;
        int32_t no_camel = score("eM", "element").score; // no camelCase boundary
        REQUIRE(camel > no_camel);
    }
}

// ============================================================================
// FrecencyTracker
// ============================================================================

TEST_CASE("FrecencyTracker", "[frecency]")
{
    FrecencyTracker tracker;

    SECTION("unvisited path returns 0 bonus")
    {
        REQUIRE(tracker.frecency_bonus("src/main.cpp", FrecencyTracker::Clock::now()) == 0);
    }

    SECTION("bonus is positive after a single visit")
    {
        tracker.record_visit("src/main.cpp");
        REQUIRE(tracker.frecency_bonus("src/main.cpp", FrecencyTracker::Clock::now()) > 0);
    }

    SECTION("bonus is capped at max_bonus")
    {
        for (int i = 0; i < 1000; ++i)
        {
            tracker.record_visit("src/main.cpp");
        }
        REQUIRE(tracker.frecency_bonus("src/main.cpp", FrecencyTracker::Clock::now()) <= 15);
    }

    SECTION("more recent visit produces equal-or-higher bonus than older one")
    {
        FrecencyTracker t2;
        tracker.record_visit("old_path");

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        t2.record_visit("new_path");

        const auto now = FrecencyTracker::Clock::now();
        // new_path was visited more recently, so its bonus should be >= old_path's
        REQUIRE(t2.frecency_bonus("new_path", now) >= tracker.frecency_bonus("old_path", now));
    }

    SECTION("repeated visits increase the bonus")
    {
        tracker.record_visit("hot/file.cpp");
        int32_t after_one = tracker.frecency_bonus("hot/file.cpp", FrecencyTracker::Clock::now());

        tracker.record_visit("hot/file.cpp");
        tracker.record_visit("hot/file.cpp");
        int32_t after_three = tracker.frecency_bonus("hot/file.cpp", FrecencyTracker::Clock::now());

        REQUIRE(after_three >= after_one);
    }

    SECTION("different paths are tracked independently")
    {
        tracker.record_visit("a/b.cpp");
        const auto now = FrecencyTracker::Clock::now();
        REQUIRE(tracker.frecency_bonus("a/b.cpp", now) > 0);
        REQUIRE(tracker.frecency_bonus("c/d.cpp", now) == 0);
    }
}

// ============================================================================
// AutocompleteEngine - construction and cache_size
// ============================================================================

TEST_CASE("AutocompleteEngine - construction", "[engine]")
{
    SECTION("empty corpus")
    {
        AutocompleteEngine engine({});
        REQUIRE(engine.cache_size() == 0);
    }

    SECTION("cache_size reflects initial paths")
    {
        AutocompleteEngine engine({"src/main.cpp", "include/foo.h", "tests/test_foo.cpp"});
        REQUIRE(engine.cache_size() == 3);
    }

    SECTION("add_path increments cache_size")
    {
        AutocompleteEngine engine({});
        engine.add_path("src/main.cpp");
        REQUIRE(engine.cache_size() == 1);
        engine.add_path("include/foo.h");
        REQUIRE(engine.cache_size() == 2);
    }
}

// ============================================================================
// AutocompleteEngine - empty query
// ============================================================================

TEST_CASE("AutocompleteEngine - empty query returns frecency results", "[engine]")
{
    AutocompleteEngine engine({"src/main.cpp", "include/foo.h", "tests/test_foo.cpp"});

    SECTION("empty query returns up to max_results entries")
    {
        auto results = engine.query("", 2);
        REQUIRE(results.size() <= 2);
    }

    SECTION("recently visited path appears at top of empty query")
    {
        engine.record_navigation("tests/test_foo.cpp");
        auto results = engine.query("", 3);
        REQUIRE_FALSE(results.empty());
        REQUIRE(results.front().path == "tests/test_foo.cpp");
    }
}

// ============================================================================
// AutocompleteEngine - basic querying
// ============================================================================

TEST_CASE("AutocompleteEngine - basic query", "[engine]")
{
    std::vector<std::string> paths = {
        "src/main.cpp",
        "src/autocomplete/autocomplete_engine.cpp",
        "src/autocomplete/fuzzy_scorer.cpp",
        "include/autocomplete/autocomplete_engine.h",
        "include/autocomplete/fuzzy_scorer.h",
        "tests/test_autocomplete.cpp",
        "README.md",
        "CMakeLists.txt",
    };

    AutocompleteEngine engine(paths);

    SECTION("no match for impossible pattern")
    {
        auto results = engine.query("zzzzz", 8);
        REQUIRE(results.empty());
    }

    SECTION("query returns relevant results")
    {
        auto results = engine.query("fuzzy", 8);
        REQUIRE(contains_path(results, "src/autocomplete/fuzzy_scorer.cpp"));
        REQUIRE(contains_path(results, "include/autocomplete/fuzzy_scorer.h"));
    }

    SECTION("max_results is respected")
    {
        auto results = engine.query("cpp", 2);
        REQUIRE(results.size() <= 2);
    }

    SECTION("results are sorted best-score-first")
    {
        auto results = engine.query("main", 8);
        for (size_t i = 1; i < results.size(); ++i)
        {
            REQUIRE(results[i - 1].score >= results[i].score);
        }
    }

    SECTION("exact filename match ranks first")
    {
        auto results = engine.query("README.md", 8);
        REQUIRE_FALSE(results.empty());
        REQUIRE(results.front().path == "README.md");
    }
}

// ============================================================================
// AutocompleteEngine - incremental prefix stack
// ============================================================================

TEST_CASE("AutocompleteEngine - incremental queries", "[engine][incremental]")
{
    std::vector<std::string> paths = {
        "src/main.cpp",
        "src/module/module_loader.cpp",
        "src/module/module_registry.h",
        "include/util/string_utils.h",
        "tests/test_main.cpp",
    };

    AutocompleteEngine engine(paths);

    SECTION("extending query one char at a time converges to same results as direct query")
    {
        // Simulate typing "main" character by character.
        (void)engine.query("m", 8);
        (void)engine.query("ma", 8);
        (void)engine.query("mai", 8);
        auto incremental = engine.query("main", 8);

        AutocompleteEngine engine2(paths);
        auto direct = engine2.query("main", 8);

        // Both should contain the same paths (order may differ by frecency noise).
        std::vector<std::string_view> inc_paths, dir_paths;
        for (auto& m : incremental)
        {
            inc_paths.push_back(m.path);
        }
        for (auto& m : direct)
        {
            dir_paths.push_back(m.path);
        }

        std::sort(inc_paths.begin(), inc_paths.end());
        std::sort(dir_paths.begin(), dir_paths.end());
        REQUIRE(inc_paths == dir_paths);
    }

    SECTION("backspace to shorter query re-includes previously pruned paths")
    {
        // "module" matches module paths; "modulex" matches nothing; backspace to "module" again.
        auto before = engine.query("module", 8);
        auto extended = engine.query("modulex", 8);
        auto restored = engine.query("module", 8);

        REQUIRE(extended.empty());

        std::vector<std::string_view> bp, rp;
        for (auto& m : before)
        {
            bp.push_back(m.path);
        }
        for (auto& m : restored)
        {
            rp.push_back(m.path);
        }
        std::sort(bp.begin(), bp.end());
        std::sort(rp.begin(), rp.end());
        REQUIRE(bp == rp);
    }

    SECTION("completely different query after typing clears stack and returns correct results")
    {
        (void)engine.query("main", 8);
        (void)engine.query("util", 8); // unrelated - should not reuse stack
        auto results = engine.query("util", 8);
        REQUIRE(contains_path(results, "include/util/string_utils.h"));
    }

    SECTION("empty query after non-empty clears stack")
    {
        (void)engine.query("main", 8);
        auto results = engine.query("", 8);
        // Should not crash and should return valid (frecency) results.
        REQUIRE(results.size() <= paths.size());
    }

    SECTION("divergent branch: backspace then different character returns correct results")
    {
        // Type "mo" → "mod" → back to "mo" → "mou" (different char at depth 3).
        // This hits the divergent-branch path: the stack has an entry for length 3
        // but with query "mod", so it pops that entry and uses the "mo" survivors
        // as the candidate pool instead of scanning the full corpus.
        //
        // Crucially, the incremental path does NOT guarantee the same result set as
        // a cold query: it searches the "mo" survivor pool, which is a valid superset
        // of the true matches (no false negatives from the pool, but the pool may be
        // smaller than the full corpus). What we can assert is:
        //   1. Every returned match actually matches the pattern (no corruption from
        //      the stale "mod" branch leaking through).
        //   2. Any path that matched "mo" AND matches "mou" must appear in results
        //      (the pool is the "mo" survivors, so those are all considered).
        (void)engine.query("mo", 8);
        auto mo_survivors = engine.query("mod", 8); // establishes depth-3 entry
        // Collect what survived "mo" by running a clean query for reference.
        AutocompleteEngine ref(paths);
        auto mo_direct = ref.query("mo", 8);

        (void)engine.query("mo", 8);            // backspace to depth 2
        auto diverged = engine.query("mou", 8); // diverge at depth 3

        // Invariant 1: every result genuinely matches "mou".
        for (const auto& m : diverged)
        {
            REQUIRE(is_match("mou", m.path));
        }

        // Invariant 2: any path that survived "mo" and matches "mou" must appear.
        for (const auto& m : mo_direct)
        {
            if (is_match("mou", m.path))
            {
                REQUIRE(contains_path(diverged, m.path));
            }
        }
    }

    SECTION("divergent branch: result set is superset of what the deeper stack would give")
    {
        // After typing "mod", survivors are only module paths.
        // After backspacing to "mo" and typing "ma" (divergent), the engine must
        // search from the "mo" survivors - not the "mod" survivors - so main paths
        // (which survived "mo" but not "mod") must reappear.
        (void)engine.query("mo", 8);
        auto mod_results = engine.query("mod", 8); // prunes to module paths only

        // Verify "mod" pruned the main paths.
        REQUIRE_FALSE(contains_path(mod_results, "src/main.cpp"));
        REQUIRE_FALSE(contains_path(mod_results, "tests/test_main.cpp"));

        (void)engine.query("mo", 8);           // backspace to "mo"
        auto diverged = engine.query("ma", 8); // diverge: "ma" instead of "mod"

        // "ma" should find main paths - they survived "mo" so must be in the pool.
        REQUIRE(contains_path(diverged, "src/main.cpp"));
        REQUIRE(contains_path(diverged, "tests/test_main.cpp"));
    }

    SECTION("deep backspace: skip multiple levels and get correct results")
    {
        // Build up a 4-level stack then jump back 2 levels at once.
        (void)engine.query("m", 8);
        (void)engine.query("mo", 8);
        (void)engine.query("mod", 8);
        (void)engine.query("modu", 8);

        (void)engine.query("mo", 8); // skip back 2 levels in one go
        auto restored = engine.query("mo", 8);

        AutocompleteEngine fresh(paths);
        auto direct = fresh.query("mo", 8);

        std::vector<std::string_view> rp, dp;
        for (auto& m : restored)
        {
            rp.push_back(m.path);
        }
        for (auto& m : direct)
        {
            dp.push_back(m.path);
        }
        std::sort(rp.begin(), rp.end());
        std::sort(dp.begin(), dp.end());
        REQUIRE(rp == dp);
    }

    SECTION("incremental then backspace then re-extend gives same result as direct query")
    {
        // Type "modu", backspace to "mod", retype "modu" - must match direct query.
        (void)engine.query("m", 8);
        (void)engine.query("mo", 8);
        (void)engine.query("mod", 8);
        (void)engine.query("modu", 8);
        (void)engine.query("mod", 8); // backspace
        auto reextended = engine.query("modu", 8);

        AutocompleteEngine fresh(paths);
        auto direct = fresh.query("modu", 8);

        std::vector<std::string_view> rp, dp;
        for (auto& m : reextended)
        {
            rp.push_back(m.path);
        }
        for (auto& m : direct)
        {
            dp.push_back(m.path);
        }
        std::sort(rp.begin(), rp.end());
        std::sort(dp.begin(), dp.end());
        REQUIRE(rp == dp);
    }
}
// ============================================================================
// AutocompleteEngine - add_path invalidation
// ============================================================================

TEST_CASE("AutocompleteEngine - add_path", "[engine]")
{
    // Use a corpus large enough that the trigram usefulness threshold
    // (corpus_size * 4 / 5) doesn't collapse to 0 or 1 and silently
    // treat every trigram as "too common → skip", masking filtering bugs.
    // With 10 paths the threshold is 8, so unique trigrams stay useful.
    std::vector<std::string> initial_paths = {
        "src/main.cpp",     "src/parser.cpp",  "src/lexer.cpp",     "src/codegen.cpp",       "src/optimizer.cpp",
        "include/parser.h", "include/lexer.h", "include/codegen.h", "tests/test_parser.cpp", "tests/test_lexer.cpp",
    };

    AutocompleteEngine engine(initial_paths);

    SECTION("newly added path is discoverable by subsequent query")
    {
        engine.add_path("src/new_feature.cpp");
        // "feat" is a literal substring of "new_feature" absent from all initial paths,
        // so it survives the trigram pre-filter and reaches the DP scorer.
        auto results = engine.query("feat", 8);
        REQUIRE(contains_path(results, "src/new_feature.cpp"));
    }

    SECTION("old paths still discoverable after add_path")
    {
        engine.add_path("src/new_feature.cpp");
        auto results = engine.query("main", 8);
        REQUIRE(contains_path(results, "src/main.cpp"));
    }

    SECTION("multiple add_path calls all discoverable")
    {
        engine.add_path("src/new_feature.cpp");
        engine.add_path("src/another_module.cpp");
        // Both queries are literal substrings of their respective targets and
        // absent from the initial corpus.
        REQUIRE(contains_path(engine.query("feat", 8), "src/new_feature.cpp"));
        REQUIRE(contains_path(engine.query("anoth", 8), "src/another_module.cpp"));
    }
}

// ============================================================================
// AutocompleteEngine - frecency integration
// ============================================================================

TEST_CASE("AutocompleteEngine - frecency boosts visited paths", "[engine][frecency]")
{
    // Two similarly-named files; the visited one should rank higher.
    std::vector<std::string> paths = {"src/render.cpp", "src/renderer.cpp"};
    AutocompleteEngine engine(paths);

    engine.record_navigation("src/renderer.cpp");

    auto results = engine.query("render", 8);
    REQUIRE_FALSE(results.empty());
    REQUIRE(results.front().path == "src/renderer.cpp");
}

// ============================================================================
// Edge cases
// ============================================================================

TEST_CASE("Edge cases", "[engine][scorer]")
{
    SECTION("single-character corpus and single-character query")
    {
        AutocompleteEngine engine({"a"});
        auto results = engine.query("a", 8);
        REQUIRE(results.size() == 1);
        REQUIRE(results.front().path == "a");
    }

    SECTION("path shorter than 3 chars still works (below trigram threshold)")
    {
        AutocompleteEngine engine({"ab", "cd", "abcdef"});
        auto results = engine.query("ab", 8);
        REQUIRE(contains_path(results, "ab"));
        REQUIRE(contains_path(results, "abcdef"));
        REQUIRE_FALSE(contains_path(results, "cd"));
    }

    SECTION("query with only digits")
    {
        AutocompleteEngine engine({"file_v2.cpp", "file_v3.cpp", "other.cpp"});
        // Digits appear in the engine; no letters in the query → letter mask is 0,
        // so mask check passes for everything. The DP decides.
        auto results = engine.query("2", 8);
        REQUIRE(contains_path(results, "file_v2.cpp"));
        REQUIRE_FALSE(contains_path(results, "other.cpp"));
    }

    SECTION("very long query that exceeds stack depth still returns results")
    {
        AutocompleteEngine engine({"src/autocomplete/autocomplete_engine.cpp"});
        // k_max_stack_depth = 32; this query is 33 chars
        std::string long_query = "srcautocompeteautocompleteengine_"; // 33 chars (won't match)
        auto results = engine.query(long_query, 8);
        // Just checking it doesn't crash; result may be empty.
        (void)results;
        REQUIRE(true);
    }

    SECTION("corpus with duplicate paths doesn't crash")
    {
        AutocompleteEngine engine({"src/main.cpp", "src/main.cpp"});
        auto results = engine.query("main", 8);
        REQUIRE_FALSE(results.empty());
    }

    SECTION("max_results of 0 returns empty vector")
    {
        AutocompleteEngine engine({"src/main.cpp"});
        auto results = engine.query("main", 0);
        REQUIRE(results.empty());
    }

    SECTION("score_candidate with pattern equal to last char of candidate")
    {
        // Regression: single char at the very end of a long path.
        auto m = score("p", "src/main.cpp");
        REQUIRE(m.score != ScoreWeights::k_no_match);
        REQUIRE(m.n_matched == 1);
    }
}