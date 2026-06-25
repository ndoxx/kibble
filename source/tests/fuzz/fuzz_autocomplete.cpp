/**
 * @file fuzz_autocomplete.cpp
 * @brief Fuzz harness for AutocompleteEngine and its scoring/filtering layers.
 *
 * BUILD MODES
 * -----------
 * Standalone (deterministic, self-contained - good for CI and manual runs):
 *
 *   clang++ -std=c++20 -O2 -g \
 *       -Wall -Wextra -Wpedantic -Werror \
 *       -Wno-c99-extensions \
 *       -I<include_root> \
 *       fuzz_autocomplete.cpp autocomplete_engine.cpp fuzzy_scorer.cpp \
 *       -lfmt \
 *       -o fuzz_autocomplete
 *   ./fuzz_autocomplete              # runs all suites, exits 0 on pass
 *   ./fuzz_autocomplete 200000       # override iteration count
 *
 * LibFuzzer (coverage-guided, runs until killed or a crash is found):
 *
 *   clang++ -std=c++20 -O1 -g \
 *       -fsanitize=fuzzer,address,undefined \
 *       -I<include_root> \
 *       fuzz_autocomplete.cpp autocomplete_engine.cpp fuzzy_scorer.cpp \
 *       -lfmt \
 *       -o fuzz_autocomplete_libfuzzer
 *   ./fuzz_autocomplete_libfuzzer -max_len=256 corpus/
 *
 * The same translation unit compiles in both modes: the libFuzzer entry point
 * (LLVMFuzzerTestOneInput) is compiled only when
 * FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION is defined, which clang's
 * -fsanitize=fuzzer sets automatically. The standalone main() is compiled
 * otherwise.
 *
 * WHAT IS TESTED
 * --------------
 * Suite A - score_candidate oracle checks
 *   For every (pattern, candidate) pair drawn from random bytes, the greedy
 *   feasibility check and the full DP must agree on match/no-match. This is
 *   the soundness invariant established by feasibility_check.cpp; we keep
 *   testing it here so a future DP change can't silently break it.
 *
 * Suite B - score_candidate determinism & structural properties
 *   1. Determinism: same inputs → same score, same matched_indices.
 *   2. matched_indices are strictly ascending, within candidate bounds, and
 *      each entry case-insensitively matches the corresponding pattern char.
 *   3. Full-match shortcut: when |pattern| == |candidate| and they are
 *      case-insensitively equal, score must equal INT32_MAX/2 and
 *      matched_indices must be [0, 1, ..., n-1].
 *
 * Suite C - AutocompleteEngine::query invariants
 *   Drives the engine with simulated keystroke events (appends, backspaces,
 *   pastes, navigations) and checks:
 *   1. Result count is <= max_results.
 *   2. Results are in non-increasing score order.
 *   3. Every returned path exists in the corpus.
 *   4. Every returned path genuinely matches the pattern (feasibility check).
 *   5. Empty pattern always returns results with non-negative scores.
 *
 * Suite D - edge cases (empty strings, single chars, very long inputs, high
 *   bytes, null bytes, repeated characters).
 *
 * CRASH SEMANTICS
 * ---------------
 * Any failed assertion calls std::abort(), which libFuzzer catches as a crash
 * and saves the reproducer. In standalone mode it prints a diagnostic and
 * returns a nonzero exit code after accumulating up to 20 failures.
 */

#include "kibble/autocomplete/autocomplete_engine.h"
#include "kibble/autocomplete/fuzzy_scorer.h"

#include <fmt/core.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <random>
#include <string>
#include <string_view>
#include <vector>

using namespace kb::autocomplete;

// ---------------------------------------------------------------------------
// Assertion infrastructure
// ---------------------------------------------------------------------------
// In libFuzzer mode every failure aborts immediately (libFuzzer catches the
// signal and saves the minimised reproducer). In standalone mode failures are
// accumulated so a single run surfaces as many problems as possible, up to
// k_max_standalone_failures, after which we abort anyway.

#if !defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION)
static int32_t g_failures = 0;
static constexpr int32_t k_max_standalone_failures = 20;
#endif

/// Called by FUZZ_ASSERT on failure. Never returns.
#if defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION)
[[noreturn]] static void fuzz_fail_abort(std::string_view file, int32_t line, std::string_view cond,
                                         std::string_view msg)
{
    fmt::print(stderr, "[FAIL] {}:{}: {}\n       {}\n", file, line, cond, msg);
    std::abort();
}
#endif

#if !defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION)
/// Called by FUZZ_ASSERT on failure in standalone mode. May return.
static void fuzz_fail_accumulate(std::string_view file, int32_t line, std::string_view cond, std::string_view msg)
{
    fmt::print(stderr, "[FAIL] {}:{}: {}\n       {}\n", file, line, cond, msg);
    ++g_failures;
    if (g_failures >= k_max_standalone_failures)
    {
        fmt::print(stderr, "Too many failures, aborting.\n");
        std::abort();
    }
}
#endif

// In libFuzzer mode: abort on first failure so the reproducer is saved.
// In standalone mode: accumulate failures and continue.
#if defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION)
#define FUZZ_ASSERT(cond, msg)                                                                                         \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(cond))                                                                                                   \
        {                                                                                                              \
            fuzz_fail_abort(__FILE__, static_cast<int32_t>(__LINE__), #cond, (msg));                                   \
        }                                                                                                              \
    } while (false)
#else
#define FUZZ_ASSERT(cond, msg)                                                                                         \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(cond))                                                                                                   \
        {                                                                                                              \
            fuzz_fail_accumulate(__FILE__, static_cast<int32_t>(__LINE__), #cond, (msg));                              \
        }                                                                                                              \
    } while (false)
#endif

// ---------------------------------------------------------------------------
// Shared helpers
// ---------------------------------------------------------------------------

/// Case-insensitive greedy two-pointer subsequence feasibility check.
/// Returns true iff every character of `pattern` appears in `candidate` in
/// order (with possible gaps). Necessary-and-sufficient for a subsequence
/// match; O(clen) with early exit.
[[nodiscard]] static bool is_feasible(std::string_view pattern, std::string_view candidate) noexcept
{
    size_t pi = 0;
    for (size_t ci = 0; ci < candidate.size() && pi < pattern.size(); ++ci)
    {
        if (std::tolower(static_cast<unsigned char>(candidate[ci])) ==
            std::tolower(static_cast<unsigned char>(pattern[pi])))
        {
            ++pi;
        }
    }
    return pi == pattern.size();
}

// ---------------------------------------------------------------------------
// Suite A: score_candidate - feasibility oracle agreement
// ---------------------------------------------------------------------------

static void check_oracle(std::string_view pattern, std::string_view candidate)
{
    const bool feasible = is_feasible(pattern, candidate);
    FuzzyScorer scorer;
    scorer.prepare(pattern, candidate);
    const int32_t dp_score = scorer.score();
    const bool dp_matched = (dp_score != ScoreWeights::k_no_match);

    FUZZ_ASSERT(feasible == dp_matched, fmt::format("pattern=\"{}\" candidate=\"{}\" feasible={} dp_matched={}",
                                                    pattern, candidate, feasible, dp_matched));
}

// ---------------------------------------------------------------------------
// Suite B: score_candidate - structural invariants on results
// ---------------------------------------------------------------------------

static void check_score_invariants(std::string_view pattern, std::string_view candidate)
{
    // 1. Determinism
    FuzzyScorer scorer;

    auto run = [&]() {
        scorer.prepare(pattern, candidate);
        ScoredMatch m;
        m.path = candidate;
        m.score = scorer.score();
        if (m.score != ScoreWeights::k_no_match)
        {
            scorer.fill_matched_indices(m);
        }
        return m;
    };

    const ScoredMatch m1 = run();
    const ScoredMatch m2 = run();

    FUZZ_ASSERT(m1.score == m2.score, fmt::format("score not deterministic: pattern=\"{}\" candidate=\"{}\" {} vs {}",
                                                  pattern, candidate, m1.score, m2.score));
    FUZZ_ASSERT(m1.n_matched == m2.n_matched &&
                    std::equal(m1.matched_indices.begin(), m1.matched_indices.begin() + m1.n_matched,
                               m2.matched_indices.begin()),
                fmt::format("matched_indices not deterministic: pattern=\"{}\" candidate=\"{}\"", pattern, candidate));

    if (m1.score == ScoreWeights::k_no_match)
    {
        FUZZ_ASSERT(m1.n_matched == 0, fmt::format("k_no_match but matched_indices non-empty: "
                                                   "pattern=\"{}\" candidate=\"{}\"",
                                                   pattern, candidate));
        return;
    }

    // 2. matched_indices count equals pattern length
    FUZZ_ASSERT(m1.n_matched == pattern.size(),
                fmt::format("n_matched={} != pattern.size()={}", m1.n_matched, pattern.size()));

    // 3. matched_indices are strictly ascending and within candidate bounds
    for (size_t i = 0; i < m1.n_matched; ++i)
    {
        FUZZ_ASSERT(
            m1.matched_indices[i] < candidate.size(),
            fmt::format("matched_indices[{}]={} out of bounds (clen={})", i, m1.matched_indices[i], candidate.size()));

        if (i > 0)
        {
            FUZZ_ASSERT(m1.matched_indices[i] > m1.matched_indices[i - 1],
                        fmt::format("matched_indices not strictly ascending at [{}]: {} <= {}", i,
                                    m1.matched_indices[i], m1.matched_indices[i - 1]));
        }
    }

    // 4. Every matched index case-insensitively matches the corresponding pattern char
    for (size_t i = 0; i < m1.n_matched; ++i)
    {
        const char pc = static_cast<char>(std::tolower(static_cast<unsigned char>(pattern[i])));
        const char cc = static_cast<char>(std::tolower(static_cast<unsigned char>(candidate[m1.matched_indices[i]])));

        FUZZ_ASSERT(pc == cc, fmt::format("matched_indices[{}]={}: pattern[{}]='{}' != candidate[{}]='{}'", i,
                                          m1.matched_indices[i], i, pc, m1.matched_indices[i], cc));
    }
}

static void check_full_match_shortcut(std::string_view s)
{
    if (s.empty())
    {
        return;
    }

    // pattern == candidate (same bytes) must hit the shortcut path and return
    // the sentinel score INT32_MAX/2 with identity matched_indices.
    FuzzyScorer scorer;
    scorer.prepare(s, s);
    ScoredMatch m;
    m.path = s;
    m.score = scorer.score();
    if (m.score != ScoreWeights::k_no_match)
    {
        scorer.fill_matched_indices(m);
    }

    FUZZ_ASSERT(m.score == std::numeric_limits<int32_t>::max() / 2,
                fmt::format("full-match shortcut wrong score for s=\"{}\": got {}", s, m.score));
    FUZZ_ASSERT(m.n_matched == s.size(),
                fmt::format("full-match shortcut wrong indices size: {} vs {}", m.n_matched, s.size()));

    for (uint32_t i = 0; i < static_cast<uint32_t>(s.size()); ++i)
    {
        FUZZ_ASSERT(m.matched_indices[i] == i,
                    fmt::format("full-match shortcut matched_indices[{}]={} != {}", i, m.matched_indices[i], i));
    }
}

// ---------------------------------------------------------------------------
// Suite C: AutocompleteEngine::query invariants
// ---------------------------------------------------------------------------

static void check_query_result(const std::vector<ScoredMatch>& results, std::string_view pattern, size_t max_results,
                               const std::vector<std::string>& corpus)
{
    // 1. Count
    FUZZ_ASSERT(results.size() <= max_results,
                fmt::format("results.size()={} > max_results={}", results.size(), max_results));

    for (size_t i = 0; i < results.size(); ++i)
    {
        const ScoredMatch& r = results[i];

        // 2. Non-increasing score order
        if (i > 0)
        {
            FUZZ_ASSERT(results[i].score <= results[i - 1].score,
                        fmt::format("results not sorted: results[{}].score={} > results[{}].score={}", i,
                                    results[i].score, i - 1, results[i - 1].score));
        }

        // 3. Path exists in corpus
        const bool found_in_corpus =
            std::any_of(corpus.begin(), corpus.end(), [&r](const std::string& cp) { return cp == r.path; });

        FUZZ_ASSERT(found_in_corpus, fmt::format("result path not in corpus: \"{}\"", r.path));

        // 4. Path genuinely matches pattern (non-empty pattern only)
        if (!pattern.empty())
        {
            FUZZ_ASSERT(is_feasible(pattern, r.path), fmt::format("result path fails feasibility: "
                                                                  "pattern=\"{}\" path=\"{}\"",
                                                                  pattern, r.path));
        }
    }
}

// ---------------------------------------------------------------------------
// Corpus / pattern generators
// ---------------------------------------------------------------------------

static constexpr std::string_view k_path_chars = "abcdefghijklmnopqrstuvwxyz0123456789_/.-ABCDEF";

[[nodiscard]] static std::string random_path(std::mt19937& rng, size_t min_len = 4, size_t max_len = 48)
{
    std::uniform_int_distribution<size_t> len_dist(min_len, max_len);
    std::uniform_int_distribution<size_t> char_dist(0, k_path_chars.size() - 1);
    const size_t len = len_dist(rng);
    std::string s(len, '\0');
    for (char& c : s)
    {
        c = k_path_chars[char_dist(rng)];
    }
    return s;
}

[[nodiscard]] static std::string random_pattern(std::mt19937& rng, size_t max_len = 16)
{
    std::uniform_int_distribution<size_t> len_dist(0, max_len);
    std::uniform_int_distribution<size_t> char_dist(0, k_path_chars.size() - 1);
    const size_t len = len_dist(rng);
    std::string s(len, '\0');
    for (char& c : s)
    {
        c = k_path_chars[char_dist(rng)];
    }
    return s;
}

// ---------------------------------------------------------------------------
// Standalone main
// ---------------------------------------------------------------------------

#if !defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION)

int main(int argc, char* argv[])
{
    const int32_t iterations = (argc > 1) ? static_cast<int32_t>(std::strtol(argv[1], nullptr, 10)) : 100000;

    fmt::print("=== fuzz_autocomplete standalone, {} iterations ===\n", iterations);

    // ------------------------------------------------------------------
    // Suite A + B: score_candidate properties
    // ------------------------------------------------------------------
    fmt::print("[Suite A+B] score_candidate oracle + invariants...\n");
    {
        std::mt19937 rng(0xDEADBEEFU);
        for (int32_t t = 0; t < iterations; ++t)
        {
            const std::string pattern = random_pattern(rng);
            const std::string candidate = random_path(rng, 0, 40);
            check_oracle(pattern, candidate);
            check_score_invariants(pattern, candidate);
        }

        // Edge: empty pattern
        check_oracle("", "some/path/here");
        check_score_invariants("", "some/path/here");

        // Edge: pattern longer than candidate
        check_oracle("toolongpattern", "short");
        check_score_invariants("toolongpattern", "short");

        // Edge: both empty
        check_oracle("", "");
        check_score_invariants("", "");

        // Edge: single character
        for (char c : std::string_view("abcz/_.-0"))
        {
            check_oracle(std::string(1, c), "abc_def/ghi.jkl");
            check_oracle(std::string(1, c), std::string(1, c));
        }

        // Edge: all same character
        check_oracle("aaaa", "aaabaaacaaad");
        check_score_invariants("aaaa", "aaabaaacaaad");

        // Edge: null bytes and high bytes (engine must not crash)
        {
            const std::string p("\x00\x01\xFF", 3);
            const std::string c("abc\x00\xFF\x01xyz", 9);
            check_oracle(p, c);
            check_score_invariants(p, c);
        }

        fmt::print("[Suite A+B] PASS\n");
    }

    // ------------------------------------------------------------------
    // Suite B2: full-match shortcut
    // ------------------------------------------------------------------
    fmt::print("[Suite B2] full-match shortcut...\n");
    {
        std::mt19937 rng(0xC0FFEEU);
        for (int32_t t = 0; t < iterations / 10; ++t)
        {
            check_full_match_shortcut(random_path(rng, 1, 32));
        }
        check_full_match_shortcut("a");
        check_full_match_shortcut("abc/def_ghi.jkl");
        fmt::print("[Suite B2] PASS\n");
    }

    // ------------------------------------------------------------------
    // Suite C: AutocompleteEngine::query invariants under keystroke simulation
    // ------------------------------------------------------------------
    fmt::print("[Suite C] engine query invariants...\n");
    {
        std::mt19937 rng(0xFACEFEEDU);

        // Build a realistic corpus of game-asset-style paths.
        const std::vector<std::string> path_words = {
            "levels",      "encounters", "cinematics", "hubs",        "forest", "desert",   "volcano",
            "sky_citadel", "ruins",      "entrance",   "throne_room", "armory", "overlook", "shrine",
            "main",        "alt",        "boss",       "night",       "storm",  "test",     "assets",
            "meshes",      "textures",   "audio",      "sfx",         "music",  "ui",       "player",
            "enemy",       "npc",        "world",      "zone",        "room",   "trigger",
        };
        std::uniform_int_distribution<size_t> wd(0, path_words.size() - 1);
        std::uniform_int_distribution<int32_t> num(1, 99);

        std::vector<std::string> corpus;
        corpus.reserve(2006);
        for (int32_t i = 0; i < 2000; ++i)
        {
            corpus.push_back(path_words[wd(rng)] + "/" + path_words[wd(rng)] + "_" + path_words[wd(rng)] + "_" +
                             std::to_string(num(rng)) + "/" + path_words[wd(rng)] + ".scn");
        }
        // Add tricky edge cases explicitly.
        corpus.emplace_back("");
        corpus.emplace_back("a");
        corpus.emplace_back("ab");
        corpus.emplace_back("abc");
        corpus.emplace_back("ALLCAPS/PATH_HERE.EXT");
        corpus.emplace_back("CamelCaseMeshAsset.fbx");

        AutocompleteEngine engine(corpus);

        // Record a few navigations to exercise the frecency layer.
        for (size_t i = 0; i < corpus.size(); i += 7)
        {
            engine.record_navigation(corpus[i]);
        }

        constexpr size_t k_max = 8;

        // Simulate random queries.
        for (int32_t t = 0; t < iterations / 5; ++t)
        {
            const std::string pattern = random_pattern(rng, 12);
            const auto results = engine.query(pattern, k_max);
            check_query_result(results, pattern, k_max, corpus);
        }

        // Simulate incremental typing (the hot path).
        {
            const std::vector<std::string> typed_queries = {
                "",       "l",     "le",  "lev", "leve", "level", "levels", "leve", "lev", // backspace
                "lev2",                                                                    // new branch after backspace
                "",       "e",     "en",  "enc", "enco",                                   // fresh query
                "skytrn", "skytr", "sky",                                                  // backspace mid-abbreviation
                "CaM",                                                                     // camelCase prefix
            };
            for (const auto& q : typed_queries)
            {
                const auto results = engine.query(q, k_max);
                check_query_result(results, q, k_max, corpus);
            }
        }

        // add_path then query - engine must not crash and results stay bounded.
        {
            engine.add_path("newly/added/path_unique_99.scn");
            const auto results = engine.query("unique", k_max);
            // The new path is not in the original corpus vector, so skip the
            // corpus-membership check; just verify the structural invariants.
            FUZZ_ASSERT(results.size() <= k_max,
                        fmt::format("add_path: results.size()={} > k_max={}", results.size(), k_max));
            for (size_t i = 1; i < results.size(); ++i)
            {
                FUZZ_ASSERT(results[i].score <= results[i - 1].score,
                            fmt::format("add_path: results not sorted at [{}]: {} > {}", i, results[i].score,
                                        results[i - 1].score));
            }
        }

        // Empty query must return <= k_max results with non-negative scores.
        {
            const auto results = engine.query("", k_max);
            FUZZ_ASSERT(results.size() <= k_max,
                        fmt::format("empty query: results.size()={} > k_max={}", results.size(), k_max));
            for (size_t i = 0; i < results.size(); ++i)
            {
                FUZZ_ASSERT(results[i].score >= 0,
                            fmt::format("empty query: results[{}].score={} < 0", i, results[i].score));
            }
        }

        fmt::print("[Suite C] PASS\n");
    }

    // ------------------------------------------------------------------
    // Suite D: targeted edge cases for known tricky inputs
    // ------------------------------------------------------------------
    fmt::print("[Suite D] edge cases...\n");
    {
        // Very long pattern, short candidate - must not match, must not crash.
        const std::string long_pattern(300, 'a');
        check_oracle(long_pattern, "a/b/c");

        // Very long candidate.
        std::string long_candidate(300, 'x');
        long_candidate += "/some/path.scn";
        check_oracle("xps", long_candidate);
        check_score_invariants("xps", long_candidate);

        // Pattern == candidate == single character.
        check_full_match_shortcut("z");

        // Consecutive bonus: all-same chars - matched_indices must be ascending.
        FuzzyScorer scorer;
        {
            scorer.prepare("aaaa", "aaaabbbbaaaa");
            ScoredMatch m;
            m.path = "aaaabbbbaaaa";
            m.score = scorer.score();
            if (m.score != ScoreWeights::k_no_match)
            {
                scorer.fill_matched_indices(m);
                FUZZ_ASSERT(m.n_matched == 4, fmt::format("aaaa: n_matched={}, expected 4", m.n_matched));
            }
        }

        // Boundary bonus: subsequence that can land after '/'.
        check_oracle("src", "project/src/main.cpp");
        check_score_invariants("src", "project/src/main.cpp");

        // camelCase bonus.
        check_oracle("MB", "MyBigAsset.fbx");
        check_score_invariants("MB", "MyBigAsset.fbx");

        // Repeating chars in pattern - must not produce duplicate indices.
        {
            scorer.prepare("aaa", "alpha_array_assets");
            ScoredMatch m;
            m.path = "alpha_array_assets";
            m.score = scorer.score();
            if (m.score != ScoreWeights::k_no_match)
            {
                scorer.fill_matched_indices(m);
                for (size_t i = 1; i < m.n_matched; ++i)
                {
                    FUZZ_ASSERT(m.matched_indices[i] > m.matched_indices[i - 1],
                                fmt::format("repeated-char: indices not strictly ascending "
                                            "at [{}]: {} <= {}",
                                            i, m.matched_indices[i], m.matched_indices[i - 1]));
                }
            }
        }

        fmt::print("[Suite D] PASS\n");
    }

    // ------------------------------------------------------------------
    // Summary
    // ------------------------------------------------------------------
    if (g_failures == 0)
    {
        fmt::print("\nAll suites PASSED ({} iterations).\n", iterations);
        return 0;
    }

    fmt::print(stderr, "\n{} failure(s) detected.\n", g_failures);
    return 1;
}

#else // FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION - libFuzzer entry point

/**
 * LibFuzzer driver. Interprets the raw fuzz input as:
 *   byte[0]        : action selector (mod 4)
 *   byte[1]        : split point within the remainder
 *   byte[2..]      : payload split into (pattern, candidate/path) at split point
 *
 * This keeps the harness thin and lets libFuzzer explore the input space
 * freely while each action exercises a different code path.
 */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size < 2)
    {
        return 0;
    }

    const uint8_t action = static_cast<uint8_t>(data[0] % 4u);
    const size_t split = std::min(static_cast<size_t>(data[1]) % std::max(size_t{1}, size - 2) + 2, size);

    const std::string_view part_a(reinterpret_cast<const char*>(data + 2), split > 2 ? split - 2 : size_t{0});
    const std::string_view part_b(reinterpret_cast<const char*>(data + split), size - split);

    switch (action)
    {
    case 0: // Oracle agreement
        check_oracle(part_a, part_b);
        break;

    case 1: // Score invariants
        check_score_invariants(part_a, part_b);
        break;

    case 2: // Full-match shortcut (use part_a as the string)
        if (!part_a.empty())
        {
            check_full_match_shortcut(part_a);
        }
        break;

    case 3: // Engine query (part_b = '\n'-separated corpus, part_a = pattern)
    {
        std::vector<std::string> corpus;
        std::string entry;
        for (const char c : part_b)
        {
            if (c == '\n')
            {
                if (!entry.empty())
                {
                    corpus.push_back(std::move(entry));
                    entry.clear();
                }
            }
            else
            {
                entry.push_back(c);
            }
        }
        if (!entry.empty())
        {
            corpus.push_back(std::move(entry));
        }
        if (corpus.empty())
        {
            corpus.emplace_back("placeholder");
        }

        AutocompleteEngine engine(corpus);
        const std::string pattern(part_a);
        const auto results = engine.query(pattern, 8);
        check_query_result(results, pattern, 8, corpus);
        break;
    }

    default:
        break;
    }

    return 0;
}

#endif // FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION