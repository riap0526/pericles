// The game's 3D sounds off at every start (sound.cpp). Zeus plays its layer-1 ambience (Audio\sound_fxU.txt: wind,
// bells, shimmer moving around the listener) on 3D channels. On today's Windows the game falls back to Intel's RSX
// software 3D provider (MSSRSX.M3D), which waits 10 ms in every AIL_end_3D_sample; the game stops all its 3D channels
// at once now and then, so the main thread stood still for about 220 ms every few seconds in the city. With the
// game's own "3D sounds" switch off, game_frame neither starts 3D sounds nor stops them: no wait. The switch is turned
// off before the game's sound starts; the game keeps it in Zeus.inf itself (0 is one of its own values), and its
// sound options can still turn it on for a run. Nothing of ours is written to the game's files.
#pragma once

#include "games/zeus/site.h"

namespace zeus::sound {

// From zeus_attach (before game_main runs): checks sound_init and hooks it; when it does not match nothing is
// installed (logged): the game's own switch is used as it is.
void Attach();

// For the offline tests (src/games/zeus/test_restore.cpp): the detours by site.
const Detour *Detours(int *count);

} // namespace zeus::sound
