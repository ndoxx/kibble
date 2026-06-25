/**
 * @file autocomplete_engine.h
 * @brief Ties the path cache, fuzzy scorer, and frecency tracker together
 * to answer "user typed this, what goes in the dropdown" on every keystroke.
 *
 * Speed strategy, cheapest layer first:
 *
 *  1. Incremental candidate set: the engine remembers the last query and
 *     the candidate indices that survived it. A one-character extension only
 *     re-scores those survivors. Backspace walks a small per-prefix-length
 *     stack, so recovery is O(survivors_at_shorter_prefix). Unrelated queries
 *     fall back to a full scan.
 *
 *  2. Letter-mask pre-filter: a 26-bit bitmask AND, O(1) per candidate, never
 *     rejects a true match.
 *
 *  3. Greedy feasibility pre-filter: FuzzyScorer::prepare() + feasible(),
 *     O(clen), no false rejections.
 *
 *  4. Early exit inside FuzzyScorer::score() once the remaining candidate
 *     suffix can't fit the remaining pattern characters.
 *
 *  5. Partial sort to top-K, since the dropdown only needs the top 8.
 *
 *  6. Reused allocations: FuzzyScorer owns its DP buffers, the engine owns
 *     the transient query-loop vectors. Everything grows on demand and
 *     never shrinks, so a warm session runs without touching the allocator.
 *
 *  7. Deferred traceback: fill_matched_indices() only runs for the final
 *     top-K, skipping index reconstruction for every eliminated candidate.
 *
 *  8. Inline matched-index storage via std::array, so building the result
 *     list never allocates.
 *
 *  9. Clock::now() is snapshotted once per query() call instead of once per
 *     candidate.
 */
#pragma once

#include "kibble/autocomplete/fuzzy_scorer.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace kb::autocomplete
{

/// Precomputed per-path data kept warm so keystroke-time work is scoring only.
struct CachedPath
{
    std::string path;
    uint32_t letter_mask = 0; ///< Bit i set if letter ('a' + i) appears anywhere in path.
};

/// @brief Lowercase letter bitmask of `ss`, bit i set if ('a' + i) appears.
[[nodiscard]] uint32_t compute_letter_mask(std::string_view ss) noexcept;

class AutocompleteEngine
{
public:
    explicit AutocompleteEngine(std::vector<std::string> paths, ScoreWeights weights = {});

    /**
     * @brief Adds a path to the engine.
     *
     * Invalidates the query cache; it is rebuilt lazily on the next query() call.
     */
    void add_path(std::string path);

    /**
     * @brief Adds multiple paths to the engine in one call.
     *
     * Invalidates the query cache; it is rebuilt lazily on the next query() call.
     */
    void add_paths(std::vector<std::string> paths);

    /// @brief Records a user navigation to `path`, feeding the frecency layer.
    void record_navigation(const std::string& path);

    /**
     * @brief The hot path, called on every keystroke.
     *
     * Pass the full current query string each call. The engine figures out
     * internally whether this extends the last query (incremental fast path)
     * or is a new/shorter query (prefix-stack restore or full scan).
     *
     * @param pattern the user's current query
     * @param max_results cap on how many matches to return
     * @return up to `max_results` matches, best first.
     */
    [[nodiscard]] std::vector<ScoredMatch> query(std::string_view pattern, size_t max_results = 8);

    /// @brief Number of paths currently cached.
    [[nodiscard]] size_t cache_size() const noexcept;

private:
    /**
     * @internal
     * @brief Scores the candidates at `indices`, appending hits to `matches_`.
     *
     * Runs the letter-mask, feasibility, and DP layers but not traceback;
     * call fill_matched_indices() separately on the final top-K after sorting.
     */
    void score_indices(std::string_view pattern, const std::vector<uint32_t>& indices, size_t max_results,
                       FrecencyTracker::Clock::time_point now);

    /// @internal @brief Scores every candidate in cache_ (full corpus scan).
    void score_all(std::string_view pattern, size_t max_results, FrecencyTracker::Clock::time_point now);

    /// @internal @brief Fills matches_ with top-K entries by frecency (empty query).
    void top_by_frecency_only(size_t max_results, FrecencyTracker::Clock::time_point now);

    /*
       NOTE(ndx): prefix_stack_ has one entry per prefix length. prefix_stack_[n]
       holds the candidate indices that survived when the query was exactly n
       characters long, so backspace can restore a prior level without a full
       re-scan.
    */
    struct PrefixLevel
    {
        std::string query;               ///< The exact query string at this depth.
        std::vector<uint32_t> survivors; ///< Indices into cache_ that matched.
    };

    // Queries longer than this always fall back to a full scan.
    // Must match k_max_pattern_len in fuzzy_scorer.h.
    static constexpr size_t k_max_stack_depth = k_max_pattern_len;

    std::vector<CachedPath> cache_;
    FrecencyTracker frecency_;
    FuzzyScorer scorer_; ///< Owns DP working buffers, reused every call.

    std::vector<PrefixLevel> prefix_stack_;

    // Transient query-loop buffers, owned here so they survive across calls
    // and never need reallocating once they hit their steady-state size.
    std::vector<ScoredMatch> matches_;        ///< Scored hits for the current query.
    std::vector<uint32_t> survivors_;         ///< Cache indices that passed all filters.
    std::vector<uint32_t> candidate_indices_; ///< Working set fed into the scoring loop.
};

} // namespace kb::autocomplete