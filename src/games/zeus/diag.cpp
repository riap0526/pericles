// Diagnostics and layout logs for Zeus (diag.h; the addresses are in the Site and Var
// declarations below).
#include "games/zeus/diag.h"

#include "core/config.h"
#include "core/ddraw/surface_memory.h"
#include "core/diag/guard.h"
#include "core/diag/state_line.h"
#include "core/display/virtual_display.h"
#include "core/log.h"
#include "core/scroll/zoom.h"
#include "core/settings/settings.h"
#include "core/ui/panel.h"
#include "core/ui/perf_view.h"
#include "games/zeus/fullscreen.h"
#include "games/zeus/resolution.h"
#include "games/zeus/site.h"

#include <imgui.h>

#include <cstdio>
#include <cstring>

#ifndef ENGINE_VERSION
#define ENGINE_VERSION "dev"
#endif

namespace zeus::timing {
void GameEnded(); // timing.cpp: the last rate line, screenshots still being written, the timer resolution
}

namespace {

using zeus::Install;
using zeus::Matches;
using zeus::Site;
using zeus::Var;

const Site kGameMain{"game_main", 0x5c37c0, {0x55, 0x8b, 0xec, 0x6a, 0xff, 0x68, 0xa8, 0x4b}};
const Site kJpegBackground{"jpeg_draw_background", 0x504fa0, {0x81, 0xec, 0x90, 0x00, 0x00, 0x00, 0xa1, 0x24}};

const Var<unsigned char> kGameScreen{0xf2a6dd};
const Var<int> kScreenMode{0xf2a770}; // 1 / 2 / 3 = 640x480 / 800x600 / 1024x768
const Var<int> kWidth{0xf2a6a0}, kHeight{0xf2a69c};              // dd_width, dd_height
const Var<int> kDialogX{0xf2a6ac}, kDialogY{0xf2a6a8};           // dialog_offset_x / _y
const Var<int> kGameSpeed{0xf2a774};
// The city viewport city_render clips to, and the view it was given (its three arguments, kept by it).
const Var<int> kCityLeft{0x7d3b94}, kCityTop{0x7d3b8c}, kCityRight{0x7d3b90}, kCityBottom{0x7d3b88};
// The city view (viewport_init's 5th and 6th arguments, columns and half rows) and its tile (58 x 15 in the city).
const Var<int> kViewCols{0x7d3bbc}, kViewRows{0x7d3bb8}, kTileW{0x7d3bb0}, kTileHalfH{0x7d3ba4};

// ---------------------------------------------------------------------------------------------
// The state line

int g_viewA = 0, g_viewB = 0, g_viewC = 0;

// The same fields as Yongle's (core/diag/state_line.h); Zeus's own at the end: the mode number and city_render's
// three arguments as last seen.
void DescribeState(char *out, size_t size)
{
    char extra[64];
    std::snprintf(extra, sizeof(extra), "mode number %d, city_render %d,%d,%d", *kScreenMode, g_viewA, g_viewB,
                  g_viewC);
    core::diag::Format(core::diag::StateLine(*kGameScreen, *kWidth, *kHeight, *kCityLeft, *kCityTop, *kCityRight,
                                             *kCityBottom, *kViewCols, *kViewRows, *kTileW, *kTileHalfH * 2,
                                             *kDialogX, *kDialogY, *kGameSpeed, extra),
                       out, size);
}

// ---------------------------------------------------------------------------------------------
// Panel (drawn on the output thread; reads only)

// The same lines as Yongle's (games/emperor/emperor.cpp InfoSection): the logical resolution (core's mode size; the
// game's own 1024 x 768 on the front-end screens), the screen named city (0x01) or front end.
void InfoSection()
{
    static const char *const kScreens[] = {"front end", "city"};
    const SIZE mode = core::display::ModeSize();
    const int screen = *kGameScreen;
    ImGui::Text("Zeus: Master of Olympus + Poseidon 2.1.4.0");
    ImGui::Text("Logical resolution: %ld x %ld", mode.cx, mode.cy);
    ImGui::Text("Game speed: %d%%", *kGameSpeed);
    ImGui::Text("Screen: %s (%d, 0x%02x)", screen == 1 ? kScreens[1] : kScreens[0], screen, screen);
    char off[512];
    core::diag::SwitchedOff(off, sizeof(off));
    if (off[0]) {
        ImGui::TextWrapped("Switched off after an error (see ddraw_proxy.log): %s", off);
    }
}

// ---------------------------------------------------------------------------------------------
// game_main

using GameMain_fn = int(WINAPI *)(HINSTANCE, HINSTANCE, LPSTR, int);
GameMain_fn o_GameMain;

int WINAPI GameMain_detour(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show)
{
    const int r = o_GameMain(inst, prev, cmd, show);
    zeus::fullscreen::OnGameEnd(); // the city seen last saved (before the screenshots still being written)
    zeus::timing::GameEnded();
    core_log("zeus: game_main returned %d", r);
    return r;
}

// ---------------------------------------------------------------------------------------------
// Screen, mode and city viewport changes

int g_lastScreen = -1, g_lastMode = -1, g_lastW = -1, g_lastH = -1;
int g_lastCity[4] = {-1, -1, -1, -1};

// ---------------------------------------------------------------------------------------------
// Backgrounds (jpeg_draw_background)

using Jpeg_fn = int(__cdecl *)(const char *, int, int);
Jpeg_fn o_Jpeg;
char g_lastJpeg[128] = "";
int g_lastJpegX = -99999, g_lastJpegY = -99999, g_lastJpegScreen = -1;
core::diag::Feature g_jpegFeature{"background picture log (games/zeus/diag)"};
bool g_picturesHooked = false; // jpeg_draw_background is hooked (the framing needs it)

struct JpegCall {
    const char *file;
    int x, y;
};

void JpegLog(void *p)
{
    const JpegCall &c = *static_cast<const JpegCall *>(p);
    char name[128] = "?";
    if (c.file) {
        std::snprintf(name, sizeof(name), "%s", c.file);
    }
    const int screen = *kGameScreen;
    if (std::strcmp(name, g_lastJpeg) == 0 && c.x == g_lastJpegX && c.y == g_lastJpegY && screen == g_lastJpegScreen) {
        return;
    }
    std::snprintf(g_lastJpeg, sizeof(g_lastJpeg), "%s", name);
    g_lastJpegX = c.x;
    g_lastJpegY = c.y;
    g_lastJpegScreen = screen;
    core_log("zeus: background picture %s at (%d,%d) on screen 0x%02x, mode %dx%d", name, c.x, c.y,
             static_cast<unsigned>(screen), *kWidth, *kHeight);
}

int __cdecl Jpeg_detour(const char *file, int x, int y)
{
    zeus::resolution::OnBackground(); // a full-screen picture outside the city starts the 1024x768 frame
    if (core::diag::Enabled(g_jpegFeature)) {
        JpegCall c{file, x, y};
        core::diag::Guarded(g_jpegFeature, &JpegLog, &c);
    }
    zeus::fullscreen::BeforePicture(file); // debug.loading_stage_ms (not guarded: only a wait)
    const int r = o_Jpeg(file, x, y);
    zeus::fullscreen::AfterPicture(file, x, y); // framed (the core's guard, not this log's)
    return r;
}

} // namespace

namespace zeus::diag {

void RegisterSettings()
{
    // A development tool for listing the game's screens, no longer built: the line is dropped from the file.
    core::settings::Retire("debug", "draw_census", nullptr);
}

bool GameMainMatches()
{
    return Matches(kGameMain);
}

bool PicturesHooked()
{
    return g_picturesHooked;
}

void Attach()
{
    core::ddraw::SetStateDescriber(&DescribeState);
    core::ui::SetTitle("Pericles " ENGINE_VERSION " debug panel");
    core::ui::AddSection("Game", &InfoSection);
    core::ui::AddSection("Performance", &core::ui::PerfWidget);
    if (Matches(kGameMain) && Install(kGameMain, &GameMain_detour, o_GameMain)) {
        core_log("zeus: game_main hooked (the last rate line and the screenshots still being written at the end)");
    }
    else {
        core_log("zeus: game_main at 0x%lx not hooked: no last rate line at the end", static_cast<unsigned long>(kGameMain.address));
    }
    if (!Matches(kJpegBackground) || !Install(kJpegBackground, &Jpeg_detour, o_Jpeg)) {
        core::diag::Disable(g_jpegFeature, "jpeg_draw_background does not match or could not be hooked");
        core_log("zeus: front-end screens after the first city screen stay at the city's resolution (no 1024x768 "
                 "frame, not framed)");
    }
    else {
        g_picturesHooked = true;
    }
}

void FrameEnd()
{
    const int screen = *kGameScreen, mode = *kScreenMode, w = *kWidth, h = *kHeight;
    if (mode != g_lastMode || w != g_lastW || h != g_lastH) {
        core_log("zeus: mode %d = %dx%d (was %d = %dx%d), dialog offset (%d,%d), screen 0x%02x", mode, w, h, g_lastMode,
                 g_lastW, g_lastH, *kDialogX, *kDialogY, static_cast<unsigned>(screen));
        g_lastMode = mode;
        g_lastW = w;
        g_lastH = h;
    }
    if (screen != g_lastScreen) {
        core_log("zeus: screen 0x%02x -> 0x%02x (mode %dx%d)", static_cast<unsigned>(g_lastScreen & 0xff),
                 static_cast<unsigned>(screen), w, h);
        g_lastScreen = screen;
    }
}

void CityRender(int a, int b, int c)
{
    g_viewA = a;
    g_viewB = b;
    g_viewC = c;
    const int now[4] = {*kCityLeft, *kCityTop, *kCityRight, *kCityBottom};
    // Not while the wheel moves the zoom (the viewport changes every frame then); the layout it ends on is logged.
    if (!core::scroll::zoom::Moving() && std::memcmp(now, g_lastCity, sizeof(now)) != 0) {
        core_log("zeus: city viewport (%d,%d)-(%d,%d) = %dx%d in mode %dx%d (view %d,%d,%d)", now[0], now[1], now[2],
                 now[3], now[2] - now[0], now[3] - now[1], *kWidth, *kHeight, a, b, c);
        std::memcpy(g_lastCity, now, sizeof(now));
    }
}

void Describe(char *out, std::size_t size)
{
    DescribeState(out, size);
}

} // namespace zeus::diag
