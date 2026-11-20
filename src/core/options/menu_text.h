// The texts for players of our pause menu and of the File menu items both games rename (shared, so the two games say the
// same). Moved from Yongle's games/emperor/options_text for Pericles.
#pragma once

namespace core::options::menu_text {

// The File menu's renamed items (also the titles of their confirmations) and the pause menu.
extern const char *const kMenuReplayMap, *const kMenuExitToMainMenu;
extern const char *const kPauseTitle, *const kPauseContinue, *const kPauseOptions;
// The text of the confirmation before Replay map / Exit to main menu, for a game whose texts have none (Zeus; Emperor
// uses its own text 5, 129, which says the same).
extern const char *const kLeaveGame;

} // namespace core::options::menu_text
