// Frames, the simulation's pace and the figures' animation timing in Zeus.
//
//   game_frame     0x513580  one frame (counted; drives the rate log). While our panel is open it is not run: the
//                            game is paused (no simulation, drawing or input), as in Emperor's layer. With present =
//                            display / cap60 it runs at most twice per screen refresh (the main loop never rests).
//   screen_present 0x50c1b0  the game's presentation (__cdecl, 1 argument): it presents only when more than 49 ms
//                            have accumulated, redraw_request is set or the argument is 1. When the output thread
//                            wants a new frame the call is made with 1, so every frame the game draws can reach the
//                            screen. Frames the game itself insists on are held for the 50 ms the game would have
//                            shown them. Hooked only for present = display / cap60.
//   sim_tick_due   0x5168a0  whether a tick runs in this frame: gameplay.keep_speed makes up the ticks owed after a
//                            slow frame, gameplay.full_speed gives 100% a steady pace, gameplay.simulate_while_scrolling
//                            lets the city go on while the view scrolls at the screen edge (as Emperor's layer).
//   sim_tick       0x513a30  one tick: the ticks made up run right after the game's own one; every tick runs with the
//                            animation flags of the game's time (games/zeus/anim_clock.h).
//   city_render    0x44a830, 0x44a930  the city drawing: drawn on every frame, but only one drawing every 50 ms counts
//                            for the animations that move on each drawing; after the others the tile animation bytes,
//                            buildings, one figure field and the drawing counters are put back (timing.cpp).
#pragma once

#include "games/zeus/site.h"

namespace zeus::timing {

// From zeus_register_settings: [gameplay] keep_speed, full_speed, simulate_while_scrolling.
void RegisterSettings();

// From zeus_attach after the exe check. Installs the hooks above; logs what is not installed and why.
void Attach();

// Offline tests only (src/games/zeus/test_restore.cpp): the detours above by site name; the copy buffer of the
// drawn-frame animations without the site checks of Attach; whether each feature is still on.
const Detour *Detours(int *count);
bool PrepareForTest();
bool PaceWorking();
bool AnimWorking();
bool DrawnWorking();

} // namespace zeus::timing
