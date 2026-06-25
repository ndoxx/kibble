#include <chrono>
#include <cstdio>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "kibble/autocomplete/autocomplete_engine.h"

using namespace kb;

namespace
{

[[nodiscard]] std::vector<std::string> generate_scene_corpus(uint32_t seed = 1337)
{
    std::mt19937 rng(seed);

    // -----------------------------------------------------------------------
    // Vocabulary
    // -----------------------------------------------------------------------

    // 40 zones: the original 12 kept verbatim so existing demo queries still
    // hit recognisable targets, plus 28 new regions of the same style.
    static const std::vector<std::string> kZones = {
        "forest",           "desert",        "tundra",          "swamp",
        "volcano",          "ocean_depths",  "sky_citadel",     "underdark",
        "ashlands",         "ruins_of_ket",  "starport",        "wastes",
        "glacier",          "canyon",        "wetlands",        "badlands",
        "coral_reef",       "magma_fields",  "storm_peaks",     "crystalline_caves",
        "iron_fortress",    "sunken_city",   "plague_quarter",  "arcane_tower",
        "frozen_wastes",    "lava_tubes",    "overgrown_ruins", "obsidian_spire",
        "drifting_islands", "frozen_harbor", "blight_hollows",  "amber_vaults",
        "verdant_nexus",    "ashen_plains",  "drowned_keep",    "shattered_coast",
        "thornwood",        "frostmere",     "clockwork_city",  "ember_falls",
    };

    // 40 area names: the original 20 plus 20 new ones.
    static const std::vector<std::string> kAreas = {
        "entrance", "courtyard", "throne_room", "armory",    "garden",     "crypt",     "overlook", "bridge",
        "market",   "shrine",    "barracks",    "cistern",   "spire",      "hollow",    "outpost",  "sanctum",
        "wreckage", "campsite",  "lookout",     "tunnel",    "gatehouse",  "vault",     "sewers",   "watchtower",
        "forge",    "arena",     "library",     "chapel",    "harbor",     "aqueduct",  "cellar",   "rampart",
        "workshop", "dungeon",   "observatory", "mess_hall", "undercroft", "bathhouse", "gallery",  "stockade",
    };

    // 20 variants: the original 13 plus 7 that reflect late-stage production
    // states common on larger projects.
    static const std::vector<std::string> kVariants = {
        "main",      "alt",     "night",    "storm",          "ruined",    "festival",    "siege",
        "prologue",  "boss",    "vignette", "vertical_slice", "greybox",   "polish_pass", "flooded",
        "overgrown", "burning", "frozen",   "occupied",       "abandoned", "wip",
    };

    // 8 subsystem roots: the original 5 plus 3 that appear on larger projects.
    static const std::vector<std::string> kSubsystems = {
        "levels",      "levels/encounters",      "levels/cinematics", "levels/test",
        "levels/hubs", "levels/scripted_events", "levels/arenas",     "levels/transitions",
    };

    // -----------------------------------------------------------------------
    // Generation
    // -----------------------------------------------------------------------

    std::uniform_int_distribution<size_t> pick_area(0, kAreas.size() - 1);
    std::uniform_int_distribution<size_t> pick_variant(0, kVariants.size() - 1);
    std::uniform_int_distribution<size_t> pick_sub(0, kSubsystems.size() - 1);
    std::uniform_int_distribution<size_t> area_count(4, 8);    // areas per zone
    std::uniform_int_distribution<size_t> variant_count(3, 5); // variants per area
    std::uniform_int_distribution<int> area_number(1, 12);     // instance number

    // Use a set to silently drop the rare duplicate that arises when the RNG
    // picks the same (subsystem, zone, area, number, variant) combo twice.
    ankerl::unordered_dense::set<std::string> seen;
    std::vector<std::string> paths;
    paths.reserve(1100);

    for (const auto& zone : kZones)
    {
        size_t n_areas = area_count(rng);
        for (size_t a = 0; a < n_areas; ++a)
        {
            const std::string& area = kAreas[pick_area(rng)];
            int num = area_number(rng);

            size_t n_variants = variant_count(rng);
            for (size_t v = 0; v < n_variants; ++v)
            {
                const std::string& variant = kVariants[pick_variant(rng)];
                const std::string& subsystem = kSubsystems[pick_sub(rng)];

                std::string path = subsystem + "/" + zone + "_" + area + "_" + (num < 10 ? "0" : "") +
                                   std::to_string(num) + "_" + variant + ".scn.hades";

                if (seen.insert(path).second)
                {
                    paths.push_back(std::move(path));
                }
            }
        }
    }

    // -----------------------------------------------------------------------
    // Hand-placed memorable paths
    // These are the demo targets used by autocomplete.cpp's typed walkthroughs.
    // They are unconditional so demo output is seed-independent.
    // -----------------------------------------------------------------------
    static const std::vector<std::string> kPinned = {
        "levels/hubs/sky_citadel_throne_room_01_main.scn.hades",
        "levels/encounters/volcano_sanctum_07_boss.scn.hades",
        "levels/test/forest_overlook_02_greybox.scn.hades",
        "levels/cinematics/ruins_of_ket_courtyard_03_prologue.scn.hades",
        // A few extras that stress the distinctive-trigram fast path.
        "levels/arenas/clockwork_city_arena_05_siege.scn.hades",
        "levels/scripted_events/drowned_keep_undercroft_09_flooded.scn.hades",
        "levels/transitions/drifting_islands_harbor_03_storm.scn.hades",
        "levels/encounters/crystalline_caves_observatory_11_night.scn.hades",
    };
    for (const auto& p : kPinned)
    {
        if (seen.insert(p).second)
        {
            paths.push_back(p);
        }
    }

    return paths;
}

constexpr const char* k_ansi_underline_bold_green = "\x1b[1;4;32m";
constexpr const char* k_ansi_reset = "\x1b[0m";
constexpr const char* k_ansi_dim = "\x1b[2m";
constexpr const char* k_ansi_cyan = "\x1b[36m";
constexpr const char* k_ansi_yellow = "\x1b[33m";

/// Prints `path` with the characters at `matched_indices` highlighted.
void print_highlighted(const std::string& path,
                       const std::array<uint32_t, autocomplete::k_max_pattern_len>& matched_indices)
{
    size_t next_match = 0;
    for (size_t i = 0; i < path.size(); ++i)
    {
        bool is_match = next_match < matched_indices.size() && static_cast<size_t>(matched_indices[next_match]) == i;
        if (is_match)
        {
            std::printf("%s%c%s", k_ansi_underline_bold_green, path[i], k_ansi_reset);
            ++next_match;
        }
        else
        {
            std::putchar(path[i]);
        }
    }
}

/// Renders one "frame" of the nav bar: the typed query plus a dropdown of
/// the current top matches, each annotated with its score.
void render_frame(const std::string& typed_so_far, const std::vector<autocomplete::ScoredMatch>& matches,
                  double query_micros)
{
    std::printf("\n");
    std::printf("%s+--------------------------------------------------------------+%s\n", k_ansi_dim, k_ansi_reset);
    std::printf(" %s>%s scn://%s%s%s", k_ansi_cyan, k_ansi_reset, k_ansi_yellow, typed_so_far.c_str(), k_ansi_reset);
    std::printf("%s_%s   %s(query: %.1f us, %zu results shown)%s\n", k_ansi_dim, k_ansi_reset, k_ansi_dim, query_micros,
                matches.size(), k_ansi_reset);
    std::printf("%s+--------------------------------------------------------------+%s\n", k_ansi_dim, k_ansi_reset);

    if (matches.empty())
    {
        std::printf("   %s(no matches)%s\n", k_ansi_dim, k_ansi_reset);
    }
    else
    {
        for (size_t i = 0; i < matches.size(); ++i)
        {
            const auto& m = matches[i];
            std::printf("   %s%zu.%s ", k_ansi_dim, i + 1, k_ansi_reset);
            print_highlighted(std::string(m.path), m.matched_indices);
            std::printf("  %s[score %d]%s\n", k_ansi_dim, m.score, k_ansi_reset);
        }
    }
}

/// Simulates the user typing `full_query` one character at a time, querying
/// the engine and rendering a fresh dropdown after every keystroke.
void simulate_typing(autocomplete::AutocompleteEngine& engine, const std::string& full_query,
                     std::chrono::milliseconds frame_delay = std::chrono::milliseconds(220))
{
    std::string typed;
    for (char c : full_query)
    {
        typed.push_back(c);

        auto start = std::chrono::high_resolution_clock::now();
        auto matches = engine.query(typed, /*max_results=*/8);
        auto end = std::chrono::high_resolution_clock::now();
        double micros = std::chrono::duration<double, std::micro>(end - start).count();

        render_frame(typed, matches, micros);
        std::this_thread::sleep_for(frame_delay);
    }
}

} // namespace

int main()
{
    std::printf("%s[directory watch daemon] cold start: scanning project tree...%s\n", k_ansi_dim, k_ansi_reset);

    auto corpus = generate_scene_corpus();

    auto build_start = std::chrono::high_resolution_clock::now();
    autocomplete::AutocompleteEngine engine(corpus);
    auto build_end = std::chrono::high_resolution_clock::now();
    double build_ms = std::chrono::duration<double, std::milli>(build_end - build_start).count();

    std::printf("%s[directory watch daemon] cached %zu scene paths in %.3f ms, idle-watching for changes.%s\n",
                k_ansi_dim, engine.cache_size(), build_ms, k_ansi_reset);

    // ---- Scenario 1: cold dropdown, then type "scs07" for sky_citadel ----
    // sky_citadel_throne_room_01_main: let's instead type something that
    // matches our hand-placed memorable path via initials + numbers.
    std::printf("\n%s=== Scenario 1: typing 'skytrn01' (cold, no history yet) ===%s\n", k_ansi_yellow, k_ansi_reset);
    simulate_typing(engine, "skytrn01");

    // User picks the top result -> this is what "navigating" looks like;
    // feed it back into frecency so it ranks higher next time.
    std::printf("\n%s[user presses Enter on result #1 -> navigates there]%s\n", k_ansi_cyan, k_ansi_reset);
    engine.record_navigation("levels/hubs/sky_citadel_throne_room_01_main.scn.hades");

    // ---- Scenario 2: same query, now with one visit's worth of frecency ----
    std::printf("\n%s=== Scenario 2: typing 'skytrn01' again, right after navigating there ===%s\n", k_ansi_yellow,
                k_ansi_reset);
    simulate_typing(engine, "skytrn01");

    // ---- Scenario 3: a vaguer, more "address-bar-ish" query ----
    std::printf("\n%s=== Scenario 3: typing 'volsanboss' for the volcano boss arena ===%s\n", k_ansi_yellow,
                k_ansi_reset);
    simulate_typing(engine, "volsanboss");

    std::printf("\n%s[user presses Enter on result #1 -> navigates there]%s\n", k_ansi_cyan, k_ansi_reset);
    engine.record_navigation("levels/encounters/volcano_sanctum_07_boss.scn.hades");

    // ---- Scenario 4: ambiguous short query, many candidates share letters ----
    std::printf("\n%s=== Scenario 4: typing just 'fore' (ambiguous, many forest_* paths) ===%s\n", k_ansi_yellow,
                k_ansi_reset);
    simulate_typing(engine, "fore");

    // ---- Scenario 5: a query that matches nothing in the corpus ----
    std::printf("\n%s=== Scenario 5: typing 'xqz999' (no plausible matches) ===%s\n", k_ansi_yellow, k_ansi_reset);
    simulate_typing(engine, "xqz999");

    std::printf("\n%sDone.%s\n", k_ansi_dim, k_ansi_reset);
    return 0;
}