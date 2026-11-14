// The world map in Zeus (world_map.h).
//
// The flags: anim_advance_ui 0x599460 (__cdecl image id, frame; returns the new frame; ret; called only by the
// world map's drawing 0x599da0, at 0x59a90d and 0x59ab2f): a picture of animation speed 0 (the allies' flags) steps
// whenever anim_due[0] (0x6668ec) is set, and anim_timers_update sets it on every game_frame. So the flags moved 20
// steps a second at the original's 20 frames and look still at 200 (seen). As in Emperor's china_map.cpp, speed 0
// steps at most every 50 ms (core/sim/ui_anim.h): on the other frames anim_due[0] is 0 for the call (other speeds keep
// their own timers).
//
// The map's height: world_map_draw 0x599da0 (no arguments; ret; called for screens 0x14, 0x34 and the editor's
// map; also as the background of 0x03, 0x09, 0x1a over the map): the map picture (838 x 738 at (0, 30)) is drawn into a
// box min(sidebar_x + 16, map width) wide but dd_height - 30 high, never cut to the map's height (0x599e07..0x599e20;
// the width is, at 0x599e01). At the game's own 768 that is 738, the map's own height; at 1440 it is 1410 and the
// drawing reads past the picture's memory (seen in testing: a crash at 0x413e00 on the first world map frame). The same
// height also ends the clip of the map's objects (0x599f02). So while the map is drawn the game sees its own 768
// (dd_height 0xf2a69c, put back right after).
//
// The map's layout: the core's map_screen (shared with Emperor's China map) draws the
// map into a buffer of ours and shows it in the middle of the map area, in the dialog panel's border, over the same map
// blurred or the city ([video] world_map, world_map_scale, world_map_background). Not at the game's own sizes (all 14
// maps are 838 x 738, the map area at 1024x768: the game's own drawing fills it), never on the editor's map. Hooked:
//   world_map_draw 0x599da0: the core's Draw (the game's drawing with the height above, into our buffer);
//   frame_top_panel 0x58eb20 (__cdecl 1 argument, ret; world_map_draw calls it with 1): waits until the map is shown
//               and is drawn on the screen (it draws only into the top bar; at 0x34 it returns at once);
//   0x407360 (__cdecl 2 pointers, ret; world_map_hit): the mouse against each city ("mouse - origin + scroll") and the
//               screen rectangles recorded while drawing; screen_handle_input's 0x14 calls it twice (hover, the left
//               click) while the mouse is right of the origin: run with the mouse in the game's map coordinates;
//   campaign map (0x34): 0x422d40 (no arguments, ret; the background on redraws: world_map_draw, then the top bar
//               picture at 800 and 1024 wide only): wider, the top bar tiled as on the city screen
//               (games/zeus/resolution); 0x421c70 (no arguments, ret; every frame: the sidebar, the four candidate
//               cities' markers and names relative to the origin, hover by 0x50b770 on the mouse): the core's
//               CampaignDraw, then the sidebar's filler (the campaign map draws the world map's sidebar picture
//               itself); 0x421af0 (no arguments, ret; the left click on a marker, 0x50b770 again): with the map's
//               mouse. The campaign map always has the blurred map around it: it comes after a victory screen, there
//               is no city behind it.
//
// Failure: the flags: the game's own stepping (fast again). The map's height cannot fail by itself (writes of our own,
// undone after the call); it is off with the rest of the map when its site does not match, and then the map is not
// drawn above 768 either way (the logical resolution falls back to the game's 1024x768: see Attach). The layout: one
// guard (the core's); on an error the map is drawn the game's way (top left) with the real mouse from then on.
#include "games/zeus/world_map.h"

#include "core/diag/guard.h"
#include "core/display/map_screen.h"
#include "core/log.h"
#include "core/settings/settings.h"
#include "core/sim/ui_anim.h"
#include "games/zeus/resolution.h"
#include "games/zeus/timing.h"

#include <mmsystem.h>

#include <cstdio>

namespace zeus::world_map {

namespace {

namespace map_screen = core::display::map_screen;

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

// ---------------------------------------------------------------------------------------------
// The map's drawing and layout

const Site kMapDraw{"world_map_draw", 0x599da0, {0x83, 0xec, 0x54, 0xe8, 0x58, 0xfc, 0xf6, 0xff}};
const Site kTopPanel{"frame_top_panel", 0x58eb20, {0xa1, 0x20, 0xc8, 0x2b, 0x01, 0x53, 0x55, 0xbd}};
const Site kHit{"world_map_hit", 0x407360, {0x83, 0xec, 0x10, 0xa1, 0x38, 0xa7, 0xf2, 0x00}};
const Site kCampaignBack{"campaign_map_background", 0x422d40, {0x83, 0xec, 0x08, 0xe8, 0x58, 0x70, 0x17, 0x00}};
const Site kCampaignDraw{"campaign_map_draw", 0x421c70, {0x0f, 0xbe, 0x05, 0xb0, 0xa7, 0xf2, 0x00, 0x83}};
const Site kCampaignClick{"campaign_map_click", 0x421af0, {0xa0, 0x08, 0xa7, 0xf2, 0x00, 0x83, 0xec, 0x08}};

const Var<BYTE *> kDrawBuffer{0xf9c100}; // draw_buffer (RGB565)
const Var<int> kPitch{0xf2a698};
const Var<int> kDdWidth{0xf2a6a0};
const Var<int> kDdHeight{0xf2a69c};
const Var<int> kClipBottom{0xf139b0}; // the drawing clip's bottom (exclusive; symbols graphics, resolution.cpp)
const Var<int> kModeWidth{0xf2a6b4}, kModeHeight{0xf2a6b0};
const Var<char> kRedraw{0xf2a6ce}; // screen_redraw
const Var<unsigned char> kGameScreen{0xf2a6dd};
const Var<int> kSidebarX{0x10dff34};
const Var<int> kMouseX{0xf2a738}, kMouseY{0xf2a734};
const Var<int> kOriginX{0x5fb7b4}, kOriginY{0x5fb7b8}; // world_map_draw writes (0, 30)
const Var<int> kScrollX{0x8f9838}, kScrollY{0x8f9834}; // cut by world_map_draw (see kScrollSlack)
const Var<int> kMapW{0x5fb7ac}, kMapH{0x5fb7b0};       // world_map_load 0x460580 (inside world_map_draw)
const Var<int> kMapNumber{0x90311c};                   // 1..14: Zeus_MapOfGreece01..10, Poseidon_map01..04
const Var<int> kEditorMode{0x665e2c};                  // 1 in the editors
const Var<int> kBeneath{0x781894};                     // the screen under the drop-down menu 0x03

constexpr int kGameHeight = 768; // the game's largest height: the map box is then the map's own 738
constexpr int kTopBarH = 30;     // the origin's y
constexpr unsigned char kCampaignMap = 0x34;
// world_map_draw cuts the x scroll to (map width - box) - 1 when it is not below it (0x599e1e..0x599e2b): -1 for a map
// that fills its box (838 = 838, as at the game's own 1024x768). It draws the picture with the scroll raised to 0
// (0x599e6f) and puts -1 back afterwards (0x599ec1), so the cities and markers are drawn, and the mouse compared
// (0x407360: mouse - origin + scroll), 1 px right of the picture's 0, as in the original. y is raised to 0 and kept.
// Seen in a smoke test: scroll -1,0 on the campaign map, drawn the game's way.
constexpr int kScrollSlack = 1;

using Void_fn = void(__cdecl *)();
using Top_fn = int(__cdecl *)(int);
using Hit_fn = int(__cdecl *)(void *, void *);
Void_fn o_MapDraw, o_CampaignBack, o_CampaignDraw, o_CampaignClick;
Top_fn o_TopPanel;
Hit_fn o_Hit;
bool g_heightLogged = false;
core::diag::Feature g_layoutFeature{"world map layout (games/zeus/world_map)"};
bool g_layout = false; // the layout's hooks are installed (AttachLayout)

bool Standard(int w, int h)
{
    return (w == 640 && h == 480) || (w == 800 && h == 600) || (w == 1024 && h == 768);
}

bool EditorMap()
{
    const unsigned char s = *kGameScreen;
    return *kEditorMode == 1 || s == 0x15 || s == 0x2d || (s == 0x03 && *kBeneath == 0x15);
}

bool TakeOver()
{
    const int w = *kModeWidth, h = *kModeHeight;
    return !(Standard(w, h) || w < 1024) && !EditorMap();
}

bool CityBackground()
{
    return *kGameScreen != kCampaignMap; // after a victory screen: nothing of the city is behind it
}

// The game's map drawing, with the game's own height above 768 (through r: undone after it, also after an error).
void DrawMap(core::diag::Restore &r)
{
    const int h = *kDdHeight;
    if (h > kGameHeight) {
        if (!g_heightLogged) {
            g_heightLogged = true;
            core_log("zeus: world map: drawn with the game's own height %d (the screen is %d high)", kGameHeight, h);
        }
        r.Set(kDdHeight.ptr(), kGameHeight);
    }
    Original(o_MapDraw, kMapDraw)();
}

void TopPanel(int arg)
{
    Original(o_TopPanel, kTopPanel)(arg);
}

// The game's map box on the screen: (0, 30) .. (min(sidebar_x + 16, map width), 30 + 738).
RECT GameBox(int ddW, int ddH, int sidebarX, int mapW, int)
{
    int right = sidebarX + 16 < ddW ? sidebarX + 16 : ddW;
    if (mapW > 0 && mapW < right) {
        right = mapW;
    }
    return RECT{0, kTopBarH, right, ddH < kGameHeight ? ddH : kGameHeight};
}

// Far outside the map: 0x407360 and 0x50b770 only compare the point with the cities and the recorded rectangles.
void EmptyPoint(int, int, int, int *gx, int *gy)
{
    *gx = -10000;
    *gy = -10000;
}

const map_screen::MapGame kGame("zeus: world map", &g_layoutFeature, "world map layer (ours)",
                                kDrawBuffer, kPitch, kDdWidth, kDdHeight, kClipBottom, kRedraw, kSidebarX, kMouseX, kMouseY,
                                kOriginX, kOriginY, kScrollX, kScrollY, kMapW, kMapH, kMapNumber, kTopBarH, kScrollSlack,
                                &TakeOver,
                                &CityBackground, &DrawMap, &TopPanel, &GameBox, &EmptyPoint);

void __cdecl MapDraw_detour()
{
    if (g_layout) {
        map_screen::Draw(); // ours, or the game's own way (DrawMap)
        return;
    }
    core::diag::Restore r; // without the layout: only the game's height for the call
    DrawMap(r);
    r.Undo();
}

int __cdecl TopPanel_detour(int arg)
{
    if (map_screen::TopBar(arg)) {
        return 0;
    }
    return Original(o_TopPanel, kTopPanel)(arg);
}

struct HitCall {
    void *a, *b;
    int result;
};

int __cdecl Hit_detour(void *a, void *b)
{
    HitCall c{a, b, 0};
    map_screen::WithMapMouse(
        [](void *p) {
            HitCall &c = *static_cast<HitCall *>(p);
            c.result = Original(o_Hit, kHit)(c.a, c.b);
        },
        &c);
    return c.result;
}

void __cdecl CampaignBack_detour()
{
    Original(o_CampaignBack, kCampaignBack)();
    zeus::resolution::WideTopBar();
}

void __cdecl CampaignDraw_detour()
{
    const zeus::resolution::SidebarMark mark = zeus::resolution::MarkWorldSidebar();
    map_screen::CampaignDraw([] { Original(o_CampaignDraw, kCampaignDraw)(); });
    zeus::resolution::WorldSidebarFiller(mark);
}

void __cdecl CampaignClick_detour()
{
    map_screen::WithMapMouse([](void *) { Original(o_CampaignClick, kCampaignClick)(); }, nullptr);
}

const Detour kDetours[] = {
    {&kAnim, reinterpret_cast<void *>(&Anim_detour)},
    {&kMapDraw, reinterpret_cast<void *>(&MapDraw_detour)},
    {&kTopPanel, reinterpret_cast<void *>(&TopPanel_detour)},
    {&kHit, reinterpret_cast<void *>(&Hit_detour)},
    {&kCampaignBack, reinterpret_cast<void *>(&CampaignBack_detour)},
    {&kCampaignDraw, reinterpret_cast<void *>(&CampaignDraw_detour)},
    {&kCampaignClick, reinterpret_cast<void *>(&CampaignClick_detour)},
};

} // namespace

void RegisterSettings()
{
    using core::settings::Effect;
    using core::settings::Item;
    using core::settings::Type;
    int i = core::settings::Register(Item{
        "video", "world_map", "World map",
        "The world map at resolutions larger than the map. scaled: larger than its own size (World map: scale), in "
        "a fret border, over the background (World map: background); native: the picture's own size, centered, the "
        "same background.",
        Type::Choice, "native", Effect::Live, true, true, 0, 0, 0, nullptr, map_screen::kModes, map_screen::kModeLabels,
        map_screen::kModeChoices});
    core::settings::OnChange(i, &map_screen::ApplyMode);
    i = core::settings::Register(Item{
        "video", "world_map_scale", "World map: scale",
        "For World map = scaled: how large, in percent of the map's own size. Auto: as large as the map area allows "
        "(fit). The slider ends at what fits for the map last shown at this resolution; a larger value in "
        "zeus_engine.ini is shown as large as fits.",
        Type::Int, "fit", Effect::Live, true, true, 100, 400, 5, "fit", nullptr, nullptr, 0, "%",
        &map_screen::FitPercent, "world_map", "scaled"});
    core::settings::OnChange(i, &map_screen::ApplyScale);
    i = core::settings::Register(Item{
        "video", "world_map_background", "World map: background",
        "What is around the map (World map = scaled or native). map: the same map, blurred and darkened; city: your "
        "city as it was when you opened the map, as it is (the map like a window over it); it stands still while "
        "the map is shown.",
        Type::Choice, "map", Effect::Live, true, true, 0, 0, 0, nullptr, map_screen::kBackgrounds,
        map_screen::kBackgroundLabels, 2});
    core::settings::OnChange(i, &map_screen::ApplyBackground);
}

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

bool AttachLayout()
{
    if (!o_MapDraw) {
        return false; // Attach did not hook the drawing
    }
    const Site *sites[] = {&kTopPanel, &kHit, &kCampaignBack, &kCampaignDraw, &kCampaignClick};
    for (const Site *s : sites) {
        if (!Matches(*s)) {
            core_log("zeus: world map: %s at 0x%lx does not match: the world map is drawn the game's way (top left)",
                     s->name, static_cast<unsigned long>(s->address));
            return false;
        }
    }
    map_screen::Configure(kGame);
    if (!Install(kTopPanel, &TopPanel_detour, o_TopPanel) || !Install(kHit, &Hit_detour, o_Hit) ||
        !Install(kCampaignClick, &CampaignClick_detour, o_CampaignClick) ||
        !Install(kCampaignDraw, &CampaignDraw_detour, o_CampaignDraw) ||
        !Install(kCampaignBack, &CampaignBack_detour, o_CampaignBack)) {
        // Hooks already installed stay, but do nothing of ours: Draw is not called, so the map is never shown by us
        // (TopBar never waits, WithMapMouse and CampaignDraw see no map shown).
        core_log("zeus: world map: a hook could not be installed: the world map is drawn the game's way (top left)");
        return false;
    }
    g_layout = true;
    char scale[16];
    std::snprintf(scale, sizeof(scale), map_screen::ScalePercent() ? "%d%%" : "fit", map_screen::ScalePercent());
    core_log("zeus: world map: layout installed (video.world_map %s, scale %s, background %s)", map_screen::ModeName(),
             scale, map_screen::BackgroundName());
    return true;
}

const Detour *Detours(int *count)
{
    *count = static_cast<int>(sizeof(kDetours) / sizeof(kDetours[0]));
    return kDetours;
}

void LayoutForTest(bool on)
{
    map_screen::Configure(kGame);
    g_layout = on;
}

} // namespace zeus::world_map
