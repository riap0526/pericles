// The city's zoom in Zeus: the mouse wheel over the city zooms 25% a notch (50..150)
// around the cursor, the middle button goes back to 100; [video] city_zoom is the zoom at start-up, city_zoom_shrink
// how the city is made smaller. The zoom is the core's (core/scroll/zoom.h); this file gives it Zeus's addresses (the
// map's range, the bottom margin, the city screens) and hooks view_clamp. games/zeus/resolution lays the view out for
// it, games/zeus/scroll passes its drawing and camera callbacks to the picture and the camera.
#pragma once

#include "games/zeus/site.h"

namespace zeus::zoom {

// From the settings registration (DllMain): the core's [video] city_zoom and city_zoom_shrink.
void RegisterSettings();

// After smooth scrolling and the layout are installed: view_clamp's hook and the core's zoom (the wheel's mouse
// filter). False (logged) when view_clamp does not match or cannot be hooked: the city stays at 100.
bool Attach();

// The current map's range (columns x half rows; 0 or less before a map), for the rate log.
void MapRange(int *cols, int *rows);

// The limits view_clamp works with now (the city picture, games/zeus/city_picture): view x from left to right -
// columns, view y from top to bottom - half rows (bottom: the map's bottom less the margin + 4 the game keeps below the
// screen); mapRows: the map's range in half rows.
void ViewLimits(int *left, int *right, int *top, int *bottom, int *mapRows);

// games/zeus/resolution: lays the city's view out again (the viewport hooks' work, for the last viewport kind).
void SetRelayout(void (*fn)());

// For the offline tests (src/games/zeus/test_restore.cpp): the core's zoom given Zeus's view without the hook, and
// the detour by site.
void InstallForTest();
const Detour *Detours(int *count);

} // namespace zeus::zoom
