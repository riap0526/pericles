// Key bindings: Zeus's actions for the core's key bindings (core/input/keys; [keys] in zeus_engine.ini,
// the same names and words as Yongle's). On the city screens a key press that belongs to an action becomes the key
// the game itself expects for it; on the editor's map only the actions the editor has too (scrolling, rotating the
// map, bookmarks); other screens get the keys as they are. F6, F7 and F8 never reach the game from the keyboard
// (the Display options buttons still post theirs to the game until our own buttons replace them). The game's pause
// banner names the player's pause keys.
#pragma once

#include "games/zeus/site.h"

namespace zeus::keys {

// The Hotkeys window's sections (core::input::keys::ActionInfo::section).
enum Section { kArrows, kGlobal, kCity, kBuild, kBookmarks, kArmy, kSectionCount };

// From zeus_register_settings: one setting per action in [keys].
void RegisterSettings();
// After the game's hooks: the translator and the key-state filter; the pause banner's text hook (logged when its site
// does not match: the banner then keeps the game's 'P').
void Attach();

// The offline tests (src/games/zeus/test_restore.cpp): the detours (lang_get_string).
const Detour *Detours(int *count);

} // namespace zeus::keys
