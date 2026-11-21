// The game's own Display options dialog (game_screen 0x0a) with our display settings: the dialog's buttons, layout
// and what they do are the core's (core/options/display_dialog.h); here, Zeus's part (the addresses are in the Site
// declarations of display_options.cpp).
//
// Drawn with Pericles's own drawing of the game's art (games/zeus/painter: the panel, the dialog's 20-high button bars,
// the round arrows, the game's fonts), in the original's place.
//
// How: display_options_draw 0x59af60 is hooked (our layout), and the three places in screen_handle_input that hand
// the dialog's button table (display_options_buttons 0x5ed528, 4 entries) to the game's button functions get ours
// (9 entries, the same 28-byte layout, the core's handlers). Laid out again behind the open dialog, the city is drawn
// again inside timing's drawing that never counts. Failure: the original table and drawing come back (the game's own
// four buttons; F5 still goes to our window mode, F6 and F7 from its buttons are not passed to the game).
//
// Threads: Attach at start-up; the rest on the game thread.
#pragma once

#include "games/zeus/site.h"

namespace zeus::display_options {

// From zeus_attach, after resolution, painter and keys (the game's texts through keys' lang_get_string hook). Checks
// the game's bytes and installs the dialog (logged). False: the game's own dialog stays.
bool Attach(bool painter);

// Once per main-loop iteration (game thread), before the game's pass: a dialog closed without OK puts the settings
// back.
void OnLoop();

// The smoke test's action: the dialog opened as Options > Display options opens it, with the screen it goes back to
// the one shown now (the game takes the screen under its drop-down menu). False: not installed.
bool OpenForTest();

// For the offline test (src/games/zeus/test_restore.cpp): installed on the test's copy of the game without the site
// checks (its table pushes must be there); the detour of display_options_draw.
bool AttachForTest();
const Detour *Detours(int *count);

} // namespace zeus::display_options
