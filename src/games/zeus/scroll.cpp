// Smooth scrolling in Zeus (scroll.h). The camera is the core's (core/scroll/camera.h), the picture too
// (core/scroll/picture.h: the city drawn with the pixel remainder, the conversions moved the same way); this file gives
// them Zeus's addresses and hooks the game's functions. Zeus scrolls, draws and converts with the same code as Emperor
// (read from the decompilation): the same units (view_x columns of tile_width + 2
// px, view_y half rows kept even by view_clamp), the same step (one column across, 4 half rows down:
// scroll_y_step_flag 0x63fe70 is 1 and nothing writes it), the same speed formula (scroll_step_due 0x516950), the same
// viewport and conversions (viewport_init, mouse_to_grid, screen_to_grid line for line) and the same drag selection
// (its structure, the 2 px per column scrolled).
//
// Zeus's drawing: city_draw_tiles 0x458170 covers columns view-6 .. +width+8 and rows view-8 .. +height+21 (and checks
// the view grid's bounds), 0x44ad70 columns -6 .. +width+8, rows -8 .. +height+14, the markers 0x454420 columns 0 ..
// width, rows 0 .. height-1 (as Emperor's 0x46e1b0): the picture's larger buffer and viewport are covered.
// city_render 0x44a830 is hooked by games/zeus/timing (the drawn-frame state), which calls DrawCity here inside it;
// the extra drawing after an abandoned one never counts (timing's DrawCityNotCounting). The figure portrait
// (city_render_2) is drawn without the remainder (PortraitDrawing).
//
// Failure: the camera's frame runs under its guard (the camera, edge_scrolling and the key flags put back on a fault,
// then the game's own direction); the picture's calls under theirs (core/scroll/picture.h); the feature is this file's
// g_feature. Without timing's city_render hook nothing is installed (the game's own scrolling): the conversions would
// otherwise move by a remainder the city is not drawn with.
#include "games/zeus/scroll.h"

#include "core/diag/guard.h"
#include "core/input/keys.h"
#include "core/log.h"
#include "core/scroll/camera.h"
#include "core/scroll/drag.h"
#include "core/scroll/picture.h"
#include "core/scroll/zoom.h"
#include "games/zeus/timing.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace zeus::scroll {

namespace {

namespace camera = core::scroll::camera;
namespace picture = core::scroll::picture;

// ---------------------------------------------------------------------------------------------
// Addresses (Zeus.exe 2.1.4.0; calling conventions checked by disassembly)

// scroll_get_direction(): __cdecl, no arguments, returns the direction 0..7 or 8 (none); clears the key flags.
const Site kScrollGetDirection{"scroll_get_direction", 0x50b7b0, {0x51, 0x53, 0x56, 0x57, 0xe8, 0x97, 0xb1, 0x00}};
// view_scroll_step(dir): __cdecl, one argument; 8 does nothing.
const Site kViewScrollStep{"view_scroll_step", 0x51cad0, {0xa1, 0x70, 0xfe, 0x63, 0x00, 0x8b, 0x4c, 0x24}};
// view_clamp(): __cdecl, no arguments (keeps view_x / view_y in the map's range, view_y even). Called here (through the
// zoom's hook once games/zeus/zoom has installed it, as Yongle's camera does).
const Site kViewClamp{"view_clamp", 0x51cd60, {0x83, 0xec, 0x08, 0xa1, 0x88, 0x2f, 0xda, 0x00}};
// sound_view_decay(): __cdecl, no arguments (the ambient sound weights, once per scroll step). Called, not hooked.
const Site kSoundViewDecay{"sound_view_decay", 0x42e560, {0xb9, 0x54, 0x74, 0x77, 0x00, 0x8d, 0x51, 0x08}};
// The picture's: screen_to_grid(x, y, unused) __cdecl, the callers push 3 (the
// third is not read); mouse_to_grid() __cdecl, no arguments; screen_to_grid_unbounded(x, y, unused) __cdecl 3;
// grid_to_screen(gx, gy, int *x, int *y) __cdecl 4; city_view_width_px() __cdecl, no arguments. All ret.
const Site kScreenToGrid{"screen_to_grid", 0x44a460, {0x51, 0x53, 0x8b, 0x5c, 0x24, 0x0c, 0x55, 0x56}};
const Site kMouseToGrid{"mouse_to_grid", 0x44a270, {0x83, 0xec, 0x0c, 0x53, 0x8b, 0x1d, 0x38, 0xa7}};
const Site kScreenToGridRaw{"screen_to_grid_unbounded", 0x433590, {0x51, 0x8b, 0x4c, 0x24, 0x0c, 0x53, 0x55, 0x56}};
const Site kGridToScreen{"grid_to_screen", 0x56bf20, {0xa1, 0xcc, 0x3b, 0x7d, 0x00, 0x83, 0xec, 0x08}};
const Site kCityWidth{"city_view_width_px", 0x44a7c0, {0xa1, 0xb0, 0x3b, 0x7d, 0x00, 0x83, 0xc0, 0x02}};
// The drag selection: __thiscall on the drag structure 0x7818b0 (ecx), no stack arguments, ret (taken as
// __fastcall(int *)).
const Site kDragStart{"drag_select_start", 0x433380, {0x8b, 0x15, 0x74, 0x2f, 0xda, 0x00, 0x83, 0xec}};
const Site kDragDraw{"drag_select_draw", 0x433400, {0x83, 0xec, 0x0c, 0x8b, 0x15, 0x74, 0x2f, 0xda}};
const Site kDragFinish{"drag_select_finish", 0x433700, {0x83, 0xec, 0x20, 0xa0, 0xd9, 0xa6, 0xf2, 0x00}};
// line_draw(x1, y1, x2, y2, colour): __cdecl, ret (the caller pops 5). Its pixels are dropped
// at dd_width / dd_height (0x5096d0), as Emperor's: the drag selection's frame, zoomed, is worked out in
// buffer pixels and drawn on the screen (the core's LineDraw).
const Site kLineDraw{"line_draw", 0x509700, {0x8b, 0x44, 0x24, 0x10, 0x8b, 0x4c, 0x24, 0x08}};

// Return addresses of the scroll_get_direction calls whose result goes to view_scroll_step (the city view; each is
// "call 0x50b7b0; push eax; call 0x51cad0"). The two other callers (0x516218, 0x5c1e1e) keep the original behavior.
const DWORD kCityScrollCalls[] = {0x5149b3, 0x514b9d, 0x514d87, 0x514e70, 0x514ea5, 0x515070, 0x516697};
constexpr int kCityScrollCallCount = static_cast<int>(sizeof(kCityScrollCalls) / sizeof(kCityScrollCalls[0]));

// Return addresses of grid_to_screen's calls for the drag frame's corner in drag_select_start and drag_select_draw
// (the corner less the tile's height x 30).
constexpr DWORD kDragStartCorner = 0x4333a9;
constexpr DWORD kDragDrawCorner = 0x433425;

const Var<int> kViewX{0xda2f64};
const Var<int> kViewY{0xda2f68};
const Var<int> kTileWidth{0x7d3bb0};
const Var<int> kTileHalfHeight{0x7d3ba4};
const Var<int> kCursorX{0xf2a6c8}; // cursor_screen_x (the raw cursor the game's edge test reads)
const Var<int> kCursorY{0xf2a6c4};
const Var<int> kDesktopW{0xf2a6bc}; // desktop_width
const Var<int> kDesktopH{0xf2a6b8};
const Var<int> kKeyLeft{0xf9c118};  // set by game_wndproc on VK_LEFT (0x5c4fbb), cleared by scroll_get_direction
const Var<int> kKeyRight{0xf9c114};
const Var<int> kKeyUp{0xf9c120};
const Var<int> kKeyDown{0xf9c11c};
const Var<int> kEdgeScrolling{0xf2a6d0}; // edge_scrolling: no simulation tick while set (sim_tick_due)
const Var<int> kScrollSpeed{0xf2a778};   // 0..100
const Var<int> kFrameCount{0xf2aac0};    // +1 on every game_frame
const Var<int> kOriginX{0x7d3be8};       // viewport_init's variables (the same order as Emperor's)
const Var<int> kOriginY{0x7d3be4};
const Var<int> kWidthTiles{0x7d3bbc};
const Var<int> kHeightRows{0x7d3bb8};
const Var<int> kClipLeft{0x7d3b94};      // city clip (city_render copies it to the drawing clip)
const Var<int> kClipTop{0x7d3b8c};
const Var<int> kClipRight{0x7d3b90};     // exclusive
const Var<int> kClipBottom{0x7d3b88};    // exclusive
const Var<BYTE *> kDrawBuffer{0xf9c100}; // draw_buffer
const Var<int> kMouseX{0xf2a738};        // the game's mouse (mouse_to_grid reads these)
const Var<int> kMouseY{0xf2a734};
const Var<int> kPitch{0xf2a698};         // dd_pitch: bytes per row
const Var<int> kDdWidth{0xf2a6a0};
const Var<int> kDdHeight{0xf2a69c};
const Var<unsigned char> kMouseMode{0xf2a6d9}; // 0x4d during a military drag selection (drag_select_start)
constexpr unsigned char kMouseModeDragSelect = 0x4d;
const Var<int> kCursorTile{0xda2f6c};             // the cursor's tile index
const Var<signed char> kTileHeights{0x8e01fa};    // 0x8e01f8 + 2: 228 x 228 (cleared by 0x45dc90)
constexpr int kMapTiles = 228 * 228;
constexpr int kHeightPx = 30; // drag_select_start: the corner less the tile's height x 30 (0x4333b2..)

core::diag::Feature g_feature{"smooth scrolling (games/zeus/scroll)"};

using VoidFn = void(__cdecl *)();

// The camera's view of Zeus (core/scroll/camera.h).
const camera::CameraGame kCamera{
    "zeus",
    &g_feature,
    camera::CameraVars{kViewX, kViewY, kTileWidth, kTileHalfHeight, kCursorX, kCursorY, kDesktopW, kDesktopH,
                       kKeyLeft, kKeyRight, kKeyUp, kKeyDown, kEdgeScrolling, kScrollSpeed, kFrameCount},
    [] { Function<VoidFn>(kViewClamp)(); },
    [] { Function<VoidFn>(kSoundViewDecay)(); },
    // A key bound to this direction (W, say); with the bindings off, the arrow key itself.
    [](unsigned vk) { return core::input::keys::Held(vk); },
    // After a fault: the picture's state back to rest, and the zoom back to 100% first (the game then draws straight
    // into the back buffer, which a zoomed view would overrun).
    [] {
        picture::Reset();
        core::scroll::zoom::Off("smooth scrolling was switched off after an error");
    },
    // The city's zoom (core/scroll/zoom.h).
    &core::scroll::zoom::CameraMoving,
    &core::scroll::zoom::CameraStep,
    &core::scroll::zoom::CameraScale,
};

// The picture's view of Zeus (core/scroll/picture.h). The zoom's callbacks are the core's (core/scroll/zoom.h).
const picture::PictureGame kPicture{
    "zeus",
    &kCamera,
    picture::PictureVars{kOriginX, kOriginY, kWidthTiles, kHeightRows, kClipLeft, kClipTop, kClipRight, kClipBottom,
                         kDrawBuffer, kMouseX, kMouseY, kPitch, kDdWidth, kDdHeight, kMouseMode, kCursorTile,
                         kTileHeights},
    kMouseModeDragSelect,
    kHeightPx,
    kMapTiles,
    kDragStartCorner,
    kDragDrawCorner,
    &zeus::timing::DrawCityOriginal,
    &zeus::timing::DrawCityNotCounting, // the extra drawing after an abandoned one never counts
    &core::scroll::zoom::Percent,
    &core::scroll::zoom::BufferSize,
    &core::scroll::zoom::RenderZoomed,
    &core::scroll::zoom::DragDrawZoom,
    &core::scroll::zoom::DragLinesMap,
};

using GetDirection_fn = int(__cdecl *)();
using ScrollStep_fn = void(__cdecl *)(int);
using ScreenToGrid_fn = int(__cdecl *)(int, int, int);
using MouseToGrid_fn = int(__cdecl *)();
using GridToScreen_fn = void(__cdecl *)(int, int, int *, int *);
using CityWidth_fn = int(__cdecl *)();
using Drag_fn = void(__fastcall *)(int *);
using Line_fn = void(__cdecl *)(int, int, int, int, int);
GetDirection_fn o_GetDirection;
ScrollStep_fn o_ScrollStep;
ScreenToGrid_fn o_ScreenToGrid, o_ScreenToGridRaw;
MouseToGrid_fn o_MouseToGrid;
GridToScreen_fn o_GridToScreen;
CityWidth_fn o_CityWidth;
Drag_fn o_DragStart, o_DragDraw, o_DragFinish;
Line_fn o_Line;

int __cdecl GetDirection_detour()
{
    if (camera::IsCall(__builtin_return_address(0), kCityScrollCalls, kCityScrollCallCount)) {
        if (camera::CityDirection(kCamera)) {
            return 8; // view_scroll_step(8) does nothing: the camera has been moved already
        }
        // The game's own steps, its edge test through gameplay.edge_scroll_width.
        return camera::GameDirection(kCamera, [] { return Original(o_GetDirection, kScrollGetDirection)(); });
    }
    return Original(o_GetDirection, kScrollGetDirection)();
}

// Counting only (the rate log); nothing here can fault, so it is not guarded.
void __cdecl ScrollStep_detour(int dir)
{
    camera::CountStep(kCamera, dir);
    Original(o_ScrollStep, kViewScrollStep)(dir);
}

int __cdecl ScreenToGrid_detour(int x, int y, int unused)
{
    return picture::ScreenToGrid(kPicture, x, y, unused, [](int x, int y, int unused) {
        return Original(o_ScreenToGrid, kScreenToGrid)(x, y, unused);
    });
}

int __cdecl MouseToGrid_detour()
{
    return picture::MouseToGrid(kPicture, 0, [](int) { return Original(o_MouseToGrid, kMouseToGrid)(); });
}

int __cdecl ScreenToGridRaw_detour(int x, int y, int unused)
{
    return picture::ScreenToGridRaw(kPicture, x, y, unused, [](int x, int y, int unused) {
        return Original(o_ScreenToGridRaw, kScreenToGridRaw)(x, y, unused);
    });
}

void __cdecl GridToScreen_detour(int gx, int gy, int *sx, int *sy)
{
    picture::GridToScreen(kPicture, GameAddress(__builtin_return_address(0)), gx, gy, sx, sy,
                          [](int gx, int gy, int *sx, int *sy) {
                              Original(o_GridToScreen, kGridToScreen)(gx, gy, sx, sy);
                          });
}

// The "Game paused" banner (its only caller 0x58f5a8: x = (width - 0x1c0) / 2) centered on the city as shown.
int __cdecl CityWidth_detour()
{
    return picture::CityWidth(kPicture, [] { return Original(o_CityWidth, kCityWidth)(); });
}

void __fastcall DragStart_detour(int *drag)
{
    picture::DragStart(kPicture, drag, [](int *drag) { Original(o_DragStart, kDragStart)(drag); });
}

void __fastcall DragDraw_detour(int *drag)
{
    picture::DragAdjusted(kPicture, drag, [](int *drag) { Original(o_DragDraw, kDragDraw)(drag); }, true);
}

void __fastcall DragFinish_detour(int *drag)
{
    picture::DragAdjusted(kPicture, drag, [](int *drag) { Original(o_DragFinish, kDragFinish)(drag); }, false);
}

// Zoom: the drag selection's frame, zoomed, drawn on the screen (the core's LineDraw).
void __cdecl Line_detour(int x1, int y1, int x2, int y2, int colour)
{
    core::scroll::zoom::LineDraw(x1, y1, x2, y2, colour, [](int x1, int y1, int x2, int y2, int colour) {
        Original(o_Line, kLineDraw)(x1, y1, x2, y2, colour);
    });
}

const Detour kDetours[] = {
    {&kScrollGetDirection, reinterpret_cast<void *>(&GetDirection_detour)},
    {&kViewScrollStep, reinterpret_cast<void *>(&ScrollStep_detour)},
    {&kScreenToGrid, reinterpret_cast<void *>(&ScreenToGrid_detour)},
    {&kMouseToGrid, reinterpret_cast<void *>(&MouseToGrid_detour)},
    {&kScreenToGridRaw, reinterpret_cast<void *>(&ScreenToGridRaw_detour)},
    {&kGridToScreen, reinterpret_cast<void *>(&GridToScreen_detour)},
    {&kCityWidth, reinterpret_cast<void *>(&CityWidth_detour)},
    {&kDragStart, reinterpret_cast<void *>(&DragStart_detour)},
    {&kDragDraw, reinterpret_cast<void *>(&DragDraw_detour)},
    {&kDragFinish, reinterpret_cast<void *>(&DragFinish_detour)},
    {&kLineDraw, reinterpret_cast<void *>(&Line_detour)},
};

bool g_attached = false;

} // namespace

bool Attach(bool smooth)
{
    const Site *sites[] = {&kScrollGetDirection, &kViewScrollStep, &kViewClamp,   &kSoundViewDecay, &kScreenToGrid,
                           &kMouseToGrid,        &kScreenToGridRaw, &kGridToScreen, &kCityWidth,      &kDragStart,
                           &kDragDraw,           &kDragFinish,      &kLineDraw};
    for (const Site *s : sites) {
        if (!Matches(*s)) {
            core_log("zeus: %s at 0x%lx does not match; smooth scrolling not installed (the game's own scrolling)",
                     s->name, static_cast<unsigned long>(s->address));
            return false;
        }
    }
    if (!zeus::timing::CityDrawingHooked()) {
        core_log("zeus: city_render is not hooked (frame and timing hooks); smooth scrolling not installed (the "
                 "game's own scrolling)");
        return false;
    }
    if (!camera::CallsTarget("zeus", kCityScrollCalls, kCityScrollCallCount, kScrollGetDirection)) {
        return false;
    }
    bool ok = Install(kViewScrollStep, &ScrollStep_detour, o_ScrollStep); // counts steps in both modes (rate log)
    ok = Install(kScreenToGrid, &ScreenToGrid_detour, o_ScreenToGrid) && ok;
    ok = Install(kMouseToGrid, &MouseToGrid_detour, o_MouseToGrid) && ok;
    ok = Install(kScreenToGridRaw, &ScreenToGridRaw_detour, o_ScreenToGridRaw) && ok;
    ok = Install(kGridToScreen, &GridToScreen_detour, o_GridToScreen) && ok;
    ok = Install(kCityWidth, &CityWidth_detour, o_CityWidth) && ok;
    ok = Install(kDragStart, &DragStart_detour, o_DragStart) && ok;
    ok = Install(kDragDraw, &DragDraw_detour, o_DragDraw) && ok;
    ok = Install(kDragFinish, &DragFinish_detour, o_DragFinish) && ok;
    ok = Install(kLineDraw, &Line_detour, o_Line) && ok;
    ok = Install(kScrollGetDirection, &GetDirection_detour, o_GetDirection) && ok;
    if (!ok) {
        // The hooks that are in pass the calls through once the feature is off (remainder 0, no extent use).
        core::diag::Disable(g_feature, "a hook could not be installed");
        core_log("zeus: smooth scrolling not installed (a hook could not be installed): the game's own scrolling");
        return false;
    }
    g_attached = true;
    camera::Start(kCamera, smooth);
    if (smooth) {
        picture::LogInstalled(kPicture);
    }
    return true;
}

bool SetSmooth(bool smooth)
{
    return camera::SetSmooth(kCamera, smooth);
}

void OnFrame(double ms)
{
    if (g_attached) {
        camera::OnLoop(kCamera, ms);
    }
}

void Resume()
{
    camera::Resume();
}

void TakeStats(char *out, std::size_t size)
{
    if (!g_attached) {
        out[0] = 0;
        return;
    }
    camera::TakeStats(kCamera, out, size);
}

bool Working()
{
    return g_attached && core::diag::Enabled(g_feature);
}

void DrawCity(int x, int y, int z)
{
    if (g_attached) {
        picture::CityRender(kPicture, x, y, z);
    }
    else {
        zeus::timing::DrawCityOriginal(x, y, z);
    }
}

void PortraitDrawing(core::diag::Restore &r)
{
    if (g_attached) {
        picture::PictureDrawing(r, *kDdWidth, *kDdHeight); // the conversions it makes: the game's own
    }
}

void SetCityExtent(int right, int bottom)
{
    picture::SetCityExtent(right, bottom);
}

bool DrawnCity(int *left, int *top, int *right, int *bottom, const char **why)
{
    return picture::DrawnCity(kPicture, left, top, right, bottom, why);
}

void SaveCityBuffer(int report)
{
    picture::SaveCityBuffer(report);
}

const Detour *Detours(int *count)
{
    *count = static_cast<int>(sizeof(kDetours) / sizeof(kDetours[0]));
    return kDetours;
}

const picture::PictureGame &Picture()
{
    return kPicture;
}

void StartForTest(bool smooth)
{
    g_attached = true;
    camera::Start(kCamera, smooth);
}

namespace {

const Var<unsigned char> kGameScreen{0xf2a6dd};

// Drag scrolling: the city screens of the zoom (0x01, 0x20, 0x48) and the editors' city
// too (the adventure editor edits a city on 0x01 with editor_mode 1; 0x58 is the editor's city
// screen). Both reach the hooked scroll_get_direction call (0x514e6b, 0x514d82; jump table 0x51670c
// of screen_handle_input).
bool DragScreen()
{
    const unsigned char s = *kGameScreen;
    return s == 0x01 || s == 0x20 || s == 0x48 || s == 0x58;
}

const core::scroll::drag::DragGame kDrag{
    "zeus",
    &DragScreen,
    [](int x, int y) { return picture::OnCity(kPicture, x, y); },
    [] { return *kMouseMode == kMouseModeDragSelect; },
    &Working,
    &core::scroll::zoom::Mouse,
};

} // namespace

void AttachDrag()
{
    core::scroll::drag::Install(kDrag, core::scroll::drag::RealSystem());
}

void SetRemainderForTest(int px, int py)
{
    camera::SetRemainderForTest(kCamera, px, py);
}

void Remainder(int *px, int *py)
{
    camera::Remainder(px, py);
}

} // namespace zeus::scroll
