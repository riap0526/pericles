// The ways into Pericles's options window (as Yongle's games/emperor/options_entry): the Options menu of the game and
// of the three editors starts with General Settings, Video and Audio (the game's own Display, Sound and Speed options
// after them for now, then its Difficulty, Autosave, Auto Defend and Messages); the File menu is Yongle's (Replay map,
// Load, Save, the two screenshots, Delete, Exit to main menu, Exit game; ours ask first); the main menu's "Activision
// Website" is Options. The menus are the game's own: we give its menu bars our item tables and our texts for the items
// it does not have (through lang_get_string, hooked by games/zeus/keys) and widen the drop-downs (menu_width).
//
// Failure: our texts run under the guard "menu texts (games/zeus/options_entry)"; when it trips the game's own tables
// are put back.
//
// Threads: the game thread (the game's menus).
#pragma once

namespace zeus::options_entry {

// From zeus_attach after keys (lang_get_string's hook), game_menu (the pause menu's confirmations) and city_picture
// (the screenshots). False (logged): the game's menus stay as they are.
bool Attach();

// lang_get_string's detour (games/zeus/keys) asks this first: our text for (group, index), or nullptr (the game's).
// original: the game's lang_get_string (Autosave and Auto Defend show the game's own ON / OFF texts).
const char *MenuText(int group, int index, const char *(__cdecl *original)(int group, int index));

// Offline tests only (src/games/zeus/test_restore.cpp): our tables in without the checks, the hook and the main menu's
// patch (the bars and tables are the test's, at the game's addresses).
void AttachForTest(bool shots, bool pauseMenu);

} // namespace zeus::options_entry
