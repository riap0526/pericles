// The world map in Zeus (world_map.h).
//
// anim_advance_ui 0x599460 (__cdecl image id, frame; returns the new frame; ret; called only by the world map's
// drawing 0x599da0, at 0x59a90d and 0x59ab2f): a picture of animation speed 0 (the allies' flags) steps whenever
// anim_due[0] (0x6668ec) is set, and anim_timers_update sets it on every game_frame. So the flags moved 20 steps a
// second at the original's 20 frames and look still at 200. As in Emperor's china_map.cpp, speed 0 steps
// at most every 50 ms (core/sim/ui_anim.h): on the other frames anim_due[0] is 0 for the call (other speeds keep
// their own timers).
//
// world_map_draw 0x599da0 (no arguments; ret; called for screens 0x14, 0x34 and the editor's map): the map picture
// (838 x 738 at (0, 30)) is drawn into a box min(sidebar_x + 16, map width) wide but dd_height - 30 high, never cut to
// the map's height (0x599e07..0x599e20; the width is, at 0x599e01). At the game's own 768 that is 738, the map's own
// height; at 1440 it is 1410 and the drawing reads past the picture's memory (seen in testing: a crash at 0x413e00 on the
// first world map frame). The same height also ends the clip of the map's objects (0x599f02). So while the map is
// drawn the game sees its own 768 (dd_height 0xf2a69c, put back right after); the map stays at the top left with the
// rest of the screen as it was, until the map's own layout.
//
// Failure: the flags: the game's own stepping (fast again). The map's height cannot fail by itself (two writes of our
// own); it is off with the rest of the map when its site does not match, and then the map is not drawn above 768
// either way (the logical resolution falls back to the game's 1024x768: see Attach).
#include "games/zeus/world_map.h"

#include "core/diag/guard.h"
#include "core/log.h"
#include "core/sim/ui_anim.h"
#include "games/zeus/timing.h"

#include <mmsystem.h>

namespace zeus::world_map {

namespace {

const Site kAnim{"anim_advance_ui", 0x599460, {0x53, 0x8b, 0x5c, 0x24, 0x0c, 0x56, 0x57, 0x33}};
const Var<unsigned char> kAnimDue0{0x6668ec}; // anim_due[0]

using Anim_fn = unsigned(__cdecl *)(int, unsigned);
Anim_fn o_Anim;

core::diag::Feature g_flags{"world map flags at the original's pace (games/zeus/world_map)"};
core::sim::Speed0Gate g_gate;

struct AnimCall {
    int id;
    unsigned frame, result;
    core::diag::Restore r;
};

unsigned __cdecl Anim_detour(int id, unsigned frame)
{
    if (!core::diag::Enabled(g_flags) || g_gate.Step(zeus::timing::FrameNumber(), timeGetTime())) {
        return Original(o_Anim, kAnim)(id, frame);
    }
    AnimCall c{id, frame, 0, {}};
    if (core::diag::Guarded(
            g_flags,
            [](void *p) {
                AnimCall &c = *static_cast<AnimCall *>(p);
                c.r.Set(kAnimDue0.ptr(), static_cast<unsigned char>(0)); // speed 0 waits this frame
                c.result = Original(o_Anim, kAnim)(c.id, c.frame);
                c.r.Undo();
            },
            &c, &c.r)) {
        return c.result;
    }
    return Original(o_Anim, kAnim)(id, frame); // anim_due[0] is back: the game's own step
}

const Site kMapDraw{"world_map_draw", 0x599da0, {0x83, 0xec, 0x54, 0xe8, 0x58, 0xfc, 0xf6, 0xff}};
const Var<int> kDdHeight{0xf2a69c};
constexpr int kGameHeight = 768; // the game's largest height: the map box is then the map's own 738

using Void_fn = void(__cdecl *)();
Void_fn o_MapDraw;
bool g_heightLogged = false;

// Not guarded: two plain writes of a value of our own. A fault inside the game's drawing is the game's (as without
// this hook; the height would then stay 768, and the game is ending anyway).
void __cdecl MapDraw_detour()
{
    const int h = *kDdHeight;
    if (h <= kGameHeight) {
        Original(o_MapDraw, kMapDraw)();
        return;
    }
    if (!g_heightLogged) {
        g_heightLogged = true;
        core_log("zeus: world map: drawn with the game's own height %d (the screen is %d high; the map stays at the "
                 "top left)",
                 kGameHeight, h);
    }
    *kDdHeight = kGameHeight;
    Original(o_MapDraw, kMapDraw)();
    *kDdHeight = h;
}

} // namespace

bool Attach()
{
    if (!Matches(kAnim) || !Install(kAnim, &Anim_detour, o_Anim)) {
        core::diag::Disable(g_flags, "anim_advance_ui does not match or could not be hooked");
        core_log("zeus: world map: %s not hooked: the flags step on every frame (the game's own way)", kAnim.name);
    }
    else {
        core_log("zeus: world map: the flags (animation speed 0) step at most every %u ms, as at the original's 20 "
                 "frames a second",
                 core::sim::Speed0Gate::kSpeed0Ms);
    }
    if (!Matches(kMapDraw) || !Install(kMapDraw, &MapDraw_detour, o_MapDraw)) {
        core_log("zeus: world map: %s not hooked: the map would read past its picture above 768 rows", kMapDraw.name);
        return false;
    }
    return true;
}

const Detour *Detours(int *count)
{
    static const Detour kDetours[] = {
        {&kAnim, reinterpret_cast<void *>(&Anim_detour)},
        {&kMapDraw, reinterpret_cast<void *>(&MapDraw_detour)},
    };
    *count = 2;
    return kDetours;
}

} // namespace zeus::world_map
