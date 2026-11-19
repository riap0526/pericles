// See keys.h. The keys are the game's own (game_wndproc 0x5c4a90: WM_KEYDOWN, WM_SYSKEYDOWN, WM_CHAR; the R poll in 8
// places under 0x451820); the bindings are the core's.
#include "games/zeus/keys.h"

#include "core/diag/guard.h"
#include "core/input/keys.h"
#include "core/log.h"

namespace zeus::keys {
namespace {

using core::input::keys::Def;
using core::input::keys::Send;
using core::settings::Effect;
using core::settings::Item;
using core::settings::Type;

constexpr int kNoText = 0;
constexpr bool kEditorToo = true; // the editor's map has it too (game_wndproc: PgUp/PgDn/Home on screens 1 and 0x58,
                                  // the arrows, F1-F4 on screen 1 without an editor_mode check)

// The same names and words as Yongle's for the same actions; Zeus has no
// overlay letters, no F9 help, army groups 1-5 only (army_group_set 0x433ab0 / army_group_recall 0x433b60 return for
// n > 5); Tab is Zeus's own (0x516d30: the sidebar's Info / Overview).
const Def kDefs[] = {
    {Item{"keys", "scroll_up", "Scroll up", "", Type::Hotkey, "Up", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::KeyDown, VK_UP, false, {"scroll_up", kArrows, kNoText, 0}, kEditorToo},
    {Item{"keys", "scroll_down", "Scroll down", "", Type::Hotkey, "Down", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::KeyDown, VK_DOWN, false, {"scroll_down", kArrows, kNoText, 0}, kEditorToo},
    {Item{"keys", "scroll_left", "Scroll left", "", Type::Hotkey, "Left", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::KeyDown, VK_LEFT, false, {"scroll_left", kArrows, kNoText, 0}, kEditorToo},
    {Item{"keys", "scroll_right", "Scroll right", "", Type::Hotkey, "Right", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::KeyDown, VK_RIGHT, false, {"scroll_right", kArrows, kNoText, 0}, kEditorToo},
    {Item{"keys", "open_options", "Open Options", "No effect in Zeus yet.", Type::Hotkey, "F6", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::Options, 0, false, {"open_options", kGlobal, kNoText, 0}},
    {Item{"keys", "speed_down", "Decrease game speed", "", Type::Hotkey, "LeftBracket", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::Char, '[', false, {"speed_down", kCity, kNoText, 0}},
    {Item{"keys", "speed_up", "Increase game speed", "", Type::Hotkey, "RightBracket", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::Char, ']', false, {"speed_up", kCity, kNoText, 0}},
    {Item{"keys", "pause", "Toggle pause", "", Type::Hotkey, "P", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::Char, 'p', false, {"pause", kCity, kNoText, 0}},
    {Item{"keys", "rotate_map_left", "Rotate map left", "", Type::Hotkey, "PageUp", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::KeyDown, VK_PRIOR, false, {"rotate_map_left", kCity, kNoText, 0}, kEditorToo},
    {Item{"keys", "rotate_map_right", "Rotate map right", "", Type::Hotkey, "PageDown", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::KeyDown, VK_NEXT, false, {"rotate_map_right", kCity, kNoText, 0}, kEditorToo},
    {Item{"keys", "rotate_map_north", "Rotate map to north", "", Type::Hotkey, "Home", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::KeyDown, VK_HOME, false, {"rotate_map_north", kCity, kNoText, 0}, kEditorToo},
    // Zeus only polls R (its WM_KEYDOWN R flips 0x160ac8c, which nothing reads); sent the same way as Yongle's.
    {Item{"keys", "rotate_building", "Rotate building", "", Type::Hotkey, "R", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::Rotate, 'R', false, {"rotate_building", kCity, kNoText, 0}},
    {Item{"keys", "hold_preview", "Hold building preview", "", Type::Hotkey, "M", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::Char, 'm', false, {"hold_preview", kCity, kNoText, 0}},
    {Item{"keys", "toggle_overlay", "Toggle current overlay", "", Type::Hotkey, "Space", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::Char, ' ', false, {"toggle_overlay", kCity, kNoText, 0}},
    {Item{"keys", "sidebar_overview", "Toggle sidebar overview", "", Type::Hotkey, "Tab", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::KeyDown, VK_TAB, false, {"sidebar_overview", kCity, kNoText, 0}},
    {Item{"keys", "category_1", "Building menu 1", "", Type::Hotkey, "1", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::Char, '1', false, {"category_1", kBuild, kNoText, 0}},
    {Item{"keys", "category_2", "Building menu 2", "", Type::Hotkey, "2", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::Char, '2', false, {"category_2", kBuild, kNoText, 0}},
    {Item{"keys", "category_3", "Building menu 3", "", Type::Hotkey, "3", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::Char, '3', false, {"category_3", kBuild, kNoText, 0}},
    {Item{"keys", "category_4", "Building menu 4", "", Type::Hotkey, "4", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::Char, '4', false, {"category_4", kBuild, kNoText, 0}},
    {Item{"keys", "category_5", "Building menu 5", "", Type::Hotkey, "5", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::Char, '5', false, {"category_5", kBuild, kNoText, 0}},
    {Item{"keys", "category_6", "Building menu 6", "", Type::Hotkey, "6", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::Char, '6', false, {"category_6", kBuild, kNoText, 0}},
    {Item{"keys", "category_7", "Building menu 7", "", Type::Hotkey, "7", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::Char, '7', false, {"category_7", kBuild, kNoText, 0}},
    {Item{"keys", "category_8", "Building menu 8", "", Type::Hotkey, "8", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::Char, '8', false, {"category_8", kBuild, kNoText, 0}},
    {Item{"keys", "category_9", "Building menu 9", "", Type::Hotkey, "9", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::Char, '9', false, {"category_9", kBuild, kNoText, 0}},
    {Item{"keys", "category_10", "Building menu 10", "", Type::Hotkey, "0", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::Char, '0', false, {"category_10", kBuild, kNoText, 0}},
    {Item{"keys", "category_11", "Building menu 11", "", Type::Hotkey, "Minus", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::Char, '-', false, {"category_11", kBuild, kNoText, 0}},
    {Item{"keys", "bookmark_go_1", "Go to bookmark 1", "", Type::Hotkey, "F1", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::KeyDown, VK_F1, false, {"bookmark_go_1", kBookmarks, kNoText, 0}, kEditorToo},
    {Item{"keys", "bookmark_go_2", "Go to bookmark 2", "", Type::Hotkey, "F2", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::KeyDown, VK_F2, false, {"bookmark_go_2", kBookmarks, kNoText, 0}, kEditorToo},
    {Item{"keys", "bookmark_go_3", "Go to bookmark 3", "", Type::Hotkey, "F3", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::KeyDown, VK_F3, false, {"bookmark_go_3", kBookmarks, kNoText, 0}, kEditorToo},
    {Item{"keys", "bookmark_go_4", "Go to bookmark 4", "", Type::Hotkey, "F4", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::KeyDown, VK_F4, false, {"bookmark_go_4", kBookmarks, kNoText, 0}, kEditorToo},
    {Item{"keys", "bookmark_set_1", "Set bookmark 1", "", Type::Hotkey, "Ctrl+F1", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::KeyDown, VK_F1, true, {"bookmark_set_1", kBookmarks, kNoText, 0}, kEditorToo},
    {Item{"keys", "bookmark_set_2", "Set bookmark 2", "", Type::Hotkey, "Ctrl+F2", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::KeyDown, VK_F2, true, {"bookmark_set_2", kBookmarks, kNoText, 0}, kEditorToo},
    {Item{"keys", "bookmark_set_3", "Set bookmark 3", "", Type::Hotkey, "Ctrl+F3", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::KeyDown, VK_F3, true, {"bookmark_set_3", kBookmarks, kNoText, 0}, kEditorToo},
    {Item{"keys", "bookmark_set_4", "Set bookmark 4", "", Type::Hotkey, "Ctrl+F4", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::KeyDown, VK_F4, true, {"bookmark_set_4", kBookmarks, kNoText, 0}, kEditorToo},
    {Item{"keys", "army_set_1", "Set army group 1", "", Type::Hotkey, "Ctrl+1", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::KeyDown, '1', true, {"army_set_1", kArmy, kNoText, 0}},
    {Item{"keys", "army_set_2", "Set army group 2", "", Type::Hotkey, "Ctrl+2", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::KeyDown, '2', true, {"army_set_2", kArmy, kNoText, 0}},
    {Item{"keys", "army_set_3", "Set army group 3", "", Type::Hotkey, "Ctrl+3", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::KeyDown, '3', true, {"army_set_3", kArmy, kNoText, 0}},
    {Item{"keys", "army_set_4", "Set army group 4", "", Type::Hotkey, "Ctrl+4", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::KeyDown, '4', true, {"army_set_4", kArmy, kNoText, 0}},
    {Item{"keys", "army_set_5", "Set army group 5", "", Type::Hotkey, "Ctrl+5", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::KeyDown, '5', true, {"army_set_5", kArmy, kNoText, 0}},
    {Item{"keys", "army_recall_1", "Recall army group 1", "", Type::Hotkey, "Alt+1", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::SysKey, '1', false, {"army_recall_1", kArmy, kNoText, 0}},
    {Item{"keys", "army_recall_2", "Recall army group 2", "", Type::Hotkey, "Alt+2", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::SysKey, '2', false, {"army_recall_2", kArmy, kNoText, 0}},
    {Item{"keys", "army_recall_3", "Recall army group 3", "", Type::Hotkey, "Alt+3", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::SysKey, '3', false, {"army_recall_3", kArmy, kNoText, 0}},
    {Item{"keys", "army_recall_4", "Recall army group 4", "", Type::Hotkey, "Alt+4", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::SysKey, '4', false, {"army_recall_4", kArmy, kNoText, 0}},
    {Item{"keys", "army_recall_5", "Recall army group 5", "", Type::Hotkey, "Alt+5", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::SysKey, '5', false, {"army_recall_5", kArmy, kNoText, 0}},
};

constexpr int kCount = static_cast<int>(sizeof(kDefs) / sizeof(kDefs[0]));

const Var<unsigned char> kGameScreen{0xf2a6dd};
const Var<int> kEditorMode{0x665e2c};
const Var<int> kKeyCtrl{0xf9c108}; // key_ctrl: game_wndproc's record of Ctrl (Ctrl+F1, Ctrl+1)

// The city screens: the city 1, a building menu open 7, 0x20 and 0x48 (the military views). Not in
// the editor (its map is also screen 1; EditorScreen).
bool CityScreen()
{
    if (*kEditorMode != 0) {
        return false;
    }
    const unsigned char s = *kGameScreen;
    return s == 1 || s == 7 || s == 0x20 || s == 0x48;
}

// The editor's map: screen 1, or 0x58 (where its PgUp/PgDn still rotate the map). Its other screens (the world map,
// lists, the scenario's texts) get the keys as they are.
bool EditorScreen()
{
    if (*kEditorMode == 0) {
        return false;
    }
    const unsigned char s = *kGameScreen;
    return s == 1 || s == 0x58;
}

// Pericles has no options window or pause menu of its own yet: F6 does nothing (the game's 800x600 stays blocked),
// Esc stays the game's ("Exit Game?"). The Display options dialog's 800 x 600 / 1024 x 768 buttons post F6/F7 to the
// game (lParam 0): passed until our own buttons replace them.
const core::input::keys::KeysGame &Game()
{
    static const core::input::keys::KeysGame game(kDefs, kCount, &CityScreen, &EditorScreen, kKeyCtrl.ptr(),
                                                  &core::input::keys::NoOptions, &core::input::keys::NoEscape, true);
    return game;
}

// The pause banner (0x58f5a0: while sim_paused, 0x512b50(13, 2, ...) -> 0x508d40 -> 0x508060 -> lang_get_string): the
// player's pause keys instead of the game's 'P'. lang_get_string is __cdecl(group, index), the caller pops
// (checked by disassembly).
constexpr int kPauseGroup = 13, kTextPaused = 2;
const Site kLangGetString{"lang_get_string", 0x507bf0, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x15, 0x64, 0x9f}};
using LangGetString_fn = const char *(__cdecl *)(int group, int index);
LangGetString_fn o_LangGetString = nullptr;
core::diag::Feature g_banner{"pause banner keys (games/zeus/keys)"};

const char *__cdecl LangGetString_detour(int group, int index)
{
    if (group == kPauseGroup && index == kTextPaused && core::diag::Enabled(g_banner)) {
        const char *text = nullptr;
        if (!core::diag::Guarded(g_banner,
                                 [](void *p) { *static_cast<const char **>(p) = core::input::keys::PauseText(); },
                                 &text)) {
            core_log("keys: the pause banner shows the game's own text from now on (an error)");
        }
        else if (text) {
            return text;
        }
    }
    return Original(o_LangGetString, kLangGetString)(group, index);
}

} // namespace

void RegisterSettings()
{
    core::input::keys::RegisterSettings(&Game());
}

void Attach()
{
    core::input::keys::Install();
    if (!Matches(kLangGetString)) {
        core_log("keys: lang_get_string at 0x%lx does not match; the pause banner keeps the game's 'P'",
                 static_cast<unsigned long>(kLangGetString.address));
        core::diag::Disable(g_banner, "the site does not match");
    }
    else if (!Install(kLangGetString, &LangGetString_detour, o_LangGetString)) {
        core::diag::Disable(g_banner, "the hook could not be installed");
    }
    else {
        core_log("keys: the pause banner names the player's pause keys");
    }
}

const Detour *Detours(int *count)
{
    static const Detour kDetours[] = {
        {&kLangGetString, reinterpret_cast<void *>(&LangGetString_detour)},
    };
    *count = static_cast<int>(sizeof(kDetours) / sizeof(kDetours[0]));
    return kDetours;
}

} // namespace zeus::keys
