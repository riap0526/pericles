// Diagnostics and layout logs for Zeus (diag.h; the addresses are in the Site and Var
// declarations below).
#include "games/zeus/diag.h"

#include "core/capture/screenshot.h"
#include "core/config.h"
#include "core/ddraw/surface_memory.h"
#include "core/diag/guard.h"
#include "core/display/virtual_display.h"
#include "core/log.h"
#include "core/settings/settings.h"
#include "core/ui/panel.h"
#include "core/ui/perf_view.h"
#include "games/zeus/site.h"

#include <imgui.h>

#include <algorithm>
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
const Site kImageDraw{"image_draw", 0x5acf50, {0x81, 0xec, 0x14, 0x01, 0x00, 0x00, 0x8b, 0x84}};

const Var<unsigned char> kGameScreen{0xf2a6dd};
const Var<int> kScreenMode{0xf2a770}; // 1 / 2 / 3 = 640x480 / 800x600 / 1024x768
const Var<int> kWidth{0xf2a6a0}, kHeight{0xf2a69c};              // dd_width, dd_height
const Var<int> kDialogX{0xf2a6ac}, kDialogY{0xf2a6a8};           // dialog_offset_x / _y
const Var<int> kGameSpeed{0xf2a774};
// The city viewport city_render clips to, and the view it was given (its three arguments, kept by it).
const Var<int> kCityLeft{0x7d3b94}, kCityTop{0x7d3b8c}, kCityRight{0x7d3b90}, kCityBottom{0x7d3b88};
constexpr DWORD kImagePaks = 0x12afbe8; // image_paks: 0xcc44 per pak, +0 entries (0x48 each), +0xcc34 entry count
constexpr DWORD kPakSize = 0xcc44, kPakCount = 0xcc34, kEntrySize = 0x48;

// ---------------------------------------------------------------------------------------------
// The state line

int g_viewA = 0, g_viewB = 0, g_viewC = 0;

void DescribeState(char *out, size_t size)
{
    std::snprintf(out, size,
                  "screen 0x%02x, mode %d = %dx%d, dialog offset (%d,%d), city viewport (%d,%d)-(%d,%d), view %d,%d,%d, "
                  "game speed %d%%",
                  static_cast<unsigned>(*kGameScreen), *kScreenMode, *kWidth, *kHeight, *kDialogX, *kDialogY,
                  *kCityLeft, *kCityTop, *kCityRight, *kCityBottom, g_viewA, g_viewB, g_viewC, *kGameSpeed);
}

// ---------------------------------------------------------------------------------------------
// Panel (drawn on the output thread; reads only)

void InfoSection()
{
    ImGui::Text("Zeus: Master of Olympus + Poseidon 2.1.4.0");
    ImGui::Text("Game resolution: %d x %d (mode %d)", *kWidth, *kHeight, *kScreenMode);
    ImGui::Text("Game speed: %d%%", *kGameSpeed);
    ImGui::Text("Screen: 0x%02x", static_cast<unsigned>(*kGameScreen));
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
    zeus::timing::GameEnded();
    core_log("zeus: game_main returned %d", r);
    return r;
}

// ---------------------------------------------------------------------------------------------
// Screen, mode and city viewport changes

int g_lastScreen = -1, g_lastMode = -1, g_lastW = -1, g_lastH = -1;
int g_lastCity[4] = {-1, -1, -1, -1};
bool g_seen[256] = {};

// ---------------------------------------------------------------------------------------------
// Backgrounds (jpeg_draw_background)

using Jpeg_fn = int(__cdecl *)(const char *, int, int);
Jpeg_fn o_Jpeg;
char g_lastJpeg[128] = "";
int g_lastJpegX = -99999, g_lastJpegY = -99999, g_lastJpegScreen = -1;
core::diag::Feature g_jpegFeature{"background picture log (games/zeus/diag)"};

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
    if (core::diag::Enabled(g_jpegFeature)) {
        JpegCall c{file, x, y};
        core::diag::Guarded(g_jpegFeature, &JpegLog, &c);
    }
    return o_Jpeg(file, x, y);
}

// ---------------------------------------------------------------------------------------------
// Draw census

bool g_censusOn = false;      // [debug] draw_census, read at start-up
bool g_censusFrame = false;   // this frame is being summed up
bool g_censusReport = false;  // the next frame: for a problem report
const char *g_censusWhy = "";
core::diag::Feature g_censusFeature{"draw census (games/zeus/diag)"};

struct PakSum {
    int count = 0;
    int left = 0, top = 0, right = 0, bottom = 0;
};
constexpr int kMaxPaks = 128;
PakSum g_paks[kMaxPaks];
struct Big {
    int id, x, y, w, h;
};
constexpr int kBig = 24;
Big g_big[kBig];
int g_bigCount = 0;
long g_draws = 0;

using ImageDraw_fn = void(__thiscall *)(void *, int, int, int, int, int, int);
ImageDraw_fn o_ImageDraw;

struct DrawCall {
    int id, x, y;
};

void CensusAdd(void *p)
{
    const DrawCall &c = *static_cast<const DrawCall *>(p);
    const int pak = c.id >> 14, index = c.id & 0x3fff;
    if (pak < 0 || pak >= kMaxPaks) {
        return;
    }
    const DWORD base = kImagePaks + static_cast<DWORD>(pak) * kPakSize;
    const int count = *reinterpret_cast<const int *>(base + kPakCount);
    const DWORD entries = *reinterpret_cast<const DWORD *>(base);
    if (!entries || index > count) {
        return;
    }
    const DWORD e = entries + static_cast<DWORD>(index) * kEntrySize;
    const int w = *reinterpret_cast<const short *>(e + 0x14), h = *reinterpret_cast<const short *>(e + 0x16);
    ++g_draws;
    PakSum &s = g_paks[pak];
    if (s.count == 0) {
        s.left = c.x;
        s.top = c.y;
        s.right = c.x + w;
        s.bottom = c.y + h;
    }
    else {
        s.left = std::min(s.left, c.x);
        s.top = std::min(s.top, c.y);
        s.right = std::max(s.right, c.x + w);
        s.bottom = std::max(s.bottom, c.y + h);
    }
    ++s.count;
    // The largest pictures of the frame (by area).
    const Big b{c.id, c.x, c.y, w, h};
    if (g_bigCount < kBig) {
        g_big[g_bigCount++] = b;
    }
    else {
        int smallest = 0;
        for (int i = 1; i < kBig; ++i) {
            if (g_big[i].w * g_big[i].h < g_big[smallest].w * g_big[smallest].h) {
                smallest = i;
            }
        }
        if (w * h > g_big[smallest].w * g_big[smallest].h) {
            g_big[smallest] = b;
        }
    }
}

void __fastcall ImageDraw_detour(void *self, void * /*edx*/, int id, int x, int y, int a, int b, int c);

// __thiscall through __fastcall: ecx = self, edx unused, the rest on the stack (the callee removes them, as
// image_draw's ret 0x18 does). A __fastcall with 8 bytes of register arguments cleans the same 6 stack dwords.
void __fastcall ImageDraw_detour(void *self, void *, int id, int x, int y, int a, int b, int c)
{
    if (g_censusFrame && core::diag::Enabled(g_censusFeature)) {
        DrawCall d{id, x, y};
        core::diag::Guarded(g_censusFeature, &CensusAdd, &d);
    }
    o_ImageDraw(self, id, x, y, a, b, c);
}

void CensusStart(const char *why)
{
    std::memset(g_paks, 0, sizeof(g_paks));
    g_bigCount = 0;
    g_draws = 0;
    g_censusWhy = why;
    g_censusFrame = true;
}

void CensusEnd()
{
    g_censusFrame = false;
    core_log("zeus: draw census (%s), screen 0x%02x, mode %dx%d: %ld pictures drawn", g_censusWhy,
             static_cast<unsigned>(*kGameScreen), *kWidth, *kHeight, g_draws);
    for (int p = 0; p < kMaxPaks; ++p) {
        const PakSum &s = g_paks[p];
        if (s.count) {
            core_log("zeus: draw census:   image file %d: %d pictures in (%d,%d)-(%d,%d)", p, s.count, s.left, s.top,
                     s.right, s.bottom);
        }
    }
    std::sort(g_big, g_big + g_bigCount, [](const Big &l, const Big &r) { return l.w * l.h > r.w * r.h; });
    for (int i = 0; i < g_bigCount; ++i) {
        const Big &b = g_big[i];
        core_log("zeus: draw census:   picture %d:%d %dx%d at (%d,%d)", b.id >> 14, b.id & 0x3fff, b.w, b.h, b.x, b.y);
    }
}

} // namespace

namespace zeus::diag {

void RegisterSettings()
{
    using core::settings::Effect;
    using core::settings::Item;
    using core::settings::Type;
    const int i = core::settings::Register(Item{
        "debug", "draw_census", "Draw census",
        "For development: on = the log lists what each screen draws (the pictures, by image file, with their area), "
        "the first time a screen is shown and with each problem report. Read at start-up.",
        Type::Bool, "off", Effect::Restart, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0});
    core::settings::OnChange(i, [](int, const char *v, bool) {
        g_censusOn = std::strcmp(v, "on") == 0;
        return true;
    });
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
    }
    if (g_censusOn) {
        using Fast = void(__fastcall *)(void *, void *, int, int, int, int, int, int);
        Fast detour = &ImageDraw_detour;
        if (Matches(kImageDraw) &&
            core_hook_install(kImageDraw.name, reinterpret_cast<void *>(kImageDraw.address),
                              reinterpret_cast<void *>(detour), reinterpret_cast<void **>(&o_ImageDraw))) {
            core_log("zeus: draw census on: each screen's first frame and each problem report are summed up");
        }
        else {
            g_censusOn = false;
            core::diag::Disable(g_censusFeature, "image_draw does not match or could not be hooked");
        }
    }
}

void FrameBegin()
{
    if (!g_censusOn || !core::diag::Enabled(g_censusFeature)) {
        return;
    }
    const int screen = *kGameScreen;
    if (g_censusReport) {
        g_censusReport = false;
        CensusStart("problem report");
    }
    else if (!g_seen[screen & 0xff]) {
        g_seen[screen & 0xff] = true;
        CensusStart("first frame of the screen");
    }
}

void FrameEnd()
{
    if (g_censusFrame) {
        CensusEnd();
    }
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

void CensusForReport()
{
    g_censusReport = g_censusOn;
}

void CityRender(int a, int b, int c)
{
    g_viewA = a;
    g_viewB = b;
    g_viewC = c;
    const int now[4] = {*kCityLeft, *kCityTop, *kCityRight, *kCityBottom};
    if (std::memcmp(now, g_lastCity, sizeof(now)) != 0) {
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
