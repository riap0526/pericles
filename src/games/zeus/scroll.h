// Smooth scrolling of the city view in Zeus: the camera (core/scroll/camera) and the picture (core/scroll/picture),
// both shared with Emperor's layer. At the game's city calls of scroll_get_direction (0x50b7b0) the camera moves
// view_x / view_y by time at a steady speed for each scroll speed setting; the game's whole-tile step is then told "no
// direction". The city is drawn with the camera's pixel remainder, up to the sidebar and down to the screen's bottom,
// and the positions the game converts (clicks, the cursor tile, sounds, the drag selection) move the same way.
#pragma once

#include "core/diag/guard.h"
#include "core/scroll/picture.h"
#include "games/zeus/site.h"

#include <cstddef>

namespace zeus::scroll {

// Checks the sites, the city call sites and that city_render is hooked (games/zeus/timing), installs the hooks. False
// (logged, nothing installed) when one does not match: the game's own scrolling. With smooth off
// (gameplay.smooth_scroll) the remainder stays 0, the city is still drawn up to the sidebar and the game's own steps
// are counted; the setting can be switched on from the panel.
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

// From city_render's detour (games/zeus/timing, inside the drawn-frame state): the city drawn (the game's own drawing
// when smooth scrolling is not installed).
void DrawCity(int x, int y, int z);

// From city_render_2's detour (the figure portrait): the conversions made while it draws are not moved (the remainder
// is 0 meanwhile); the changes go into r, undone by the caller after the drawing.
void PortraitDrawing(core::diag::Restore &r);

// From the layout (games/zeus/resolution) after each viewport and sidebar position: where the city is seen to beyond
// its clip (the sidebar's left edge, the screen's bottom), 0 = the clip (the game's own layouts).
void SetCityExtent(int right, int bottom);

// The city as shown right after it was drawn (games/zeus/fullscreen: the menus' background): false when it is not
// inside the back buffer (why: the reason).
bool DrawnCity(int *left, int *top, int *right, int *bottom, const char **why);

// For a problem report (game thread): saves our city buffer as last drawn, or logs that it has not been used yet.
void SaveCityBuffer(int report);

// The picture's view of Zeus, for the core's zoom (games/zeus/zoom: core/scroll/zoom.h ZoomGame).
const core::scroll::picture::PictureGame &Picture();

// For the offline tests (src/games/zeus/test_restore.cpp): the detours by site, the setting without the hooks, the
// pixel remainder.
const Detour *Detours(int *count);
void StartForTest(bool smooth);
void SetRemainderForTest(int px, int py);
void Remainder(int *px, int *py);

} // namespace zeus::scroll
