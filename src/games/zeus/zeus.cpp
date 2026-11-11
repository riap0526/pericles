// Zeus's game layer (zeus.h): the exe check, the game's own log messages, the keys the game must not see, and the
// frames, simulation pace and animation timing (games/zeus/timing.h).
#include "games/zeus/zeus.h"

#include "core/capture/screenshot.h"
#include "core/diag/guard.h"
#include "core/diag/present_mode.h"
#include "core/display/virtual_display.h"
#include "core/hook/hook.h"
#include "core/log.h"
#include "core/settings/settings.h"
#include "games/zeus/debug_log.h"
#include "games/zeus/diag.h"
#include "games/zeus/site.h"
#include "games/zeus/timing.h"
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
// not use Alt+Enter. F6 and F7 (the game's 800x600 and 1024x768) stay the game's.
bool KeyFilter(UINT message, WPARAM key, LPARAM l)
{
    const bool f5 = message == WM_KEYDOWN && key == VK_F5 && !(GetKeyState(VK_SHIFT) & 0x8000) &&
                    !(GetKeyState(VK_CONTROL) & 0x8000);
    const bool altEnter = message == WM_SYSKEYDOWN && key == VK_RETURN && (l & (1 << 29));
    const bool otherF5 = message == WM_KEYDOWN && key == VK_F5; // with Shift or Ctrl: still not the game's
    if (!f5 && !altEnter) {
        return otherF5;
    }
    if (!(l & (1 << 30))) { // not an auto-repeat
        const bool to = !core::display::Windowed();
        core_log("zeus: %s -> %s (the game does not see the key)", f5 ? "F5" : "Alt+Enter", to ? "windowed" : "fullscreen");
        core::settings::Request(core::settings::Find("video", "window"), to ? "windowed" : "fullscreen", true);
    }
    return true;
}

} // namespace

extern "C" void zeus_register_settings(void)
{
    zeus::timing::RegisterSettings(); // [gameplay] keep_speed, full_speed, simulate_while_scrolling
    zeus::diag::RegisterSettings();   // [debug] draw_census
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
    // Screenshot file names and the present mode log's lines carry game_screen 0xf2a6dd (one byte, reads only).
    core::capture::SetTagProvider([](char *out, size_t size) {
        std::snprintf(out, size, "screen_0x%02x", *reinterpret_cast<volatile unsigned char *>(0xf2a6dd));
    });
    core::diag::present_mode::SetScreenProvider(
        [] { return static_cast<int>(*reinterpret_cast<volatile unsigned char *>(0xf2a6dd)); });
}
