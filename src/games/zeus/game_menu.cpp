#include "games/zeus/game_menu.h"

#include "core/diag/guard.h"
#include "core/display/virtual_display.h"
#include "core/log.h"
#include "core/options/menu_text.h"
#include "core/options/pause_menu.h"
#include "games/zeus/painter.h"
#include "games/zeus/site.h"

#include <string>
#include <vector>

// Evidence: Zeus.exe 2.1.4.0, SHA256 382bcd28...0a57.
// The game's menus, dialogs and every screen's right click read from the
// decompilation; calling conventions checked by disassembly.
namespace zeus::game_menu {
namespace {

namespace pm = core::options::pause_menu;
namespace mt = core::options::menu_text;

// modal_dialog_run 0x5a0980: __cdecl (title, text, buttons), plain ret; buttons 4: Yes / No, returns 6 for Yes; runs
// its own message loop (game_screen 0x54 meanwhile).
const Site kModalDialog{"modal_dialog_run", 0x5a0980, {0x83, 0xec, 0x20, 0x53, 0x33, 0xc0, 0xa0, 0xdd}};
using ModalDialog_fn = int(__cdecl *)(const char *title, const char *text, int buttons);
constexpr int kYesNo = 4, kYes = 6;
// The game's File menu (0x5efcc0): New adventure (to the game choice 0x30, as Yongle's Exit to main menu goes back to
// the menus), Replay episode, Load / Save / Delete adventure. __cdecl, no arguments (the menu calls them with
// button_param 0xf135f8 = 0, the items' +0xc).
const Site kNewAdventure{"file_menu_new_adventure", 0x401170, {0x53, 0xe8, 0x3a, 0xee, 0x00, 0x00, 0xe8, 0x55}};
const Site kReplay{"file_menu_replay_episode", 0x421730, {0x83, 0xec, 0x64, 0x53, 0x56, 0x57, 0x68, 0x92}};
const Site kLoad{"file_menu_load", 0x4014d0, {0x81, 0xec, 0x04, 0x01, 0x00, 0x00, 0x55, 0x56}};
const Site kSave{"file_menu_save", 0x401680, {0x81, 0xec, 0x04, 0x01, 0x00, 0x00, 0x53, 0x56}};
const Site kDelete{"file_menu_delete", 0x401770, {0x81, 0xec, 0x04, 0x01, 0x00, 0x00, 0x53, 0x56}};
// What a right click on the city calls (screen_handle_input): drops the building tool, back to game_screen 1;
// __cdecl, no arguments, returns al.
const Site kCityCancel{"city_cancel_tool", 0x404c70, {0x53, 0x8a, 0x1d, 0xdd, 0xa6, 0xf2, 0x00, 0xba}};
// The world map's "back to the city": __cdecl, no arguments.
const Site kMapToCity{"world_map_to_city", 0x408bb0, {0x83, 0x3d, 0x2c, 0x5e, 0x66, 0x00, 0x01, 0x75}};
// input_update_mouse 0x50b470: __cdecl, no arguments; the buttons' edges of this pass. A pending right click of ours
// is added after it (mouse_right_released 0xf2a6fc).
const Site kInputMouse{"input_update_mouse", 0x50b470, {0x83, 0xec, 0x0c, 0x8d, 0x44, 0x24, 0x04, 0x53}};
// ui_sound(n) 0x5a4c00: __cdecl, 1 argument, plain ret; input_update_mouse plays sound 0 at each left press.
const Site kUiSound{"ui_sound", 0x5a4c00, {0xa1, 0x30, 0x42, 0x0e, 0x01, 0x85, 0xc0, 0x74}};

// The editors' File menus (pe_dump.py: city map 0x5efdd0, world map 0x5efe20, adventure editor 0x5effa0); __cdecl, no
// arguments; called with button_param = the item's +0xc (1 for Create Adventure, 0 for the others).
const Site kNewMap{"editor_menu_new_map", 0x4081f0, {0xb9, 0x08, 0xa8, 0x60, 0x01, 0xe8, 0xd6, 0x6c}};
const Site kRevertMap{"editor_menu_revert_map", 0x408250, {0xc7, 0x05, 0xf8, 0x35, 0xf1, 0x00, 0x01, 0x00}};
const Site kSaveMap{"editor_menu_save_map", 0x408600, {0xa0, 0xdd, 0xa6, 0xf2, 0x00, 0xc6, 0x05, 0x3d}};
const Site kImportCity{"editor_menu_import_map", 0x51ebe0, {0x56, 0x57, 0x6a, 0x01, 0x6a, 0x00, 0xc6, 0x05}};
const Site kImportWorld{"editor_menu_import_world", 0x51ec50, {0x56, 0x57, 0x6a, 0x01, 0x6a, 0x00, 0xe8, 0xf5}};
const Site kEditorReturn{"editor_menu_main_menu", 0x401cd0, {0xa0, 0xdd, 0xa6, 0xf2, 0x00, 0x53, 0x3c, 0x01}};
const Site kNewAdv{"editor_menu_new_adventure", 0x425120, {0xe8, 0xeb, 0x1a, 0x00, 0x00, 0x83, 0xf8, 0x02}};
const Site kLoadAdv{"editor_menu_load_adventure", 0x427100, {0xb9, 0x50, 0xa7, 0x6a, 0x00, 0xe8, 0x16, 0xfb}};
const Site kSaveSettings{"editor_menu_save_settings", 0x427140, {0x6a, 0x01, 0xb9, 0x50, 0xa7, 0x6a, 0x00, 0xe8}};
const Site kSaveAs{"editor_menu_save_as", 0x425190, {0x56, 0x57, 0xb9, 0x41, 0x00, 0x00, 0x00, 0x33}};
const Site kCreateAdv{"editor_menu_create_adventure", 0x42a8e0, {0xb9, 0x50, 0xa7, 0x6a, 0x00, 0xe9, 0x06, 0x00}};

using Void_fn = void(__cdecl *)();
using Bool_fn = bool(__cdecl *)();
using UiSound_fn = void(__cdecl *)(int);
Void_fn o_InputMouse = nullptr;

// lang_get_string 0x507bf0 (__cdecl(group, index); hooked by games/zeus/keys for the pause banner, which passes every
// other text on unchanged).
const DWORD kLangString = 0x507bf0;
using Lang_fn = const char *(__cdecl *)(int group, int index);

const Var<unsigned char> kGameScreen{0xf2a6dd};
const Var<char> kRightUp{0xf2a6fc};        // mouse_right_released: the dialogs close
const Var<int> kMapSelected{0x903120};     // the world map's selection (0: none); its right click clears it
const Var<int> kMapSelected2{0x5fb7d0};    // ... its second one (-1: none)
const Var<char> kQuit{0xf1389d};           // quit_request
const Var<char> kRedraw{0xf2a6ce};         // screen_redraw
const Var<char> kMouseButtons{0xf2a70b};   // cleared after each confirmation, as the game's own Esc does
const Var<char> kModalDone{0x10df2e8};     // modal_dialog_done
const Var<int> kModalChoice{0x10df2ec};    // modal_dialog_choice
const Var<int> kModalButtons{0x10df2f0};   // modal_dialog_buttons
const Var<int> kBuildType{0x665dcc};       // the building tool (0: none)
const Var<int> kBuilding{0x665dc8};        // a building being placed
const Var<int> kEditor{0x665e2c};          // editor_mode: 1 in the editors
const Var<char> kEditorTool{0xf135ec};     // the city map editor's placing flag: its right click drops the tool (0x51e3b0)
const Var<int> kButtonParam{0xf135f8};     // button_param
const Var<int> kDialogX{0xf2a6ac}, kDialogY{0xf2a6a8}; // dialog_offset: the game's 640 x 480 dialogs
// The list pop-up (screen 0x0f; object 0x160a808, set up by 0x5beed0 / 0x5befe0, drawn by 0x5bf1e0): its screen x, y
// (+0xc, +0x10), rows per column (+8), item count (+0x14), column width (+0x1a0), columns (+0x1a4). The editor's New map
// puts it at 50, 50 (0x4081f0: under the File menu).
const Var<int> kListX{0x160a814}, kListY{0x160a818}, kListPerColumn{0x160a810}, kListCount{0x160a81c};
const Var<int> kListColumnW{0x160a9a8}, kListColumns{0x160a9ac};
constexpr int kListFromFileMenu = 50;

const Var<void *> kDrawBuffer{0xf9c100};   // draw_buffer: 0 while the game makes its surfaces again
// screen_present's call in the window procedure's WM_PAINT: nothing of ours is drawn on that frame.
constexpr DWORD kPresentFromPaint = 0x5c4ccd;

core::diag::Feature g_feature{"Esc and the pause menu (games/zeus/game_menu)"};
bool g_attached = false;
bool g_rightClick = false; // a right click for the next input pass

const char *Lang(int group, int index)
{
    const char *t = reinterpret_cast<Lang_fn>(kLangString + g_imageShift)(group, index);
    return t ? t : "";
}

// Screens that close with a right click: the dialogs and pop-ups. The dialogs' code does
// not look at editor_mode, so the editors use this list too.
bool RightClickScreen(int s)
{
    switch (s) {
    case 0x03: case 0x05: case 0x06: case 0x07: case 0x08: case 0x09: case 0x0a: case 0x0b: case 0x0c: case 0x0d:
    case 0x0e: case 0x0f: case 0x10: case 0x11: case 0x1a: case 0x1d: case 0x21: case 0x22: case 0x3b: case 0x3f:
    case 0x45:
        return true;
    default:
        return false;
    }
}

// The menu screens that go back a step with a right click (C5 and 0.6): tutorials 0x02, choose an adventure 0x1f, the
// family 0x2f, choose a game 0x30, the name entry 0x36, mission choice 0x3d, 0x51, 0x53. The mission intro 0x35 does
// not read a right click: listed as Yongle lists it, so Esc does nothing there.
bool MenuBackScreen(int s)
{
    switch (s) {
    case 0x02: case 0x1f: case 0x2f: case 0x30: case 0x35: case 0x36: case 0x3d: case 0x51: case 0x53:
        return true;
    default:
        return false;
    }
}

// The screens where Esc does nothing (not the game's "Exit Game?", not back either, as the mission
// intro): the timed full-screen picture 0x04, the final ratings 0x33, the demo and end screens 0x37-0x39, the
// scoreboard 0x3a, the victory screens 0x42 / 0x43. Their right click moves on or ends the game,
// so no right click is added either. The videos (0x4a) are the game's: Esc skips them.
bool NoEscScreen(int s)
{
    switch (s) {
    case 0x04: case 0x33: case 0x37: case 0x38: case 0x39: case 0x3a: case 0x42: case 0x43:
        return true;
    default:
        return false;
    }
}

// The editors' pages that go back with a right click (0.6): event editing 0x18, the adventure editor's pages 0x1b,
// 0x1c, 0x27, 0x29, 0x2b, 0x2c, 0x2e, 0x46.
bool EditorBackScreen(int s)
{
    switch (s) {
    case 0x18: case 0x1b: case 0x1c: case 0x27: case 0x29: case 0x2b: case 0x2c: case 0x2e: case 0x46:
        return true;
    default:
        return false;
    }
}

// The editor screens with a pause menu: the city map 0x01, the world map 0x15, the adventure
// editor 0x57. Not 0x58 (its Return to Main Menu does not ask to save).
bool EditorPauseScreen(int s)
{
    return s == 0x01 || s == 0x15 || s == 0x57;
}

// modal_dialog_draw 0x5a04f0: the title centred in the box's width from x + 16 (sized by group 6, drawn in group 7),
// the text in the width - 10 from x + 5 (core/options/pause_menu LayOutConfirm puts both in the panel's middle).
const pm::ConfirmBox &Box()
{
    static const pm::ConfirmBox box(16, 5, &zeus::painter::ConfirmTitleWidth, &zeus::painter::ConfirmTitleDrawWidth,
                                    &zeus::painter::ConfirmTextWidth);
    return box;
}

// minWidth: the box at least this wide (the text's measure; 0: as its texts make it).
bool Confirm(const char *title0, const char *text0, int minWidth = 0)
{
    std::string title = title0, text = text0;
    pm::LayOutConfirm(Box(), &title, &text, minWidth);
    const int r = Function<ModalDialog_fn>(kModalDialog)(title.c_str(), text.c_str(), kYesNo);
    *kMouseButtons = 0;
    return r == kYes;
}

// ---------------------------------------------------------------------------------------------------------------
// The pause menus' items

enum class Act { Continue, Replay, Load, Save, Delete, MainMenu, Exit, Call };

struct Item {
    Act act;
    const char *label;  // ours, or nullptr: the game's text (group, index)
    int group, index;
    const Site *call;   // Act::Call: the editor File menu's function
    int param;          // ... and its button_param
};

// The game: Continue, Replay map, Load / Save / Delete adventure, Exit to main menu, Exit game (Yongle's order; the
// game's texts for Load (1, 3), Save (1, 4), Delete (1, 6), Exit game (1, 5): Zeus's 5 and 6 are Emperor's 6 and 5).
// Options: later, with Pericles' own options window.
const Item kGameItems[] = {
    {Act::Continue, nullptr, 0, 0, nullptr, 0},       {Act::Replay, nullptr, 0, 0, nullptr, 0},
    {Act::Load, nullptr, 1, 3, nullptr, 0},           {Act::Save, nullptr, 1, 4, nullptr, 0},
    {Act::Delete, nullptr, 1, 6, nullptr, 0},         {Act::MainMenu, nullptr, 0, 0, nullptr, 0},
    {Act::Exit, nullptr, 1, 5, nullptr, 0},
};
// The editors: Continue and that screen's File menu, the same order and the game's texts; no Exit game
// (leaving an editor goes through Return to Main Menu / Exit Editor, which ask to save).
const Item kCityMapItems[] = {
    {Act::Continue, nullptr, 0, 0, nullptr, 0},       {Act::Call, nullptr, 7, 1, &kNewMap, 0},
    {Act::Call, nullptr, 7, 2, &kRevertMap, 0},       {Act::Call, nullptr, 7, 3, &kSaveMap, 0},
    {Act::Call, nullptr, 7, 4, &kImportCity, 0},      {Act::Call, nullptr, 7, 5, &kEditorReturn, 0},
};
const Item kWorldMapItems[] = {
    {Act::Continue, nullptr, 0, 0, nullptr, 0},       {Act::Call, nullptr, 7, 6, &kRevertMap, 0},
    {Act::Call, nullptr, 7, 3, &kSaveMap, 0},         {Act::Call, nullptr, 7, 4, &kImportWorld, 0},
    {Act::Call, nullptr, 7, 5, &kEditorReturn, 0},
};
const Item kAdventureItems[] = {
    {Act::Continue, nullptr, 0, 0, nullptr, 0},       {Act::Call, nullptr, 93, 1, &kNewAdv, 0},
    {Act::Call, nullptr, 93, 2, &kLoadAdv, 0},        {Act::Call, nullptr, 93, 3, &kSaveSettings, 0},
    {Act::Call, nullptr, 93, 4, &kSaveAs, 0},         {Act::Call, nullptr, 93, 5, &kCreateAdv, 1},
    {Act::Call, nullptr, 93, 6, &kEditorReturn, 0},
};

const Item *g_items = kGameItems; // the open menu's items
int g_count = 0;
const Item *g_pending = nullptr;  // the item to carry out at the next loop (set with pause_menu's Request)

const char *Label(const Item &it)
{
    switch (it.act) {
    case Act::Continue: return mt::kPauseContinue;
    case Act::Replay: return mt::kMenuReplayMap;
    case Act::MainMenu: return mt::kMenuExitToMainMenu;
    default: return Lang(it.group, it.index);
    }
}

bool OpenMenu(const Item *items, int count)
{
    std::vector<std::string> labels;
    for (int i = 0; i < count; ++i) {
        labels.emplace_back(Label(items[i]));
    }
    if (pm::IsOpen()) {
        return true;
    }
    g_items = items;
    g_count = count;
    return pm::Open(mt::kPauseTitle, std::move(labels));
}

template <size_t N>
bool OpenMenu(const Item (&items)[N])
{
    return OpenMenu(items, static_cast<int>(N));
}

void PauseAct(int i)
{
    if (i < 0 || i >= g_count) {
        return;
    }
    const Item &it = g_items[i];
    if (it.act == Act::Continue) {
        pm::Close("Continue");
        return;
    }
    pm::Close(Label(it));
    g_pending = &it;
    pm::Request(i);
}

bool NoOptionsKey(unsigned, unsigned)
{
    return false; // Pericles has no options window yet: no options key in the pause menu
}

void Redraw()
{
    *kRedraw = 1;
}

void ClickSound()
{
    Function<UiSound_fn>(kUiSound)(0);
}

bool CanDraw(unsigned long caller)
{
    return caller != kPresentFromPaint && *kDrawBuffer != nullptr;
}

const pm::PauseGame &PauseGame()
{
    static const pm::PauseGame game("zeus", zeus::painter::Get(), zeus::painter::MenuLook(), &zeus::painter::DialogOrigin,
                                    &zeus::painter::BeginDraw, &Redraw, &ClickSound, &CanDraw, &PauseAct,
                                    &NoOptionsKey);
    return game;
}

void ReplayNow()
{
    Function<Void_fn>(kReplay)();
}

void LeaveNow()
{
    Function<Void_fn>(kNewAdventure)();
}

void QuitNow()
{
    *kQuit = 1;
    *kRedraw = 1;
}

// Replay map / Exit to main menu ask first: our text (Zeus's texts have no "leave the
// current game"), the button's name as the title; answered No from the pause menu, the pause menu again.
void Replay(bool fromPause)
{
    pm::AfterConfirm(Confirm(mt::kMenuReplayMap, mt::kLeaveGame), fromPause, &ReplayNow, "game menu", "replay");
}

void MainMenu(bool fromPause)
{
    pm::AfterConfirm(Confirm(mt::kMenuExitToMainMenu, mt::kLeaveGame), fromPause, &LeaveNow, "game menu",
                     "back to the game choice");
}

// Title "Quit", text "Exit Game?" (Yongle's way round; the game's own box has them the other way). The short title
// would make the box small, so it is at least as wide as the game's own box ("Exit Game?" in the title's font).
void Exit(bool fromPause)
{
    const bool yes = Confirm(Lang(5, 0), Lang(5, 1), zeus::painter::ConfirmTitleWidth(Lang(5, 1)));
    pm::AfterConfirm(yes, fromPause, &QuitNow, "game menu", "exit");
}

// The editor's New map opens its list pop-up under the File menu (50, 50): from our pause menu it goes in the middle
// of the dialogs' area instead. Its size as 0x5bf1e0 draws it (whole 16 px tiles); its buttons are relative to x, y.
void CentreListPopUp()
{
    if (*kGameScreen != 0x0f || *kListX != kListFromFileMenu || *kListY != kListFromFileMenu) {
        return;
    }
    const int perColumn = *kListPerColumn, count = *kListCount, columnW = (*kListColumnW + 15) / 16;
    int w = 0, h = 0;
    if (perColumn < count) {
        h = (perColumn * 20 + 24) / 16 * 16;
        w = columnW * *kListColumns * 16;
    }
    else {
        h = (count * 21 + 24) / 16 * 16;
        w = columnW * 16;
    }
    *kListX = *kDialogX + (640 - w) / 2;
    *kListY = *kDialogY + (480 - h) / 2;
    core_log("game menu: the list pop-up (%d x %d) in the middle at %d, %d", w, h, *kListX, *kListY);
}

void RunPending(void *p)
{
    const Item &it = *static_cast<const Item *>(p);
    switch (it.act) {
    case Act::Replay: Replay(true); break;
    case Act::MainMenu: MainMenu(true); break;
    case Act::Exit: Exit(true); break;
    case Act::Load: *kButtonParam = 0; Function<Void_fn>(kLoad)(); break;
    case Act::Save: *kButtonParam = 0; Function<Void_fn>(kSave)(); break;
    case Act::Delete: *kButtonParam = 0; Function<Void_fn>(kDelete)(); break;
    case Act::Call: // as the File menu calls it (0x431fce): button_param = the item's +0xc
        core_log("game menu: editor %s", it.call->name);
        *kButtonParam = it.param;
        Function<Void_fn>(*it.call)();
        CentreListPopUp();
        break;
    default: break;
    }
    // The item's screen draws only itself over the last frame (the list pop-up): the screen under it again, so the
    // pause menu just closed does not stay behind it.
    *kRedraw = 1;
}

// ---------------------------------------------------------------------------------------------------------------
// Esc

struct EscCall {
    LPARAM lparam;
    bool handled;
};

// The editors (editor_mode 1): Esc as a right click where the game closes or goes back with one; the pause menu on
// the three editor screens (the city map only when no tool is held: its right click drops the tool first); the
// game's own Esc elsewhere.
void EditorEscape(int s, EscCall &c)
{
    const bool held = (c.lparam & (1 << 30)) != 0;
    const bool ours = RightClickScreen(s) || EditorBackScreen(s) || MenuBackScreen(s) || EditorPauseScreen(s);
    if (held) {
        c.handled = ours; // repeats do nothing more
        return;
    }
    // The city map editor holding a brush: build_tool is set (28 for a brush; 0 after the right click, seen in a
    // test log): the right click drops it first.
    const bool holding = s == 0x01 && (*kBuildType != 0 || *kEditorTool != 0);
    if (RightClickScreen(s) || EditorBackScreen(s) || MenuBackScreen(s) || holding) {
        g_rightClick = true;
        c.handled = true;
        return;
    }
    if (s == 0x01) {
        c.handled = OpenMenu(kCityMapItems);
    }
    else if (s == 0x15) {
        c.handled = OpenMenu(kWorldMapItems);
    }
    else if (s == 0x57) {
        c.handled = OpenMenu(kAdventureItems);
    }
}

void EscapeNow(void *p)
{
    EscCall &c = *static_cast<EscCall *>(p);
    const int s = *kGameScreen;
    if (s == 0x54) { // a confirmation: its last button (No / Cancel)
        const int n = *kModalButtons;
        if (n > 0 && (c.lparam & (1 << 30)) == 0) {
            *kModalChoice = n - 1;
            *kModalDone = 1;
        }
        c.handled = true;
        return;
    }
    if (c.lparam == 0) {
        return; // posted by the game (File > Exit game, Alt+X): the game's own Esc
    }
    if (*kEditor == 1) {
        EditorEscape(s, c);
        return;
    }
    const bool city = s == 0x01 || s == 0x20 || s == 0x48;
    if ((c.lparam & (1 << 30)) != 0) { // held: repeats do nothing more
        c.handled = s == 0 || city || s == 0x14 || RightClickScreen(s) || MenuBackScreen(s) || NoEscScreen(s);
        return;
    }
    if (NoEscScreen(s)) {
        c.handled = true; // nothing at all
        return;
    }
    if (s == 0) { // the main menu: the game quits at once; ours asks first (as Yongle), the same box as Exit game
        c.handled = true;
        Exit(false);
        return;
    }
    if (MenuBackScreen(s)) {
        g_rightClick = true;
        c.handled = true;
        return;
    }
    if (s == 0x14) { // the world map: its selection let go as its right click does, else back to the city
        if (*kMapSelected != 0 || *kMapSelected2 != -1) {
            *kMapSelected = 0;
            *kMapSelected2 = -1;
            *kRedraw = 1;
        }
        else {
            Function<Void_fn>(kMapToCity)();
        }
        c.handled = true;
        return;
    }
    if (city) {
        if (s == 0x01 && (*kBuildType != 0 || *kBuilding != 0)) {
            Function<Bool_fn>(kCityCancel)(); // as a right click: the tool dropped
            c.handled = true;
            return;
        }
        if (s != 0x01) { // a selected company 0x48 or ship 0x20: let go by the game's own right click
            g_rightClick = true;
            c.handled = true;
            return;
        }
        c.handled = OpenMenu(kGameItems);
        return;
    }
    if (RightClickScreen(s)) {
        g_rightClick = true;
        c.handled = true;
    }
}

void __cdecl InputMouse_detour()
{
    Original(o_InputMouse, kInputMouse)();
    if (g_rightClick) {
        g_rightClick = false;
        *kRightUp = 1;
    }
}

// The pause menu's input: Pericles registers core::display's one modal input slot for it (no options window yet).
void ModalMessage(UINT m, WPARAM w, LPARAM l, int x, int y)
{
    pm::HandleMessage(m, w, l, x, y);
}

const core::display::ModalInput kInput{&pm::Active, &ModalMessage};

} // namespace

bool Attach(bool painter, bool presentHooked)
{
    if (!painter || !presentHooked) {
        core_log("game menu: %s; Esc and the File menu stay the game's",
                 !painter ? "the painter's sites do not match" : "screen_present is not hooked (the menu is drawn there)");
        return false;
    }
    const Site *sites[] = {&kModalDialog, &kNewAdventure, &kReplay, &kLoad, &kSave, &kDelete, &kCityCancel,
                           &kMapToCity, &kInputMouse, &kUiSound};
    for (const Site *s : sites) {
        if (!Matches(*s)) {
            core_log("game menu: %s at 0x%lx does not match; Esc and the pause menu stay the game's", s->name,
                     s->address);
            return false;
        }
    }
    // The editors' items: one that does not match leaves the whole of Esc and the pause menu the game's (as the other
    // sites; the game's own menus work either way).
    const Site *editor[] = {&kNewMap, &kRevertMap, &kSaveMap, &kImportCity, &kImportWorld, &kEditorReturn,
                            &kNewAdv, &kLoadAdv, &kSaveSettings, &kSaveAs, &kCreateAdv};
    for (const Site *s : editor) {
        if (!Matches(*s)) {
            core_log("game menu: %s at 0x%lx does not match; Esc and the pause menu stay the game's", s->name,
                     s->address);
            return false;
        }
    }
    if (!Install(kInputMouse, &InputMouse_detour, o_InputMouse)) {
        core_log("game menu: input_update_mouse could not be hooked; Esc and the pause menu stay the game's");
        return false;
    }
    pm::Attach(PauseGame());
    core::display::SetModalInput(&kInput);
    g_attached = true;
    core_log("game menu: Esc closes windows (as a right click), goes back on the menu screens (as a right click), "
             "asks before quitting on the main menu and opens the pause menu on the city screen and in the editors; "
             "confirmations take Esc as No");
    return true;
}

bool Escape(LPARAM lparam)
{
    if (!g_attached || !core::diag::Enabled(g_feature)) {
        return false;
    }
    EscCall c{lparam, false};
    if (!core::diag::Guarded(g_feature, &EscapeNow, &c)) {
        g_rightClick = false;
        pm::Close("an error in Esc");
        core_log("game menu: off after an error; Esc is the game's own again");
        return false;
    }
    return c.handled;
}

bool IsOpen()
{
    return pm::IsOpen();
}

bool OnLoop()
{
    int index = 0;
    if (!pm::TakePending(&index)) {
        return false;
    }
    const Item *it = g_pending;
    g_pending = nullptr;
    if (it == nullptr || !g_attached || !core::diag::Enabled(g_feature) ||
        !core::diag::Guarded(g_feature, &RunPending, const_cast<Item *>(it))) {
        core_log("game menu: the pause menu's item was not carried out (an error)");
    }
    return true;
}

void OnPresent(unsigned long caller)
{
    pm::OnPresent(caller);
}

void AttachForTest()
{
    pm::Attach(PauseGame());
    g_attached = true;
}

void InputMouseForTest()
{
    InputMouse_detour();
}

} // namespace zeus::game_menu
