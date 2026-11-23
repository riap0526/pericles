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
//   ui_set_sidebar_position 0x58e280 (no arguments; ret): wider than 1024 the sidebar is against the screen's right
//                       edge (earlier builds: where the city's clip ends, 60 x columns - 2, with up to 59 px of marble
//                       right of it). The city's clip stays on the tile grid (whole columns, NarrowColumns); the gap
//                       between it and the sidebar is drawn by smooth scrolling's picture (games/zeus/scroll, the
//                       extent), and filled with the marble when that is not working. The game sets nothing for any
//                       width but 640, 800 and 1024;
//   city_viewport_set_640 0x51cba0 and 0x51cc00 (no arguments; ret; screen_after_mode_change picks one by 0x10dff60):
//                       the city viewport (viewport_init 0x449e50) for any size, with formulas that give the game's
//                       own columns and half rows at 640, 800 and 1024; the game uses 8 x 31 for any other width.
//                       Ours: half rows down to the screen's bottom (the clip up to 14 rows past it, into the spare
//                       rows), the map's view range in the log;
// The city's layout, each part with its own guard:
//   frame_top_panel 0x58eb20's `cmp width, 0x400; jne` at 0x58ef80 becomes jl: the money, people and date at their
//                       1024 places at any width of 1024 or more (the game left them at the last mode's);
//   top_bar_draw 0x59ebe0: wider than 1024 the top bar is tiled from the sidebar to the left edge;
//   fill_rect 0x509b20 (only city_bottom_and_sidebar's call): the bottom strip is not blanked while the city reaches
//                       the screen's bottom;
//   city_message_layout 0x4f2d80: the city message is put on the screen's bottom, not the clip's;
//   sidebar_draw 0x517b10, sidebar_draw_units 0x525280: below the sidebar (above 768 high) its picture moved to
//                       the screen's bottom and the game's inset panel between, right of it the marble
//                       (see the filler's section below);
//   world_map_sidebars 0x597e50: the world map's sidebar filled the same way on its own picture;
//   editor_city_sidebar_draw 0x51e6c0, editor_world_map_sidebar 0x5c2800: the editors' sidebars, the same way;
//   info_window_place 0x5715a0, info_fixed_window_draw 0x5b2880, info_fixed_window_foreground 0x5b38c0,
//   info_window_input 0x5726e0, buttons_draw 0x4316f0: the building information window where [ui] info_window
//                       says; the four kinds the game keeps at (0, 0x20) moved on screen (see its section below).
// Front-end screens (menus, adventure screens, scores, loading pictures) are laid out for the game's own 1024x768: from
// a full-screen picture (jpeg_draw_background, games/zeus/diag) until the next city screen the game runs at 1024x768
// and the output frames it (the core's OnLoop calls the game's own screen_switch_mode 0x5c4a30, __cdecl mode, as its
// F7 does). Overlays (menus, messages, the file dialog) keep the screen beneath. The world map, the campaign map and
// the editor's world map run at the city's resolution (their layout: games/zeus/world_map).
// Shift+F7 cycles a few settings until the game ends (on a front-end screen it applies on the next city screen). The
// game's own F6 (800x600) and F7 (1024x768) never reach the game: with our resolutions it stays in mode 3 (the key
// bindings in games/zeus/keys block them; KeyFilter below blocks F6 when the bindings are off after an error).
// The game's own screenshot (Alt+1 / Alt+2 in the city: game_wndproc -> 0x42c700 -> bmp_screenshot 0x50b030,
// shot1.bmp / shot2.bmp; __cdecl file name, the caller pops) puts the picture into a fixed 0x2f4d60-byte buffer: enough
// for 1024x768 x 3 bytes, not for larger modes (it would write past it, the crash Emperor had at 0x52abc0). As in
// Emperor's layer (games/emperor/resolution) it is not run; F11 takes our screenshots.
//
// Failure (each part's guard): switching (mode size, changing it, Shift+F7; the core's guard) -> the game's own
// 1024x768 from the next loop; city layout (sidebar, viewport) -> the 1024 layout's values at once, then the game's
// own 1024x768; top bar -> not drawn above 1024 wide; message panel and bottom strip -> the game's own (the panel on
// the clip's bottom, the strip blanked); sidebar filler -> not drawn (the game's own sidebar); info window -> the
// game's own position (a window being moved: draw_buffer and the mouse put back, drawn where the game has it).
#include "games/zeus/resolution.h"

#include "core/assets/sg3.h"
#include "core/diag/guard.h"
#include "core/display/city_layout.h"
#include "core/display/logical_size.h"
#include "core/display/moved_rect.h"
#include "core/display/window_move.h"
#include "core/display/resolution_switch.h"
#include "core/hook/hook.h"
#include "core/log.h"
#include "core/scroll/zoom.h"
#include "core/settings/settings.h"
#include "core/ui/info_window.h"
#include "games/zeus/scroll.h"
#include "games/zeus/side_panel.h"
#include "games/zeus/side_panel_layout.h"
#include "games/zeus/timing.h"
#include "games/zeus/zoom.h"

#include <cstring>
#include <cwchar>
#include <vector>

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
// The city layout: the top bar, the bottom strip and the city message panel.
// top_bar_draw: __cdecl, the caller passes 2 arguments (0, mode_width) the function does not read; ret.
const Site kTopBarDraw{"top_bar_draw", 0x59ebe0, {0xa1, 0xb4, 0xa6, 0xf2, 0x00, 0x83, 0xec, 0x08}};
// image_group_get: __thiscall on the image groups 0x12bc82c, 1 argument (group), ret 4; returns the group's entry,
// whose +4 is the first image (+ 0x4000 = its id for image_draw), as top_bar_draw and sidebar_draw_units use it.
const Site kImageGroup{"image_group_get", 0x40fcf0, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x91, 0x38, 0xcc}};
// image_draw: __thiscall on the image manager 0x12aebe8, 6 arguments (id, x, y, 0, 0, 0), ret 0x18 (called, not
// hooked). image_get_size: __thiscall on the same, (id, int *w, int *h), ret 0xc, al = found.
const Site kImageDraw{"image_draw", 0x5acf50, {0x81, 0xec, 0x14, 0x01, 0x00, 0x00, 0x8b, 0x84}};
const Site kImageSize{"image_get_size", 0x5aceb0, {0x53, 0x56, 0x8b, 0x74, 0x24, 0x0c, 0x8b, 0x5c}};
// fill_rect: __cdecl (x, y, w, h, colour), ret. city_bottom_and_sidebar 0x58e2e0 blanks the screen's bottom rows with
// it (the call at 0x58e320, returning to 0x58e325).
const Site kFillRect{"fill_rect", 0x509b20, {0x8b, 0x54, 0x24, 0x0c, 0x53, 0x56, 0x57, 0x85}};
constexpr DWORD kBottomStripCall = 0x58e325;
// city_message_layout: no arguments, ret (entered by a jmp from 0x4f2830): screen 0x05's layout; a city message is
// put on the city clip's bottom 0x7d3b88.
const Site kMessagePanel{"city_message_layout", 0x4f2d80, {0x81, 0xec, 0x9c, 0x05, 0x00, 0x00, 0xa1, 0x4c}};
// frame_top_panel 0x58eb20: the x of the money, people and date (0x10dfd94/90/8c) are written only at widths 800
// and 1024 (`cmp eax, 0x400; jne`); the jne becomes jl so that any width of 1024 or more takes the 1024 places.
constexpr DWORD kTopValuesCompare = 0x58ef7b; // 3d 00 04 00 00 75 23
const BYTE kTopValuesBytes[] = {0x3d, 0x00, 0x04, 0x00, 0x00, 0x75, 0x23};

const Var<int> kModeWidth{0xf2a6b4};
const Var<int> kModeHeight{0xf2a6b0};
const Var<int> kScreenMode{0xf2a770};
const Var<unsigned char> kGameScreen{0xf2a6dd};
const Var<char> kRedraw{0xf2a6ce};
const Var<int> kSidebarX{0x10dff34};
const Var<int> kSidebarX2{0x10dff30};
const Var<int> kEditorMode{0x665e2c}; // editor_mode: 1 in the editors (the zoom is not used there)
const Var<int> kCityClipBottom{0x7d3b88}; // viewport_init: (half rows - 1) x 15 + 30
const Var<int> kCityClipRight{0x7d3b90};  // viewport_init: 60 x columns - 2
const Var<int> kMapColMin{0xfa05b4}; // view_clamp 0x51cd60's limits (as Emperor's order)
const Var<int> kMapColMax{0xfa05b0};
const Var<int> kMapRowMin{0xfa05ac};
const Var<int> kMapRowMax{0xfa05a8};
const DWORD kImageGroups = 0x12bc82c, kImages = 0x12aebe8; // the image groups and the image manager (this)
constexpr int kGroupTopBar = 0x6b; // Zeus_Interface entry 1231, 838x30: its right end meets the sidebar

constexpr int kMinW = 1024;               // the game's largest size
constexpr int kSidebarWidth = 186;        // the sidebar (1024 - 838)
constexpr int kSidebarX2Offset = 0x78;    // sidebar_x2 - sidebar_x
constexpr int kTopBar = 0x1e;             // the city viewport starts below the top bar
constexpr int kTileW = 0x3a, kTileH = 0x1e;
constexpr int kViewCols = 231, kViewRows = 457; // the view grid 0x7d3bec (0x449f30 fills 0xe7 x 457)

// Guard (core/diag) of the city layout; switching has the core's.
core::diag::Feature g_layout{"city layout at this resolution (games/zeus/resolution)"};
core::diag::Feature g_bars{"top bar (games/zeus/resolution)"};
core::diag::Feature g_message{"city message panel and bottom strip (games/zeus/resolution)"};

// The screens that run at our resolution: the city (also the adventure editor's city, 0x01 with editor_mode 1) and
// the editor's world map (0x15, and 0x2d, its name box over it), which games/zeus/world_map lays out as the game's
// world map. Overlays on them keep their size. The editor's world map is entered from the adventure editor (a front-end
// screen in the 1024x768 frame): leaving the frame there is one mode change, as the editor's city has.
bool CityScreen(unsigned char s)
{
    return s == 0x01 || s == 0x20 || s == 0x48 || s == 0x15 || s == 0x2d;
}

using Mode_fn = void(__cdecl *)(int);
using Void_fn = void(__cdecl *)();
using Config_fn = int(__cdecl *)();
using ViewportInit_fn = void(__cdecl *)(int, int, int, int, int, int, int, int);
Mode_fn o_ModeSize;
Config_fn o_ConfigLoad;
void (*g_afterConfigLoad)() = nullptr;
Void_fn o_SidebarPos, o_ViewportWide, o_ViewportNarrow;

// Around the game's mode change, each with the guard g_modeScreens:
//   help 0x05 (also About): screen_switch_mode does nothing there, so the old picture stayed until the help was left.
//   The help is closed first as its right click closes it (help_close 0x4f51e0: __cdecl, no arguments, ret; back to
//   the screen it was opened from), when its right click would (no choice buttons: 0xef8cba is 0, 0x4f4cf0).
//   mission goals 0x3b: its drawing is the dialog only (screen_draw_background 0x513b30, screen_draw 0x514020), so the
//   new picture was black around it. Its right click goes back to the city (0x52be10: game_screen 1), so the city is
//   drawn behind it once, as the city screen draws it, inside timing's drawing that never counts (as the Display
//   options dialog draws the city behind it).
const Site kHelpClose{"help_close", 0x4f51e0, {0xa0, 0xb0, 0x8c, 0xef, 0x00, 0xc6, 0x05, 0xce}};
const Site kScreenBackground{"screen_draw_background", 0x513b30, {0x8b, 0x44, 0x24, 0x04, 0xb1, 0x01, 0x85, 0xc0}};
const Site kScreenDraw{"screen_draw", 0x514020, {0x33, 0xc0, 0x53, 0xa0, 0xdd, 0xa6, 0xf2, 0x00}};
const Var<short> kHelpChoices{0xef8cba};
// The two hover variables: screen_draw_background reads both, clears them for a drawing call and puts them back;
// screen_draw does the same with the first one.
const Var<int> kHover{0x781844}, kHover2{0x781840};
constexpr unsigned char kScreenHelp = 0x05, kScreenGoals = 0x3b, kScreenCity = 0x01;
core::diag::Feature g_modeScreens{"help and mission goals at a mode change (games/zeus/resolution)"};
bool g_modeScreensOk = false; // its sites matched
bool g_behindDrawn = false;
bool g_behindLogged = false;

void CloseHelpNow(void *)
{
    if (*kGameScreen == kScreenHelp && *kHelpChoices == 0) {
        Function<Void_fn>(kHelpClose)();
        core_log("zeus: resolution: the help closed for the mode change (back to screen 0x%02x)", *kGameScreen);
    }
}

using ScreenBackground_fn = void(__cdecl *)(int force);

void GameDrawsCity(void *)
{
    Function<ScreenBackground_fn>(kScreenBackground)(1);
    Function<Void_fn>(kScreenDraw)();
}

void DrawCityNow(void *)
{
    g_behindDrawn = true;
    core::diag::Restore undo;
    undo.Save(kGameScreen.ptr());
    undo.Save(kHover.ptr());
    undo.Save(kHover2.ptr());
    *kGameScreen = kScreenCity;
    const bool ok = core::diag::Guarded(g_modeScreens, &GameDrawsCity, nullptr, &undo);
    if (ok) {
        undo.Undo(); // the goals' screen and hover back
    }
}

void DrawCityBehindGoals()
{
    g_behindDrawn = false;
    zeus::timing::PutBackDrawingOutside(&DrawCityNow, nullptr);
    if (!g_behindDrawn && !g_behindLogged) {
        g_behindLogged = true;
        core_log("zeus: resolution: the city behind the mission goals is not drawn (timing's put-back is not working)");
    }
    *kRedraw = 1; // the goals over it in the next frame
}

// The game's own mode change (as its F7). It does nothing on screens 0x0f, 0x0e, 0x21 and 0x05; the core tries again.
void ChangeMode()
{
    const bool screens = g_modeScreensOk && core::diag::Enabled(g_modeScreens);
    if (screens) {
        core::diag::Guarded(g_modeScreens, &CloseHelpNow, nullptr);
    }
    Function<Mode_fn>(kSwitchMode)(3);
    *kRedraw = 1;
    if (screens && core::diag::Enabled(g_modeScreens) && *kGameScreen == kScreenGoals) {
        DrawCityBehindGoals();
    }
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
    if (g_afterConfigLoad) {
        g_afterConfigLoad();
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

// Where the city is seen to beyond its clip (smooth scrolling's picture, games/zeus/scroll SetCityExtent), after
// the sidebar or the viewport changed: with our layout the sidebar's left edge (the screen's right edge without a
// sidebar: city_viewport_set_640, which the game does not use) and the screen's bottom; the game's own layouts and our
// fallback: just the clip (0).
bool g_wideViewport = false; // the last viewport was the one without a sidebar

bool OurLayout(int w, int h)
{
    return !(StandardSize(w, h) || w < 1024) && core::diag::Enabled(g_layout);
}

void UpdateCityExtent()
{
    const int w = *kModeWidth, h = *kModeHeight;
    const bool ours = OurLayout(w, h);
    zeus::scroll::SetCityExtent(ours ? (g_wideViewport ? w : *kSidebarX) : 0, ours ? h : 0);
}

void __cdecl SidebarPos_detour()
{
    const int w = *kModeWidth;
    if (w <= 1024) {
        Original(o_SidebarPos, kSidebarPos)();
        UpdateCityExtent();
        return;
    }
    core::diag::Restore r;
    if (core::diag::Guarded(
            g_layout,
            [](void *p) {
                auto &r = *static_cast<core::diag::Restore *>(p);
                r.Set(kSidebarX.ptr(), *kModeWidth - kSidebarWidth);
                r.Set(kSidebarX2.ptr(), *kSidebarX + kSidebarX2Offset);
                r.Forget();
            },
            &r, &r)) {
        UpdateCityExtent();
        return;
    }
    *kSidebarX = kMinW - kSidebarWidth;
    *kSidebarX2 = kMinW - kSidebarWidth + kSidebarX2Offset;
    UpdateCityExtent(); // off after the error: the clip only
    LayoutFallBack("the sidebar position could not be set");
}

struct ViewportCall {
    int cols, rows;
};

void SetViewport(int cols, int rows)
{
    // The widest draw loops (0x458170, 0x44ad70) check the view grid's bounds; the one that does not (0x454420)
    // reads columns view_drawn_x .. + cols and rows view_drawn_y .. + rows - 1, well
    // inside these margins (Emperor's).
    const int maxCols = kViewCols - 11, maxRows = kViewRows - 30;
    if (cols > maxCols) cols = maxCols;
    if (rows > maxRows) rows = maxRows;
    Function<ViewportInit_fn>(kViewportInit)(0xe4, 0xe4, 0, kTopBar, cols, rows, kTileW, kTileH);
    // The map's view range (view_clamp's limits, set when a map is loaded): a viewport larger than it shows the
    // outside of the map, logged as Emperor's layer does.
    const int mapCols = *kMapColMax - *kMapColMin, mapRows = *kMapRowMax - *kMapRowMin;
    static int logged[6] = {-1, -1, -1, -1, -1, -1};
    const int key[6] = {cols, rows, *kModeWidth, *kModeHeight, mapCols, mapRows};
    if (!core::scroll::zoom::Moving() && std::memcmp(key, logged, sizeof(key)) != 0) { // not every wheel frame
        std::memcpy(logged, key, sizeof(key));
        core_log("zeus: resolution: city viewport %d x %d for %dx%d (clip bottom %d); current map view range %d x %d%s",
                 cols, rows, *kModeWidth, *kModeHeight, *kCityClipBottom, mapCols, mapRows,
                 mapCols > 0 && (cols > mapCols || rows > mapRows) ? " -- VIEWPORT LARGER THAN THE MAP" : "");
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

// As many half rows as reach the screen's bottom (the clip, (rows - 1) x 15 + 30, is >= the height): the city
// is drawn down to the bottom and up to 14 rows into the spare rows below the screen (core/ddraw/surface_memory),
// as Emperor's small tiles do (its ViewportSmall_detour). The old (h - 30) / 15 + 1 left 0..14 rows black. The game's
// own sizes keep the game's own rows (480, 600, 768 -> 31, 39, 50; StandardSize).
int Rows(int h)
{
    return (h - kTopBar + 14) / 15 + 1;
}

// After a viewport (ours or the game's): the extent and the spare rows the city may draw into (the canary's
// allowance): the clip's overshoot (with our layout the city is shown down to the screen's bottom, which the clip
// already reaches).
void AfterViewport(bool wide)
{
    g_wideViewport = wide;
    UpdateCityExtent();
    const int h = *kModeHeight;
    core::display::ApplySpareRows("zeus: resolution: ", *kCityClipBottom, OurLayout(*kModeWidth, h) ? h : 0, 15, h);
}

// Our layout, with the city's clip reaching the screen's bottom (or past it): the bottom strip is not blanked and the
// city message panel is put on the screen's bottom. The case without smooth scrolling's picture (CityExtended).
bool CityToBottom()
{
    const int w = *kModeWidth, h = *kModeHeight;
    return !(StandardSize(w, h) || w < 1024) && core::diag::Enabled(g_layout) && *kCityClipBottom >= h;
}

// Zoom: the city is shown up to the sidebar (the screen's right edge without one) and down to the
// screen's bottom just now, whatever the clip: our layout and smooth scrolling's picture working (it draws the city
// past its clip, and the zoomed city, whose clip is our buffer's). Emperor's CityExtended.
bool CityExtended()
{
    return OurLayout(*kModeWidth, *kModeHeight) && zeus::scroll::Working();
}

int ShownRight()
{
    return g_wideViewport ? *kModeWidth : *kSidebarX;
}

// 640, 800, 1024 -> 10, 13, 17 columns (the city under the whole width).
void __cdecl ViewportWide_detour()
{
    const int w = *kModeWidth, h = *kModeHeight;
    if (StandardSize(w, h) || w < 1024) {
        core::scroll::zoom::ResetView(); // the game's own sizes: no zoom
        Original(o_ViewportWide, kViewportWide)();
    }
    else {
        int cols = w / 60, rows = Rows(h);
        if (*kEditorMode != 0) {
            core::scroll::zoom::ResetView(); // the editors: no zoom
        }
        else {
            core::scroll::zoom::ViewFor(w, h - kTopBar, kTileW + 2, kTileH / 2, &cols, &rows); // zoom
        }
        Viewport({cols, rows}, {17, 50});
    }
    AfterViewport(true);
}

// 640, 800, 1024 -> 8, 10, 14 columns (left of the sidebar; the game's own below 1024).
void __cdecl ViewportNarrow_detour()
{
    const int w = *kModeWidth, h = *kModeHeight;
    if (StandardSize(w, h) || w < 1024) {
        core::scroll::zoom::ResetView(); // the game's own sizes: no zoom
        Original(o_ViewportNarrow, kViewportNarrow)();
    }
    else {
        // Zoom: the zoomed view for the city as shown, from the top bar to the bottom and to the sidebar;
        // capped to the map's range by the core's zoom (100: the columns and rows as before).
        // The editors (the adventure editor's city is screen 0x01 at our resolution): no zoom.
        int cols = NarrowColumns(w), rows = Rows(h);
        if (*kEditorMode != 0) {
            core::scroll::zoom::ResetView();
        }
        else {
            core::scroll::zoom::ViewFor(*kSidebarX, h - kTopBar, kTileW + 2, kTileH / 2, &cols, &rows);
        }
        Viewport({cols, rows}, {14, 50});
    }
    AfterViewport(false);
}

// Zoom: the city view laid out again after the scale or the map changed, as the last viewport was (the game
// lays out the narrow one; the wide one is not used by the game).
void RelayoutCity()
{
    if (g_wideViewport) {
        ViewportWide_detour();
    }
    else {
        ViewportNarrow_detour();
    }
}

// ---------------------------------------------------------------------------------------------
// The top bar, the bottom strip and the city message panel

using ImageGroup_fn = BYTE *(__thiscall *)(void *, int);
using ImageDraw_fn = void(__thiscall *)(void *, int, int, int, int, int, int);
using ImageSize_fn = char(__thiscall *)(void *, int, int *, int *);
using TopBar_fn = void(__cdecl *)(int, int);
using FillRect_fn = void(__cdecl *)(int, int, int, int, int);
TopBar_fn o_TopBarDraw;
FillRect_fn o_FillRect;
Void_fn o_MessagePanel;

// The first image of a group, as the game's own drawing gets it (top_bar_draw, sidebar_draw_units).
int GroupImage(int group)
{
    BYTE *entry = Function<ImageGroup_fn>(kImageGroup)(reinterpret_cast<void *>(kImageGroups + g_imageShift), group);
    return entry ? *reinterpret_cast<int *>(entry + 4) + 0x4000 : -1;
}

void ImageSize(int id, int *w, int *h)
{
    *w = *h = 0;
    Function<ImageSize_fn>(kImageSize)(reinterpret_cast<void *>(kImages + g_imageShift), id, w, h);
}

void DrawImage(int id, int x, int y)
{
    Function<ImageDraw_fn>(kImageDraw)(reinterpret_cast<void *>(kImages + g_imageShift), id, x, y, 0, 0, 0);
}

// The game's drawing clip (screen_set_mode sets it to the screen; city_render and a few panels narrow it for a
// while; bottom and right exclusive): image_draw draws only inside it.
const Var<int> kClipBottom{0xf139b0}, kClipTop{0xf139b4}, kClipRight{0xf139b8}, kClipLeft{0xf139bc};

// The top bar's even section (columns 158..397 of its 838: lighter at the left end, darker towards the sidebar;
// seen in a mockup), repeated left of the whole bar so that no dark end meets a light one.
constexpr int kBarSectionX = 158, kBarSectionW = 240;

// top_bar_draw 0x59ebe0: the top bar (838 px, its right end at the sidebar) at x 0 at 1024, cropped at 800, at
// x -360 at 640; nothing at any other width. Wider than 1024: tiled from the sidebar to the left edge (Emperor's
// FrameBars_detour). frame_top_panel draws it only when a value changed, so this is not every frame; the world map
// draws it too (frame_top_panel is also called by world_map_draw 0x599da0).
void __cdecl TopBarDraw_detour(int a, int b)
{
    if (*kModeWidth <= 1024 || !core::diag::Enabled(g_bars)) {
        Original(o_TopBarDraw, kTopBarDraw)(a, b);
        return;
    }
    core::diag::Restore r; // the clip, after an error
    if (!core::diag::Guarded(
            g_bars,
            [](void *p) {
                auto &r = *static_cast<core::diag::Restore *>(p);
                const int id = GroupImage(kGroupTopBar);
                int w, h;
                ImageSize(id, &w, &h);
                if (w < kBarSectionX + kBarSectionW) {
                    return;
                }
                // image_clip_x 0x509c20 clips one side only (its modes: left, right, or not shown), so each section
                // is drawn with only its left side clipped, from the leftmost one on: the rest of the image right of
                // the section is covered by the next section, the last one's by the whole bar, drawn last (its right
                // end at the sidebar, as at 1024; seen in testing).
                const int whole = *kSidebarX - w;
                int first = whole; // the leftmost section: at x in (-240, 0]
                while (first > 0) {
                    first -= kBarSectionW;
                }
                r.Save(kClipLeft.ptr());
                const int left = *kClipLeft;
                for (int x = first; x < whole; x += kBarSectionW) {
                    *kClipLeft = x > left ? x : left; // the section's columns start at x
                    DrawImage(id, x - kBarSectionX, 0);
                }
                r.Undo();
                DrawImage(id, whole, 0);
            },
            &r, &r)) {
        core_log("zeus: resolution: the top bar is no longer drawn after an error");
    }
}

// The bottom strip city_bottom_and_sidebar blanks (1024 x 9 rows at 1024 wide, the full width x 6 rows otherwise):
// not blanked while the city reaches the screen's bottom (it covers the strip in the city screen anyway; in screen
// 0x05 the city is drawn before it, so the strip would stay black under the message). Emperor's FillRect_detour.
void __cdecl FillRect_detour(int x, int y, int w, int h, int colour)
{
    if (GameAddress(__builtin_return_address(0)) == kBottomStripCall && core::diag::Enabled(g_message) &&
        (CityExtended() || CityToBottom())) {
        return;
    }
    Original(o_FillRect, kFillRect)(x, y, w, h, colour);
}

// The city message panel laid out in the city as it is shown: the layout puts it on the city clip's bottom and makes
// it as wide as the clip. While it runs the clip's bottom is the screen's and, while the city is shown up to the
// sidebar (CityExtended: smooth scrolling's picture, also when zoomed, where the clip is our buffer's), its right is
// the sidebar's left edge (as wide as the city up to the sidebar, as Yongle's). Without the
// picture only the bottom (the clip reaches up to 14 rows below the screen with our half rows). The city it
// draws meanwhile still uses the grid clip (the picture works it out from the viewport). Emperor's
// MessagePanel_detour.
struct PanelCall {
    core::diag::Restore r;
    bool right;
};

void __cdecl MessagePanel_detour()
{
    const bool extended = CityExtended();
    if (!core::diag::Enabled(g_message) || !(extended || (CityToBottom() && *kCityClipBottom > *kModeHeight))) {
        Original(o_MessagePanel, kMessagePanel)();
        return;
    }
    PanelCall c{{}, extended};
    if (core::diag::Guarded(
            g_message,
            [](void *p) {
                auto &c = *static_cast<PanelCall *>(p);
                if (c.right) {
                    c.r.Set(kCityClipRight.ptr(), ShownRight());
                }
                c.r.Set(kCityClipBottom.ptr(), *kModeHeight);
                Original(o_MessagePanel, kMessagePanel)();
                c.r.Undo();
            },
            &c, &c.r)) {
        return;
    }
    Original(o_MessagePanel, kMessagePanel)(); // put back by the guard: the game's own layout from now on
}

// ---------------------------------------------------------------------------------------------
// The sidebar's filler
//
// sidebar_draw 0x517b10 draws the sidebar's backdrop (186x768: Zeus_Interface entry 1232, or the Poseidon pack's when
// 0x4200f0(1)) at sidebar_x and its buttons and boxes over it; sidebar_draw_units 0x525280 (screens 0x48, 0x20) the
// same backdrop with the army panel (186x476) over it. Above 768 high, after the game's drawing:
//   - the backdrop's rows 600..767 (the frame and the picture) are moved to the screen's bottom, as the game drew
//     them (whichever pack);
//   - rows 600 .. bottom - 168: the backdrop's left edge (columns 0..6, its rows 640..699 repeated) and, right of it,
//     an inset panel of the game's own tiles (group 0x84: inner_panel_draw 0x59f2a0's 7 x 7 set of 16 px tiles) edge
//     to edge, for later information and the minimap; less than 32 rows: the plain marble;
// and right of the sidebar (the sidebar is now against the right edge, so nothing at our sizes), and on the city
// screens, while smooth scrolling is not working, between the city's clip and the sidebar (up to 59 px): the
// backdrop's plain marble (entry 1232, x 50..185, y 30..419), mirrored tiles. The marble is read from the
// player's own DATA\Zeus_Interface.sg3 in memory only (core/assets/sg3); when it cannot be read, the 800x600 strip
// (entry 1234, 16x600) is drawn whole with image_draw instead.
// The world map's sidebar (world_map_sidebars 0x597e50, and the campaign map's, games/zeus/world_map) the same
// way on its own picture (Zeus_Interface entry 1600: a gold beaded edge of 11 columns, its rows 470..529 repeated,
// which stay where the game drew them), the city's marble right of it.
constexpr int kBackdropH = 768, kPictureTop = 600, kPictureH = 768 - 600;

// The sidebar's left edge carried down beside the inset panel: its width and the 60 rows repeated (on the screen:
// the picture's rows moved to the bottom, or the rows above 600 that stay).
struct Edge {
    int w;
    int bandTop;    // the band's first row on the game's 768 sidebar
    bool bandMoved; // the band is in the picture's rows (600..767): read where they were moved to
};
// Both 11 columns wide: the edge and the strip of plain blue right of it, so the inset panel keeps the same distance
// from the edge on both sidebars (seen in testing: on the city's it touched the red edge; columns 7..10 of rows
// 640..699 are the plain blue left of the picture's frame).
constexpr Edge kCityEdge{11, 640, true};   // the city sidebar's red edge (entry 1232)
constexpr Edge kWorldEdge{11, 470, false}; // the world map sidebar's gold beaded edge (entry 1600)

// The picture's frame is not at the same place on every sidebar (measured on the screenshots of two test
// builds; games/zeus/side_panel_layout finds which one it is, shared with the side panel's world map sidebar). The
// frame is placed in the 168 rows at the screen's bottom one row above the middle: (168 - height) / 2 - 1 rows below
// the inset panel -- 4 for Zeus's (the frame where the game has it, 600 -> the bottom) and 10 for both of Poseidon's
// (as the game has the city's); and Poseidon's city picture is moved 1 px right (its left frame line then 1 px right of
// the world map's).
using zeus::side_panel_layout::Picture;
using zeus::side_panel_layout::PictureOf;

int FrameTop(const core::display::Surface16 &s, int x)
{
    return zeus::side_panel_layout::FrameTop(reinterpret_cast<const WORD *>(s.px) + x, s.pitch / 2, s.h);
}
constexpr int kGroupInset = 0x84, kGroupStrip = 0x62; // the 800x600 strip: sidebar_draw's 800 branch (entry 1234)
constexpr int kMarbleEntry = 1232, kMarbleX = 50, kMarbleY = 30, kMarbleW = 136, kMarbleH = 390;
constexpr wchar_t kInterfacePack[] = L"DATA\\Zeus_Interface.sg3"; // next to Zeus.exe
const Site kSidebarDraw{"sidebar_draw", 0x517b10, {0xa1, 0x60, 0xff, 0x0d, 0x01, 0x85, 0xc0, 0x75}};
const Site kSidebarUnits{"sidebar_draw_units", 0x525280, {0x56, 0x57, 0x8b, 0x3d, 0x38, 0x7b, 0xdb, 0x00}};
const Var<unsigned char *> kDrawBuffer{0xf9c100};
const Var<int> kPitch{0xf2a698};
const Var<int> kDdWidth{0xf2a6a0}, kDdHeight{0xf2a69c};
constexpr unsigned char kScreenWorldMap = 0x14;
core::diag::Feature g_filler{"sidebar filler (games/zeus/resolution)"};
Void_fn o_SidebarDraw;
using Units_fn = void(__cdecl *)(int);
Units_fn o_SidebarUnits;

struct Marble {
    bool tried = false, ok = false;
    std::vector<WORD> px; // kMarbleW x kMarbleH, RGB565
};
Marble g_marble;

bool LoadMarble()
{
    if (g_marble.tried) {
        return g_marble.ok;
    }
    g_marble.tried = true;
    wchar_t path[MAX_PATH];
    DWORD len = GetModuleFileNameW(nullptr, path, MAX_PATH);
    while (len > 0 && path[len - 1] != L'\\') {
        --len;
    }
    path[len] = L'\0';
    if (len + sizeof(kInterfacePack) / sizeof(wchar_t) >= MAX_PATH) {
        return false;
    }
    std::wcscat(path, kInterfacePack);
    core::assets::Sg3Pack pack;
    std::vector<WORD> whole;
    int w = 0, h = 0;
    if (!pack.Open(path) || !pack.Size(kMarbleEntry, &w, &h) || w != kSidebarWidth || h != kBackdropH ||
        !pack.Decode565(kMarbleEntry, 0, &whole)) {
        core_log("zeus: resolution: sidebar marble (Zeus_Interface entry %d, %dx%d) not read: the 800x600 strip is "
                 "drawn instead right of the sidebar",
                 kMarbleEntry, w, h);
        return false;
    }
    g_marble.px.resize(static_cast<size_t>(kMarbleW) * kMarbleH);
    for (int y = 0; y < kMarbleH; ++y) {
        std::memcpy(&g_marble.px[static_cast<size_t>(y) * kMarbleW],
                    &whole[static_cast<size_t>(kMarbleY + y) * kSidebarWidth + kMarbleX], kMarbleW * 2);
    }
    g_marble.ok = true;
    core_log("zeus: resolution: sidebar marble read (Zeus_Interface entry %d, in memory only): right of the sidebar "
             "and where the inset panel has no room",
             kMarbleEntry);
    return true;
}

core::display::Surface16 Screen()
{
    const int pitch = *kPitch, w = *kDdWidth, h = *kDdHeight;
    return core::display::Surface16{*kDrawBuffer, pitch, w, h,
                                    pitch > 0 && h > 0 ? static_cast<size_t>(pitch) * static_cast<size_t>(h) : 0};
}

void DrawInsetTile(int k, int x, int y, void *base)
{
    DrawImage(*static_cast<int *>(base) + k, x, y);
}

// Marble (or, without the pack, the 800x600 strip with the game's clip narrowed to `area`) over `area`. The clip is
// put back here, or by the filler's guard (r) after an error.
void Plain(const core::display::Surface16 &s, const RECT &area, core::diag::Restore &r)
{
    if (area.left >= area.right || area.top >= area.bottom) {
        return;
    }
    if (LoadMarble()) {
        core::display::MirrorTile(s, g_marble.px.data(), kMarbleW, kMarbleH, area);
        return;
    }
    const int strip = GroupImage(kGroupStrip);
    int w, h;
    ImageSize(strip, &w, &h);
    if (w <= 0 || h <= 0) {
        return;
    }
    r.Set(kClipLeft.ptr(), static_cast<int>(area.left));
    r.Set(kClipTop.ptr(), static_cast<int>(area.top));
    r.Set(kClipRight.ptr(), static_cast<int>(area.right));
    r.Set(kClipBottom.ptr(), static_cast<int>(area.bottom));
    for (int y = area.top; y < area.bottom; y += h) {
        for (int x = area.left; x < area.right; x += w) {
            DrawImage(strip, x, y);
        }
    }
    r.Undo();
}

// Below 768 on the world map's sidebar laid out by Pericles's side panel (its buttons on the bottom row of the 768
// layout, the picture above them): the gold beaded edge (columns 0..10 of rows 470..529 repeated), the plain row 469
// beside it, the bevel on the right (columns 183..185 of rows 470..529) and an inset panel between them (172 wide), as
// the city's side panel has it below 768.
constexpr int kWorldPlainRow = 469, kWorldInsetW = 172, kWorldBevelX = 183;

void WorldBelow(const core::display::Surface16 &s, int x, int h, core::diag::Restore &r)
{
    const int band = kWorldEdge.bandTop;
    core::display::RepeatRows(s, RECT{x, band, x + kWorldEdge.w, band + 60}, kBackdropH, h);
    core::display::RepeatRows(s, RECT{x + kWorldEdge.w, kWorldPlainRow, x + kWorldBevelX, kWorldPlainRow + 1},
                              kBackdropH, h);
    core::display::RepeatRows(s, RECT{x + kWorldBevelX, band, x + kSidebarWidth, band + 60}, kBackdropH, h);
    const RECT inset{x + kWorldEdge.w, kBackdropH, x + kWorldEdge.w + kWorldInsetW, h};
    int base = GroupImage(kGroupInset);
    if (base < 0) {
        Plain(s, inset, r);
    }
    else {
        core::display::InsetPanel(inset, &DrawInsetTile, &base); // lower than its two edges: the plain row stays
    }
}

void DrawFiller(core::diag::Restore &r, const Edge &edge)
{
    const core::display::Surface16 s = Screen();
    const int w = *kModeWidth, h = *kModeHeight, x = *kSidebarX;
    if (s.w < w || s.h < h || x <= 0 || x + kSidebarWidth > w) {
        return;
    }
    Plain(s, RECT{x + kSidebarWidth, 0, w, h}, r); // right of the sidebar (nothing now: against the edge)
    // Left of the city's sidebar, between the city's clip and the sidebar: drawn by smooth scrolling's picture;
    // marble only when that is not working (else this would show for a frame under the city on every redraw).
    if (&edge == &kCityEdge && !zeus::scroll::Working() && *kCityClipRight > 0 && *kCityClipRight < x) {
        Plain(s, RECT{*kCityClipRight, kTopBar, x, h}, r);
    }
    if (h <= kBackdropH || (&edge == &kCityEdge && zeus::side_panel::Active())) {
        return; // Pericles's side panel lays out its rows below 768 itself (no picture)
    }
    if (&edge == &kWorldEdge && zeus::side_panel::WorldLayout()) {
        WorldBelow(s, x, h, r); // the picture is above the buttons there: not moved
        return;
    }
    const int pictureY = h - kPictureH;
    const Picture pic = PictureOf(FrameTop(s, x));
    const int from = pic.frameTop - ((kPictureH - pic.frameH) / 2 - 1); // 600 on Zeus's and Poseidon's city sidebars
    core::display::CopyRect(s, RECT{x, from, x + kSidebarWidth, kBackdropH}, x, pictureY);
    if (pic.dx > 0) { // the picture right by dx: columns edge .. 182 (the right bevel stays), the edge's plain blue in
        const int top = pictureY + (pic.frameTop - from), bottom = top + pic.frameH; // the frame's rows
        core::display::CopyRect(s, RECT{x + edge.w, top, x + 183 - pic.dx, bottom}, x + edge.w + pic.dx, top);
        core::display::CopyRect(s, RECT{x + edge.w - pic.dx, top, x + edge.w, bottom}, x + edge.w, top);
    }
    // The left edge, from its 60 rows (the city's: the picture's rows 640..699, now at the bottom).
    const int band = edge.bandMoved ? pictureY + (edge.bandTop - from) : edge.bandTop;
    core::display::RepeatRows(s, RECT{x, band, x + edge.w, band + 60}, kPictureTop, pictureY);
    // Below a picture whose frame starts lower (Poseidon's): the edge carried on, the last plain row repeated.
    const int end = pictureY + (kBackdropH - from);
    if (end < h) {
        core::display::RepeatRows(s, RECT{x, band, x + edge.w, band + 60}, end, h);
        core::display::RepeatRows(s, RECT{x + edge.w, end - 1, x + kSidebarWidth, end}, end, h);
    }
    const RECT inset{x + edge.w, kPictureTop, x + kSidebarWidth, pictureY};
    int base = GroupImage(kGroupInset);
    if (base < 0) {
        Plain(s, inset, r);
    }
    else if (!core::display::InsetPanel(inset, &DrawInsetTile, &base)) {
        // Lower than its two edges (32 rows): the panel squeezed, its top edge's first half and its
        // bottom edge's last half, each drawn inside a clip of its half.
        const int mid = kPictureTop + (pictureY - kPictureTop) / 2;
        r.Save(kClipTop.ptr());
        r.Save(kClipBottom.ptr());
        *kClipTop = kPictureTop;
        *kClipBottom = mid;
        core::display::InsetPanel(RECT{inset.left, kPictureTop, inset.right, kPictureTop + 32}, &DrawInsetTile, &base);
        *kClipTop = mid;
        *kClipBottom = pictureY;
        core::display::InsetPanel(RECT{inset.left, pictureY - 32, inset.right, pictureY}, &DrawInsetTile, &base);
        r.Undo();
    }
}

// The world map's sidebar: world_map_sidebars 0x597e50 (no arguments, ret; `call 0x597e60; jmp 0x5980c0`):
// world_map_sidebar_draw 0x597e60 draws its picture (entry 1600, or the Poseidon pack's) down to 768, then 0x5980c0
// the texts and buttons in it (none below row 599: the button table 0x5ef488). Called by the world map (0x14) and the
// screens over it (0x1a, 0x09, 0x03, ...); the campaign map draws the same picture itself (games/zeus/world_map calls
// WorldSidebarFiller after it).
const Site kWorldSidebars{"world_map_sidebars", 0x597e50, {0xe8, 0x0b, 0x00, 0x00, 0x00, 0xe9, 0x66, 0x02}};
Void_fn o_WorldSidebars;

bool Wanted(int w, int h)
{
    return !(StandardSize(w, h) || w < 1024) && (w > kMinW || h > kBackdropH) && core::diag::Enabled(g_filler);
}

void Filler(const Edge &edge)
{
    struct Ctx {
        const Edge *edge;
        core::diag::Restore r;
    } c{&edge, {}};
    if (!core::diag::Guarded(
            g_filler, [](void *p) { DrawFiller(static_cast<Ctx *>(p)->r, *static_cast<Ctx *>(p)->edge); }, &c,
            &c.r)) {
        core_log("zeus: resolution: the space below and right of the sidebar is no longer filled after an error");
    }
}

void __cdecl WorldSidebars_detour()
{
    Original(o_WorldSidebars, kWorldSidebars)();
    if (Wanted(*kModeWidth, *kModeHeight)) {
        Filler(kWorldEdge);
    }
}

bool FillerWanted()
{
    return Wanted(*kModeWidth, *kModeHeight) && *kGameScreen != kScreenWorldMap;
}

void __cdecl SidebarDraw_detour()
{
    zeus::side_panel::BackdropDrawing(true); // its column is taken while the backdrop is fresh
    Original(o_SidebarDraw, kSidebarDraw)();
    zeus::side_panel::BackdropDrawing(false);
    if (FillerWanted()) {
        Filler(kCityEdge);
    }
}

void __cdecl SidebarUnits_detour(int screen)
{
    Original(o_SidebarUnits, kSidebarUnits)(screen);
    zeus::side_panel::ArmyBackdropDrawn(); // its lower half, as the city's
    if (FillerWanted()) {
        Filler(kCityEdge);
    }
}

// The editors' sidebars (Zeus_Interface entries 1752 and 1675, 186x768): the same red edge and plain blue as the city's
// (columns 0..10 of rows 640..699), the picture's frame from row 604 as on the city's, so the city's filler fits.
//   editor_city_sidebar_draw 0x51e6c0 (__thiscall on the editor's city screen object 0xfa0600, no stack arguments,
//      ret; only through its vtable 0x5d9538 + 0x10, called by city_bottom_and_sidebar when editor_mode is 1): blanks
//      sidebar_x + 6 .. the right edge from row 0x18 to the bottom, then draws its picture at (sidebar_x, 0), down to
//      768 only, and its buttons and minimap above row 600;
//   editor_world_map_sidebar 0x5c2800 (no arguments, ret; vtable 0x5e4b58 + 0x10 of the editor's world map, 0x15):
//      its picture at sidebar_x (wider than 800), its buttons, frame_top_panel(1).
const Site kEditorCitySidebar{"editor_city_sidebar_draw", 0x51e6c0, {0xa1, 0x9c, 0xa6, 0xf2, 0x00, 0x83, 0xec, 0x28}};
const Site kEditorWorldSidebar{"editor_world_map_sidebar", 0x5c2800, {0x83, 0xec, 0x08, 0x8d, 0x44, 0x24, 0x04, 0x8d}};
using EditorSidebar_fn = void(__fastcall *)(void *, void *); // __thiscall: this in ecx, edx unused
EditorSidebar_fn o_EditorCitySidebar;
Void_fn o_EditorWorldSidebar;

void __fastcall EditorCitySidebar_detour(void *self, void * /*edx*/)
{
    Original(o_EditorCitySidebar, kEditorCitySidebar)(self, nullptr);
    if (FillerWanted()) {
        Filler(kCityEdge);
    }
}

void __cdecl EditorWorldSidebar_detour()
{
    Original(o_EditorWorldSidebar, kEditorWorldSidebar)();
    if (Wanted(*kModeWidth, *kModeHeight)) {
        Filler(kCityEdge);
    }
}

// ---------------------------------------------------------------------------------------------
// The building information window ([ui] info_window)
//
// info_window_place 0x5715a0 (no arguments, ret): the window at (8, 0x20) and its size, then the game's own place at
// heights 480, 600 and 768 only (768: x 0xbc; y 0x2f8 - rows x 16 while the cursor is above y 0x18d, else 0x20); any
// other height keeps the top left corner. Kinds 0x39 (granary), 0x3a, 0x3d, 0x3e (storehouses) at (0, 0x20), 0x7d at
// (0, 0x2d). Ours: original = the game's rule at any height, moved with the centre of the city area; centered /
// cursor = core::ui::info_window::Position in the city area (0, 30) .. (sidebar_x, height); a window opened on the
// world map (the screen under it 0x14) in the middle, whatever the setting (as Emperor's China map).
// The four kinds the game keeps at (0, 0x20) draw parts at fixed y (the employee
// panel, the granary's and storehouse's list and its buttons), so for them the game keeps its place and the window is
// moved only on screen (core/display/window_move, as Emperor's storage windows): info_fixed_window_draw 0x5b2880
// and info_fixed_window_foreground 0x5b38c0 (no arguments, ret) draw into a copy of the screen, info_window_input
// 0x5726e0 (no arguments, ret) sees the mouse in the game's place, and the window's help / close buttons
// (buttons_draw 0x4316f0, __cdecl x, y, table, count; screen_draw's call returning to 0x514214) are drawn shifted.
// 0x7d's drawing, foreground and input follow the window's place: it is simply placed.
const Site kInfoPlace{"info_window_place", 0x5715a0, {0x56, 0x57, 0xbf, 0x20, 0x00, 0x00, 0x00, 0xbe}};
const Site kFixedDraw{"info_fixed_window_draw", 0x5b2880, {0x51, 0x8b, 0x0d, 0x58, 0xa1, 0xff, 0x00, 0x55}};
const Site kFixedFore{"info_fixed_window_foreground", 0x5b38c0, {0x81, 0xec, 0x40, 0x02, 0x00, 0x00, 0x8b, 0x0d}};
const Site kInfoInput{"info_window_input", 0x5726e0, {0xa0, 0xfc, 0xa6, 0xf2, 0x00, 0x83, 0xec, 0x08}};
const Site kButtonsDraw{"buttons_draw", 0x4316f0, {0x8b, 0x44, 0x24, 0x10, 0x85, 0xc0, 0x0f, 0x8e}};
constexpr DWORD kInfoButtonsCall = 0x514214;
const Var<int> kInfoX{0xffa17c}, kInfoY{0xffa178}, kInfoCols{0xffa174}, kInfoRows{0xffa170};
const Var<int> kInfoKind{0xffa13c};     // 2: a building's window
const Var<int> kInfoBuilding{0xffa158}; // the building
const Var<unsigned char> kInfoPrevScreen{0xffa180};
const Var<int> kMouseX{0xf2a738}, kMouseY{0xf2a734};
constexpr DWORD kBuildingTypes = 0x92e668; // short, every 0x118 bytes
constexpr int kBuildingStride = 0x118;
constexpr unsigned char kScreenInfo = 0x09;
constexpr int kFixedX = 0, kFixedY = 0x20;
core::diag::Feature g_info{"info window position (games/zeus/resolution)"};
Void_fn o_InfoPlace, o_FixedDraw, o_FixedFore, o_InfoInput;
using ButtonsDraw_fn = void(__cdecl *)(int, int, void *, int);
ButtonsDraw_fn o_ButtonsDraw;

using InfoMode = core::ui::info_window::Mode;

struct Shift {
    bool on = false;
    int dx = 0, dy = 0;
};
Shift g_shift;
core::display::WindowMove g_move("zeus: resolution: ");
// The window placed last (on screen) and whether it is still open: a window placed again while open (the trading
// post's and harbour's arrow to the next one, found in testing) keeps its place in cursor mode instead of
// jumping to the cursor, which is on the arrow by then. OnLoop clears it when screen 0x09 is gone.
bool g_infoOpen = false;
POINT g_infoLast{};

bool Shifted()
{
    return g_shift.on && (g_shift.dx || g_shift.dy) && *kGameScreen == kScreenInfo && core::diag::Enabled(g_info);
}

int BuildingType()
{
    return *reinterpret_cast<volatile short *>(kBuildingTypes + g_imageShift + *kInfoBuilding * kBuildingStride);
}

void PlaceInfo(core::diag::Restore &r)
{
    r.Save(kInfoX.ptr()); // the game's own position comes back if this is abandoned
    r.Save(kInfoY.ptr());
    g_shift = Shift{};
    const int w = *kModeWidth, h = *kModeHeight;
    const int type = *kInfoKind == 2 ? BuildingType() : -1;
    const bool fixed = (type == 0x39 || type == 0x3a || type == 0x3d || type == 0x3e) && *kInfoX == kFixedX &&
                       *kInfoY == kFixedY;
    const InfoMode mode = core::ui::info_window::Current();
    if (mode != InfoMode::Original) {
        const bool map = *kInfoPrevScreen == kScreenWorldMap;
        const core::ui::info_window::Area a{0, kTopBar, *kSidebarX, h}; // the city reaches the bottom
        POINT p = core::ui::info_window::Position(map ? InfoMode::Centered : mode, a, *kInfoCols * 16,
                                                  *kInfoRows * 16, *kMouseX, *kMouseY);
        if (mode == InfoMode::Cursor && !map && g_infoOpen) { // the same place, kept inside the area for the new size
            const int ww = *kInfoCols * 16, wh = *kInfoRows * 16;
            p = g_infoLast;
            p.x = p.x + ww > a.right ? a.right - ww : p.x;
            p.y = p.y + wh > a.bottom ? a.bottom - wh : p.y;
            p.x = p.x < a.left ? a.left : p.x;
            p.y = p.y < a.top ? a.top : p.y;
        }
        g_infoLast = p;
        g_infoOpen = true;
        if (fixed) {
            g_shift = Shift{true, static_cast<int>(p.x) - kFixedX, static_cast<int>(p.y) - kFixedY};
        }
        else {
            *kInfoX = p.x;
            *kInfoY = p.y;
        }
    }
    else if (!(w <= 1024 && (h == 480 || h == 600 || h == 768))) { // the game's own sizes keep the game's rule
        constexpr int x768 = 0xbc;                                    // the middle of the city area at 1024
        const int shift = (*kSidebarX - (1024 - kSidebarWidth)) / 2;
        if (h == 768 && *kInfoX == x768) { // the game's 768 rule ran: only the centre of the city area moved
            *kInfoX = x768 + shift;
        }
        else if (*kInfoX == 8 && *kInfoY == 0x20) { // not one of the game's special cases
            const int bottom = h - 8;              // as the game's 768 rule (0x2f8 - rows x 16)
            *kInfoY = *kMouseY < (h - 768) / 2 + 0x18d ? bottom - *kInfoRows * 16 : 0x20;
            *kInfoX = x768 + shift;
        }
    }
    r.Forget(); // the position is the result
}

void __cdecl InfoPlace_detour()
{
    Original(o_InfoPlace, kInfoPlace)();
    if (!core::diag::Enabled(g_info)) {
        return;
    }
    core::diag::Restore r;
    if (!core::diag::Guarded(g_info, [](void *p) { PlaceInfo(*static_cast<core::diag::Restore *>(p)); }, &r, &r)) {
        g_shift = Shift{};
        core_log("zeus: resolution: the info window keeps the game's own position after an error");
    }
}

// A fixed-place window's drawing (0x5b2880, 0x5b38c0): into a copy of the screen, shown where it is placed.
void MovedDraw(Void_fn original)
{
    if (!Shifted()) {
        original();
        return;
    }
    if (core::diag::Guarded(
            g_info,
            [](void *p) {
                const Void_fn original = reinterpret_cast<Void_fn>(p);
                if (!g_move.Begin(kDrawBuffer.ptr(), *kPitch, *kDdWidth, *kDdHeight, *kInfoX, *kInfoY,
                                  *kInfoCols * 16, *kInfoRows * 16, g_shift.dx, g_shift.dy, kMouseX.ptr(),
                                  kMouseY.ptr())) {
                    g_shift = Shift{}; // cannot be shown elsewhere: where the game has it (buttons and input too)
                    original();
                    return;
                }
                original();
                g_move.End();
            },
            reinterpret_cast<void *>(original), &g_move.Changes())) {
        return;
    }
    g_move.Abandon(); // put back by the guard; the window is drawn where the game has it from now on
    g_shift = Shift{};
    original();
}

void __cdecl FixedDraw_detour()
{
    MovedDraw(Original(o_FixedDraw, kFixedDraw));
}

void __cdecl FixedFore_detour()
{
    MovedDraw(Original(o_FixedFore, kFixedFore));
}

// info_window_input 0x5726e0: the mouse in the game's own place of the window. A point outside the shown window lands
// outside the game's window, so nothing hidden can be clicked.
void __cdecl InfoInput_detour()
{
    if (!Shifted()) {
        Original(o_InfoInput, kInfoInput)();
        return;
    }
    core::diag::Restore r;
    if (core::diag::Guarded(
            g_info,
            [](void *p) {
                auto &r = *static_cast<core::diag::Restore *>(p);
                r.Set(kMouseX.ptr(), *kMouseX - g_shift.dx);
                r.Set(kMouseY.ptr(), *kMouseY - g_shift.dy);
                Original(o_InfoInput, kInfoInput)();
                r.Undo();
            },
            &r, &r)) {
        return;
    }
    g_shift = Shift{};
    Original(o_InfoInput, kInfoInput)();
}

// buttons_draw 0x4316f0, for screen_draw's help / close buttons of the window only (by return address): shown with
// the window. Their hover was worked out by the input in the game's own place.
void __cdecl ButtonsDraw_detour(int x, int y, void *buttons, int count)
{
    if (GameAddress(__builtin_return_address(0)) == kInfoButtonsCall && Shifted()) {
        x += g_shift.dx;
        y += g_shift.dy;
    }
    Original(o_ButtonsDraw, kButtonsDraw)(x, y, buttons, count);
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

// The layout's parts, each with its own guard: a part whose sites do not match is left as the game has it (the
// city layout works without them, as with the logical resolution alone).
void AttachCityLayout()
{
    const BYTE *cmp = reinterpret_cast<const BYTE *>(kTopValuesCompare + g_imageShift);
    if (std::memcmp(cmp, kTopValuesBytes, sizeof(kTopValuesBytes)) == 0) {
        const BYTE jl = 0x7c; // jne rel8 -> jl rel8
        if (core::site::PatchCode(kTopValuesCompare + 5 + g_imageShift, &jl, 1)) {
            core_log("zeus: resolution: the top bar values (money, people, date) take the 1024 places at any width of "
                     "1024 or more (0x58ef80 jne -> jl)");
        }
        else {
            core_log("zeus: resolution: the top bar values' compare could not be changed");
        }
    }
    else {
        core_log("zeus: resolution: the top bar values' compare at 0x%lx does not match; left as the game has it",
                 static_cast<unsigned long>(kTopValuesCompare));
    }
    const bool draw = Matches(kImageDraw);
    if (!(draw && Matches(kTopBarDraw) && Matches(kImageGroup) && Matches(kImageSize) &&
          Install(kTopBarDraw, &TopBarDraw_detour, o_TopBarDraw))) {
        core::diag::Disable(g_bars, "its sites do not match or the hook could not be installed");
    }
    const bool fill = Matches(kFillRect) && Install(kFillRect, &FillRect_detour, o_FillRect);
    const bool panel = Matches(kMessagePanel) && Install(kMessagePanel, &MessagePanel_detour, o_MessagePanel);
    if (!fill || !panel) {
        core::diag::Disable(g_message, "its sites do not match or a hook could not be installed");
    }
    // The filler draws the inset panel's tiles with image_draw (image_group_get, image_get_size).
    const bool images = draw && Matches(kImageGroup) && Matches(kImageSize);
    const bool side = images && Matches(kSidebarDraw) && Install(kSidebarDraw, &SidebarDraw_detour, o_SidebarDraw);
    const bool units =
        images && Matches(kSidebarUnits) && Install(kSidebarUnits, &SidebarUnits_detour, o_SidebarUnits);
    const bool world =
        images && Matches(kWorldSidebars) && Install(kWorldSidebars, &WorldSidebars_detour, o_WorldSidebars);
    if (!side || !units || !world) {
        core::diag::Disable(g_filler, "its sites do not match or a hook could not be installed");
    }
    // The editors' sidebars: without them only the editors' sidebars stay as the game draws them (black below 768).
    for (const Site *s : {&kEditorCitySidebar, &kEditorWorldSidebar}) {
        const bool ok = images && Matches(*s) &&
                        (s == &kEditorCitySidebar
                             ? Install(kEditorCitySidebar, &EditorCitySidebar_detour, o_EditorCitySidebar)
                             : Install(kEditorWorldSidebar, &EditorWorldSidebar_detour, o_EditorWorldSidebar));
        if (!ok) {
            core_log("zeus: resolution: %s at 0x%lx not hooked: that editor's sidebar is not filled below 768",
                     s->name, static_cast<unsigned long>(s->address));
        }
    }
    // The info window: its place, and the four fixed-place kinds moved on screen (all or none of these).
    const Site *info[] = {&kInfoPlace, &kFixedDraw, &kFixedFore, &kInfoInput, &kButtonsDraw};
    bool infoOk = true;
    for (const Site *s : info) {
        infoOk = infoOk && Matches(*s);
    }
    if (infoOk) {
        infoOk = Install(kFixedDraw, &FixedDraw_detour, o_FixedDraw) &&
                 Install(kFixedFore, &FixedFore_detour, o_FixedFore) &&
                 Install(kInfoInput, &InfoInput_detour, o_InfoInput) &&
                 Install(kButtonsDraw, &ButtonsDraw_detour, o_ButtonsDraw) &&
                 Install(kInfoPlace, &InfoPlace_detour, o_InfoPlace); // last: nothing is shifted without the rest
    }
    if (!infoOk) {
        core::diag::Disable(g_info, "its sites do not match or a hook could not be installed");
    }
}

} // namespace

void RegisterSettings()
{
    using core::settings::Effect;
    using core::settings::Item;
    using core::settings::Type;
    namespace iw = core::ui::info_window;
    const int i = core::settings::Register(Item{
        "ui", "info_window", "Building info window",
        "Where the window opens when you right-click a building. original: as the game does it (above or below, "
        "moved with the center of the city area); centered: in the middle of the city area; cursor: beside the "
        "cursor, never past the edges. Applies to the next window you open.",
        Type::Choice, "cursor", Effect::Live, true, true, 0, 0, 0, nullptr, iw::kChoices, iw::kChoiceLabels,
        iw::kChoiceCount});
    iw::Watch(i, "zeus: resolution: "); // the choices and the mode are the core's (shared with Yongle)
}

// The campaign map draws its sidebar only while its adventure record is there (0x42b480(..., 1) != 0, 0x421c70):
// the filler moves the picture's rows only after a frame that drew them, never twice. A pixel of the picture's rows
// (x + 93, 700) is set to a value of ours before the drawing; still ours afterwards: not drawn (put back as it was).
constexpr WORD kMark = 0xF81F;
constexpr int kMarkX = 93, kMarkY = 700;

SidebarMark MarkWorldSidebar()
{
    SidebarMark m{false, 0};
    const core::display::Surface16 s = Screen();
    const int x = *kSidebarX + kMarkX, h = *kModeHeight;
    if (!Wanted(*kModeWidth, h) || h <= kBackdropH || !s.px || x < 0 || x >= s.w || kMarkY >= s.h) {
        return m;
    }
    WORD *p = reinterpret_cast<WORD *>(s.px + static_cast<size_t>(kMarkY) * s.pitch) + x;
    m.set = true;
    m.saved = *p;
    *p = kMark;
    return m;
}

void WorldSidebarFiller(const SidebarMark &m)
{
    if (!Wanted(*kModeWidth, *kModeHeight)) {
        return;
    }
    if (m.set) {
        const core::display::Surface16 s = Screen();
        const int x = *kSidebarX + kMarkX;
        WORD *p = x >= 0 && x < s.w && kMarkY < s.h && s.px
                      ? reinterpret_cast<WORD *>(s.px + static_cast<size_t>(kMarkY) * s.pitch) + x
                      : nullptr;
        if (p && *p == kMark) { // the sidebar was not drawn this frame: as it was
            *p = m.saved;
            return;
        }
    }
    Filler(kWorldEdge);
}

void WideTopBar()
{
    if (*kModeWidth > kMinW && core::diag::Enabled(g_bars)) { // off: its sites did not match (Attach) or after an error
        TopBarDraw_detour(0, *kModeWidth); // tiled from the sidebar to the left edge (or nothing after an error)
    }
}

void OnBackground()
{
    switching::EnterFrontEnd(-1);
}

void OnLoop()
{
    switching::OnLoop();
    if (*kGameScreen != kScreenInfo) {
        g_infoOpen = false; // the next window is placed anew
    }
}

namespace {
bool g_installed = false; // Attach succeeded: the game is in our mode 3
} // namespace

// F6 (the game's 800x600) is blocked while our resolution is installed: it would take the game out of our mode 3 (as
// Yongle's). The key bindings (core/input/keys) block it first; this is for when they are off after an error. Also when
// the game posts it itself (lParam 0: the Display options dialog's 800 x 600 button). Shift+F7 cycles our resolution.
bool KeyFilter(UINT message, WPARAM key, LPARAM l)
{
    if (g_installed && message == WM_KEYDOWN && key == VK_F6) {
        static long logs = 0;
        if (++logs <= 5) {
            core_log("zeus: resolution: F6 (800x600) ignored: with our resolutions the game stays in mode 3");
        }
        return true;
    }
    return switching::CycleKey(message, key, l);
}

bool ConfigLoadHooked()
{
    return o_ConfigLoad != nullptr;
}

void SetAfterConfigLoad(void (*after)())
{
    g_afterConfigLoad = after;
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
        {&kTopBarDraw, reinterpret_cast<void *>(&TopBarDraw_detour)},
        {&kFillRect, reinterpret_cast<void *>(&FillRect_detour)},
        {&kMessagePanel, reinterpret_cast<void *>(&MessagePanel_detour)},
        {&kSidebarDraw, reinterpret_cast<void *>(&SidebarDraw_detour)},
        {&kSidebarUnits, reinterpret_cast<void *>(&SidebarUnits_detour)},
        {&kInfoPlace, reinterpret_cast<void *>(&InfoPlace_detour)},
        {&kFixedDraw, reinterpret_cast<void *>(&FixedDraw_detour)},
        {&kFixedFore, reinterpret_cast<void *>(&FixedFore_detour)},
        {&kInfoInput, reinterpret_cast<void *>(&InfoInput_detour)},
        {&kButtonsDraw, reinterpret_cast<void *>(&ButtonsDraw_detour)},
        {&kWorldSidebars, reinterpret_cast<void *>(&WorldSidebars_detour)},
        {&kEditorCitySidebar, reinterpret_cast<void *>(&EditorCitySidebar_detour)},
        {&kEditorWorldSidebar, reinterpret_cast<void *>(&EditorWorldSidebar_detour)},
    };
    *count = static_cast<int>(sizeof(kDetours) / sizeof(kDetours[0]));
    return kDetours;
}

void InstalledForTest(const core_config *cfg)
{
    switching::InstalledForTest(Game(), cfg);
    zeus::zoom::SetRelayout(&RelayoutCity);
    g_modeScreensOk = true; // the help and the goals at a mode change: the test's fakes at their sites
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
    g_modeScreensOk = true;
    for (const Site *s : {&kHelpClose, &kScreenBackground, &kScreenDraw}) {
        if (!Matches(*s)) {
            core_log("zeus: resolution: %s at 0x%lx does not match; the help and the mission goals stay as the game "
                     "leaves them at a mode change",
                     s->name, static_cast<unsigned long>(s->address));
            g_modeScreensOk = false;
        }
    }
    // Before the hooks, as Yongle's: their fallbacks go back to the game's size through it (switching::ToGameSize
    // does nothing more than switch itself off before it is installed).
    switching::Install(Game(), cfg);
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
    AttachCityLayout();
    zeus::zoom::SetRelayout(&RelayoutCity); // zoom: the scale or the map changed
    char what[48];
    core::display::DescribeSetting(cfg->resolution, cfg->resolution_w, cfg->resolution_h, cfg->scale_percent, what,
                                   sizeof(what));
    core_log("zeus: resolution: installed (video.resolution %s; front-end screens at 1024x768; Shift+F7 cycles the "
             "resolution; each part guarded)",
             what);
    g_installed = true;
    return true;
}

} // namespace zeus::resolution
