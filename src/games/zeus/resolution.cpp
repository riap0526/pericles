// The logical resolution in Zeus, the same way as Emperor's layer (games/emperor/resolution.cpp).
//
// The game knows three modes (screen_mode_size 0x50bcc0: 1 = 640x480, 2 = 800x600, 3 = 1024x768). Mode 3 is made to
// return our logical resolution; screen_mode stays 3, so Zeus.inf (a dump of the settings block 0xf2a760..0xf2aa9b)
// keeps a value the original game reads, and every `mode == 3` test in the game still takes the 1024x768 path.
// mode_width/height and dd_width/height lie outside that block. screen_set_mode 0x50bd20 derives dd_width/height, the
// pitch, dialog_offset and the clip from the returned size.
//
// The switching itself (the setting, the 1024x768 frame of the front-end screens, Shift+F7, the panel's setting, the
// way back to the game's own size) is the core's (core/display/resolution_switch, shared with Emperor's layer); this
// layer gives it the game's variables, the city screens and the mode change.
//
// Hooked:
//   screen_mode_size 0x50bcc0 (__cdecl mode; ret): mode 3's size is ours (the core's OnModeSize);
//   config_load 0x5c44c0 (no arguments; eax; ret): a Zeus.inf with mode 1 or 2 (800x600 is the game's default) starts
//                       in mode 3, a value the original reads too;
//   ui_set_sidebar_position 0x58e280 (no arguments; ret): the sidebar starts where the city's clip ends with the
//                       sidebar open (60 x columns - 2: 478, 598, 838 at the game's three widths), as many columns as
//                       leave room for its 186 px; up to 59 px right of it stay empty (a later step fills them). The
//                       game sets nothing for any other width;
//   city_viewport_set_640 0x51cba0 and 0x51cc00 (no arguments; ret; screen_after_mode_change picks one by 0x10dff60):
//                       the city viewport (viewport_init 0x449e50) for any size, with formulas that give the game's
//                       own columns and half rows at 640, 800 and 1024; the game uses 8 x 31 for any other width.
// Front-end screens (menus, adventure screens, scores, loading pictures) are laid out for the game's own 1024x768: from
// a full-screen picture (jpeg_draw_background, games/zeus/diag) until the next city screen the game runs at 1024x768
// and the output frames it (the core's OnLoop calls the game's own screen_switch_mode 0x5c4a30, __cdecl mode, as its
// F7 does). Overlays (menus, messages, the file dialog) keep the screen beneath. The world map runs at the city's
// resolution (its layout is a later step).
// Shift+F7 cycles a few settings until the game ends (on a front-end screen it applies on the next city screen). F6
// and F7 stay the game's (F6: the game's own 800x600; F7: mode 3, i.e. ours).
// The game's own screenshot (Alt+1 / Alt+2 in the city: game_wndproc -> 0x42c700 -> bmp_screenshot 0x50b030,
// shot1.bmp / shot2.bmp; __cdecl file name, the caller pops) puts the picture into a fixed 0x2f4d60-byte buffer: enough
// for 1024x768 x 3 bytes, not for larger modes (it would write past it, the crash Emperor had at 0x52abc0). As in
// Emperor's layer (keys.cpp) it is not run; F11 takes our screenshots.
//
// Failure (each part's guard): switching (mode size, changing it, Shift+F7; the core's guard) -> the game's own
// 1024x768 from the next loop; city layout (sidebar, viewport) -> the 1024 layout's values at once, then the game's
// own 1024x768.
#include "games/zeus/resolution.h"

#include "core/diag/guard.h"
#include "core/display/logical_size.h"
#include "core/display/resolution_switch.h"
#include "core/hook/hook.h"
#include "core/log.h"

#include <cstring>

namespace zeus::resolution {

namespace switching = core::display::resolution;

namespace {

// ---------------------------------------------------------------------------------------------
// Addresses

const Site kModeSize{"screen_mode_size", 0x50bcc0, {0x8b, 0x44, 0x24, 0x04, 0x83, 0xf8, 0x01, 0x75}};
const Site kConfigLoad{"config_load", 0x5c44c0, {0x53, 0x55, 0x56, 0xbb, 0x01, 0x00, 0x00, 0x00}};
const Site kSwitchMode{"screen_switch_mode", 0x5c4a30, {0xa0, 0xdd, 0xa6, 0xf2, 0x00, 0x3c, 0x0f, 0x74}};
const Site kSidebarPos{"ui_set_sidebar_position", 0x58e280, {0xa1, 0xb4, 0xa6, 0xf2, 0x00, 0x3d, 0x80, 0x02}};
const Site kViewportWide{"city_viewport_set_640", 0x51cba0, {0xa1, 0xb4, 0xa6, 0xf2, 0x00, 0x3d, 0x80, 0x02}};
const Site kViewportNarrow{"city_viewport_narrow", 0x51cc00, {0xa1, 0xb4, 0xa6, 0xf2, 0x00, 0x3d, 0x80, 0x02}};
const Site kViewportInit{"viewport_init", 0x449e50, {0x8b, 0x4c, 0x24, 0x18, 0x8b, 0x44, 0x24, 0x08}};
const Site kBmpShot{"bmp_screenshot", 0x50b030, {0x83, 0xec, 0x38, 0x53, 0x33, 0xdb, 0x68, 0x60}};

const Var<int> kModeWidth{0xf2a6b4};
const Var<int> kModeHeight{0xf2a6b0};
const Var<int> kScreenMode{0xf2a770};
const Var<unsigned char> kGameScreen{0xf2a6dd};
const Var<char> kRedraw{0xf2a6ce};
const Var<int> kSidebarX{0x10dff34};
const Var<int> kSidebarX2{0x10dff30};

constexpr int kMinW = 1024;               // the game's largest size
constexpr int kSidebarWidth = 186;        // the sidebar (1024 - 838)
constexpr int kSidebarX2Offset = 0x78;    // sidebar_x2 - sidebar_x
constexpr int kTopBar = 0x1e;             // the city viewport starts below the top bar
constexpr int kTileW = 0x3a, kTileH = 0x1e;
constexpr int kViewCols = 231, kViewRows = 457; // the view grid 0x7d3bec (0x449f30 fills 0xe7 x 457)

// Guard (core/diag) of the city layout; switching has the core's.
core::diag::Feature g_layout{"city layout at this resolution (games/zeus/resolution)"};

// The screens the game draws the city for; overlays on them keep the city's size.
bool CityScreen(unsigned char s)
{
    return s == 0x01 || s == 0x20 || s == 0x48;
}

using Mode_fn = void(__cdecl *)(int);
using Void_fn = void(__cdecl *)();
using Config_fn = int(__cdecl *)();
using ViewportInit_fn = void(__cdecl *)(int, int, int, int, int, int, int, int);
Mode_fn o_ModeSize;
Config_fn o_ConfigLoad;
Void_fn o_SidebarPos, o_ViewportWide, o_ViewportNarrow;

// The game's own mode change (as its F7). It does nothing on screens 0x0f, 0x0e, 0x21 and 0x05; the core tries again.
void ChangeMode()
{
    Function<Mode_fn>(kSwitchMode)(3);
    *kRedraw = 1;
}

switching::SwitchGame Game()
{
    return switching::SwitchGame("zeus: resolution", kModeWidth, kModeHeight, kScreenMode, kGameScreen, &CityScreen,
                                 &ChangeMode);
}

// ---------------------------------------------------------------------------------------------
// Mode size and the start-up mode

void __cdecl ModeSize_detour(int mode)
{
    Original(o_ModeSize, kModeSize)(mode);
    if (mode == 3) {
        switching::OnModeSize(); // ours, guarded; the game's own size when switching is off
    }
}

// Modes 1 and 2 have no use with our resolutions (mode 3 carries our size). Zeus.inf has 2 (800x600) unless the
// player chose 1024x768 in the game.
int __cdecl ConfigLoad_detour()
{
    const int r = Original(o_ConfigLoad, kConfigLoad)();
    if (*kScreenMode != 3) {
        core_log("zeus: resolution: Zeus.inf has mode %d (%s); the game starts in mode 3 (1024x768, our resolution) "
                 "instead",
                 *kScreenMode, *kScreenMode == 1 ? "640x480" : *kScreenMode == 2 ? "800x600" : "unknown");
        *kScreenMode = 3;
    }
    return r;
}

// ---------------------------------------------------------------------------------------------
// Sidebar and city viewport

bool StandardSize(int w, int h)
{
    return (w == 640 && h == 480) || (w == 800 && h == 600) || (w == 1024 && h == 768);
}

void LayoutFallBack(const char *what)
{
    switching::ToGameSize(what);
}

// The city's columns with the sidebar open, for widths of 1024 or more (1024 -> 14, as the game): as many 60 px
// columns as leave the sidebar its 186 px (the sidebar starts at 60 x columns - 2, as at 640, 800 and 1024).
int NarrowColumns(int w)
{
    return (w - kSidebarWidth + 2) / 60;
}

void __cdecl SidebarPos_detour()
{
    const int w = *kModeWidth;
    if (w <= 1024) {
        Original(o_SidebarPos, kSidebarPos)();
        return;
    }
    core::diag::Restore r;
    if (core::diag::Guarded(
            g_layout,
            [](void *p) {
                auto &r = *static_cast<core::diag::Restore *>(p);
                r.Set(kSidebarX.ptr(), 60 * NarrowColumns(*kModeWidth) - 2);
                r.Set(kSidebarX2.ptr(), *kSidebarX + kSidebarX2Offset);
                r.Forget();
            },
            &r, &r)) {
        return;
    }
    *kSidebarX = kMinW - kSidebarWidth;
    *kSidebarX2 = kMinW - kSidebarWidth + kSidebarX2Offset;
    LayoutFallBack("the sidebar position could not be set");
}

struct ViewportCall {
    int cols, rows;
};

void SetViewport(int cols, int rows)
{
    const int maxCols = kViewCols - 11, maxRows = kViewRows - 30; // margins as in Emperor's draw loop [guess]
    if (cols > maxCols) cols = maxCols;
    if (rows > maxRows) rows = maxRows;
    Function<ViewportInit_fn>(kViewportInit)(0xe4, 0xe4, 0, kTopBar, cols, rows, kTileW, kTileH);
    static int logged[4] = {-1, -1, -1, -1};
    const int key[4] = {cols, rows, *kModeWidth, *kModeHeight};
    if (std::memcmp(key, logged, sizeof(key)) != 0) {
        std::memcpy(logged, key, sizeof(key));
        core_log("zeus: resolution: city viewport %d x %d for %dx%d", cols, rows, *kModeWidth, *kModeHeight);
    }
}

// Guarded viewport_init; the fallback calls it again with the game's own 1024 values.
void Viewport(ViewportCall c, ViewportCall at1024)
{
    if (core::diag::Guarded(
            g_layout,
            [](void *p) {
                auto &v = *static_cast<ViewportCall *>(p);
                SetViewport(v.cols, v.rows);
            },
            &c)) {
        return;
    }
    Function<ViewportInit_fn>(kViewportInit)(0xe4, 0xe4, 0, kTopBar, at1024.cols, at1024.rows, kTileW, kTileH);
    LayoutFallBack("the city viewport could not be set");
}

int Rows(int h) // 480, 600, 768 -> 31, 39, 50
{
    return (h - kTopBar) / 15 + 1;
}

// 640, 800, 1024 -> 10, 13, 17 columns (the city under the whole width).
void __cdecl ViewportWide_detour()
{
    const int w = *kModeWidth, h = *kModeHeight;
    if (StandardSize(w, h) || w < 1024) {
        Original(o_ViewportWide, kViewportWide)();
        return;
    }
    Viewport({w / 60, Rows(h)}, {17, 50});
}

// 640, 800, 1024 -> 8, 10, 14 columns (left of the sidebar; the game's own below 1024).
void __cdecl ViewportNarrow_detour()
{
    const int w = *kModeWidth, h = *kModeHeight;
    if (StandardSize(w, h) || w < 1024) {
        Original(o_ViewportNarrow, kViewportNarrow)();
        return;
    }
    Viewport({NarrowColumns(w), Rows(h)}, {14, 50});
}

// ---------------------------------------------------------------------------------------------
// The game's own BMP screenshot (Alt+1 / Alt+2): not run

using BmpShot_fn = int(__cdecl *)(const char *);
BmpShot_fn o_BmpShot;

int __cdecl BmpShot_detour(const char *file)
{
    static long logs = 0;
    if (++logs <= 5) {
        core_log("zeus: resolution: the game's own screenshot (%s) is not taken: its buffer is for the game's own "
                 "sizes (F11 takes ours)",
                 file ? file : "?");
    }
    return 0; // the game's "could not" (its callers do not look)
}

} // namespace

void OnBackground()
{
    switching::EnterFrontEnd(-1);
}

void OnLoop()
{
    switching::OnLoop();
}

bool KeyFilter(UINT message, WPARAM key, LPARAM l)
{
    return switching::CycleKey(message, key, l);
}

bool SettingsChanged(const core_config *cfg)
{
    return switching::SettingsChanged(cfg);
}

const Detour *Detours(int *count)
{
    static const Detour kDetours[] = {
        {&kModeSize, reinterpret_cast<void *>(&ModeSize_detour)},
        {&kConfigLoad, reinterpret_cast<void *>(&ConfigLoad_detour)},
        {&kSidebarPos, reinterpret_cast<void *>(&SidebarPos_detour)},
        {&kViewportWide, reinterpret_cast<void *>(&ViewportWide_detour)},
        {&kViewportNarrow, reinterpret_cast<void *>(&ViewportNarrow_detour)},
        {&kBmpShot, reinterpret_cast<void *>(&BmpShot_detour)},
    };
    *count = static_cast<int>(sizeof(kDetours) / sizeof(kDetours[0]));
    return kDetours;
}

void InstalledForTest(const core_config *cfg)
{
    switching::InstalledForTest(Game(), cfg);
}

bool FrontEndForTest()
{
    return switching::FrontEnd();
}

bool Attach(const core_config *cfg)
{
    if (cfg->backend != CORE_BACKEND_EMULATED) {
        core_log("zeus: resolution: real DirectDraw (video.backend=system): the game's own modes only");
        return false;
    }
    const Site *sites[] = {&kModeSize,     &kConfigLoad,     &kSwitchMode,   &kSidebarPos, &kViewportWide,
                           &kViewportNarrow, &kViewportInit, &kBmpShot};
    for (const Site *s : sites) {
        if (!Matches(*s)) {
            core_log("zeus: resolution: %s at 0x%lx does not match; custom resolutions not installed", s->name,
                     static_cast<unsigned long>(s->address));
            return false;
        }
    }
    bool ok = Install(kModeSize, &ModeSize_detour, o_ModeSize);
    ok = Install(kSidebarPos, &SidebarPos_detour, o_SidebarPos) && ok;
    ok = Install(kViewportWide, &ViewportWide_detour, o_ViewportWide) && ok;
    ok = Install(kViewportNarrow, &ViewportNarrow_detour, o_ViewportNarrow) && ok;
    ok = Install(kBmpShot, &BmpShot_detour, o_BmpShot) && ok; // without it Alt+1/2 would crash above 1024x768
    if (!ok) {
        core_log("zeus: resolution: a hook could not be installed; custom resolutions not used");
        switching::ToGameSize("a hook could not be installed");
        core::diag::Disable(g_layout, "a hook could not be installed");
        return false;
    }
    if (!Install(kConfigLoad, &ConfigLoad_detour, o_ConfigLoad)) {
        core_log("zeus: resolution: %s could not be hooked: Zeus.inf's mode is used as it is (our resolution in mode 3 "
                 "only)",
                 kConfigLoad.name);
    }
    switching::Install(Game(), cfg);
    char what[48];
    core::display::DescribeSetting(cfg->resolution, cfg->resolution_w, cfg->resolution_h, cfg->scale_percent, what,
                                   sizeof(what));
    core_log("zeus: resolution: installed (video.resolution %s; front-end screens at 1024x768; Shift+F7 cycles the "
             "resolution; each part guarded)",
             what);
    return true;
}

} // namespace zeus::resolution
