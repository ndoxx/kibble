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

/*
    NOTE(ndx): DP formulation (corrected from the original 1D-scalar approach).

    best[pj * clen + ci] = best score for aligning pattern[0..pj] with
    pattern[pj] landing exactly at candidate index ci.
    k_no_match where candidate[ci] != pattern[pj], or no valid alignment exists.

    pred_of[pj * clen + ci] = the ci (predecessor candidate index) that
    achieved best[pj][ci], used for traceback. UINT32_MAX at row 0 (no
    predecessor) or in cells that were never reached.

    The old code kept a single scalar best[pj] (the best score so far for
    pattern row pj) and best_end[pj] (where it landed). That collapses the
    entire row to one representative cell, so each row could only
    extend from whichever placement of pattern[pj-1] happened to win first.
    If a better, tighter run appeared later in the string (e.g. "doors"
    appearing as a consecutive boundary-aligned substring after an earlier
    scattered match of the same characters) the earlier winner's position
    is already locked in and the later run can never reach it as a
    predecessor. A full 2D table lets every candidate position compete
    independently as a predecessor for every subsequent row, so the true
    global optimum is always reachable. Tried to be smart, I'm not.

    Complexity: O(plen * clen^2).
*/
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
    const size_t table_size = static_cast<size_t>(plen) * clen;

    best_.assign(table_size, ScoreWeights::k_no_match);

    if (pred_of_.size() < table_size)
    {
        pred_of_.resize(table_size);
    }
    std::fill(pred_of_.begin(), pred_of_.begin() + static_cast<ptrdiff_t>(table_size), UINT32_MAX);

    best_final_ci_ = UINT32_MAX;

    // Fill row 0: pattern[0] matched at every qualifying candidate index.
    for (uint32_t ci = 0; ci < clen; ++ci)
    {
        if (candidate_lower_[ci] == pattern_lower_[0])
        {
            best_[ci] = char_bonus(candidate_orig_, ci, UINT32_MAX, weights_);
        }
    }

    // Fill rows 1..plen-1.
    for (uint32_t pj = 1; pj < plen; ++pj)
    {
        const size_t prev_row = static_cast<size_t>(pj - 1) * clen;
        const size_t cur_row = static_cast<size_t>(pj) * clen;

        for (uint32_t ci = pj; ci < clen; ++ci) // ci < pj can never fit pj+1 chars
        {
            if (candidate_lower_[ci] != pattern_lower_[pj])
            {
                continue;
            }

            int32_t best_here = ScoreWeights::k_no_match;
            uint32_t best_pred = UINT32_MAX;

            // Try every valid predecessor ci' < ci in the previous row.
            for (uint32_t cip = 0; cip < ci; ++cip)
            {
                if (best_[prev_row + cip] == ScoreWeights::k_no_match)
                {
                    continue;
                }
                uint32_t gap = ci - cip - 1;
                int32_t gap_cost =
                    std::min(static_cast<int32_t>(gap) * weights_.gap_penalty_per_char, weights_.max_gap_penalty);
                int32_t s = best_[prev_row + cip] + char_bonus(candidate_orig_, ci, cip, weights_) - gap_cost;
                if (s > best_here)
                {
                    best_here = s;
                    best_pred = cip;
                }
            }

            if (best_here != ScoreWeights::k_no_match)
            {
                best_[cur_row + ci] = best_here;
                pred_of_[cur_row + ci] = best_pred;
            }
        }
    }

    // Find the best score in the last row and record which ci achieved it.
    int32_t result = ScoreWeights::k_no_match;
    const size_t last_row = static_cast<size_t>(plen - 1) * clen;
    for (uint32_t ci = 0; ci < clen; ++ci)
    {
        if (best_[last_row + ci] > result)
        {
            result = best_[last_row + ci];
            best_final_ci_ = ci;
        }
    }
    return result;
}

void FuzzyScorer::fill_matched_indices(ScoredMatch& match) const
{
    const uint32_t plen = static_cast<uint32_t>(pattern_lower_.size());
    const uint32_t clen = static_cast<uint32_t>(candidate_lower_.size());

    // Empty pattern: score() returned 0 early without populating any DP state.
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

    // Walk pred_of[] backwards from the winning cell in the last row.
    match.n_matched = plen;
    uint32_t ci = best_final_ci_;
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