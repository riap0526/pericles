// Key bindings: Zeus's actions for the core's key bindings (core/input/keys; [keys] in zeus_engine.ini, written only
// for the actions whose keys were changed; the same names and words as Yongle's; changed in the Hotkeys window). On
// the city screens a key press that belongs to an action becomes the key the game itself expects for it; on the
// editor's map only the actions the editor has too (scrolling, rotating the map, bookmarks); other screens get the keys
// as they are. F6, F7 and F8 never reach the game. The game's pause banner names the player's pause keys.
#pragma once

#include "core/input/keys.h"
#include "games/zeus/site.h"

namespace zeus::keys {

// The Hotkeys window's sections (core's, the same in both games; core::input::keys::ActionInfo::section).
using core::input::keys::kArmy;
using core::input::keys::kArrows;
using core::input::keys::kBookmarks;
using core::input::keys::kBuild;
using core::input::keys::kCity;
using core::input::keys::kGlobal;
using core::input::keys::kOverlays;

// From zeus_register_settings: one setting per action in [keys].
void RegisterSettings();
// After the game's hooks: the translator and the key-state filter; the debug panel's note on Zeus's own keys
// (games/zeus/options_text); the pause banner's text hook (logged when its site
// does not match: the banner then keeps the game's 'P'); the overlay keys' three functions checked (when one does not
// match, logged: the overlay keys do nothing, the others work).
void Attach();
// lang_get_string is hooked here (the pause banner; games/zeus/options_entry's menu texts go through the same hook).
bool LangHooked();
// The game's own text (lang_get_string as the game has it: without our menu texts and the banner's keys); nullptr
// when the hook is not installed. Game thread.
const char *GameText(int group, int index);

// The offline tests (src/games/zeus/test_restore.cpp): the detours (lang_get_string); the overlay keys' functions
// taken as matching (the test's copy of the game has fakes there); the game's texts without the hook.
const Detour *Detours(int *count);
void OverlaySitesForTest();
void GameTextForTest(); // GameText reads the fake at lang_get_string's address (no hook installed)

} // namespace zeus::keys
