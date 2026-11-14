// Diagnostics and layout logs for Zeus (diag.h; the addresses are in the Site and Var
// declarations below).
#include "games/zeus/diag.h"

#include "core/capture/screenshot.h"
#include "core/config.h"
#include "core/ddraw/surface_memory.h"
#include "core/diag/guard.h"
#include "core/diag/state_line.h"
#include "core/display/virtual_display.h"
#include "core/log.h"
#include "core/settings/settings.h"
#include "core/ui/panel.h"
#include "core/ui/perf_view.h"
#include "games/zeus/fullscreen.h"
#include "games/zeus/resolution.h"
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
// The city view (viewport_init's 5th and 6th arguments, columns and half rows) and its tile (58 x 15 in the city).
const Var<int> kViewCols{0x7d3bbc}, kViewRows{0x7d3bb8}, kTileW{0x7d3bb0}, kTileHalfH{0x7d3ba4};
constexpr DWORD kImagePaks = 0x12afbe8; // image_paks: 0xcc44 per pak, +0 entries (0x48 each), +0xcc34 entry count
constexpr DWORD kPakSize = 0xcc44, kPakCount = 0xcc34, kEntrySize = 0x48;

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
bool g_inFrame = false; // inside game_frame (FrameBegin .. FrameEnd)

// A screen as the census tells screens apart: game_screen, the editor, the mode, and for the screens that show one of
// several dialogs, which one (0x07: the sidebar panels' dialog 0x665e34; 0x09: the info window's kind 0xffa13c).
const Var<int> kEditorMode{0x665e2c};
const Var<int> kSidebarDialog{0x665e34};
const Var<int> kInfoKind{0xffa13c};

unsigned ScreenKey()
{
    const unsigned screen = *kGameScreen;
    unsigned sub = 0;
    if (screen == 0x07) {
        sub = static_cast<unsigned>(*kSidebarDialog) & 0xff;
    }
    else if (screen == 0x09) {
        sub = static_cast<unsigned>(*kInfoKind) & 0xff;
    }
    return screen | (static_cast<unsigned>(*kEditorMode & 1) << 8) | ((static_cast<unsigned>(*kScreenMode) & 0xf) << 9) |
           (sub << 16);
}

void KeyText(unsigned key, char *out, size_t size)
{
    std::snprintf(out, size, "screen 0x%02x%s, mode %u", key & 0xff, (key >> 8) & 1 ? " (editor)" : "",
                  (key >> 9) & 0xf);
    if (key >> 16) {
        const size_t n = std::strlen(out);
        std::snprintf(out + n, size - n, ", %s %u", (key & 0xff) == 0x07 ? "dialog" : "kind", key >> 16);
    }
}

constexpr int kMaxSeen = 512;
unsigned g_seenKeys[kMaxSeen];
int g_seenCount = 0;

bool Seen(unsigned key)
{
    for (int i = 0; i < g_seenCount; ++i) {
        if (g_seenKeys[i] == key) {
            return true;
        }
    }
    return false;
}

void MarkSeen(unsigned key)
{
    if (!Seen(key) && g_seenCount < kMaxSeen) {
        g_seenKeys[g_seenCount++] = key;
    }
}

// A screen counts once it has stayed the same this long (a screen passed through on the way is not summed up).
constexpr DWORD kSettleMs = 400;
unsigned g_candidate = 0xffffffffu;
DWORD g_candidateSince = 0;

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

bool g_censusOn = false; // [debug] draw_census, read at start-up (defined here: the background pictures use it too)
bool g_savingCensus = false; // our own census pictures ask for the file name tag too: not a screenshot
const Var<unsigned char *> kDrawBuffer{0xf9c100}; // draw_buffer: the back buffer the game draws into

// Backgrounds drawn outside game_frame (the loading screens between the game's frames): with the census on, each
// picture file is saved once as shots\census_<tag>_background_....png (the screen list).
constexpr int kMaxPictures = 64;
char g_savedPictures[kMaxPictures][48];
int g_savedPictureCount = 0;

void SaveBackground(void *p)
{
    const char *file = static_cast<const char *>(p);
    for (int i = 0; i < g_savedPictureCount; ++i) {
        if (std::strcmp(g_savedPictures[i], file) == 0) {
            return;
        }
    }
    if (g_savedPictureCount < kMaxPictures) {
        std::snprintf(g_savedPictures[g_savedPictureCount++], sizeof(g_savedPictures[0]), "%s", file);
    }
    const unsigned char *px = *kDrawBuffer;
    const int w = *kWidth, h = *kHeight;
    if (px && w > 0 && h > 0 && core::capture::HasRoom("census background")) {
        core_log("zeus: draw census: background %s drawn outside the game's frames, saved", file);
        g_savingCensus = true;
        core::capture::SaveOne(px, w * 2, w, h, "census", "background");
        g_savingCensus = false;
    }
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
    if (g_censusOn && !g_inFrame && file && core::diag::Enabled(g_jpegFeature)) {
        char name[48];
        std::snprintf(name, sizeof(name), "%s", file);
        core::diag::Guarded(g_jpegFeature, &SaveBackground, name);
    }
    return r;
}

// ---------------------------------------------------------------------------------------------
// Draw census

bool g_censusFrame = false;   // this frame is being summed up
bool g_censusReport = false;  // the next frame: for a problem report
bool g_censusShotRequest = false; // the next frame: a screenshot (F11) was taken
const char *g_censusWhy = "";
unsigned g_censusKey = 0;      // the screen as it was when this census started
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
long g_draws = 0, g_cityDraws = 0, g_offScreen = 0;
bool g_inCity = false; // inside city_render (timing.cpp): the city's own tiles and figures, only counted

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
    if (g_inCity) {
        ++g_cityDraws; // the city's tiles and figures: not part of the screen's layout
        return;
    }
    // Only what lands on the screen: the area is cut to the mode, pictures wholly outside are counted apart.
    const int sw = *kWidth, sh = *kHeight;
    const int l = std::max(c.x, 0), t = std::max(c.y, 0), r = std::min(c.x + w, sw), btm = std::min(c.y + h, sh);
    if (l >= r || t >= btm) {
        ++g_offScreen;
        return;
    }
    PakSum &s = g_paks[pak];
    if (s.count == 0) {
        s.left = l;
        s.top = t;
        s.right = r;
        s.bottom = btm;
    }
    else {
        s.left = std::min(s.left, l);
        s.top = std::min(s.top, t);
        s.right = std::max(s.right, r);
        s.bottom = std::max(s.bottom, btm);
    }
    ++s.count;
    // The largest pictures of the frame (by area); the same picture at the same place once.
    const Big b{c.id, c.x, c.y, w, h};
    for (int i = 0; i < g_bigCount; ++i) {
        if (g_big[i].id == b.id && g_big[i].x == b.x && g_big[i].y == b.y) {
            return;
        }
    }
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

bool g_censusShot = false; // save the frame with this census (the first frame of a screen)

void CensusStart(const char *why, bool shot)
{
    std::memset(g_paks, 0, sizeof(g_paks));
    g_bigCount = 0;
    g_draws = g_cityDraws = g_offScreen = 0;
    g_censusWhy = why;
    g_censusShot = shot;
    g_censusKey = ScreenKey(); // the screen this census belongs to: as it is when it starts
    g_censusFrame = true;
}


// The frame just drawn, as census_<tag>_game_<w>x<h>_<time>.png in shots\ (the screen list: which screen number is
// which picture). The back buffer still holds it at the end of game_frame.
void CensusShot(void *)
{
    const unsigned char *px = *kDrawBuffer;
    const int w = *kWidth, h = *kHeight;
    if (px && w > 0 && h > 0) {
        g_savingCensus = true;
        core::capture::SaveOne(px, w * 2, w, h, "census", "game");
        g_savingCensus = false;
    }
}

void CensusEnd()
{
    g_censusFrame = false;
    char key[96];
    KeyText(g_censusKey, key, sizeof(key));
    core_log("zeus: draw census (%s), %s, %dx%d: %ld pictures drawn (%ld by the city drawing, %ld outside the "
             "screen; the rest by image file, cut to the screen)",
             g_censusWhy, key, *kWidth, *kHeight, g_draws, g_cityDraws, g_offScreen);
    // No picture for an empty frame or the start-up placeholder screen 0xff.
    if (g_censusShot && g_draws > 0 && (g_censusKey & 0xff) != 0xff && core::capture::HasRoom("census picture")) {
        core::diag::Guarded(g_censusFeature, &CensusShot, nullptr);
    }
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
        "the first time a screen is shown and with each problem report; the first frame of each screen is also saved "
        "in shots\\. Read at start-up.",
        Type::Bool, "off", Effect::Restart, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0});
    core::settings::OnChange(i, [](int, const char *v, bool) {
        g_censusOn = std::strcmp(v, "on") == 0;
        return true;
    });
}

bool GameMainMatches()
{
    return Matches(kGameMain);
}

bool ImageDrawHooked()
{
    return g_censusOn; // true only after Matches and the hook (Attach), false when either failed or census is off
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
    g_inFrame = true;
    if (!g_censusOn || !core::diag::Enabled(g_censusFeature)) {
        return;
    }
    const unsigned key = ScreenKey();
    if (g_censusReport) {
        g_censusReport = false;
        g_censusShotRequest = false;
        CensusStart("problem report", false); // the report saves its own pictures
        return;
    }
    if (g_censusShotRequest) {
        g_censusShotRequest = false;
        CensusStart("screenshot", false); // F11 saves its own pictures
        return;
    }
    // A new screen is summed up (and saved) once it has stayed the same for kSettleMs.
    const DWORD now = GetTickCount();
    if (key != g_candidate) {
        g_candidate = key;
        g_candidateSince = now;
        return;
    }
    if (!Seen(key) && now - g_candidateSince >= kSettleMs) {
        MarkSeen(key);
        CensusStart("a new screen, settled", true);
    }
}

void FrameEnd()
{
    g_inFrame = false;
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

void ScreenshotTaken()
{
    if (g_censusOn && !g_savingCensus) {
        g_censusShotRequest = true;
    }
}

void CityDrawing(bool on)
{
    g_inCity = on;
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
