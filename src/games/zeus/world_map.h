// The world map in Zeus (world_map.cpp). The allies' flags at the original's pace.
#pragma once

#include "games/zeus/site.h"

namespace zeus::world_map {

// From zeus_attach after the exe check: installs the hooks; logs what is not installed and why. Returns whether the
// map is drawn with the game's own height (needed for any screen higher than 768: see world_map.cpp).
bool Attach();

// For the offline tests (src/games/zeus/test_restore.cpp): the detours by site.
const Detour *Detours(int *count);

} // namespace zeus::world_map
