// The camera half of smooth scrolling in Zeus (scroll.h). The camera is the core's (core/scroll/camera.h); this
// file gives it Zeus's addresses and hooks the two scroll functions. Zeus scrolls with the same code as Emperor
// (read from the decompilation): the same units (view_x columns of tile_width + 2 px, view_y half rows kept even
// by view_clamp), the same step (one column across, 4 half rows down: scroll_y_step_flag 0x63fe70 is 1 and nothing
// writes it) and the same speed formula (scroll_step_due 0x516950).
//
// Failure: the camera's frame runs under its guard (the camera, edge_scrolling and the key flags put back on a fault,
// then the game's own direction); the feature is this file's g_feature. Nothing here draws.
#include "games/zeus/scroll.h"

#include "core/diag/guard.h"
#include "core/log.h"
#include "core/scroll/camera.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace zeus::scroll {

namespace {

namespace camera = core::scroll::camera;

// ---------------------------------------------------------------------------------------------
// Addresses (Zeus.exe 2.1.4.0; calling conventions checked by disassembly)

// scroll_get_direction(): __cdecl, no arguments, returns the direction 0..7 or 8 (none); clears the key flags.
const Site kScrollGetDirection{"scroll_get_direction", 0x50b7b0, {0x51, 0x53, 0x56, 0x57, 0xe8, 0x97, 0xb1, 0x00}};
// view_scroll_step(dir): __cdecl, one argument; 8 does nothing.
const Site kViewScrollStep{"view_scroll_step", 0x51cad0, {0xa1, 0x70, 0xfe, 0x63, 0x00, 0x8b, 0x4c, 0x24}};
// view_clamp(): __cdecl, no arguments (keeps view_x / view_y in the map's range, view_y even). Called, not hooked.
const Site kViewClamp{"view_clamp", 0x51cd60, {0x83, 0xec, 0x08, 0xa1, 0x88, 0x2f, 0xda, 0x00}};
// sound_view_decay(): __cdecl, no arguments (the ambient sound weights, once per scroll step). Called, not hooked.
const Site kSoundViewDecay{"sound_view_decay", 0x42e560, {0xb9, 0x54, 0x74, 0x77, 0x00, 0x8d, 0x51, 0x08}};

// Return addresses of the scroll_get_direction calls whose result goes to view_scroll_step (the city view; each is
// "call 0x50b7b0; push eax; call 0x51cad0"). The two other callers (0x516218, 0x5c1e1e) keep the original behavior.
const DWORD kCityScrollCalls[] = {0x5149b3, 0x514b9d, 0x514d87, 0x514e70, 0x514ea5, 0x515070, 0x516697};
constexpr int kCityScrollCallCount = static_cast<int>(sizeof(kCityScrollCalls) / sizeof(kCityScrollCalls[0]));

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
    // Pericles has no key bindings yet: the arrow key itself (as Yongle's own keys do with its bindings off).
    [](unsigned vk) { return (GetAsyncKeyState(static_cast<int>(vk)) & 0x8000) != 0; },
    // Nothing of ours to reset after a fault: the city is drawn by the game on whole tiles and Zeus has no zoom yet.
    [] {},
    // No city zoom in Zeus yet: never moving, so no zoom step; the speed is never scaled.
    [] { return false; },
    [] {},
    [](double *, double *) { return false; },
};

using GetDirection_fn = int(__cdecl *)();
using ScrollStep_fn = void(__cdecl *)(int);
GetDirection_fn o_GetDirection;
ScrollStep_fn o_ScrollStep;

int __cdecl GetDirection_detour()
{
    if (camera::IsCall(__builtin_return_address(0), kCityScrollCalls, kCityScrollCallCount) &&
        camera::CityDirection(kCamera)) {
        return 8; // view_scroll_step(8) does nothing: the camera has been moved already
    }
    return Original(o_GetDirection, kScrollGetDirection)();
}

// Counting only (the rate log); nothing here can fault, so it is not guarded.
void __cdecl ScrollStep_detour(int dir)
{
    camera::CountStep(kCamera, dir);
    Original(o_ScrollStep, kViewScrollStep)(dir);
}

const Detour kDetours[] = {
    {&kScrollGetDirection, reinterpret_cast<void *>(&GetDirection_detour)},
    {&kViewScrollStep, reinterpret_cast<void *>(&ScrollStep_detour)},
};

bool g_attached = false;

} // namespace

bool Attach(bool smooth)
{
    const Site *sites[] = {&kScrollGetDirection, &kViewScrollStep, &kViewClamp, &kSoundViewDecay};
    for (const Site *s : sites) {
        if (!Matches(*s)) {
            core_log("zeus: %s at 0x%lx does not match; smooth scrolling not installed (the game's own scrolling)",
                     s->name, static_cast<unsigned long>(s->address));
            return false;
        }
    }
    if (!camera::CallsTarget("zeus", kCityScrollCalls, kCityScrollCallCount, kScrollGetDirection)) {
        return false;
    }
    bool ok = Install(kViewScrollStep, &ScrollStep_detour, o_ScrollStep); // counts steps in both modes (rate log)
    ok = Install(kScrollGetDirection, &GetDirection_detour, o_GetDirection) && ok;
    if (!ok) {
        core::diag::Disable(g_feature, "a hook could not be installed");
        core_log("zeus: smooth scrolling not installed (a hook could not be installed): the game's own scrolling");
        return false;
    }
    g_attached = true;
    camera::Start(kCamera, smooth);
    if (smooth) {
        core_log("zeus: smooth scrolling's picture: not yet, the city is drawn on whole tiles (the camera keeps the "
                 "fraction of a tile for the next frames)");
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

const Detour *Detours(int *count)
{
    *count = static_cast<int>(sizeof(kDetours) / sizeof(kDetours[0]));
    return kDetours;
}

void StartForTest(bool smooth)
{
    g_attached = true;
    camera::Start(kCamera, smooth);
}

void Remainder(int *px, int *py)
{
    camera::Remainder(px, py);
}

} // namespace zeus::scroll
