// The city's zoom in Zeus (zoom.h). The zoom is the core's (core/scroll/zoom.h, shared with Emperor's layer);
// this file gives it Zeus's addresses and hooks view_clamp.
//
// Evidence (Zeus.exe 2.1.4.0, SHA256 382bcd28...0a57): view_clamp 0x51cd60 decompiled: the map's
// range 0xfa05b4 / b0 / ac / a8 (as Emperor's order), the bottom margin 0x8f9820 + (0 for direction 0, 1 for 2, 2
// for 4, 3 for any other; the direction 0xda2f88); its bottom limit is the map's minus (margin + 4), also when the
// margin is 0 (Emperor's only when it is not 0).
#include "games/zeus/zoom.h"

#include "core/log.h"
#include "core/scroll/zoom.h"
#include "games/zeus/scroll.h"
#include "games/zeus/site.h"

namespace zeus::zoom {
namespace {

namespace czoom = core::scroll::zoom;

// view_clamp(): __cdecl, no arguments, ret; keeps view_x / view_y within the map's range
// minus the view's size, view_y even.
const Site kViewClamp{"view_clamp", 0x51cd60, {0x83, 0xec, 0x08, 0xa1, 0x88, 0x2f, 0xda, 0x00}};
// build_cost_text_draw(): __cdecl, no arguments, ret (its only caller is screen_draw
// 0x5140c3, after the city): the building preview's cost, at grid_to_screen's position, clipped to the city clip
// 0x7d3b88..94. Seen in testing: cut off below 100% (the clip is the zoomed buffer's).
const Site kCostText{"build_cost_text_draw", 0x4542b0, {0xa1, 0xa8, 0x2f, 0xda, 0x00, 0x83, 0xec, 0x08}};
// save_load(file, ?, ?, ?): __cdecl, 4 arguments (both callers push 4 and pop), ret: reads a .sav's chunks; called by
// save_game_load 0x50e610 (saved games, /sg=) and campaign_mission_load 0x511c90 (a mission of an adventure).
const Site kSaveLoad{"save_load", 0x50d6e0, {0x83, 0xec, 0x3c, 0xc7, 0x05, 0x40, 0xd9, 0xf9}};
const Var<int> kMapColMin{0xfa05b4};
const Var<int> kMapColMax{0xfa05b0};
const Var<int> kMapRowMin{0xfa05ac};
const Var<int> kMapRowMax{0xfa05a8};
const Var<unsigned char> kGameScreen{0xf2a6dd};
const Var<char> kRedraw{0xf2a6ce}; // screen_redraw: the city screens' background is the bottom strip and the sidebar
const Var<unsigned char> kBottomMargin{0x8f9820}; // 4 bytes, by direction
const Var<int> kDirection{0xda2f88};              // 0, 2, 4, 6
// view_drawn_x / view_drawn_y: the view as city_render last drew it (mouse_to_grid's lookup); viewport_init sets them
// to the map's middle.
const Var<int> kViewDrawnX{0x7d3bd0};
const Var<int> kViewDrawnY{0x7d3bcc};
const Var<int> kSidebarX{0x10dff34};
const Var<int> kEditorMode{0x665e2c}; // editor_mode: 1 in the editors
constexpr int kZoomCap = 150; // as Yongle (the speed at 150% was checked in testing, at 2560x1440)

using Void_fn = void(__cdecl *)();
using SaveLoad_fn = int(__cdecl *)(int, int, int, int);
Void_fn o_ViewClamp, o_CostText;
SaveLoad_fn o_SaveLoad;

void (*g_relayout)() = nullptr;
int g_loggedMargin = -1;

int MapCols()
{
    return *kMapColMax - *kMapColMin;
}

int MapRows()
{
    return *kMapRowMax - *kMapRowMin;
}

int MarginIndex()
{
    const int d = *kDirection;
    return d == 0 ? 0 : d == 2 ? 1 : d == 4 ? 2 : 3;
}

int Margin()
{
    return kBottomMargin.ptr()[MarginIndex()];
}

// The half rows view_clamp can hold: Zeus always keeps the margin + 4 half rows below the screen.
int ClampRows()
{
    return MapRows() - (Margin() + 4);
}

void Clamp()
{
    Original(o_ViewClamp, kViewClamp)();
}

void MapNow(int *cols, int *rows, int *clampRows)
{
    *cols = MapCols();
    *rows = MapRows();
    *clampRows = ClampRows();
}

// The city screens: the city, the city with a company selected (0x48) and a city variant
// not seen yet (0x20). Not the sidebar panels' dialogs (0x07), the information window (0x09), the city messages (0x05)
// or the build submenu (0x06): there the wheel is the game's. Not in the editors either: the adventure editor edits a
// city on screen 0x01 at our resolution, where the editor has the wheel as always.
bool CityScreen()
{
    const unsigned char s = *kGameScreen;
    return *kEditorMode == 0 && (s == 0x01 || s == 0x20 || s == 0x48);
}

void Relayout()
{
    if (g_relayout) {
        g_relayout();
    }
}

// Before every view_clamp (the camera's too): the core lays the view out again first when the map, the bottom margin
// or the view's fit changed.
void __cdecl ViewClamp_detour()
{
    const int m = Margin() | MarginIndex() << 8;
    if (czoom::Active() && m != g_loggedMargin) { // while the zoom works (as before the move)
        g_loggedMargin = m;
        core_log("zoom: the bottom margin for direction %d is %d half row(s) (+4: view_clamp keeps them below the "
                 "screen)",
                 *kDirection, m & 0xff);
    }
    czoom::OnViewClamp();
    Clamp();
}

// The building preview's cost text in the city as shown (the core's ShownClipCall).
void __cdecl CostText_detour()
{
    czoom::ShownClipCall([] { Original(o_CostText, kCostText)(); });
}

// A saved game or a mission loaded: the starting zoom again.
int __cdecl SaveLoad_detour(int a, int b, int c, int d)
{
    const int r = Original(o_SaveLoad, kSaveLoad)(a, b, c, d);
    czoom::OnGameLoaded();
    return r;
}

const czoom::ZoomGame &Game()
{
    static const czoom::ZoomGame kZoom{
        "zeus",      &zeus::scroll::Picture(), kViewDrawnX, kViewDrawnY, kRedraw, kSidebarX, kZoomCap, &MapNow,
        &CityScreen, &Relayout,                &Clamp,      &zeus::scroll::Working,
    };
    return kZoom;
}

} // namespace

void RegisterSettings()
{
    czoom::RegisterSettings();
}

bool Attach()
{
    if (!Matches(kViewClamp)) {
        core_log("zoom: %s does not match: the city stays at 100%%", kViewClamp.name);
        return false;
    }
    if (!Install(kViewClamp, &ViewClamp_detour, o_ViewClamp)) {
        core_log("zoom: %s could not be hooked: the city stays at 100%%", kViewClamp.name);
        return false;
    }
    czoom::Install(Game());
    // Not needed for the zoom to work (logged when missing): the cost text cut off below 100%, the zoom kept into
    // the next game.
    if (!Matches(kCostText) || !Install(kCostText, &CostText_detour, o_CostText)) {
        core_log("zoom: %s not hooked: the build cost text is clipped to the zoomed buffer", kCostText.name);
    }
    if (!Matches(kSaveLoad) || !Install(kSaveLoad, &SaveLoad_detour, o_SaveLoad)) {
        core_log("zoom: %s not hooked: a loaded game keeps the zoom it had", kSaveLoad.name);
    }
    return true;
}

void MapRange(int *cols, int *rows)
{
    *cols = MapCols();
    *rows = MapRows();
}

void SetRelayout(void (*fn)())
{
    g_relayout = fn;
}

void InstallForTest()
{
    czoom::Install(Game());
}

const Detour *Detours(int *count)
{
    static const Detour kDetours[] = {{&kViewClamp, reinterpret_cast<void *>(&ViewClamp_detour)},
                                      {&kCostText, reinterpret_cast<void *>(&CostText_detour)},
                                      {&kSaveLoad, reinterpret_cast<void *>(&SaveLoad_detour)}};
    *count = 3;
    return kDetours;
}

} // namespace zeus::zoom
