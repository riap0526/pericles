#include "games/zeus/options_entry.h"

#include "core/capture/city_picture.h"
#include "core/diag/guard.h"
#include "core/log.h"
#include "core/options/game_options.h"
#include "core/options/menu_text.h"
#include "core/ui/game_menu_layout.h"
#include "games/zeus/game_menu.h"
#include "games/zeus/keys.h"
#include "games/zeus/options_window.h"
#include "games/zeus/painter.h"
#include "games/zeus/site.h"

#include <algorithm>
#include <initializer_list>

// Evidence (Zeus.exe 2.1.4.0, SHA256 382bcd28...0a57): the menu bars and item tables read from the file
// (0x5eff00..0x5f002f, 0x5efcc0..0x5effff), the drop-down code decompiled (0x4310e0, 0x431210, 0x431eb0); calling
// conventions checked by disassembly.
namespace zeus::options_entry {
namespace {

namespace ow = zeus::options_window;
namespace mt = core::options::menu_text;
using core::ui::game_menu_layout::Handler_fn;
using core::ui::game_menu_layout::Menu;
using core::ui::game_menu_layout::MenuItem;
using Lang_fn = const char *(__cdecl *)(int group, int index);

// menu_width(group) 0x512870 (__cdecl; ret): the drop-down's width in 16-pixel tiles, measured once at start-up from
// the group's own texts in font group 1 (0x5127c0: the widest / 16 + 2); the drop-down's drawing and hit test read it.
const Site kMenuWidth{"menu_width", 0x512870, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x04, 0x85, 0x50}};
// menu_launch_action 0x420120, case 2 ("Activision Website", the main menu's button 3): ShellExecuteA(game, "open",
// the address, ...) at 0x42018d, call dword ptr [import] (6 bytes); its result is not used. Zeus does not minimize
// the window first.
const Site kWebsiteOpen{"main_menu_website_open", 0x42018d, {0xff, 0x15, 0x64, 0x81, 0x5d, 0x00, 0x53, 0x53}};
using Width_fn = int(__cdecl *)(int group);
Width_fn o_Width = nullptr;

// The menu bars: the game's 0x5eff00 (File, Options, Help) and the editors' 0x5eff30 (the city map, 0x01: File 7,
// Options, Help, Resets), 0x5eff70 (the world map, 0x15) and 0x5f0000 (the adventure editor). Every bar's Options
// (its second menu) shows the start of the same table 0x5efd20: 7 items in the game, 3 or 2 in the editors.
const DWORD kGameBar = 0x5eff00;
const DWORD kEditorBars[] = {0x5eff30, 0x5eff70, 0x5f0000};
const DWORD kOptionsItems = 0x5efd20, kFileItems = 0x5efcc0;
constexpr int kOptionsCount = 7, kFileCount = 6;

// Text groups of Zeus_Text.eng (Zeus_Editor_Text.eng has the same ones). Options (group 2, 14 texts): 1 Display, 2
// Sound, 3 Speed, 6 Difficulty, 9 / 10 Autosave ON / OFF, 11 / 12 Auto Defend ON / OFF, 13 Messages. File (group 1, 7
// texts): 1 New adventure, 2 Replay episode, 3 Load, 4 Save, 5 Exit game, 6 Delete. Ours use numbers the groups do
// not have (the game would read past the group into the next one's: we answer them first).
constexpr int kOptionsGroup = 2, kTextGeneral = 20, kTextVideo = 21, kTextAudio = 22, kTextHotkeys = 23,
              kTextAutosave = 25, kTextAutoDefend = 26;
constexpr int kFileGroup = 1, kTextLoad = 3, kTextSave = 4, kTextExit = 5, kTextDelete = 6;
constexpr int kTextCityShot = 40, kTextMinimapShot = 41, kTextReplayMap = 42, kTextExitToMainMenu = 43;
constexpr int kMainMenuGroup = 30, kTextWebsite = 2; // "Activision Website"
// Autosave (0x402660) and Auto Defend (0x5692d0) write their ON / OFF text number into the game's table (its +2 at
// 0x5efd62, 0x5efd72; config_load at start-up too): ours show the number the game's entry has now.
constexpr int kDifficultyRow = 3, kAutosaveRow = 4, kAutoDefendRow = 5;

// The game's dialogs, the fall-back when our window is not available: Speed, Display, Sound options, Messages.
const DWORD kSpeedDialog = 0x4023d0, kDisplayDialog = 0x401db0, kSoundDialog = 0x401f90, kMessagesDialog = 0x4026a0;
const DWORD kAutoDefendToggle = 0x5692d0; // checked by games/zeus/options_original (auto_defend_toggle)
// The game's Options items in our menu: Difficulty, Autosave, Auto Defend (Display, Sound and Speed are in our window's
// pages, Messages in General > Messages).
constexpr int kFirstGameItem = 3, kGameItems = 3;
// The message list's "Click to change how messages are received" (messages_text_buttons 0x5ee458, button 1: its
// left-click handler at +0x24 = 0x5ee47c is the game's Messages dialog 0x4026a0): our General > Messages.
const DWORD kMessagesButtonHandler = 0x5ee47c;

core::diag::Feature g_texts{"menu texts (games/zeus/options_entry)"};
bool g_testing = false; // AttachForTest: the main menu's code is not patched
bool g_optionsOn = false, g_fileOn = false, g_shotsOn = false, g_pauseMenuOn = false;

void __cdecl MenuGeneral()
{
    if (!ow::Open(ow::kPageGeneral, 0)) {
        reinterpret_cast<Handler_fn>(kSpeedDialog)();
    }
}
void __cdecl MenuVideo()
{
    if (!ow::Open(ow::kPageVideo, 0)) {
        reinterpret_cast<Handler_fn>(kDisplayDialog)();
    }
}
void __cdecl MenuAudio()
{
    if (!ow::Open(ow::kPageAudio, 0)) {
        reinterpret_cast<Handler_fn>(kSoundDialog)();
    }
}

// Hotkeys: the Hotkeys window alone (the game has no dialog for it: nothing when ours is not available, logged).
void __cdecl MenuHotkeys()
{
    if (!ow::OpenHotkeys()) {
        core_log("options window: not available; the Options menu's Hotkeys does nothing");
    }
}

// Difficulty: our window's General > Difficulty, not the game's dialog (the window changes it as the dialog's arrows
// do). The window not available or the rows switched off after a fault: nothing (logged).
void __cdecl MenuDifficulty()
{
    if (!core::options::game_options::DifficultyShown() ||
        !ow::OpenCategory(ow::kPageGeneral, mt::kCategoryDifficulty)) {
        core_log("options window: the difficulty is not available now; the Options menu's Difficulty does nothing");
    }
}

// Auto Defend: not while enemies are attacking (our check, then the game's own toggle, which writes the menu's ON /
// OFF text and keeps the drop-down open). Only after a fault switched the rows off: the game's own toggle, as without
// us (wherever the menu is opened; the menu is the game's, so it is not limited to the screens with the rows).
void __cdecl MenuAutoDefend()
{
    namespace go = core::options::game_options;
    if (!go::DifficultySwitchedOff()) {
        go::ToggleAutoDefend();
        return;
    }
    core_log("options window: auto defend is switched off; the Options menu's Auto Defend is the game's own");
    reinterpret_cast<Handler_fn>(kAutoDefendToggle + g_imageShift)(); // the game's (shifted in the offline test)
}

void __cdecl MessagesButton()
{
    if (!ow::OpenCategory(ow::kPageGeneral, mt::kCategoryMessages)) {
        reinterpret_cast<Handler_fn>(kMessagesDialog)();
    }
}

void __cdecl MenuCityShot()
{
    core::capture::city_picture::RequestCity(); // the menu has put the city screen back before calling (0x431eb0)
}
void __cdecl MenuMinimapShot()
{
    core::capture::city_picture::RequestMinimap();
}

// Our tables (filled at Attach from the game's: its handlers and parameters as they are). The game's Options: General
// Settings, Video, Audio, Hotkeys, then the game's Difficulty, Autosave and Auto Defend (Yongle's order). The editors:
// General Settings, Video, Audio, Hotkeys (their menus had the game's Display, Sound (, Speed), which the window's pages
// have; the Hotkeys window is the same: scrolling, rotating the map and the bookmarks act in the map editor too).
constexpr int kOurItems = 4;
MenuItem g_options[kOurItems + kGameItems];
MenuItem g_editor[kOurItems];
// The File menu as Yongle's: Replay map, Load, Save, the screenshots, Delete, Exit to main menu, Exit game.
MenuItem g_file[8];
int g_fileCount = 0;
Menu g_savedGame[2];      // the game bar's File and Options as they were (put back after an error)
Menu g_savedEditor[3];    // the editors' Options

void Number(MenuItem *items, int n)
{
    for (int k = 0; k < n; ++k) {
        items[k].y = static_cast<short>(20 * k);
    }
}

void MakeOptions(const MenuItem *game)
{
    const MenuItem ours[kOurItems] = {{0, kTextGeneral, &MenuGeneral, 0, 0},
                                      {0, kTextVideo, &MenuVideo, 0, 0},
                                      {0, kTextAudio, &MenuAudio, 0, 0},
                                      {0, kTextHotkeys, &MenuHotkeys, 0, 0}};
    int n = 0;
    for (const MenuItem &it : ours) {
        g_options[n++] = it;
    }
    for (int k = kFirstGameItem; k < kFirstGameItem + kGameItems; ++k) {
        g_options[n] = game[k];
        if (k == kAutosaveRow) {
            g_options[n].text = kTextAutosave;
        }
        if (k == kAutoDefendRow) {
            g_options[n].text = kTextAutoDefend;
        }
        if (core::options::game_options::DifficultyAttached()) { // else the game's dialog and toggle stay
            if (k == kDifficultyRow) {
                g_options[n].handler = &MenuDifficulty;
            }
            if (k == kAutoDefendRow) {
                g_options[n].handler = &MenuAutoDefend;
            }
        }
        ++n;
    }
    Number(g_options, n);
    for (int k = 0; k < kOurItems; ++k) {
        g_editor[k] = ours[k];
    }
    Number(g_editor, kOurItems);
}

void MakeFile(const MenuItem *game)
{
    auto copy = [&](int text) {
        for (int k = 0; k < kFileCount; ++k) {
            if (game[k].text == text) {
                g_file[g_fileCount++] = game[k];
            }
        }
    };
    g_fileCount = 0;
    if (g_pauseMenuOn) {
        g_file[g_fileCount++] = MenuItem{0, kTextReplayMap, &zeus::game_menu::MenuReplay, 0, 0};
    }
    else {
        copy(1); // New adventure
        copy(2); // Replay episode
    }
    copy(kTextLoad);
    copy(kTextSave);
    if (g_shotsOn) {
        g_file[g_fileCount++] = MenuItem{0, kTextCityShot, &MenuCityShot, 0, 0};
        g_file[g_fileCount++] = MenuItem{0, kTextMinimapShot, &MenuMinimapShot, 0, 0};
    }
    copy(kTextDelete);
    if (g_pauseMenuOn) {
        g_file[g_fileCount++] = MenuItem{0, kTextExitToMainMenu, &zeus::game_menu::MenuMainMenu, 0, 0};
        g_file[g_fileCount++] = MenuItem{0, kTextExit, &zeus::game_menu::MenuExit, 0, 0}; // our confirmation
    }
    else {
        copy(kTextExit);
    }
    Number(g_file, g_fileCount);
}

// The game's data (at g_imageShift in the offline tests); the pointers in it are the game's own addresses.
Menu &BarMenu(DWORD bar, int k)
{
    return *reinterpret_cast<Menu *>(bar + g_imageShift + sizeof(Menu) * k);
}

const MenuItem *GameItems(DWORD items)
{
    return reinterpret_cast<const MenuItem *>(items + g_imageShift);
}

// The game's tables back (an error in our texts: the game's items have the game's texts).
void PutBack()
{
    if (g_fileOn) {
        BarMenu(kGameBar, 0) = g_savedGame[0];
    }
    if (g_optionsOn) {
        BarMenu(kGameBar, 1) = g_savedGame[1];
        for (int k = 0; k < 3; ++k) {
            BarMenu(kEditorBars[k], 1) = g_savedEditor[k];
        }
    }
    g_fileOn = g_optionsOn = false;
}

struct TextCall {
    int group, index;
    Lang_fn original;
    const char *result;
};

void TextNow(void *p)
{
    TextCall &c = *static_cast<TextCall *>(p);
    const MenuItem *game = GameItems(kOptionsItems);
    if (g_fileOn && c.group == kFileGroup) {
        switch (c.index) {
        case kTextCityShot: c.result = mt::kMenuCityScreenshot; break;
        case kTextMinimapShot: c.result = mt::kMenuMinimapScreenshot; break;
        case kTextReplayMap: c.result = mt::kMenuReplayMap; break;
        case kTextExitToMainMenu: c.result = mt::kMenuExitToMainMenu; break;
        default: break;
        }
    }
    else if (g_optionsOn && c.group == kOptionsGroup) {
        switch (c.index) {
        case kTextGeneral: c.result = mt::kMenuGeneral; break;
        case kTextVideo: c.result = mt::kMenuVideo; break;
        case kTextAudio: c.result = mt::kMenuAudio; break;
        case kTextHotkeys: c.result = mt::kMenuHotkeys; break;
        case kTextAutosave: c.result = c.original(kOptionsGroup, game[kAutosaveRow].text); break;
        case kTextAutoDefend: c.result = c.original(kOptionsGroup, game[kAutoDefendRow].text); break;
        default: break;
        }
    }
    else if (g_optionsOn && c.group == kMainMenuGroup && c.index == kTextWebsite) {
        c.result = mt::kMainMenuOptions;
    }
}

int g_optionsWidth = 0, g_fileWidth = 0; // tiles, measured at the first drop-down (the fonts are loaded by then)

// The width in tiles of the widest text, as 0x5127c0 measures a group's own (font group 1).
int Tiles(std::initializer_list<const char *> texts)
{
    int px = 1;
    for (const char *t : texts) {
        px = std::max(px, zeus::painter::ConfirmTextWidth(t));
    }
    return px / 16 + 2;
}

int __cdecl Width_detour(int group)
{
    const int w = Original(o_Width, kMenuWidth)(group);
    if (g_optionsOn && group == kOptionsGroup) {
        if (g_optionsWidth == 0) {
            g_optionsWidth = Tiles({mt::kMenuGeneral, mt::kMenuVideo, mt::kMenuAudio, mt::kMenuHotkeys});
        }
        return std::max(w, g_optionsWidth);
    }
    if (g_fileOn && group == kFileGroup) {
        if (g_fileWidth == 0) {
            g_fileWidth = Tiles({g_shotsOn ? mt::kMenuCityScreenshot : "", g_shotsOn ? mt::kMenuMinimapScreenshot : "",
                                 g_pauseMenuOn ? mt::kMenuReplayMap : "", g_pauseMenuOn ? mt::kMenuExitToMainMenu : ""});
        }
        return std::max(w, g_fileWidth);
    }
    return w;
}

HINSTANCE WINAPI OpenOptions(HWND, LPCSTR, LPCSTR, LPCSTR, LPCSTR, INT)
{
    if (!ow::Open(ow::kPageGeneral, 0)) {
        core_log("options window: not available; the main menu's Options does nothing");
    }
    return reinterpret_cast<HINSTANCE>(33); // ShellExecute's success (the game does not read it)
}

// Each bar's Options as expected: the game's table, 7 in the game, 3 / 2 / 2 in the editors.
bool OptionsWanted()
{
    if (!ow::Available()) {
        core_log("options window: not available; the game's Options menu and main menu stay as they are");
        return false;
    }
    if (!Matches(kWebsiteOpen)) {
        core_log("options window: %s at 0x%lx does not match; the game's Options menu and main menu stay",
                 kWebsiteOpen.name, kWebsiteOpen.address);
        return false;
    }
    const int counts[] = {kOptionsCount, 3, 2, 2};
    const DWORD bars[] = {kGameBar, kEditorBars[0], kEditorBars[1], kEditorBars[2]};
    for (int k = 0; k < 4; ++k) {
        const Menu &m = BarMenu(bars[k], 1);
        if (m.group != kOptionsGroup || reinterpret_cast<DWORD>(m.items) != kOptionsItems ||
            m.count != counts[k]) {
            core_log("options window: the menu bar at 0x%lx is not as expected; the game's Options menus stay", bars[k]);
            return false;
        }
    }
    return true;
}

void OptionsIn()
{
    MakeOptions(GameItems(kOptionsItems));
    g_savedGame[1] = BarMenu(kGameBar, 1);
    BarMenu(kGameBar, 1).items = g_options; // .data: writable as it is
    BarMenu(kGameBar, 1).count = kOurItems + kGameItems;
    for (int k = 0; k < 3; ++k) {
        Menu &m = BarMenu(kEditorBars[k], 1);
        g_savedEditor[k] = m;
        m.items = g_editor;
        m.count = kOurItems;
    }
    const bool mainMenu = !g_testing && CallTo(kWebsiteOpen.address, reinterpret_cast<const void *>(&OpenOptions));
    core_log("options window: the Options menus open it (General Settings, Video, Audio, Hotkeys; in the game then the "
             "game's Difficulty, Autosave, Auto Defend%s); the main menu's Activision Website is Options%s",
             core::options::game_options::DifficultyAttached()
                 ? "; Difficulty opens General > Difficulty, Auto Defend not while enemies attack"
                 : "",
             mainMenu ? "" : " (not changed: the code could not be written)");
    Handler_fn &messagesButton = *reinterpret_cast<Handler_fn *>(kMessagesButtonHandler + g_imageShift);
    if (reinterpret_cast<DWORD>(messagesButton) == kMessagesDialog) {
        messagesButton = &MessagesButton; // .data
        core_log("options window: the message list's button opens it at General > Messages");
    }
    else {
        core_log("options window: the message list's button is not as expected; it opens the game's Messages");
    }
}

bool FileWanted()
{
    g_shotsOn = core::capture::city_picture::Available();
    g_pauseMenuOn = zeus::game_menu::Available();
    if (!g_shotsOn) {
        core_log("file menu: the screenshots are not available");
    }
    if (!g_pauseMenuOn) {
        core_log("file menu: the pause menu's confirmations are not available; the game's New adventure, Replay "
                 "episode and Exit game stay");
    }
    if (!g_shotsOn && !g_pauseMenuOn) {
        core_log("file menu: nothing to change; the game's File menu stays");
        return false;
    }
    const Menu &m = BarMenu(kGameBar, 0);
    if (m.group != kFileGroup || reinterpret_cast<DWORD>(m.items) != kFileItems || m.count != kFileCount) {
        core_log("file menu: not as expected; the game's File menu stays");
        return false;
    }
    return true;
}

void FileIn()
{
    MakeFile(GameItems(kFileItems));
    g_savedGame[0] = BarMenu(kGameBar, 0);
    BarMenu(kGameBar, 0).items = g_file;
    BarMenu(kGameBar, 0).count = g_fileCount;
    core_log("file menu: %s%s (the editors' File menus stay)",
             g_pauseMenuOn ? "Replay map, Load, Save, Delete, Exit to main menu, Exit game (ours: Replay map, Exit to "
                             "main menu, Exit game)"
                           : "the game's items",
             g_shotsOn ? "; Save city screenshot and Save minimap screenshot after Save" : "");
}

} // namespace

bool Attach()
{
    if (!zeus::keys::LangHooked()) {
        core_log("menus: lang_get_string is not hooked (games/zeus/keys); the game's menus stay as they are");
        return false;
    }
    if (!Matches(kMenuWidth)) {
        core_log("menus: %s at 0x%lx does not match; the game's menus stay as they are", kMenuWidth.name,
                 kMenuWidth.address);
        return false;
    }
    const bool options = OptionsWanted(), file = FileWanted();
    if (!options && !file) {
        return false;
    }
    if (!Install(kMenuWidth, &Width_detour, o_Width)) {
        core_log("menus: menu_width could not be hooked; the game's menus stay as they are");
        return false;
    }
    g_optionsOn = options;
    g_fileOn = file;
    if (options) {
        OptionsIn();
    }
    if (file) {
        FileIn();
    }
    return options;
}

const char *MenuText(int group, int index, const char *(__cdecl *original)(int group, int index))
{
    if ((!g_optionsOn && !g_fileOn) || !core::diag::Enabled(g_texts) ||
        (group != kFileGroup && group != kOptionsGroup && group != kMainMenuGroup)) {
        return nullptr; // every other text of the game (most calls) without entering the guard
    }
    TextCall c{group, index, original, nullptr};
    if (!core::diag::Guarded(g_texts, &TextNow, &c)) {
        PutBack();
        core_log("menus: our menu texts failed; the game's own menus are back");
        return nullptr;
    }
    return c.result;
}

void AttachForTest(bool shots, bool pauseMenu)
{
    g_testing = true;
    g_shotsOn = shots;
    g_pauseMenuOn = pauseMenu;
    g_optionsOn = g_fileOn = true;
    OptionsIn();
    FileIn();
}

} // namespace zeus::options_entry
