// Key bindings: Zeus's actions for the core's key bindings (core/input/keys; [keys] in zeus_engine.ini,
// the same names and words as Yongle's). On the city screens a key press that belongs to an action becomes the key
// the game itself expects for it; on the editor's map only the actions the editor has too (scrolling, rotating the
// map, bookmarks); other screens get the keys as they are. F6, F7 and F8 never reach the game from the keyboard
// (the Display options buttons still post theirs to the game until our own buttons replace them). The game's pause
// banner names the player's pause keys.
#pragma once

#include "games/zeus/site.h"

namespace zeus::keys {

// The Hotkeys window's sections (core::input::keys::ActionInfo::section; the same order as Yongle's).
enum Section { kArrows, kGlobal, kCity, kBuild, kOverlays, kBookmarks, kArmy, kSectionCount };

// From zeus_register_settings: one setting per action in [keys].
void RegisterSettings();
// After the game's hooks: the translator and the key-state filter; the pause banner's text hook (logged when its site
// does not match: the banner then keeps the game's 'P'); the overlay keys' three functions checked (when one does not
// match, logged: the overlay keys do nothing, the others work).
void Attach();
// lang_get_string is hooked here (the pause banner; games/zeus/options_entry's menu texts go through the same hook).
bool LangHooked();

// The offline tests (src/games/zeus/test_restore.cpp): the detours (lang_get_string); the overlay keys' functions
// taken as matching (the test's copy of the game has fakes there).
const Detour *Detours(int *count);
void OverlaySitesForTest();

} // namespace zeus::keys
