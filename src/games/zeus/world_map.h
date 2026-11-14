// The world map in Zeus (world_map.cpp): the allies' flags at the original's pace, the map drawn with the game's
// own height; the map's layout at resolutions larger than the map (the core's map_screen, shared with Emperor's
// China map), the campaign map with it.
#pragma once

#include "games/zeus/site.h"

namespace zeus::world_map {

// From zeus_register_settings: [video] world_map, world_map_scale, world_map_background.
void RegisterSettings();

// From zeus_attach after the exe check: installs the flags and the map's height; logs what is not installed and why.
// Returns whether the map is drawn with the game's own height (needed for any screen higher than 768: see
// world_map.cpp).
bool Attach();

// After games/zeus/resolution and fullscreen (the sidebar's filler, the top bar's tiling and the border's tiles
// are theirs): the map's layout. False (the game's own drawing, top left) when a site does not match.
bool AttachLayout();

// For the offline tests (src/games/zeus/test_restore.cpp): the detours by site; the layout on or off (as AttachLayout
// would set it, without hooks).
const Detour *Detours(int *count);
void LayoutForTest(bool on);

} // namespace zeus::world_map
