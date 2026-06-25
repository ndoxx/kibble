#include "kibble/autocomplete/autocomplete_engine.h"

#include <algorithm>
#include <cctype>
#include <numeric>

namespace kb::autocomplete
{

uint32_t compute_letter_mask(std::string_view ss) noexcept
{
    uint32_t mask = 0;
    for (char cc : ss)
    {
        char lower = static_cast<char>(std::tolower(static_cast<unsigned char>(cc)));
        if (lower >= 'a' && lower <= 'z')
        {
            mask |= (1u << (lower - 'a'));
        }
    }
    return mask;
}

AutocompleteEngine::AutocompleteEngine(std::vector<std::string> paths, ScoreWeights weights) : scorer_(weights)
{
    add_paths(std::move(paths));
}

void AutocompleteEngine::add_paths(std::vector<std::string> paths)
{
    cache_.reserve(cache_.size() + paths.size());
    for (auto& pp : paths)
    {
        CachedPath cp;
        cp.letter_mask = compute_letter_mask(pp);
        cp.path = std::move(pp);
        cache_.push_back(std::move(cp));
    }
    prefix_stack_.clear();
}

void AutocompleteEngine::add_path(std::string path)
{
    CachedPath cp;
    cp.letter_mask = compute_letter_mask(path);
    cp.path = std::move(path);
    cache_.push_back(std::move(cp));
    prefix_stack_.clear();
}

void AutocompleteEngine::record_navigation(const std::string& path)
{
    frecency_.record_visit(path);
}

void AutocompleteEngine::score_indices(std::string_view pattern, const std::vector<uint32_t>& indices,
                                       size_t max_results, FrecencyTracker::Clock::time_point now)
{
    const uint32_t pattern_mask = compute_letter_mask(pattern);

    matches_.clear();
    matches_.reserve(std::min(indices.size(), max_results * 4));

    for (uint32_t idx : indices)
    {
        const CachedPath& cp = cache_[idx];

        // Letter-mask check, O(1).
        if ((pattern_mask & cp.letter_mask) != pattern_mask)
        {
            continue;
        }

        // Feasibility check, O(clen).
        scorer_.prepare(pattern, cp.path);
        if (!scorer_.feasible())
        {
            continue;
        }

        // Full scoring DP, no traceback yet.
        int32_t s = scorer_.score();
        if (s == ScoreWeights::k_no_match)
        {
            continue;
        }

        ScoredMatch mm;
        mm.path = cp.path;
        mm.score = s + frecency_.frecency_bonus(cp.path, now);
        matches_.push_back(mm);
    }

    size_t kk = std::min(max_results, matches_.size());
    std::partial_sort(matches_.begin(), matches_.begin() + static_cast<long>(kk), matches_.end(),
                      [](const ScoredMatch& aa, const ScoredMatch& bb) { return aa.score > bb.score; });
    matches_.resize(kk);

    // Traceback only for the survivors that actually reach the caller.
    for (ScoredMatch& mm : matches_)
    {
        scorer_.prepare(pattern, mm.path);
        (void)scorer_.score(); // repopulate pred_of_ for this specific candidate
        scorer_.fill_matched_indices(mm);
    }
}

void AutocompleteEngine::score_all(std::string_view pattern, size_t max_results, FrecencyTracker::Clock::time_point now)
{
    candidate_indices_.resize(cache_.size());
    std::iota(candidate_indices_.begin(), candidate_indices_.end(), 0u);
    score_indices(pattern, candidate_indices_, max_results, now);
}

void AutocompleteEngine::top_by_frecency_only(size_t max_results, FrecencyTracker::Clock::time_point now)
{
    matches_.clear();
    matches_.reserve(cache_.size());
    for (const auto& cp : cache_)
    {
        ScoredMatch mm;
        mm.path = cp.path;
        mm.score = frecency_.frecency_bonus(cp.path, now);
        matches_.push_back(mm);
    }
    size_t kk = std::min(max_results, matches_.size());
    std::partial_sort(matches_.begin(), matches_.begin() + static_cast<long>(kk), matches_.end(),
                      [](const ScoredMatch& aa, const ScoredMatch& bb) { return aa.score > bb.score; });
    matches_.resize(kk);
}

std::vector<ScoredMatch> AutocompleteEngine::query(std::string_view pattern, size_t max_results)
{
    // Snapshot time once so the frecency loop doesn't call Clock::now() per candidate.
    const auto now = FrecencyTracker::Clock::now();

    if (pattern.empty())
    {
        prefix_stack_.clear();
        top_by_frecency_only(max_results, now);
        return matches_;
    }

    std::string pattern_str(pattern);

    bool incremental = false;
    candidate_indices_.clear();

    if (!prefix_stack_.empty())
    {
        const PrefixLevel& top = prefix_stack_.back();
        if (pattern_str.size() == top.query.size() + 1 && pattern_str.compare(0, top.query.size(), top.query) == 0)
        {
            // One-character extension: re-score only the previous survivors.
            candidate_indices_ = top.survivors;
            incremental = true;
        }
        else if (pattern_str.size() < prefix_stack_.size())
        {
            // Backspace: walk the stack back to the matching prefix.
            size_t target_depth = pattern_str.size();
            if (target_depth > 0 && target_depth <= prefix_stack_.size())
            {
                prefix_stack_.resize(target_depth);
                const PrefixLevel& restored = prefix_stack_.back();
                if (restored.query == pattern_str)
                {
                    // Exact stack hit: re-score known survivors without touching the stack.
                    score_indices(pattern, restored.survivors, max_results, now);
                    return matches_;
                }
                // Stack entry exists but for a different query of the same length
                // (user typed a different char after backspacing). Fall through to
                // a full scan, using the parent survivors as a starting set.
                prefix_stack_.pop_back();
                if (!prefix_stack_.empty())
                {
                    candidate_indices_ = prefix_stack_.back().survivors;
                    incremental = true;
                }
            }
            else
            {
                prefix_stack_.clear();
            }
        }
        else
        {
            // Non-incremental change (paste, reorder, etc.).
            prefix_stack_.clear();
        }
    }

    if (!incremental)
    {
        // Full corpus scan, indices 0..N-1.
        candidate_indices_.resize(cache_.size());
        std::iota(candidate_indices_.begin(), candidate_indices_.end(), 0u);
    }

    const uint32_t pattern_mask = compute_letter_mask(pattern);

    matches_.clear();
    matches_.reserve(std::min(candidate_indices_.size(), max_results * 4));

    survivors_.clear();
    survivors_.reserve(candidate_indices_.size() / 2);

    for (uint32_t idx : candidate_indices_)
    {
        const CachedPath& cp = cache_[idx];

        // Letter-mask check, O(1), rejects anything missing a required letter.
        if ((pattern_mask & cp.letter_mask) != pattern_mask)
        {
            continue;
        }

        // prepare() is the only tolower pass for this candidate; feasible()
        // and score() both read the already-lowercased buffers.
        scorer_.prepare(pattern, cp.path);
        if (!scorer_.feasible())
        {
            continue;
        }

        // Full scoring DP, score only, no traceback.
        int32_t s = scorer_.score();
        if (s == ScoreWeights::k_no_match)
        {
            continue;
        }

        ScoredMatch mm;
        mm.path = cp.path;
        mm.score = s + frecency_.frecency_bonus(cp.path, now);
        survivors_.push_back(idx);
        matches_.push_back(mm);
    }

    if (pattern_str.size() <= k_max_stack_depth)
    {
        if (prefix_stack_.size() >= pattern_str.size())
        {
            prefix_stack_.resize(pattern_str.size() - 1);
        }
        prefix_stack_.push_back({pattern_str, survivors_});
    }

    size_t kk = std::min(max_results, matches_.size());
    std::partial_sort(matches_.begin(), matches_.begin() + static_cast<long>(kk), matches_.end(),
                      [](const ScoredMatch& aa, const ScoredMatch& bb) { return aa.score > bb.score; });
    matches_.resize(kk);

    // Traceback only for the top-K results that actually reach the caller.
    for (ScoredMatch& mm : matches_)
    {
        scorer_.prepare(pattern, mm.path);
        (void)scorer_.score(); // repopulate pred_of_ for this specific candidate
        scorer_.fill_matched_indices(mm);
    }

    return matches_;
}

size_t AutocompleteEngine::cache_size() const noexcept
{
    return cache_.size();
}

} // namespace kb::autocomplete