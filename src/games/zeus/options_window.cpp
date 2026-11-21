#include "games/zeus/options_window.h"

#include "core/options/options_host.h"
#include "core/options/setting_rows.h"
#include "core/ui/hotkey.h"
#include "games/zeus/options_text.h"

#include <windows.h>

#include <vector>

namespace zeus::options_window {
namespace {

namespace co = core::options;
namespace oh = core::options::options_host;
namespace sr = core::options::setting_rows;
using sr::Add;
using sr::Header;
using sr::Resolve;
using sr::ShownIf;
using sr::Toggle;

// Yongle's pages with Pericles's settings. Not yet: the game's own options (game and scroll speed, the four volumes,
// autosave, messages) and the Hotkeys page; a category with no rows is left out.
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
        co::Category c{"Speed", {}};
        Add(c.rows, "gameplay", "keep_speed");
        Add(c.rows, "gameplay", "full_speed");
        Header(c.rows, "Scrolling");
        Add(c.rows, "gameplay", "simulate_while_scrolling");
        Add(c.rows, "gameplay", "smooth_scroll");
        Add(c.rows, "gameplay", "drag_scroll");
        Add(c.rows, "gameplay", "edge_scroll_width");
        general.categories.push_back(std::move(c));
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

// Keys nobody may choose (for the Hotkeys page to come): Enter (the game's, and Alt+Enter ours), F5 (window or full
// screen) and Shift+F7 (the resolution, for testing), which stay fixed.
bool ReservedKey(unsigned vk, unsigned mods)
{
    return vk == VK_RETURN || (vk == VK_F5 && mods == 0) || (vk == VK_F7 && mods == core::ui::kModShift);
}

// The debug panel lists every setting until the window has the game's own options and the Hotkeys page: no list of
// settings kept there is needed yet.
const char *const kEmergency[][2] = {{"video", "resolution"}};

} // namespace

void Attach(const core::options::pause_menu::PauseGame &pause)
{
    static const oh::OptionsGame game(pause, zeus::options_text::kOptionsTitle, &BuildPages, oh::kNoHotkeys,
                                      &ReservedKey, kEmergency, 1, false);
    zeus::options_text::Register(); // Pericles's own texts, before the shared ones
    oh::Attach(game);
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

bool IsOpen()
{
    return oh::IsOpen();
}

void OnPresent(unsigned long caller)
{
    oh::OnPresent(caller);
}

} // namespace zeus::options_window
