// Framed full-screen pictures in Zeus.
//
//   jpeg_draw_background 0x504fa0 (__cdecl file, x, y; hooked in games/zeus/diag, which calls BeforePicture and
//      AfterPicture): draws a JPEG with DirectDraw's Blt at (x, y) (a 1024-wide picture in mode 3 at x = 0, a 768-high
//      one at y = 0), only when the screen is redrawn: the front-end pictures are 1024x768 (the whole frame), the
//      loading pictures (zeus_load1..4, poseidon_load1..8) 800x600 at (112, 84). It may be called again for the same
//      picture (five callers through UI class tables, not traced), so this layer acts only when the file, its place
//      or the screen changed.
//   The dialog panel 0x59ee90 draws group 0x83 (entries 340..483 of Zeus_Interface.sg3, 144 tiles of 16x16), the inset
//      panel 0x59f2a0 group 0x84 (entries 484..532, 49 tiles), in the same layout as Emperor's: read by the core from
//      the player's own DATA\Zeus_Interface.sg3, in memory only.
//   The city's area on the back buffer after city_render 0x44a830 (hooked in games/zeus/timing): city_view_left, top,
//      right, bottom 0x7d3b94, 0x7d3b8c, 0x7d3b90, 0x7d3b88 (as games/zeus/diag logs them; right and bottom taken as
//      exclusive), draw_buffer 0xf9c100, dd_pitch 0xf2a698, dd_width / dd_height 0xf2a6a0 / 0xf2a69c.
// Zeus's loading screens (at start-up only) have their status line drawn by the game inside the picture, not with GDI:
// no status block of ours. The drop-down menu 0x03 is judged by the screen under it, 0x781894; editor_mode 0x665e2c
// (1 in the editors, which edit cities on the city screen 0x01) keeps the editors' maps out of the menus' background.
// debug.loading_stage_ms: before the next picture after a loading picture, the loading picture stays until it was
// shown that long.
// Failure: the core's guards (framing; the city background). Off: the game's own frame, scaled with black bars.
#include "games/zeus/fullscreen.h"

#include "core/config.h"
#include "core/display/framed_screens.h"
#include "core/display/resolution_switch.h"
#include "core/log.h"
#include "core/settings/settings.h"
#include "games/zeus/site.h"
#include "games/zeus/scroll.h"
#include "games/zeus/timing.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cctype>
#include <cstdio>
#include <cstring>

namespace zeus::fullscreen {

namespace {

namespace framed = core::display::framed;

const Var<int> kScreenMode{0xf2a770};
const Var<unsigned char> kGameScreen{0xf2a6dd};
const Var<unsigned char *> kDrawBuffer{0xf9c100}; // draw_buffer
const Var<int> kPitch{0xf2a698};                  // dd_pitch
const Var<int> kWidth{0xf2a6a0}; // dd_width
const Var<int> kCityLeft{0x7d3b94}, kCityTop{0x7d3b8c}, kCityRight{0x7d3b90}, kCityBottom{0x7d3b88};

constexpr wchar_t kInterfacePack[] = L"DATA\\Zeus_Interface.sg3"; // next to Zeus.exe
constexpr int kPanelFirst = 340; // group 0x83
constexpr int kInsetFirst = 484; // group 0x84
constexpr unsigned char kScreenVideo = 0x4a;
constexpr int kLoadW = 800, kLoadH = 600; // the loading pictures

const Var<int> kBeneath{0x781894};    // the screen under the drop-down menu 0x03 (screen_is_editor_main 0x58e260)
const Var<int> kEditorMode{0x665e2c}; // editor_mode: 1 in the editors (cleared at 0x421857 before an episode starts)

// Zeus draws its loading screens' status line itself, inside the picture (a small panel; seen in testing): no block of
// ours, nothing to draw.
const framed::FramedGame kGame("zeus: fullscreen", kInterfacePack, kPanelFirst, kInsetFirst, kScreenMode, kGameScreen,
                               kScreenVideo, &CityAroundShown, framed::StatusBlock{0, 0, 0, nullptr, nullptr});

bool g_installed = false;
char g_last[128] = ""; // the picture handed on last (file, place, screen) ...
int g_lastX = 0, g_lastY = 0, g_lastScreen = -1;
DWORD g_loadingSince = 0; // ... and when it was shown, if it is a loading picture

struct Picture {
    const char *file;
    int x, y;
};

void PictureBody(void *p)
{
    const Picture &c = *static_cast<Picture *>(p);
    if (LoadingPicture(c.file)) {
        const RECT r{c.x, c.y, c.x + kLoadW, c.y + kLoadH};
        framed::PictureShown(&r);
    }
    else {
        framed::PictureShown(nullptr);
    }
}

void MenuScreens()
{
    using core::settings::Effect;
    using core::settings::Item;
    using core::settings::Type;
    const int i = core::settings::Register(Item{
        "video", "menu_screens", "Menu screens",
        "Main menu, leader and adventure choice, briefings, high scores, victory and defeat, at the display scale. "
        "city: in a fret border, your city blurred around (menu_background.png or .bmp in Pericles's folder or the "
        "game folder if you put one there, otherwise the city you saw last); panel: in a fret border on the game's own "
        "panel pattern; framed: in a fret border, black around; original: the game's own picture, black "
        "around.",
        Type::Choice, "city", Effect::Live, true, true, 0, 0, 0, nullptr, framed::kMenuChoices, framed::kMenuLabels,
        4});
    core::settings::OnChange(i, &framed::MenuScreensChanged);
}

} // namespace

// The drop-down menu 0x03 lies over another screen: judged by that screen (seen in testing: the editor's menus showed
// the city around them).
bool CityAroundShown(unsigned char screen, bool playerPicture)
{
    return CityAround(screen == 0x03 ? static_cast<unsigned char>(*kBeneath) : screen, playerPicture);
}

bool LoadingPicture(const char *file)
{
    if (!file) {
        return false;
    }
    char lower[128];
    std::snprintf(lower, sizeof(lower), "%s", file);
    for (char *c = lower; *c; ++c) {
        *c = static_cast<char>(std::tolower(static_cast<unsigned char>(*c)));
    }
    const char *name = std::strrchr(lower, '\\');
    name = name ? name + 1 : lower;
    return (std::strncmp(name, "zeus_load", 9) == 0 || std::strncmp(name, "poseidon_load", 13) == 0) &&
           std::strstr(name, ".jpg") != nullptr;
}

void RegisterSettings()
{
    framed::RegisterSettings(&MenuScreens);
}

bool Attach()
{
    framed::Configure(kGame);
    if (!core::display::resolution::Installed()) {
        core_log("zeus: fullscreen: not installed (needs the resolution): the game's own frame, scaled");
        return false;
    }
    g_installed = true;
    framed::Install();
    if (!zeus::timing::CityDrawingHooked()) {
        // As in Emperor's layer: the city is taken where the game draws it; without that hook the menus show the
        // panel pattern (or the player's own menu_background picture) and this line says why.
        core_log("zeus: fullscreen: no city behind the menus (city_render is not hooked): the panel pattern around them, "
                 "or your own menu_background picture");
    }
    return true;
}

void BeforePicture(const char *file)
{
    const int hold = core_config_get()->loading_stage_ms;
    if (!g_installed || hold <= 0 || !g_loadingSince) {
        return;
    }
    char name[128] = "";
    std::snprintf(name, sizeof(name), "%s", file ? file : "");
    if (std::strcmp(name, g_last) == 0) {
        return; // the same picture again
    }
    const DWORD elapsed = GetTickCount() - g_loadingSince;
    g_loadingSince = 0;
    if (elapsed < static_cast<DWORD>(hold)) {
        Sleep(hold - elapsed); // the loading picture shown last stays on screen (the output keeps showing it)
    }
    core_log("zeus: fullscreen: loading picture %s held %d ms (debug.loading_stage_ms)", g_last, hold);
}

void AfterPicture(const char *file, int x, int y)
{
    if (!g_installed) {
        return;
    }
    char name[128] = "";
    std::snprintf(name, sizeof(name), "%s", file ? file : "");
    const int screen = *kGameScreen;
    if (std::strcmp(name, g_last) == 0 && x == g_lastX && y == g_lastY && screen == g_lastScreen) {
        return; // drawn again: nothing changed
    }
    std::snprintf(g_last, sizeof(g_last), "%s", name);
    g_lastX = x;
    g_lastY = y;
    g_lastScreen = screen;
    g_loadingSince = LoadingPicture(name) ? GetTickCount() : 0;
    Picture p{name, x, y};
    framed::Guarded(&PictureBody, &p);
}

void OnLoop()
{
    if (!g_installed) {
        return;
    }
    if (!core::display::resolution::FrontEnd()) {
        g_last[0] = '\0'; // in the city: the next picture is a new one, even the same file as before
        g_lastScreen = -1;
        g_loadingSince = 0;
    }
    framed::OnLoop();
}

void CityDrawn()
{
    // Not in the editors: the menus' background is the city the player played last. The
    // adventure editor edits a city on the city screen 0x01 at our resolution (seen in testing), so the front-end
    // test is not enough; editor_mode tells.
    if (!g_installed || core::display::resolution::FrontEnd()) {
        return;
    }
    if (*kEditorMode != 0) {
        // Noted: if editor_mode stays set after an editor on a path not seen yet, the summary line says so.
        framed::NoteCitySkipped("editor_mode is not 0 (an editor)", *kCityLeft, *kCityTop, *kCityRight, *kCityBottom);
        return;
    }
    // The city as shown (smooth scrolling's picture): from the clip's top left to the sidebar and the screen's
    // bottom, never past the screen.
    const int pitch = *kPitch, width = *kWidth;
    int left, top, right, bottom;
    const char *why = nullptr;
    if (!zeus::scroll::DrawnCity(&left, &top, &right, &bottom, &why)) {
        framed::NoteCitySkipped(why, left, top, right, bottom);
        return;
    }
    if (pitch < width * 2) {
        framed::NoteCitySkipped("the pitch is smaller than the width", left, top, right, bottom);
        return;
    }
    framed::KeepCity(*kDrawBuffer, pitch, left, top, right, bottom);
}

void WhilePaused()
{
    if (g_installed) {
        framed::OnPresent(); // only when the background, the screen, the window size or the scale changed
    }
}

void OnGameEnd()
{
    framed::OnGameEnd();
}

} // namespace zeus::fullscreen
