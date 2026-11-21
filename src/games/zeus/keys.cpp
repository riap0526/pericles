// See keys.h. The keys are the game's own (game_wndproc 0x5c4a90: WM_KEYDOWN, WM_SYSKEYDOWN, WM_CHAR; the R poll in 8
// places under 0x451820); the bindings are the core's.
#include "games/zeus/keys.h"

#include "games/zeus/game_menu.h"
#include "games/zeus/options_entry.h"
#include "games/zeus/options_window.h"

#include "core/diag/guard.h"
#include "core/input/keys.h"
#include "core/log.h"

#include <initializer_list>

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
// overlay letters of its own (ours: OverlayKey), no F9 help, army groups 1-5 only (army_group_set 0x433ab0 / army_group_recall 0x433b60 return for
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
    {Item{"keys", "open_options", "Open Options", "", Type::Hotkey, "F6", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
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
    {Item{"keys", "toggle_overlay", "Toggle current overlay", "", Type::Hotkey, "Space", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::Char, ' ', false, {"toggle_overlay", kOverlays, kNoText, 0}},
    // The overlays: the game has no keys for them, so they are ours (Send::Call, code = the overlay's
    // number). Their names are Zeus_Text.eng's group 14 (index = the number); in Poseidon's adventures the culture
    // overlays 10-14 are the science ones (the game's texts 20-24), hence both names. The same names and default
    // letters as Yongle's where the game's word is the same; no default for the ones only Zeus has.
    {Item{"keys", "overlay_supplies", "Overlay: supplies", "", Type::Hotkey, "S", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::Call, 1, false, {"overlay_supplies", kOverlays, 14, 1}},
    {Item{"keys", "overlay_husbandry", "Overlay: husbandry", "", Type::Hotkey, "G", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::Call, 2, false, {"overlay_husbandry", kOverlays, 14, 2}},
    {Item{"keys", "overlay_industry", "Overlay: industry", "", Type::Hotkey, "I", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::Call, 3, false, {"overlay_industry", kOverlays, 14, 3}},
    {Item{"keys", "overlay_distribution", "Overlay: distribution", "", Type::Hotkey, "J", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::Call, 4, false, {"overlay_distribution", kOverlays, 14, 4}},
    {Item{"keys", "overlay_water", "Overlay: water", "", Type::Hotkey, "W", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::Call, 5, false, {"overlay_water", kOverlays, 14, 5}},
    {Item{"keys", "overlay_hygiene", "Overlay: hygiene", "", Type::Hotkey, "H", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::Call, 6, false, {"overlay_hygiene", kOverlays, 14, 6}},
    {Item{"keys", "overlay_hazards", "Overlay: hazards", "", Type::Hotkey, "Z", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::Call, 7, false, {"overlay_hazards", kOverlays, 14, 7}},
    {Item{"keys", "overlay_unrest", "Overlay: unrest", "", Type::Hotkey, "U", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::Call, 8, false, {"overlay_unrest", kOverlays, 14, 8}},
    {Item{"keys", "overlay_taxes", "Overlay: taxes", "", Type::Hotkey, "X", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::Call, 9, false, {"overlay_taxes", kOverlays, 14, 9}},
    {Item{"keys", "overlay_culture", "Overlay: all culture or science", "", Type::Hotkey, "E", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::Call, 10, false, {"overlay_culture", kOverlays, 14, 10}},
    {Item{"keys", "overlay_competitors", "Overlay: competitors or curators", "", Type::Hotkey, "", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::Call, 11, false, {"overlay_competitors", kOverlays, 14, 11}},
    {Item{"keys", "overlay_philosophers", "Overlay: philosophers or astronomers", "", Type::Hotkey, "", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::Call, 12, false, {"overlay_philosophers", kOverlays, 14, 12}},
    {Item{"keys", "overlay_athletes", "Overlay: athletes or scholars", "", Type::Hotkey, "", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::Call, 13, false, {"overlay_athletes", kOverlays, 14, 13}},
    {Item{"keys", "overlay_actors", "Overlay: actors or inventors", "", Type::Hotkey, "Semicolon", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::Call, 14, false, {"overlay_actors", kOverlays, 14, 14}},
    {Item{"keys", "overlay_immortals", "Overlay: immortals", "", Type::Hotkey, "", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::Call, 15, false, {"overlay_immortals", kOverlays, 14, 15}},
    {Item{"keys", "overlay_security", "Overlay: security", "", Type::Hotkey, "V", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::Call, 16, false, {"overlay_security", kOverlays, 14, 16}},
    {Item{"keys", "overlay_appeal", "Overlay: appeal", "", Type::Hotkey, "D", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::Call, 17, false, {"overlay_appeal", kOverlays, 14, 17}},
    {Item{"keys", "overlay_problems", "Overlay: problems", "", Type::Hotkey, "Y", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::Call, 18, false, {"overlay_problems", kOverlays, 14, 18}},
    {Item{"keys", "overlay_roads", "Overlay: roads", "", Type::Hotkey, "", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0},
     Send::Call, 19, false, {"overlay_roads", kOverlays, 14, 19}},
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

// The overlay keys: what Yongle's game does for its own overlay letters
// (Emperor's build_hotkey 0x4765f0), done with Zeus's functions of the sidebar's buttons. All three take their
// argument in button_param, no stack arguments, plain ret (__cdecl). category_available is not called (it switches
// the category there and back): its bytes are checked so that the two values read below are still what it reads.
const Site kOverlaySelect{"overlay_select", 0x516cd0, {0xa1, 0xf8, 0x35, 0xf1, 0x00, 0x8b, 0x0d, 0xf0}};
const Site kSetCategory{"sidebar_set_category", 0x516d50, {0xa1, 0xf8, 0x35, 0xf1, 0x00, 0xc6, 0x05, 0xce}};
const Site kCategoryAvailable{"category_available", 0x516f50, {0xa1, 0xf0, 0x5d, 0x66, 0x00, 0x85, 0xc0, 0x8b}};
using Button_fn = void(__cdecl *)();
const Var<int> kAllCategories{0x665df0};   // non-0: every category can be used
constexpr DWORD aCategoryOpen = 0xfeae06;  // byte [category]: the category can be used
constexpr DWORD aCategoryOverlays = 0x63e2e0; // int [category * 5 + slot]: the overlays of each category (0: none)
constexpr int kCategories = 11, kSlots = 5;
const Var<int> kButtonParam{0xf135f8};
const Var<int> kBuildCategory{0xfa0004};
const Var<int> kOverlayShown{0xfa05f0};
const Var<int> kOverlayBefore{0xfa05f4};
constexpr DWORD aCategoryOverlay = 0xfa042c; // int [category]: the overlay Space toggles
const Var<unsigned char> kSidebarDialog{0xfa0428};
core::diag::Feature g_overlays{"overlay keys (games/zeus/keys)"};
bool g_overlaySites = false;

struct OverlayCall {
    unsigned overlay;
    enum Result { kNotOne, kUnusable, kShown } result; // not one of the game's overlays, a category that cannot be
                                                      // used, done
    core::diag::Restore undo;
};

void OverlayNow(void *p)
{
    OverlayCall &c = *static_cast<OverlayCall *>(p);
    int category = -1;
    for (int i = 0; i < kCategories * kSlots && category < 0; ++i) {
        if (At<int>(aCategoryOverlays)[i] == static_cast<int>(c.overlay)) {
            category = i / kSlots;
        }
    }
    if (category < 0) {
        return; // not one of the game's overlays
    }
    const unsigned char screen = *kGameScreen;
    // As Yongle (Emperor's build_hotkey 0x4765f0): nothing for a category the player cannot use, the current one too
    // (checked before anything changes). The check reads what category_available 0x516f50 decides on: 0x665df0, or
    // the category's byte at 0xfeae06 (its switching there and back afterwards always ends in "usable", 0x516f98).
    // Warships selected (screen 0x20, Zeus only: Emperor's 0x20 is a leftover no game reaches) is like an army
    // selected (0x48): the overlay changes, the selection stays (as the game's own overlay menu there).
    if (*kAllCategories == 0 && At<unsigned char>(aCategoryOpen)[category] == 0) {
        c.result = OverlayCall::kUnusable;
        return;
    }
    core::diag::Restore &u = c.undo;
    u.Save(kGameScreen.ptr());
    u.Save(kSidebarDialog.ptr());
    u.Save(kButtonParam.ptr());
    u.Save(kBuildCategory.ptr());
    u.Save(kOverlayShown.ptr());
    u.Save(kOverlayBefore.ptr());
    u.Save(At<int>(aCategoryOverlay) + category);
    u.Save(At<int>(aCategoryOverlay) + *kBuildCategory);
    const int param = *kButtonParam;
    // Back on the city first (an open building menu's overview closed), as build_hotkey does; on screen 7
    // sidebar_set_category would open the new category's overview instead.
    const bool backToCity = screen == 7;
    if (backToCity) {
        *kGameScreen = 1;
    }
    if (category != *kBuildCategory) {
        // Like build_hotkey: the overlay off first (sidebar_set_category would put the new category's own in, and
        // selecting the same one again turns it off).
        *kOverlayShown = 0;
        *kButtonParam = category;
        Function<Button_fn>(kSetCategory)();
    }
    if (backToCity) {
        *kSidebarDialog = 0;
    }
    *kButtonParam = static_cast<int>(c.overlay);
    Function<Button_fn>(kOverlaySelect)(); // the same one again turns it off
    *kButtonParam = param;
    u.Forget();
    c.result = OverlayCall::kShown;
}

// KeysGame::call: the overlay's number (1..19).
bool OverlayKey(unsigned overlay)
{
    static long logs = 0;
    if (!g_overlaySites || !core::diag::Enabled(g_overlays)) {
        if (++logs <= 5) {
            core_log("keys: the overlay keys are off");
        }
        return false;
    }
    OverlayCall c{overlay, OverlayCall::kNotOne, {}};
    if (!core::diag::Guarded(g_overlays, &OverlayNow, &c, &c.undo)) {
        core_log("keys: the overlay keys are off for the rest of the run (an error); the other keys work");
        return true; // the key is taken all the same
    }
    static long unusableLogs = 0;
    if (c.result == OverlayCall::kUnusable && ++unusableLogs <= 20) {
        core_log("keys: overlay %u: its category cannot be used now", overlay);
        return true; // said here (the core would log "did nothing" too)
    }
    return c.result != OverlayCall::kNotOne;
}

// The options key opens Pericles's options window (General), in the city, the menus and the editors. Esc closes
// windows and opens the pause menu (games/zeus/game_menu). The game's display mode keys F6 and F7 never reach the game,
// also when its Display options dialog's 800 x 600 / 1024 x 768 buttons post them (lParam 0): with our resolutions the
// game stays in mode 3 (as Yongle's).
bool OpenOptions()
{
    return zeus::options_window::Open(zeus::options_window::kPageGeneral, 0);
}

const core::input::keys::KeysGame &Game()
{
    static const core::input::keys::KeysGame game(kDefs, kCount, &CityScreen, &EditorScreen, kKeyCtrl.ptr(),
                                                  &OpenOptions, &zeus::game_menu::Escape,
                                                  &OverlayKey, false);
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
    // The menu items we add (games/zeus/options_entry; under its own guard: its error leaves the banner's).
    if (const char *menu = zeus::options_entry::MenuText(group, index, Original(o_LangGetString, kLangGetString))) {
        return menu;
    }
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
    g_overlaySites = true;
    for (const Site *s : {&kOverlaySelect, &kSetCategory, &kCategoryAvailable}) {
        if (!Matches(*s)) {
            core_log("keys: %s at 0x%lx does not match; the overlay keys do nothing", s->name,
                     static_cast<unsigned long>(s->address));
            g_overlaySites = false;
        }
    }
    if (!g_overlaySites) {
        core::diag::Disable(g_overlays, "a site does not match");
    }
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

bool LangHooked()
{
    return o_LangGetString != nullptr;
}

const char *GameText(int group, int index)
{
    if (!o_LangGetString) {
        return nullptr;
    }
    return Original(o_LangGetString, kLangGetString)(group, index);
}

void OverlaySitesForTest()
{
    g_overlaySites = true;
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
