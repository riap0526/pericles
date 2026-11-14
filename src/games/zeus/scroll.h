// Smooth scrolling of the city view in Zeus -- the camera half (core/scroll/camera, shared with Emperor's layer).
// At the game's city calls of scroll_get_direction (0x50b7b0) the camera moves view_x / view_y by time at a steady
// speed for each scroll speed setting (core/scroll/camera); the game's whole-tile step is then told "no direction".
// The city is still drawn on whole tiles (showing the pixel remainder is a later sub-step), so the remainder only
// carries the fraction of a tile to the next frames.
#pragma once

#include "games/zeus/site.h"

#include <cstddef>

namespace zeus::scroll {

// Checks the sites and the city call sites, installs the hooks. False (logged, nothing installed) when one does not
// match: the game's own scrolling. With smooth off (gameplay.smooth_scroll) the hooks pass the calls through and
// count the game's own steps; the setting can be switched on from the panel.
bool Attach(bool smooth);

// The setting changed while running (game thread; the game is paused by our panel then). False when it cannot be
// switched on: the feature was switched off after an error.
bool SetSmooth(bool smooth);

// From game_frame's detour, once per frame that runs: the milliseconds since the previous one.
void OnFrame(double ms);

// After a pause (our panel was open): the scroll speed starts again from rest.
void Resume();

// Scroll statistics since the last call, for the rate log ("" when the view did not scroll).
void TakeStats(char *out, std::size_t size);

// False when smooth scrolling is not installed (a site does not match) or was switched off after an error.
bool Working();

// For the offline tests (src/games/zeus/test_restore.cpp): the detours by site, the setting without the hooks, the
// pixel remainder.
const Detour *Detours(int *count);
void StartForTest(bool smooth);
void Remainder(int *px, int *py);

} // namespace zeus::scroll
