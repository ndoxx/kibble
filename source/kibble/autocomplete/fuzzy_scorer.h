/**
 * @file fuzzy_scorer.h
 * @brief Fuzzy scorer for path autocomplete.
 *
 * Matching is subsequence-based: pattern characters must appear in order in
 * the candidate, but not contiguously (same family as fzf / VS Code Quick Open,
 * not Levenshtein). Scoring is a DP table over (pattern_length x candidate_length).
 *
 * Bonuses reward boundary matches, camelCase transitions, consecutive runs, and
 * leading characters. A gap penalty keeps spread-out matches behind tight ones.
 *
 * Frecency (frequency + recency of past navigation) is a separate signal added
 * as a tie-breaking nudge, not a dominant ranking factor.
 */
#pragma once

#include "kibble/util/unordered_dense.h"
#include <array>
#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace kb::autocomplete
{

/// Tunable scoring weights.
struct ScoreWeights
{
    int32_t leading_char_bonus = 10;  ///< Bonus if the first path char matches.
    int32_t boundary_bonus = 8;       ///< Bonus if the match follows '/', '_', '.', '-'.
    int32_t camel_case_bonus = 6;     ///< Bonus if the match follows a lower->upper transition.
    int32_t consecutive_bonus = 12;   ///< Bonus per step of an unbroken consecutive run.
    int32_t base_match_score = 1;     ///< Flat score for any match, before bonuses.
    int32_t gap_penalty_per_char = 1; ///< Penalty per skipped character since the last match.
    int32_t max_gap_penalty = 20;     ///< Cap on a single gap's penalty.

    static constexpr int32_t k_no_match = INT32_MIN;
};

/// Maximum query length tracked by the prefix stack; also the bound on matched_indices storage.
static constexpr uint32_t k_max_pattern_len = 32;

/**
 * @brief One scored candidate.
 *
 * `matched_indices` is a fixed-size inline array, not a `std::vector`, so
 * building the match list never touches the heap. Pattern length is capped
 * at `k_max_pattern_len` (32), so that's always enough room.
 *
 * `score()` does not fill `matched_indices`. Call `FuzzyScorer::fill_matched_indices()`
 * only for the final top-K results that reach the caller.
 */
struct ScoredMatch
{
    std::string_view path;
    int32_t score = ScoreWeights::k_no_match;

    std::array<uint32_t, k_max_pattern_len>
        matched_indices{};  ///< Indices into `path`, ascending. Valid after fill_matched_indices().
    uint32_t n_matched = 0; ///< Number of valid entries in matched_indices.
};

/**
 * @brief Stateful fuzzy scorer that owns its DP working buffers.
 *
 * Keep one instance alive across calls (e.g. as a member of AutocompleteEngine)
 * so its heap allocations are reused instead of alloc/free on every query.
 *
 * Usage per query:
 *  1. `prepare(pattern, candidate)`
 *  2. `feasible()` to reject early
 *  3. `score()` for the ranking score
 *  4. `fill_matched_indices(match)`, only for survivors past the top-K cut
 *
 * @note Not thread-safe. Each thread needs its own instance.
 */
class FuzzyScorer
{
public:
    explicit FuzzyScorer(ScoreWeights weights = {}) : weights_(weights)
    {
    }

    /// @brief Lowercases `pattern` and `candidate` into internal buffers for this query.
    void prepare(std::string_view pattern, std::string_view candidate);

    /**
     * @brief Checks whether pattern is a subsequence of candidate.
     *
     * Necessary and sufficient for score() to find a match, so it never
     * produces false rejections. Must be called after prepare().
     */
    [[nodiscard]] bool feasible() const noexcept;

    /**
     * @brief Runs the DP and returns the best alignment score, without traceback.
     *
     * Must be called after prepare().
     *
     * @return best alignment score, or ScoreWeights::k_no_match.
     */
    [[nodiscard]] int32_t score();

    /**
     * @brief Reconstructs matched character indices from the last score() call.
     *
     * Only call this for final top-K survivors, not every candidate.
     */
    void fill_matched_indices(ScoredMatch& match) const;

private:
    ScoreWeights weights_;

    // DP working buffers, grown on demand, never shrunk.
    std::vector<int32_t> best_;       ///< best[pj]. Best score for pattern[0..pj] so far.
    std::vector<uint32_t> best_end_;  ///< best_end[pj]. Candidate index where that best score landed.
    std::vector<uint32_t> pred_of_;   ///< pred_of[pj*clen + ci]. Predecessor ci for the (pj,ci) winning cell.
    std::string pattern_lower_;       ///< Lowercased current pattern.
    std::string candidate_lower_;     ///< Lowercased current candidate.
    std::string_view candidate_orig_; ///< Original-case view, set by prepare(); used for char_bonus.
};

/// Tracks how often and how recently each path has been navigated to.
class FrecencyTracker
{
public:
    using Clock = std::chrono::steady_clock;

    /// @brief Records a navigation to `path`.
    void record_visit(const std::string& path);

    /**
     * @brief Frecency bonus for `path`, in the same rough units as match scores.
     * @param path      path to look up
     * @param now       current time, snapshotted once per query to avoid
     *                  repeated syscalls across the candidate loop.
     * @param max_bonus upper bound on the returned bonus
     */
    [[nodiscard]] int32_t frecency_bonus(const std::string& path, Clock::time_point now, int32_t max_bonus = 15) const;

private:
    struct Entry
    {
        int32_t count = 0;
        Clock::time_point last_visit{};
    };

    ankerl::unordered_dense::map<std::string, Entry> visits_;
};

} // namespace kb::autocomplete