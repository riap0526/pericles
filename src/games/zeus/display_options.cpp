#include "games/zeus/display_options.h"

#include "core/diag/guard.h"
#include "core/log.h"
#include "core/options/display_dialog.h"
#include "games/zeus/keys.h"
#include "games/zeus/painter.h"
#include "games/zeus/timing.h"

#include <cstring>

// Evidence (Zeus.exe 2.1.4.0, SHA256 382bcd28...0a57): the dialog's table, drawing and buttons read from the
// disassembly; calling conventions checked by disassembly (the callers and the ret of each).
namespace zeus::display_options {
namespace {

namespace dd = core::options::display_dialog;
namespace co = core::options;

// display_options_draw 0x59af60: __cdecl, no arguments, ret; called by screen_draw's dispatch (0x51421c). The panel
// (x + 0x60, y + 0x50, 18 x 12 blocks), four button frames at x 0x80, y 0x88 + 0x18 n, the texts of group 42, the
// clip reset, then 0x10dfee8 = 0x32 (0x59b1f0).
const Site kDraw{"display_options_draw", 0x59af60, {0xa1, 0x20, 0xc8, 0x2b, 0x01, 0x83, 0xf8, 0x01}};
// screen_draw_background 0x513b30: __cdecl (1 argument: force; game_frame pushes 1 and pops 4), ret; the background
// of game_screen drawn again (Emperor's 0x5371f0, the same first bytes).
const Site kScreenBackground{"screen_draw_background", 0x513b30, {0x8b, 0x44, 0x24, 0x04, 0xb1, 0x01, 0x85, 0xc0}};
// screen_draw 0x514020: __cdecl, no arguments, ret (0x5137d1 calls it with nothing pushed); the foreground of
// game_screen, the city first on the city screens (Emperor's 0x537690).
const Site kScreenDraw{"screen_draw", 0x514020, {0x33, 0xc0, 0x53, 0xa0, 0xdd, 0xa6, 0xf2, 0x00}};
// display_options_close 0x401f70: __cdecl, no arguments, ret; the dialog's OK (its fourth button): game_screen =
// options_return_screen, screen_redraw = 1. Zeus puts nothing back there.
const Site kClose{"display_options_close", 0x401f70, {0xa0, 0xa8, 0xf9, 0xf9, 0x00, 0xc6, 0x05, 0xce}};
// options_menu_display 0x401db0: __cdecl, no arguments, ret (Options > Display options, through the menu's table):
// options_return_screen = screen_beneath, game_screen = 0x0a, 0x10dfee8 = 0, screen_redraw = 1.
const Site kOpen{"options_menu_display", 0x401db0, {0xa1, 0x94, 0x18, 0x78, 0x00, 0xc6, 0x05, 0xdd}};

using Void_fn = void(__cdecl *)();
using ScreenBackground_fn = void(__cdecl *)(int force);

// The three places in screen_handle_input (case 0x0a) that pass the button table: push 4; push 0x5ed528 before
// buttons_mouse_over 0x432300, buttons_hover_index 0x513180 (-> display_options_hover) and buttons_handle_click
// 0x4321e0.
constexpr DWORD kTablePushes[] = {0x5154d3, 0x5154f1, 0x51550f};
const BYTE kTablePushBytes[7] = {0x6a, 0x04, 0x68, 0x28, 0xd5, 0x5e, 0x00};

const Var<int> kHover{0x781844};          // display_options_hover: the button under the mouse (1-based, 0 none)
const Var<unsigned char> kGameScreen{0xf2a6dd};
const Var<int> kReturnScreen{0xf9f9a8};   // options_return_screen
const Var<int> kBeneath{0x781894};        // screen_beneath: the screen under the drop-down menu
const Var<int> kScreenWidth{0xf2a6a0};    // dd_width / dd_height: the size the game draws now
const Var<int> kScreenHeight{0xf2a69c};
const Var<int> kDrawDone{0x10dfee8};      // display_options_draw sets it to 0x32 (opening sets 0)
constexpr unsigned char kScreenDisplayOptions = 0x0a;

// The game's button record (display_options_buttons 0x5ed528, the same 28-byte layout as Emperor's): the rectangle
// (x1, y1, x2, y2, exclusive) relative to dialog_offset; buttons_handle_click calls onRelease when the mouse is let go
// on the button it went down on, onClick on a click event (the original's is the empty 0x401000); kind 1 = left
// button; +0x11 is set when pressed.
struct GameButton {
    short x1, y1, x2, y2;
    void(__cdecl *onRelease)();
    void(__cdecl *onClick)();
    BYTE unknown10, pressed, unknown12, kind;
    int param;
    int param2;
};
static_assert(sizeof(GameButton) == 28, "the game's button record");

GameButton g_buttons[dd::kItemCount];
bool g_installed = false;
bool g_behindLogged = false;

void __cdecl NoClick()
{
}

void BuildTable()
{
    for (int i = 0; i < dd::kItemCount; ++i) {
        const dd::Item &it = dd::Items()[i];
        GameButton &b = g_buttons[i];
        b = GameButton{};
        b.x1 = static_cast<short>(it.x);
        b.y1 = static_cast<short>(it.y);
        b.x2 = static_cast<short>(it.x + it.w);
        b.y2 = static_cast<short>(it.y + it.h);
        b.onRelease = dd::HandlerOf(it.id);
        b.onClick = &NoClick;
        b.kind = 1;
        b.param = it.id;
    }
}

bool PushesAsExpected()
{
    for (DWORD at : kTablePushes) {
        if (std::memcmp(At<const BYTE>(at), kTablePushBytes, sizeof(kTablePushBytes)) != 0) {
            core_log("display options: the button table at 0x%lx is not passed as expected; the game's own dialog "
                     "stays",
                     at);
            return false;
        }
    }
    return true;
}

bool PatchTable(bool ours)
{
    BYTE bytes[7];
    std::memcpy(bytes, kTablePushBytes, sizeof(bytes));
    if (ours) {
        bytes[1] = static_cast<BYTE>(dd::kItemCount);
        const DWORD table = reinterpret_cast<DWORD>(g_buttons);
        std::memcpy(bytes + 3, &table, 4);
    }
    bool ok = true;
    for (DWORD at : kTablePushes) {
        ok = core::site::PatchCode(at + g_imageShift, bytes, sizeof(bytes)) && ok;
    }
    return ok;
}

// ---------------------------------------------------------------------------------------------
// The core's dialog drawn with the painter (game thread, from screen_draw)

void Close()
{
    Function<Void_fn>(kClose)();
}

bool DialogShown()
{
    return *kGameScreen == kScreenDisplayOptions;
}

void ScreenSize(int *w, int *h)
{
    *w = *kScreenWidth;
    *h = *kScreenHeight;
}

int Hover()
{
    return *kHover;
}

// The screens the game draws the city for (games/zeus/resolution), and 0x07 (the city with a building menu open:
// a city screen for the keys) [inferred: the drop-down menu's screen underneath is one of these in play].
bool CityScreen(int s)
{
    return s == 0x01 || s == 0x07 || s == 0x20 || s == 0x48;
}

bool g_behindDrawn = false;
// The game's drawing of the city behind the dialog has its own guard, inside timing's drawing that never counts: a
// fault in it ends there (the dialog's screen and hover put back), so timing still puts the city's state back and
// leaves its drawing depth as it was; the dialog stays, without the city drawn behind it from then on.
core::diag::Feature g_behindFeature{"city behind the Display options dialog (games/zeus/display_options)"};

void GameDrawsBehind(void *)
{
    Function<ScreenBackground_fn>(kScreenBackground)(1);
    Function<Void_fn>(kScreenDraw)();
}

void DrawBehindNow(void *)
{
    g_behindDrawn = true;
    core::diag::Restore undo;
    undo.Save(kGameScreen.ptr());
    undo.Save(kHover.ptr());
    *kGameScreen = static_cast<unsigned char>(*kReturnScreen);
    if (core::diag::Guarded(g_behindFeature, &GameDrawsBehind, nullptr, &undo)) {
        undo.Undo(); // the dialog's screen and hover back
    }
    else {
        core_log("display options: drawing the city behind the dialog failed; it is not drawn again");
    }
}

// The city behind the dialog drawn again, as the screen it came from draws it, inside timing's drawing that never
// counts (the city's animations and drawing counters put back). Without that put-back it is not drawn: the game's own
// dialog never draws behind itself either.
bool DrawBehind()
{
    if (!CityScreen(*kReturnScreen & 0xff) || !core::diag::Enabled(g_behindFeature)) {
        return true; // not a city screen, or switched off after an error: left as it is
    }
    g_behindDrawn = false;
    zeus::timing::PutBackDrawingOutside(&DrawBehindNow, nullptr);
    if (!g_behindDrawn && !g_behindLogged) {
        g_behindLogged = true;
        core_log("display options: the city behind the dialog is not drawn again (timing's put-back is not working)");
    }
    return true;
}

void Panel(int x, int y, int cols, int rows)
{
    zeus::painter::BeginDraw(); // the clip and the origin (after a drawing behind)
    zeus::painter::Get().Panel(x, y, cols * 16, rows * 16);
}

void Button(const dd::Item &it, bool hovered)
{
    zeus::painter::DialogButton(it.x, it.y, it.w, hovered);
}

void ArrowButton(const dd::Item &it, bool pressed)
{
    zeus::painter::Get().ArrowButton(it.x, it.y, it.arrow == dd::Arrow::Down ? co::Arrow::Down : co::Arrow::Up,
                                     pressed ? co::ArrowState::Pressed : co::ArrowState::Normal);
}

// The game's fonts carry their own colours (games/zeus/painter): the button under the mouse shows by its bar.
void Centered(const char *s, int x, int y, int width, co::Font f)
{
    co::Painter &p = zeus::painter::Get();
    p.Text(s, x + (width - p.TextWidth(s, f)) / 2, y, f);
}

void GameTextBox(dd::GameText t, int x, int y, int width, dd::TextLook)
{
    const char *s = zeus::keys::GameText(dd::kTextGroup, dd::kTextIndex[static_cast<int>(t)]);
    if (s && *s) {
        Centered(s, x, y, width, t == dd::GameText::Title ? co::Font::Title : co::Font::Normal);
    }
}

void Text(const char *s, int x, int y, int width, dd::TextLook)
{
    Centered(s, x, y, width, co::Font::Normal);
}

void EndDraw()
{
    zeus::painter::BeginDraw(); // the clip back to the screen, as the game's own drawing ends
    *kDrawDone = 0x32;
}

const dd::Game kGame(&Close, &DialogShown, &ScreenSize, &Hover, &DrawBehind, &Panel, &Button, &ArrowButton,
                     &GameTextBox, &Text, &EndDraw);

using Draw_fn = void(__cdecl *)();
Draw_fn o_Draw;

void Fallback(const char *why)
{
    if (g_installed) {
        g_installed = false;
        PatchTable(false);
        core_log("display options: the game's own dialog from now on (%s)", why);
    }
}

void __cdecl Draw_detour()
{
    if (g_installed && dd::Working()) {
        if (dd::Draw()) {
            return;
        }
        Fallback("drawing failed");
    }
    else if (g_installed) {
        Fallback("switched off after an error");
    }
    Original(o_Draw, kDraw)();
}

const Detour kDetours[] = {{&kDraw, reinterpret_cast<void *>(&Draw_detour)}};

bool InstallOurs()
{
    BuildTable();
    dd::Attach(kGame);
    g_installed = true;
    if (!PatchTable(true)) {
        Fallback("the button table could not be replaced");
        return false;
    }
    return true;
}

} // namespace

bool Attach(bool painter)
{
    if (!painter) {
        core_log("display options: our drawing of the game's art is not available; the game's own dialog stays");
        return false;
    }
    if (!zeus::keys::LangHooked()) {
        core_log("display options: lang_get_string is not hooked (the dialog's texts); the game's own dialog stays");
        return false;
    }
    const Site *sites[] = {&kDraw, &kScreenBackground, &kScreenDraw, &kClose, &kOpen};
    for (const Site *s : sites) {
        if (!Matches(*s)) {
            core_log("display options: %s at 0x%lx does not match; the game's own dialog stays", s->name, s->address);
            return false;
        }
    }
    if (!PushesAsExpected()) {
        return false;
    }
    if (!core::site::Install(kDraw, &Draw_detour, o_Draw)) {
        core_log("display options: display_options_draw could not be hooked; the game's own dialog stays");
        return false;
    }
    if (!InstallOurs()) {
        return false;
    }
    core_log("display options: installed (window, 1024x768, display scale arrows, 100%% and Auto, scaling, OK, "
             "Cancel)");
    return true;
}

void OnLoop()
{
    dd::OnLoop();
}

bool OpenForTest()
{
    if (!g_installed) {
        return false;
    }
    *kBeneath = *kGameScreen;
    Function<Void_fn>(kOpen)();
    return true;
}

bool AttachForTest()
{
    return PushesAsExpected() && InstallOurs();
}

const Detour *Detours(int *count)
{
    *count = static_cast<int>(sizeof(kDetours) / sizeof(kDetours[0]));
    return kDetours;
}

} // namespace zeus::display_options
