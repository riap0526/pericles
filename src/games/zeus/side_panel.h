// [ui] side_panel: Pericles's side panel, modeled on the side panel of Emperor: Rise of the Middle Kingdom.
//
// So far:
// - the names that the game's name bar showed under the category panel (categories, Build Road, Roadblock, Clear,
//   Undo, the buildings of a category and the report button) show as the game's own tooltips beside the cursor,
//   which follow Help > Balloon help; the name bar is no longer drawn, the building's cost under its button still is;
// - no tabs: the sidebar's right column (the title, the overlay buttons, the information panel and the category's
//   own panel) is moved up into their place and the information panel is longer; Tab does nothing; the title is
//   clicked to open the category's report and again to close it (it is lit while the mouse is on it), the report
//   button is gone; the Overview category no longer shows the minimap;
// - no picture: below the Go to buttons one box shows the minimap or the messages (as many as fit, the one clicked
//   opened), switched with the button in its corner or with Tab, and back to the minimap for every city started or
//   loaded; a Condensed message brings it to the messages; the check, gem and globe below it; on taller screens an
//   inset panel below 768. The army and ship panels have the same box;
// - the army and ship panels' upper half as Emperor's: no minimap of its own at the top, the name box, the state and
//   the command buttons moved up into its place, the inset panel below them taller;
// - the category panels in the taller column: Military's lower block against the information panel's bottom edge
//   with Auto Defend as tall as the buttons beside it; the Overview category's panel down to the old minimap's place
//   with up to 8 requests, each one clicked and with its tooltip; the City Overview report's request tooltips below
//   the cursor;
// - the world and campaign map sidebars: Back to City and the question mark on the bottom row (as the city sidebar's
//   check, gem and globe), the picture above them; on taller screens an inset panel below 768.
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

// The world map's sidebar is laid out as ours (its buttons on the bottom row, the picture above them): the game's
// code is ours now. games/zeus/resolution then fills below 768 without moving the picture.
bool WorldLayout();

// sidebar_draw (games/zeus/resolution's detour): true before the game's drawing, false after it. The column is
// taken from the screen while the backdrop has just been drawn.
void BackdropDrawing(bool drawing);

// sidebar_draw_units (games/zeus/resolution's detour), after the game's drawing: the army or ship panel's upper half
// moved up with its taller inset, its lower half laid out (the box, the button row).
void ArmyBackdropDrawn();

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
// The box shows the messages (else the minimap).
bool BoxMessagesForTest();
// The table of the Overview category's request rows (8 records).
const unsigned char *RequestTableForTest();
// Zeus_Interface entry 1600's rows 762..767 (186 x 6; nullptr: not read).
void SetWorldStoneForTest(const unsigned short *stone);

} // namespace zeus::side_panel
