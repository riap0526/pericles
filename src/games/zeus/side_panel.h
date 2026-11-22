// [ui] side_panel: Pericles's side panel, modeled on the side panel of Emperor: Rise of the Middle Kingdom.
//
// So far:
// - the names that the game's name bar showed under the category panel (categories, Build Road, Roadblock, Clear,
//   Undo, the buildings of a category and the report button) show as the game's own tooltips beside the cursor,
//   which follow Help > Balloon help; the name bar is no longer drawn, the building's cost under its button still is;
// - no tabs: the sidebar's right column (the title, the overlay buttons, the information panel and the category's
//   own panel) is moved up into their place and the information panel is longer; Tab does nothing; the title is
//   clicked to open the category's report and again to close it (it is lit while the mouse is on it), the report
//   button is gone; the Overview category no longer shows the minimap.
//
// All or nothing: every hook is installed at start-up, only when every site has the bytes of the known build; a fault
// in any of them switches the whole side panel off for the rest of the run (the game's own from then on, the screen
// drawn again). original: the game's own side panel. Only while a city is played with its sidebar (not in the
// editors).
#pragma once

#include "games/zeus/site.h"

namespace zeus::side_panel {

// Start-up, before the settings are loaded: [ui] side_panel.
void RegisterSettings();

// Start-up: checks every site and installs every hook, or none (logged). True when installed.
bool Attach();

// The setting is pericles, the hooks are in, nothing has failed, a city is played with its sidebar.
bool Active();

// sidebar_draw (games/zeus/resolution's detour): true before the game's drawing, false after it. The column is
// taken from the screen while the backdrop has just been drawn.
void BackdropDrawing(bool drawing);

// Offline tests.
const Detour *Detours(int *count);
// Attach with every site matching except the one at index mismatch (sites, then code changes; -1: all match); the
// hooks are only counted, not installed (the test calls the detours itself). Returns how many would have been
// installed.
int AttachForTest(int mismatch);
int SiteCountForTest(); // sites and code changes
int PatchCountForTest();
// Entry 1233's rows above the title and its left part (nullptr: not read).
void SetTopForTest(const unsigned short *top, const unsigned short *left);
// The setting's value as the settings would hand it over (initial: as at start-up, nothing redrawn).
void SetModeForTest(const char *value, bool initial);

} // namespace zeus::side_panel
