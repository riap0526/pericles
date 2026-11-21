// Esc and the pause menu (as Yongle's games/emperor/game_menu; the pause menu itself is core/options/
// pause_menu). Esc closes any window that closes with a right click and goes back on the menu screens that go back with
// one; on the city screen with nothing to cancel it opens our pause menu (the game paused); the game's confirmation
// boxes take Esc as their last button (No); on the main menu Esc asks before quitting. In the editors (city map 0x01,
// world map 0x15, adventure editor 0x57) Esc opens an editor pause menu: Continue and that screen's File menu.
//
// Threads: the game thread (the window procedure and the game loop).
#pragma once

#include <windows.h>

namespace zeus::game_menu {

// From zeus_attach after timing (screen_present's hook) and the painter, before keys (its Esc goes through here).
// presentHooked: screen_present is hooked (the menu is drawn there); without it Esc stays the game's.
bool Attach(bool painter, bool presentHooked);

// The keys translator's Esc (a WM_KEYDOWN of VK_ESCAPE): true when handled here (the game does not get it).
bool Escape(LPARAM lparam);

// The pause menu is open (the game is paused and draws frames only).
bool IsOpen();

// The game loop, before each pass while nothing of ours pauses it: a pending pause menu item. true: one was carried
// out (the caller checks whether the pause menu is open again: a confirmation answered No).
bool OnLoop();

// Before the game's frame is presented: the pause menu over it. caller: the game address screen_present returns to.
void OnPresent(unsigned long caller);

// Attached and not switched off after an error (games/zeus/options_entry puts our File menu items in only then).
bool Available();

// The File menu's items (called by the game's drop-down, in the game's input pass): our confirmations (the pause
// menu's), Yongle's way; the game's own New adventure and Replay episode do not ask, its Exit game asks in its own box.
void __cdecl MenuReplay();
void __cdecl MenuMainMenu();
void __cdecl MenuExit();

// Offline tests only (src/games/zeus/test_restore.cpp): Attach without the byte checks and the hook (the fakes are
// thunks at the game's addresses), and input_update_mouse's detour.
void AttachForTest();
// The same with the options window attached on it (the debug panel's list of what the windows show).
void AttachWithWindowForTest();
void InputMouseForTest();

} // namespace zeus::game_menu
