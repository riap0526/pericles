// Pericles's options window: core/options/options_host with Pericles's pages, drawn with games/zeus/painter, over the
// game's last frame while the game is paused; its pause menu (games/zeus/game_menu) goes with it. No Hotkeys page
// yet (the keys are set in the settings file or the debug panel).
//
// Threads: the game thread only.
#pragma once

#include "core/options/pause_menu.h"
#include "core/options/window.h"

#include <vector>

namespace zeus::options_window {

// From game_menu's Attach, with its pause menu (the painter's sites are checked by then): attaches the window, the
// pause menu and the modal input (one slot for both).
void Attach(const core::options::pause_menu::PauseGame &pause);
bool Available(); // attached and not switched off after an error

// Game thread: opens the window at a page and category (0-based; by name: the page's first when none has it). false:
// not available (the caller falls back to the game's own dialog).
bool Open(int page, int category);
bool OpenCategory(int page, const char *name);
// The pages and categories the menus open (BuildPages' order).
constexpr int kPageGeneral = 0, kPageVideo = 1, kPageAudio = 2;

bool IsOpen(); // the window or the pause menu: the game is paused and draws frames only

// Before the game's frame is presented: the window (or the pause menu) over it. caller: the game address
// screen_present returns to.
void OnPresent(unsigned long caller);

// For the offline test (src/games/zeus/test_restore.cpp): the pages as the window would build them now.
std::vector<core::options::Page> PagesForTest();

} // namespace zeus::options_window
