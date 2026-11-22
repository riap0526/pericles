#include "games/zeus/options_window.h"

#include "core/log.h"
#include "core/options/game_options.h"
#include "core/options/hotkeys_page.h"
#include "core/options/menu_text.h"
#include "core/options/options_host.h"
#include "core/options/setting_rows.h"
#include "core/ui/hotkey.h"
#include "games/zeus/keys.h"
#include "games/zeus/options_text.h"
#include "games/zeus/site.h"

#include <windows.h>

#include <cctype>
#include <string>
#include <vector>

namespace zeus::options_window {
namespace {

namespace co = core::options;
namespace oh = core::options::options_host;
namespace sr = core::options::setting_rows;
namespace go = core::options::game_options;
using sr::Add;
using sr::Header;
using sr::Resolve;
using sr::ShownIf;
using sr::Toggle;

const Var<int> kEditorMode{0x665e2c}; // editor_mode: 1 in the editors

// The game's picture (the options window keeps the one under it): draw_buffer, dd_pitch, dd_width / dd_height,
// game_screen. draw_buffer is 0 while the game makes its surfaces again.
const Var<unsigned char *> kDrawBuffer{0xf9c100};
const Var<int> kPitch{0xf2a698};
const Var<int> kWidth{0xf2a6a0}, kHeight{0xf2a69c};
const Var<unsigned char> kGameScreen{0xf2a6dd};

bool GameFrame(oh::GameFrame *out)
{
    out->pixels = *kDrawBuffer;
    out->pitch = *kPitch;
    out->width = *kWidth;
    out->height = *kHeight;
    out->screen = *kGameScreen;
    return out->pixels != nullptr;
}

// Yongle's pages with Pericles's settings, the game's own options among them (games/zeus/options_original). The pages
// are built each time the window opens: in the editors the game speed is left out (the editors do not run the game,
// so it does nothing there; game_speed_step 0x402430 ignores it too).
std::vector<co::Page> BuildPages()
{
    co::Page general{"General", {}};
    {
        co::Category c{"General", {}};
        Add(c.rows, "ui", "intro_movie");
        Add(c.rows, "ui", "info_window");
        Add(c.rows, "ui", "minimap_picture_scale");
        general.categories.push_back(std::move(c));
    }
    {
        // Pericles only: Yongle's side panel is Emperor's own.
        co::Category c{"Interface", {}};
        Add(c.rows, "ui", "side_panel");
        general.categories.push_back(std::move(c));
    }
    {
        co::Category c{"Speed", {}};
        if (!sr::Dry()) go::SpeedRows(c.rows, *kEditorMode != 1);
        Add(c.rows, "gameplay", "keep_speed");
        Add(c.rows, "gameplay", "full_speed");
        Header(c.rows, "Scrolling");
        Add(c.rows, "gameplay", "simulate_while_scrolling");
        Add(c.rows, "gameplay", "smooth_scroll");
        Add(c.rows, "gameplay", "drag_scroll");
        Add(c.rows, "gameplay", "edge_scroll_width");
        general.categories.push_back(std::move(c));
    }
    {
        // Only while a city is played (games/zeus/options_original's InCity): in the menus the next mission's choice
        // overrides it, and the briefing would reset the starting money.
        co::Category c{core::options::menu_text::kCategoryDifficulty, {}};
        if (!sr::Dry()) go::DifficultyRows(c.rows);
        if (!c.rows.empty()) {
            general.categories.push_back(std::move(c));
        }
    }
    {
        co::Category c{"Autosave", {}};
        if (!sr::Dry()) go::AutosaveRows(c.rows);
        if (!c.rows.empty()) {
            general.categories.push_back(std::move(c));
        }
    }
    {
        co::Category c{core::options::menu_text::kCategoryMessages, {}};
        if (!sr::Dry()) go::MessageRows(c.rows);
        if (!c.rows.empty()) {
            general.categories.push_back(std::move(c));
        }
    }
    co::Page display{"Video", {}};
    {
        co::Category c{"Display", {}};
        Add(c.rows, "video", "window");
        Add(c.rows, "video", "resolution");
        Add(c.rows, "video", "scale");
        ShownIf(c.rows, "resolution", "scaled");
        Add(c.rows, "video", "scaling");
        Add(c.rows, "video", "present");
        Add(c.rows, "video", "renderer");
        Add(c.rows, "video", "window_lock_cursor");
        display.categories.push_back(std::move(c));
    }
    {
        co::Category c{"City", {}};
        Add(c.rows, "video", "city_zoom");
        Add(c.rows, "video", "city_zoom_shrink");
        display.categories.push_back(std::move(c));
    }
    {
        co::Category c{"Menus and loading", {}};
        Add(c.rows, "video", "loading_screen");
        Add(c.rows, "video", "menu_screens");
        Header(c.rows, "City background");
        Add(c.rows, "video", "background_blur");
        Add(c.rows, "video", "background_brightness");
        display.categories.push_back(std::move(c));
    }
    {
        co::Category c{"World map", {}};
        Add(c.rows, "video", "world_map");
        Add(c.rows, "video", "world_map_scale");
        Add(c.rows, "video", "world_map_background");
        display.categories.push_back(std::move(c));
    }
    co::Page sound{"Audio", {{"", {}}}};
    auto &rows = sound.categories[0].rows;
    Add(rows, "audio", "master_volume");
    Toggle(rows, "audio", "master_sound");
    if (!sr::Dry()) go::SoundRows(rows);
    Header(rows, "More");
    Add(rows, "audio", "ambient_sounds");
    Add(rows, "audio", "nature_sounds");
    Add(rows, "audio", "nature_sounds_gap");
    Add(rows, "audio", "background_sound");
    std::vector<co::Page> pages{std::move(general), std::move(display), std::move(sound)};
    for (auto &p : pages) {
        Resolve(p);
    }
    return pages;
}

// The Hotkeys window (core/options/hotkeys_page). The overlays' names are the game's own (Zeus_Text.eng group 14,
// the same in Zeus_Editor_Text.eng), "See water" as "Water". The culture overlays 10-14 are named after the science
// buildings in Poseidon's adventures and maps: the name is the one the game's overlay menu shows now, chosen as the menu
// does (overlay_menu_build 0x5170a0 asks science_names(0) 0x4200f0 and then uses the texts 24, 21, 20, 23, 22). The
// page is built each time it opens. science_names is __cdecl(flag), plain ret, the result in al; it only reads two
// variables (the main menu's start button, the map's kind). Not as expected: Zeus's names.
const Site kScienceNames{"science_names", 0x4200f0, {0x8b, 0x0d, 0xe0, 0xa4, 0x6a, 0x00, 0xb8, 0x01}};
using ScienceNames_fn = bool(__cdecl *)(int flag);
bool g_scienceNamesOk = false;

constexpr int kOverlayGroup = 14, kFirstCulture = 10;
constexpr int kScienceText[] = {24, 21, 20, 23, 22}; // overlays 10-14

std::string OverlayName(int group, int index)
{
    if (group == kOverlayGroup && index >= kFirstCulture && index < kFirstCulture + 5 && g_scienceNamesOk &&
        Function<ScienceNames_fn>(kScienceNames)(0)) {
        index = kScienceText[index - kFirstCulture];
    }
    const char *text = zeus::keys::GameText(group, index);
    std::string word = core::options::hotkeys_page::OverlayWord(text ? text : "");
    if (!word.empty()) {
        word[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(word[0])));
    }
    return word;
}

const core::options::hotkeys_page::KeyPageGame kKeyPageGame(&OverlayName);

std::vector<co::Page> BuildKeyPages()
{
    return {core::options::hotkeys_page::Build(kKeyPageGame)};
}

// Keys nobody may choose: Enter (the game's, and Alt+Enter ours); the game's keys for development and cheating (Alt+D
// the view's limits, Alt+Z and Alt+F11 the editor's terrain check and debug output, Ctrl+Alt+C the cheat box:
// game_wndproc 0x5c4a90, cheat_key 0x42c700); ours that stay fixed: F5 (window or full screen) and Shift+F7 (the
// resolution, for testing). The game's Ctrl+A/G/H/J and Numpad2 are left free: a flag the game never sets keeps them
// from doing anything.
bool ReservedKey(unsigned vk, unsigned mods)
{
    using core::ui::kModAlt;
    using core::ui::kModCtrl;
    using core::ui::kModShift;
    return vk == VK_RETURN || (mods == kModAlt && (vk == 'D' || vk == 'Z' || vk == VK_F11)) ||
           (vk == 'C' && mods == (kModCtrl | kModAlt)) || (vk == VK_F5 && mods == 0) ||
           (vk == VK_F7 && mods == kModShift);
}

// The debug panel keeps the settings to change there when something goes wrong: the screen and the panel's own key
// (Yongle's six).
const char *const kEmergency[][2] = {{"video", "resolution"}, {"video", "scale"},   {"video", "window"},
                                     {"video", "renderer"},   {"video", "scaling"}, {"ui", "panel_key"}};

} // namespace

void Attach(const core::options::pause_menu::PauseGame &pause)
{
    static const oh::OptionsGame game(pause, zeus::options_text::kOptionsTitle, &BuildPages, &BuildKeyPages,
                                      &ReservedKey, kEmergency,
                                      static_cast<int>(sizeof(kEmergency) / sizeof(kEmergency[0])), true,
                                      &GameFrame);
    zeus::options_text::Register(); // Pericles's own texts, before the shared ones
    g_scienceNamesOk = Matches(kScienceNames);
    if (!g_scienceNamesOk) {
        core_log("options window: %s at 0x%lx does not match; the Hotkeys window names the culture overlays as in Zeus",
                 kScienceNames.name, static_cast<unsigned long>(kScienceNames.address));
    }
    oh::Attach(game);
}

std::vector<core::options::Page> PagesForTest()
{
    return BuildPages();
}

std::vector<core::options::Page> KeyPagesForTest()
{
    return BuildKeyPages();
}

void ScienceNamesForTest()
{
    g_scienceNamesOk = true;
}

bool ReservedKeyForTest(unsigned vk, unsigned mods)
{
    return ReservedKey(vk, mods);
}

bool Available()
{
    return oh::Available();
}

bool Open(int page, int category)
{
    return oh::Open(page, category);
}

bool OpenCategory(int page, const char *name)
{
    return oh::OpenCategory(page, name);
}

bool OpenHotkeys()
{
    return oh::OpenHotkeys();
}

bool IsOpen()
{
    return oh::IsOpen();
}

void OnPresent(unsigned long caller)
{
    oh::OnPresent(caller);
}

} // namespace zeus::options_window
