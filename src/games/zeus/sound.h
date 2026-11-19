// The game's sound (sound.cpp).
//   - The game's 3D sounds off, always. Zeus plays its layer-1 ambience (Audio\sound_fxU.txt: wind, bells, shimmer
//     moving around the listener) on 3D channels. On today's Windows the game falls back to Intel's RSX software 3D
//     provider (MSSRSX.M3D), which waits 10 ms in every AIL_end_3D_sample; the game stops all its 3D channels at once
//     now and then, so the main thread stood still for about 220 ms every few seconds in the city. With the game's own
//     "3D sounds" switch off, game_frame neither starts 3D sounds nor stops them: no wait. The switch is turned off
//     before the game's sound starts, and put back to off at once when the sound options' button turns it on (and on
//     every pass); the game keeps it in Zeus.inf itself (0 is one of its own values). Nothing of ours is written to the
//     game's files.
//   - The layer-1 ambience is ours instead (core/sound/layer1.h; the same player as Yongle's nature
//     sounds): [audio] nature_sounds, nature_sounds_gap. Here: where the game keeps its switch, volume and driver.
//   - The core's sound (core/sound/sound.h, core/sound/master_volume.h; the same as Yongle's): the master
//     volume, paused while switched away, ambient sounds per second, the pan limit, quiet under our windows, the
//     10 s counts. Here are Zeus's hooks and where the game keeps its sound manager.
#pragma once

#include "games/zeus/site.h"

namespace zeus::sound {

// From zeus_register_settings: [audio] master_volume, master_sound, background_sound, ambient_sounds, nature_sounds,
// nature_sounds_gap; [debug] sound_probe, nature_sounds_only (the core's).
void RegisterSettings();

// From zeus_attach (before game_main runs): checks the sites and hooks them; each part is left out (logged) when its
// site does not match: the game's own way for that part.
void Attach();

// Once per game pass that runs (timing.cpp's game_frame detour, before the game's pass): the game's 3D sounds kept
// off, then the core's sound (core::sound::OnLoop, which drives the layer-1 sounds).
void OnLoop();

// For the offline tests (src/games/zeus/test_restore.cpp): the detours by site, and the core's sound attached with
// Zeus's sound manager as if every site were hooked (nothing is hooked).
const Detour *Detours(int *count);
void AttachForTest();

} // namespace zeus::sound
