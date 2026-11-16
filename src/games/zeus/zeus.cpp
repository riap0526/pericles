// Zeus's game layer (zeus.h): the exe check, the game's own log messages, the keys the game must not see, and the
// frames, simulation pace and animation timing (games/zeus/timing.h).
#include "games/zeus/zeus.h"

#include "core/capture/screenshot.h"
#include "core/diag/guard.h"
#include "core/diag/present_mode.h"
#include "core/display/virtual_display.h"
#include "core/display/window_key.h"
#include "core/hook/hook.h"
#include "core/log.h"
#include "core/settings/settings.h"
#include "games/zeus/city_picture.h"
#include "games/zeus/debug_log.h"
#include "games/zeus/diag.h"
#include "games/zeus/fullscreen.h"
#include "games/zeus/intro.h"
#include "games/zeus/sound.h"
#include "games/zeus/notice.h"
#include "games/zeus/resolution.h"
#include "games/zeus/scroll.h"
#include "games/zeus/site.h"
#include "games/zeus/state_check.h"
#include "games/zeus/timing.h"
#include "games/zeus/world_map.h"
#include "games/zeus/zoom.h"
#include "ini_template.h" // generated from src/games/zeus/zeus_engine.ini (CMakeLists.txt)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdio>
#include <cstring>

namespace {

// Places in Zeus.exe 2.1.4.0 and the first bytes they have there (games/zeus/site.h). Nothing is hooked unless
// every site matches.
using zeus::Matches;
using zeus::Site;

// The game's log function: only "ret" and padding (the release build compiled the log out). __cdecl, called with a
// text (often with 0 and an error code after it), a number, or other values (docs: symbols debug_log_stub).
const Site kDebugLog{"debug_log_stub", 0x401000, {0xC3, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90}};
// Two more places, only to recognize the build: the game window's procedure and the surface set-up.
const Site kWndProc{"game_wndproc", 0x5c4a90, {0x8B, 0x0D, 0x7C, 0xAC, 0x60, 0x01, 0x83, 0xEC}};
const Site kCreateSurfaces{"dd_create_surfaces", 0x434380, {0x81, 0xEC, 0xD4, 0x00, 0x00, 0x00, 0x53, 0x56}};

using DebugLogFn = void(__cdecl *)(const void *message, int a, int code);
DebugLogFn o_DebugLog = nullptr;
core::diag::Feature g_gameLog{"the game's own log messages (zeus)"};

struct LogCall {
    const void *message;
    int code;
};

void LogBody(const LogCall &c)
{
    namespace dl = zeus::debug_log;
    char text[256];
    const int n = dl::ReadMessage(c.message, text, sizeof(text));
    if (n <= 0) {
        return; // a number or not a text (the game passes both)
    }
    int times = 0;
    if (!dl::ShouldLog(text, &times)) {
        return;
    }
    const char *more = times == dl::kPerMessage ? " (repeats of this message are not logged from now on)" : "";
    // "ERR:DD CreateSurface failed2:" and the like: the error code follows.
    if (c.code != 0 && text[n - 1] == ':') {
        core_log("zeus: game: %s %d%s", text, c.code, more);
    }
    else {
        core_log("zeus: game: %s%s", text, more);
    }
}

// The caller may have pushed only one value; the other two are then whatever is on its stack (read, not used as
// pointers), and the caller removes what it pushed (__cdecl).
void __cdecl DebugLog_detour(const void *message, int a, int code)
{
    (void)a;
    if (!core::diag::Enabled(g_gameLog)) {
        return;
    }
    LogCall c{message, code};
    core::diag::Guarded(g_gameLog, [](void *p) { LogBody(*static_cast<const LogCall *>(p)); }, &c);
}

// F5 and Alt+Enter switch between full screen and a window (ours: core's window mode, the game's picture scaled into
// it), as in Emperor's layer. The game's own F5 must not run: it switches the game to its own window mode and makes
// DirectDraw again, and that window mode needs a clipper, which the emulated back end does not have. The game does
// not use Alt+Enter. F6 and F7 (the game's 800x600 and 1024x768, which is our resolution) stay the game's; Shift+F7
// cycles our resolution (games/zeus/resolution).
bool KeyFilter(UINT message, WPARAM key, LPARAM l)
{
    if (zeus::resolution::KeyFilter(message, key, l) || core::display::WindowKey(message, key, l, "zeus")) {
        return true;
    }
    return message == WM_KEYDOWN && key == VK_F5; // with Shift or Ctrl: still not the game's
}

bool ApplySmooth(int, const char *v, bool initial)
{
    return initial || zeus::scroll::SetSmooth(std::strcmp(v, "on") == 0);
}

bool ApplyResolution(int, const char *, bool initial)
{
    return initial || zeus::resolution::SettingsChanged(core_config_get());
}

} // namespace

extern "C" void zeus_register_settings(void)
{
    zeus::timing::RegisterSettings(); // [gameplay] keep_speed, full_speed, simulate_while_scrolling
    zeus::diag::RegisterSettings();   // [debug] draw_census
    zeus::state_check::RegisterSettings(); // [debug] state_check
    zeus::intro::RegisterSettings();       // [ui] intro_movie
    zeus::resolution::RegisterSettings();  // [ui] info_window
    zeus::fullscreen::RegisterSettings();  // [video] loading_screen, menu_screens, background_blur, background_brightness
    zeus::zoom::RegisterSettings();        // [video] city_zoom, city_zoom_shrink (the core's, as Yongle's)
    zeus::world_map::RegisterSettings();   // [video] world_map, world_map_scale, world_map_background
    zeus::city_picture::RegisterSettings(); // [ui] city_picture_key, minimap_picture_key, minimap_picture_scale
    // The settings file this version ships: an older file gets its descriptions.
    core::settings::SetTemplate(zeus::kIniTemplate);
}

extern "C" int zeus_editor_exe(void)
{
    return 0;
}

extern "C" void zeus_attach(void)
{
    const Site *sites[] = {&kDebugLog, &kWndProc, &kCreateSurfaces};
    for (const Site *s : sites) {
        if (!Matches(*s)) {
            core_log("zeus: %s at 0x%lx does not match the known Zeus.exe (2.1.4.0); no game hooks installed", s->name,
                     static_cast<unsigned long>(s->address));
            return;
        }
    }
    core_log("zeus: Zeus.exe 2.1.4.0 recognized");
    if (core_hook_install(kDebugLog.name, reinterpret_cast<void *>(kDebugLog.address),
                          reinterpret_cast<void *>(&DebugLog_detour), reinterpret_cast<void **>(&o_DebugLog))) {
        core_log("zeus: the game's own log messages go to this log (\"zeus: game: ...\"; each text at most %d times)",
                 zeus::debug_log::kPerMessage);
    }
    else {
        core::diag::Disable(g_gameLog, "the hook could not be installed");
    }
    core::display::SetKeyFilter(&KeyFilter);
    core_log("zeus: F5 and Alt+Enter switch between full screen and a window (the game's own F5 is not used); F6 "
             "and F7 stay the game's");
    zeus::timing::Attach(); // frames follow the display, the simulation pace, figure animations
    zeus::diag::Attach();   // state line, panel sections, game end, layout logs
    zeus::state_check::Attach(); // [debug] state_check: drawings that do not count leave the save data as it was
    zeus::intro::Attach();       // [ui] intro_movie (before game_main reaches the movie)
    zeus::sound::Attach();       // the game's 3D sounds off (their provider stopped the city ~220 ms)
    // The allies' flags at the original's pace; the map drawn with the game's own height (without it the map
    // reads past its picture on a screen higher than 768, so the logical resolution needs it).
    const bool mapHeight = zeus::world_map::Attach();
    bool layout = false; // our resolution's layout is installed (the zoom's viewports)
    if (!mapHeight) {
        core_log("zeus: resolution: not installed (the world map needs the game's own height): the game's own sizes");
    }
    else if (zeus::resolution::Attach(core_config_get())) { // the logical resolution
        layout = true;
        core::settings::OnChange(core::settings::Find("video", "resolution"), &ApplyResolution);
        core::settings::OnChange(core::settings::Find("video", "scale"), &ApplyResolution);
        if (zeus::diag::PicturesHooked()) {
            zeus::fullscreen::Attach(); // the front-end screens framed
        }
        else {
            core_log("zeus: fullscreen: not installed (jpeg_draw_background is not hooked): the game's own frame");
        }
        // The world map in the middle of its area (the border's tiles come from fullscreen's Configure; without
        // them the map has no border).
        zeus::world_map::AttachLayout();
    }
    zeus::notice::Attach(); // our messages in the game's notice boxes
    // Smooth scrolling (the camera and the city drawn with its pixel remainder; after timing's city_render hook);
    // gameplay.smooth_scroll from the panel.
    const bool scroll = zeus::scroll::Attach(core_config_get()->smooth_scroll != 0);
    if (scroll) {
        core::settings::OnChange(core::settings::Find("gameplay", "smooth_scroll"), &ApplySmooth);
        // Zoom: drawn by smooth scrolling's picture, laid out by the resolution's viewports (as Yongle's).
        if (layout) {
            zeus::zoom::Attach();
        }
        else {
            core_log("zoom: not installed (our layout is not): the city stays at 100%%");
        }
        zeus::scroll::AttachDrag(); // middle-button drag scrolling; after the zoom (its filter passes on to it)
    }
    else {
        core_log("zoom: not installed (smooth scrolling's hooks are not): the city stays at 100%%");
    }
    // The city and minimap pictures (drawn from smooth scrolling's city_render path).
    zeus::city_picture::Attach(scroll);
    // Screenshot file names and the present mode log's lines carry game_screen 0xf2a6dd (one byte, reads only).
    // The tag is asked for when a screenshot is taken (game thread): the draw census sums up the next frame too.
    core::capture::SetTagProvider([](char *out, size_t size) {
        std::snprintf(out, size, "screen_0x%02x", *reinterpret_cast<volatile unsigned char *>(0xf2a6dd));
        zeus::diag::ScreenshotTaken();
    });
    core::diag::present_mode::SetScreenProvider(
        [] { return static_cast<int>(*reinterpret_cast<volatile unsigned char *>(0xf2a6dd)); });
}
