// city_picture (city_picture.h). Zeus.exe 2.1.4.0, SHA256 382bcd28...0a57; the evidence is the game's
// decompilation and disassembly (the addresses below) and testing.
//
// The city: the same 13 variables as Yongle's (Zeus's viewport_init has the same ones), the overlay 0xfa05f0 and the
// building preview 0xda2fa8 at 0, the debug markers 0x160ac80 at 0. Each strip is one drawing of the game's own
// city_render inside timing's drawing that never counts (PutBackDrawing): the tile animation bytes, the buildings,
// the figures' field, the drawing counters and the random numbers are copied and put back around every strip, and the
// pictures are drawn only in a counting frame, so every strip starts from the same state (0x44c2f0
// moves tile_anim and a per-building count on with every drawing). What city_render changes besides that (its third
// argument 0x7d3bdc, the 60-tick rhythm 0x7d3b18 / 1c, the drawing clip 0xf139b0..bc, the countdown 0x83ad68, the
// counter 0x91ea6c of 0x4517f0) and what the drawing of the whole map changes (the water marks 0x83ad6c, 228 x 228;
// the ambient sound's tile counts 0x10e41f0 x 12 and 0x10e4224) are copied before the picture and put back after it.
// The minimap: minimap_draw's own variables (0x7d3b08 .. 0x7d3b37, the drawing clip, the tile flag 0x78c570 and its
// loop variables 0x7d3bd4 / d8 / e0) copied and put back the same way; its two drawings each inside a drawing that
// never counts as well (anything it changes through a pointer is put back with the rest; [debug] state_check checks).
//
// Not in the editors: they edit a city on screen 0x01 at the game's own layout.
#include "games/zeus/city_picture.h"

#include "core/capture/city_picture.h"
#include "core/diag/guard.h"
#include "core/display/city_strips.h"
#include "core/log.h"
#include "core/scroll/picture.h"
#include "games/zeus/minimap_part.h"
#include "games/zeus/scroll.h"
#include "games/zeus/site.h"
#include "games/zeus/state_check.h"
#include "games/zeus/timing.h"
#include "games/zeus/zoom.h"

#include <cstdio>
#include <cstring>

namespace zeus::city_picture {

namespace {

namespace cp = core::capture::city_picture;

// minimap_draw(x, y, w, h): __cdecl, the callers pop 4 (add esp, 0x10 at 0x517559, 0x51c7f2).
const Site kMinimapDraw{"minimap_draw", 0x454980, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x6c, 0xa1, 0xe4}};
// The player's city (0x4621e0): __cdecl, no arguments, ret; returns its index 0..21 into the cities (-1: none) and
// writes it to the cache 0x903128 (put back here). [inferred: world_map_draw compares it with the trade routes' ends]
const Site kPlayerCity{"player_city_index", 0x4621e0, {0xa1, 0x28, 0x31, 0x90, 0x00, 0x8d, 0x0c, 0xc0}};
// lang_get_string(group, index): __cdecl, the caller pops 2 (add esp, 8); reads the text table only.
const Site kLangGetString{"lang_get_string", 0x507bf0, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x15, 0x64, 0x9f}};

const Var<BYTE *> kDrawBuffer{0xf9c100}; // draw_buffer
const Var<int> kScreenPitch{0xf2a698};   // bytes per row
const Var<int> kDdWidth{0xf2a6a0};
const Var<int> kDdHeight{0xf2a69c};
const Var<int> kClipLeft{0x7d3b94};      // the city's clip (city_render copies it to the drawing clip)
const Var<int> kClipTop{0x7d3b8c};
const Var<int> kClipRight{0x7d3b90};     // exclusive
const Var<int> kClipBottom{0x7d3b88};    // exclusive
const Var<int> kWidthTiles{0x7d3bbc};
const Var<int> kHeightRows{0x7d3bb8};
const Var<int> kTileWidth{0x7d3bb0};
const Var<int> kTileHalfHeight{0x7d3ba4};
const Var<int> kOriginX{0x7d3be8};
const Var<int> kOriginY{0x7d3be4};
const Var<int> kViewDrawnX{0x7d3bd0};    // city_render's x / y (mouse_to_grid and the minimap's frame read them)
const Var<int> kViewDrawnY{0x7d3bcc};
const Var<int> kOverlay{0xfa05f0};       // the overlay shown (0 none; the minimap reads it too)
const Var<int> kPreviewShown{0xda2fa8};  // building_preview_draw draws only when this is not 0
const Var<int> kMarkers{0x160ac80};      // 0x44ad70's debug markers (switched by 0x51e4a0)
const Var<unsigned char> kGameScreen{0xf2a6dd};
const Var<int> kEditorMode{0x665e2c};    // editor_mode: 1 in the editors
constexpr int kViewTableCols = 0xe7, kViewTableRows = 0x1c9;

// What city_render and the drawing of the whole map change besides kDrawnState (games/zeus/timing).
struct Block {
    DWORD address, size;
};
// Copied before the picture and put back after it. The scratch areas 0xf13960 .. 0xf13a7c (the drawing clip
// 0xf139b0 .. bc among them) and 0x8478c0 .. 0x847d13, and the bytes 0xfa05e4, 0xfe4f6c: found by [debug] state_check
// in testing (every drawing leaves the two scratch areas changed; the picture also the two bytes).
const Block kCityHeld[] = {
    {0x7d3bdc, 4},         // city_render's third argument
    {0x7d3b18, 8},         // its 60-tick rhythm (the tick it last passed, the flag)
    {0x7d3bd4, 8},         // the drawing's loop variables
    {0x7d3be0, 4},
    {0xf13960, 0x120},     // the drawing's scratch, with the drawing clip (city_render leaves the whole screen in it)
    {0x8478c0, 0x454},     // scratch below the drawing counters
    {0xfa05e4, 4},
    {0xfe4f6c, 4},
    {0x83ad68, 4},         // the countdown city_render lowers
    {0x91ea6c, 4},         // 0x4517f0's counter (0..4)
    {0x83ad6c, 228 * 228}, // the water marks (0x458dc2 marks the tiles drawn)
    {0x10e41f0, 12 * 4},   // the ambient sound's tile counts around the view's middle (0x458170)
    {0x10e4224, 4},
};
const Block kMinimapHeld[] = {
    {0x7d3b08, 12 * 4}, // minimap_draw's position, size and origin (minimap_center's 0x7d3b30 / 34)
    {0xf13960, 0x120},  // the drawing's scratch, with the drawing clip
    {0x78c570, 2},      // the tile functions' flag
    {0x7d3bd4, 8},      // its loop variables
    {0x7d3be0, 4},
};
constexpr DWORD kHeldBytes = 4 + 8 + 8 + 4 + 0x120 + 0x454 + 4 + 4 + 4 + 4 + 228 * 228 + 12 * 4 + 4;
unsigned char g_held[kHeldBytes];

// Copied before every strip (and each minimap drawing) and put back after it, around timing's own put-back: what the
// drawing writes into the save data for every tile and figure it draws. On the screen the drawing writes the same
// values again each frame; drawn over the whole map it changed them where the player was not looking, and the next
// strip (and the seam check's second picture) started from those (seen in testing; so they are
// put back after every strip). Save blocks 17 (4 x 51,984 B per tile), 19 (each tile's first figure and
// another per-tile word, 2 x 51,984 B) and 28 (the figures, 2000 x 388 B).
const Block kStripHeld[] = {
    {0xcce128, 4 * 51984},
    {0xd0d878, 2 * 51984},
    {0xdbd788, 2000 * 388},
};
constexpr DWORD kStripHeldBytes = 4 * 51984 + 2 * 51984 + 2000 * 388;
unsigned char g_stripHeld[kStripHeldBytes];

// The player's city and its name: the cities from 0x8fbac0, 0x23c bytes each, 22 of them; +3 is the
// index into Zeus_Text's group 21 (the city names), or 0xff for the city's own name at +0x1f9.
constexpr DWORD kCities = 0x8fbac0;
constexpr DWORD kPlayerCityCache = 0x903128;
constexpr int kCityStride = 0x23c, kCityCount = 22;
constexpr int kCityTextAt = 3, kCityNameAt = 0x1f9, kCityNameSize = kCityStride - kCityNameAt;
constexpr int kCityNameGroup = 21;
using PlayerCity_fn = int(__cdecl *)();
using LangGetString_fn = const char *(__cdecl *)(int group, int index);
bool g_cityNameOk = false; // kPlayerCity and kLangGetString matched at Attach

bool g_checking = false; // [debug] state_check checks this picture

void Copy(const Block *blocks, int count, bool back, unsigned char *p = g_held)
{
    for (int i = 0; i < count; ++i) {
        void *game = At<void>(blocks[i].address);
        back ? std::memcpy(game, p, blocks[i].size) : std::memcpy(p, game, blocks[i].size);
        p += blocks[i].size;
    }
}

constexpr int kCityHeldCount = static_cast<int>(sizeof(kCityHeld) / sizeof(kCityHeld[0]));
constexpr int kMinimapHeldCount = static_cast<int>(sizeof(kMinimapHeld) / sizeof(kMinimapHeld[0]));
constexpr int kStripHeldCount = static_cast<int>(sizeof(kStripHeld) / sizeof(kStripHeld[0]));

// One strip (or minimap drawing) inside timing's drawing that never counts, with the save data it writes put back
// after it: every strip starts from the state the picture started from.
bool AroundDrawing(void (*fn)(void *), void *ctx)
{
    if (!zeus::timing::PictureDrawingNow()) {
        return false;
    }
    Copy(kStripHeld, kStripHeldCount, false, g_stripHeld);
    const bool ok = zeus::timing::PutBackDrawing(fn, ctx);
    Copy(kStripHeld, kStripHeldCount, true, g_stripHeld);
    return ok;
}

void Hold(cp::Drawing d)
{
    g_checking = zeus::state_check::BeginPicture(d == cp::Drawing::City ? "the city picture" : "the minimap picture");
    d == cp::Drawing::City ? Copy(kCityHeld, kCityHeldCount, false) : Copy(kMinimapHeld, kMinimapHeldCount, false);
}

void Drawn(cp::Drawing)
{
    if (g_checking) {
        zeus::state_check::AfterDraw();
    }
}

void Release(cp::Drawing d)
{
    d == cp::Drawing::City ? Copy(kCityHeld, kCityHeldCount, true) : Copy(kMinimapHeld, kMinimapHeldCount, true);
    if (g_checking) {
        g_checking = false;
        zeus::state_check::AfterPutBack();
    }
}

bool DrawingWorks()
{
    return zeus::scroll::Working() && zeus::timing::DrawnWorking();
}

// The city screens without a window (no picture with
// any window, message, dialog or map open, the same in both games): the city (0x01, 0x20) and the city with a company
// selected (0x48). Not the editors (see the top of this file).
bool CityScreen()
{
    const unsigned char s = *kGameScreen;
    return *kEditorMode == 0 && (s == 0x01 || s == 0x20 || s == 0x48);
}

int GameScreen()
{
    return *kGameScreen;
}

void TileSize(int *tileWidth, int *halfHeight)
{
    *tileWidth = *kTileWidth;
    *halfHeight = *kTileHalfHeight;
}

int ViewRows()
{
    return *kHeightRows;
}

void BeginCityDrawing(core::diag::Restore &r, core::diag::Restore &r2, const core::display::StripPlan &p, BYTE *buf,
                      int pitchPx)
{
    r.Set(kDrawBuffer.ptr(), buf);
    r.Set(kScreenPitch.ptr(), pitchPx * 2);
    r.Set(kDdWidth.ptr(), pitchPx);
    r.Set(kDdHeight.ptr(), p.bufH);
    r.Set(kClipLeft.ptr(), 0);
    r.Set(kClipTop.ptr(), 0);
    r.Set(kClipRight.ptr(), p.clipRight);
    r.Set(kClipBottom.ptr(), p.clipBottom);
    r.Set(kWidthTiles.ptr(), p.cols + 1);
    r.Set(kHeightRows.ptr(), p.viewRows + 2);
    r.Set(kOriginX.ptr(), 0);
    r.Set(kOriginY.ptr(), p.topPx);
    r.Save(kViewDrawnX.ptr());
    r.Save(kViewDrawnY.ptr());
    r.Set(kOverlay.ptr(), 0);
    r.Set(kPreviewShown.ptr(), 0);
    r2.Set(kMarkers.ptr(), 0);
}

// One strip: the game's own city_render, timed as the picture's drawing (core/scroll/picture's DrawCity).
void DrawStrip(int vx, int vy, int orientation)
{
    core::scroll::picture::DrawCity(zeus::scroll::Picture(), vx, vy, orientation);
}

using MinimapDraw_fn = void(__cdecl *)(int, int, int, int);

// minimap_draw paints every cell of the view table, the map's part of the buffer is worked out by
// games/zeus/minimap_part (the drawing's rules are written there).
const Var<int> kMapColMin{0xfa05b4};
const Var<int> kMapColMax{0xfa05b0};
const Var<int> kMapRowMin{0xfa05ac};
const Var<int> kMapRowMax{0xfa05a8};
const Var<int> kMinimapCol{0x7d3b34}; // minimap_center: the view table's first column and half row drawn
const Var<int> kMinimapRow{0x7d3b30};

RECT MinimapArea(int x0, int y0)
{
    const zeus::minimap_part::MapRange m{*kMapColMin, *kMapColMax, *kMapRowMin, *kMapRowMax};
    return zeus::minimap_part::MapPart(x0, y0, *kMinimapCol, *kMinimapRow, m);
}

void DrawMinimap(core::diag::Restore &r, BYTE *buf, int pitchPx, int rows, int x0, int y0, RECT *area)
{
    r.Set(kDrawBuffer.ptr(), buf);
    r.Set(kScreenPitch.ptr(), pitchPx * 2);
    r.Set(kDdWidth.ptr(), pitchPx);
    r.Set(kDdHeight.ptr(), rows);
    r.Set(kOverlay.ptr(), 0);
    // The view's frame below the minimap, in the part of the buffer that is cut away: rect_draw 0x509870 at
    // (drawn y - 0x7d3b30) + y0, and minimap_center sets 0x7d3b30 to 0 when the map is smaller than the view table.
    r.Set(kViewDrawnX.ptr(), 0);
    r.Set(kViewDrawnY.ptr(), kViewTableRows + 8);
    Function<MinimapDraw_fn>(kMinimapDraw)(x0, y0, kViewTableCols, kViewTableRows);
    *area = MinimapArea(x0, y0); // minimap_center's origin as the drawing used it (put back after, Release)
    static int logged = 0;
    if (logged++ < 2) {
        core_log("picture: the minimap's map part: columns %d..%d, half rows %d..%d, origin (%d,%d): (%ld,%ld)-(%ld,%ld) of "
                 "the drawing",
                 *kMapColMin, *kMapColMax, *kMapRowMin, *kMapRowMax, *kMinimapCol, *kMinimapRow, area->left, area->top,
                 area->right, area->bottom);
    }
}

// The player's city's name; nullptr (logged) when there is none.
const unsigned char *CityName()
{
    if (!g_cityNameOk) {
        return nullptr; // logged at Attach
    }
    struct NameCall {
        int index;
        int text;
        const unsigned char *name;
    } call{-1, -1, nullptr};
    static core::diag::Feature nameFeature{"city name for picture file names (games/zeus/city_picture)"};
    core::diag::Restore r;
    r.Save(At<int>(kPlayerCityCache)); // 0x4621e0 writes its cache
    const bool ok = core::diag::Enabled(nameFeature) &&
                    core::diag::Guarded(
                        nameFeature,
                        [](void *v) {
                            auto *c = static_cast<NameCall *>(v);
                            c->index = Function<PlayerCity_fn>(kPlayerCity)();
                            if (c->index < 0 || c->index >= kCityCount) {
                                return;
                            }
                            const auto *city = At<const unsigned char>(kCities) +
                                               static_cast<size_t>(c->index) * kCityStride;
                            c->text = city[kCityTextAt];
                            if (c->text != 0xff) {
                                c->name = reinterpret_cast<const unsigned char *>(
                                    Function<LangGetString_fn>(kLangGetString)(kCityNameGroup, c->text));
                            }
                            else if (std::memchr(city + kCityNameAt, 0, kCityNameSize) != nullptr) {
                                c->name = city + kCityNameAt;
                            }
                        },
                        &call, &r);
    r.Undo();
    if (!ok) {
        return nullptr; // logged by the guard
    }
    if (call.index < 0 || call.index >= kCityCount) {
        core_log("picture: the player's city index is %d (not 0..%d): file names start with \"city\"", call.index,
                 kCityCount - 1);
        return nullptr;
    }
    if (!call.name) {
        core_log("picture: no name for the player's city %d (%s): file names start with \"city\"", call.index,
                 call.text == 0xff ? "its own name is not ended within its record" : "the game gave none");
        return nullptr;
    }
    static int logged = -1;
    if (logged != call.index * 256 + call.text) { // once per city
        logged = call.index * 256 + call.text;
        char what[80];
        std::snprintf(what, sizeof(what), "the player's city is %d of the world, name %s %d:", call.index,
                      call.text == 0xff ? "of its own (+0x1f9), marker" : "text 21 /", call.text);
        cp::LogText(what, call.name, 64);
    }
    return call.name;
}

const cp::CityPictureGame kGame{
    &DrawingWorks,
    &CityScreen,
    &GameScreen,
    &zeus::zoom::ViewLimits,
    &TileSize,
    kViewTableCols,
    kViewTableRows,
    &ViewRows,
    &BeginCityDrawing,
    &DrawStrip,
    &DrawMinimap,
    &Hold,
    &Drawn,
    &Release,
    &zeus::timing::PictureDrawingNow,
    &AroundDrawing,
    &CityName,
};

} // namespace

void RegisterSettings()
{
    cp::RegisterSettings();
}

bool Attach(bool scrollAttached)
{
    if (!scrollAttached) {
        core_log("picture: the city and minimap pictures need the city drawing's hooks (not installed)");
        return false;
    }
    if (!zeus::timing::DrawnWorking()) {
        core_log("picture: the drawn-frame put-back is not working; the city and minimap pictures are not installed");
        return false;
    }
    if (!Matches(kMinimapDraw)) {
        core_log("picture: %s at 0x%lx does not match; the city and minimap pictures are not installed",
                 kMinimapDraw.name, static_cast<unsigned long>(kMinimapDraw.address));
        return false;
    }
    g_cityNameOk = Matches(kPlayerCity) && Matches(kLangGetString);
    if (!g_cityNameOk) {
        core_log("picture: %s or %s does not match; picture file names start with \"city\"", kPlayerCity.name,
                 kLangGetString.name);
    }
    cp::Install(kGame);
    return true;
}

void InstallForTest()
{
    g_cityNameOk = true;
    cp::Install(kGame);
}

} // namespace zeus::city_picture
