// Zeus's game layer (zeus.h): the exe check, the game's own log messages and the keys the game must not see.
#include "games/zeus/zeus.h"

#include "core/diag/guard.h"
#include "core/display/virtual_display.h"
#include "core/hook/hook.h"
#include "core/log.h"
#include "core/settings/settings.h"
#include "games/zeus/debug_log.h"
#include "ini_template.h" // generated from src/games/zeus/zeus_engine.ini (CMakeLists.txt)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstring>

namespace {

// A place in Zeus.exe 2.1.4.0 (image base 0x400000, no relocations) and the first bytes it has there. Nothing is
// hooked unless every site matches. (Emperor's layer has the same idea in games/emperor/site.h; the two are kept
// apart until both are settled -- the layers do not include each other.)
struct Site {
    const char *name;
    DWORD address;
    BYTE expect[8];
};

bool Matches(const Site &s)
{
    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQuery(reinterpret_cast<void *>(s.address), &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT) {
        return false;
    }
    return std::memcmp(reinterpret_cast<const void *>(s.address), s.expect, sizeof(s.expect)) == 0;
}

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

// The game's F5 switches it to its own window mode and makes DirectDraw again; that window mode needs a clipper,
// which the emulated back end does not have. Until the keys are settled, F5 is kept from the game.
bool KeyFilter(UINT message, WPARAM key, LPARAM l)
{
    if (message != WM_KEYDOWN || key != VK_F5) {
        return false;
    }
    static bool noted = false;
    if (!noted && !(l & (1 << 30))) {
        noted = true;
        core_log("zeus: F5 kept from the game (its window mode does not work with the emulated DirectDraw)");
    }
    return true;
}

} // namespace

extern "C" void zeus_register_settings(void)
{
    // No settings of Zeus's own yet. The settings file this version ships: an older file gets its descriptions.
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
}
