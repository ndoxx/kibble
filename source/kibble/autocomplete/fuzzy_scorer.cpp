#include "kibble/autocomplete/fuzzy_scorer.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>

namespace kb::autocomplete
{

namespace
{

/// @internal
/// @brief True if `cc` is a path separator or word-boundary punctuation.
[[nodiscard]] bool is_boundary_char(char cc) noexcept
{
    return cc == '/' || cc == '_' || cc == '.' || cc == '-';
}

/// @internal
/// @brief True if `prev` -> `cur` is a camelCase word transition.
[[nodiscard]] bool is_camel_transition(char prev, char cur) noexcept
{
    // Both sides must be letters, otherwise "2D" or punctuation next to an
    // uppercase letter would trigger a false positive.
    if (std::isalpha(static_cast<unsigned char>(prev)) == 0 || std::isalpha(static_cast<unsigned char>(cur)) == 0)
    {
        return false;
    }

    return std::islower(static_cast<unsigned char>(prev)) != 0 && std::isupper(static_cast<unsigned char>(cur)) != 0;
}

/// @internal
/// @brief Per-character bonus for matching `candidate[idx]`.
/// @param prev_matched_idx index of the previous matched char, or UINT32_MAX if none.
[[nodiscard]] int32_t char_bonus(std::string_view candidate, uint32_t idx, uint32_t prev_matched_idx,
                                 const ScoreWeights& weights) noexcept
{
    int32_t bonus = weights.base_match_score;

    if (idx == 0)
    {
        bonus += weights.leading_char_bonus;
    }
    else
    {
        char prev = candidate[idx - 1];
        if (is_boundary_char(prev))
        {
            bonus += weights.boundary_bonus;
        }
        else if (is_camel_transition(prev, candidate[idx]))
        {
            bonus += weights.camel_case_bonus;
        }
    }

    if (prev_matched_idx != UINT32_MAX && idx == prev_matched_idx + 1)
    {
        bonus += weights.consecutive_bonus;
    }

    return bonus;
}

} // namespace

// ---------------------------------------------------------------------------
// FuzzyScorer
// ---------------------------------------------------------------------------

void FuzzyScorer::prepare(std::string_view pattern, std::string_view candidate)
{
    candidate_orig_ = candidate;
    // Lowercase both strings once so feasible() and score() never call tolower again.
    pattern_lower_.assign(pattern.begin(), pattern.end());
    std::transform(pattern_lower_.begin(), pattern_lower_.end(), pattern_lower_.begin(),
                   [](unsigned char cc) { return static_cast<char>(std::tolower(cc)); });

    candidate_lower_.assign(candidate.begin(), candidate.end());
    std::transform(candidate_lower_.begin(), candidate_lower_.end(), candidate_lower_.begin(),
                   [](unsigned char cc) { return static_cast<char>(std::tolower(cc)); });
}

bool FuzzyScorer::feasible() const noexcept
{
    // Two-pointer subsequence check, O(clen) with early exit.
    size_t pi = 0;
    const size_t plen = pattern_lower_.size();
    const size_t clen = candidate_lower_.size();
    for (size_t ci = 0; ci < clen && pi < plen; ++ci)
    {
        if (candidate_lower_[ci] == pattern_lower_[pi])
        {
            ++pi;
        }
    }
    return pi == plen;
}

int32_t FuzzyScorer::score()
{
    if (pattern_lower_.empty())
    {
        return 0;
    }
    if (pattern_lower_.size() > candidate_lower_.size())
    {
        return ScoreWeights::k_no_match;
    }

    // Exact full-path match short-circuits the DP.
    if (pattern_lower_.size() == candidate_lower_.size() && pattern_lower_ == candidate_lower_)
    {
        return std::numeric_limits<int32_t>::max() / 2;
    }

    const uint32_t plen = static_cast<uint32_t>(pattern_lower_.size());
    const uint32_t clen = static_cast<uint32_t>(candidate_lower_.size());

    /*
       NOTE(ndx): DP formulation.
       best[jj] = best score for matching pattern[0..jj], last char landing at best_end[jj].
       pred_of[jj * clen + ci] = candidate index where pattern[jj-1] landed, for the
       specific win that set best[jj] at candidate index ci. UINT32_MAX for jj==0
       (no predecessor) or for cells that were never a winner.

       We sweep the candidate left to right once. Walking pj high to low within each ci
       step keeps best[pj-1] at its pre-step value, so we never read a value that this
       same step already wrote.

       Traceback uses a flat [pj * clen + ci] matrix instead of a [pj]-only array because
       best[pj-1] can improve more than once across the sweep (a later, higher-scoring
       placement of an earlier pattern character). A [pj]-only pred array only remembers
       the predecessor from the most recent improvement, so if best[pj-1] improves again
       afterward, every pred[pj' > pj-1] that already locked onto the old best_end[pj-1]
       goes stale and the trace drifts off the actual winning alignment. Indexing by
       (pj, ci) means each cell is written exactly once, by the step that wins it, so a
       later improvement elsewhere can't retroactively invalidate it.
    */

    best_.assign(plen, ScoreWeights::k_no_match);
    best_end_.assign(plen, UINT32_MAX);

    const size_t pred_size = static_cast<size_t>(plen) * clen;
    if (pred_of_.size() < pred_size)
    {
        pred_of_.resize(pred_size);
    }
    std::fill(pred_of_.begin(), pred_of_.begin() + static_cast<ptrdiff_t>(pred_size), UINT32_MAX);

    /*
       NOTE(ndx): high_water tracks the highest pj that has ever had a match, not just
       the highest pj that has improved its score. If we only advanced it on score
       improvements, a slot could get matched without beating its current best, and the
       early-exit check below would fire before all valid completions were considered.
    */
    uint32_t high_water = 0;

    for (uint32_t ci = 0; ci < clen; ++ci)
    {
        if (best_[0] != ScoreWeights::k_no_match)
        {
            if (clen - ci < plen - high_water - 1)
            {
                break;
            }
        }

        // Walk pj high to low so best[pj-1] stays at its pre-step value.
        uint32_t pj_max = std::min(plen - 1, ci);
        for (uint32_t pj = pj_max + 1; pj-- > 0;)
        {
            if (pattern_lower_[pj] != candidate_lower_[ci])
            {
                continue;
            }

            if (pj == 0)
            {
                int32_t s = char_bonus(candidate_orig_, ci, UINT32_MAX, weights_);
                if (s > best_[0])
                {
                    best_[0] = s;
                    best_end_[0] = ci;
                }
            }
            else if (best_[pj - 1] != ScoreWeights::k_no_match && best_end_[pj - 1] < ci)
            {
                uint32_t prev_idx = best_end_[pj - 1];
                uint32_t gap = ci - prev_idx - 1;
                int32_t gap_cost =
                    std::min(static_cast<int32_t>(gap) * weights_.gap_penalty_per_char, weights_.max_gap_penalty);

                int32_t s = best_[pj - 1] + char_bonus(candidate_orig_, ci, prev_idx, weights_) - gap_cost;

                if (s > best_[pj])
                {
                    bool first_match = (best_[pj] == ScoreWeights::k_no_match);
                    best_[pj] = s;
                    best_end_[pj] = ci;
                    pred_of_[static_cast<size_t>(pj) * clen + ci] = prev_idx;
                    if (first_match && pj > high_water)
                    {
                        high_water = pj;
                    }
                }
            }
        }
    }

    return best_[plen - 1];
}

void FuzzyScorer::fill_matched_indices(ScoredMatch& match) const
{
    const uint32_t plen = static_cast<uint32_t>(pattern_lower_.size());
    const uint32_t clen = static_cast<uint32_t>(candidate_lower_.size());

    // Empty pattern: score() returned 0 early without populating any DP state.
    // best_end_ is empty, so reading best_end_[plen - 1] would be UB.
    if (plen == 0)
    {
        match.n_matched = 0;
        return;
    }

    // Exact-length match took the short-circuit path in score(), so pred_of_
    // was never populated; indices are trivially 0..plen-1.
    if (plen == clen && pattern_lower_ == candidate_lower_)
    {
        match.n_matched = plen;
        for (uint32_t ii = 0; ii < plen; ++ii)
        {
            match.matched_indices[ii] = ii;
        }
        return;
    }

    // Walk pred_of[] from the last pattern char back to 0.
    match.n_matched = plen;
    uint32_t ci = best_end_[plen - 1];
    for (uint32_t pj = plen; pj-- > 0;)
    {
        match.matched_indices[pj] = ci;
        if (pj == 0)
        {
            break;
        }
        ci = pred_of_[static_cast<size_t>(pj) * clen + ci];
    }
}

// ---------------------------------------------------------------------------
// FrecencyTracker
// ---------------------------------------------------------------------------

void FrecencyTracker::record_visit(const std::string& path)
{
    auto& entry = visits_[path];
    ++entry.count;
    entry.last_visit = Clock::now();
}

int32_t FrecencyTracker::frecency_bonus(const std::string& path, Clock::time_point now, int32_t max_bonus) const
{
    auto it = visits_.find(path);
    if (it == visits_.end())
    {
        return 0;
    }

    const auto& entry = it->second;
    double minutes_ago = std::chrono::duration<double, std::ratio<60>>(now - entry.last_visit).count();

    // Recency decays on a ~10 minute half-life. Frequency is log-scaled so
    // repeat visits help but don't snowball into total domination.
    constexpr double k_half_life_minutes = 10.0;
    double recency_factor = std::pow(0.5, minutes_ago / k_half_life_minutes);
    double frequency_factor = std::log2(static_cast<double>(entry.count) + 1.0);
    double raw = recency_factor * frequency_factor;

    double bonus = static_cast<double>(max_bonus) * (1.0 - std::exp(-raw));
    return static_cast<int32_t>(std::lround(bonus));
}

} // namespace kb::autocomplete