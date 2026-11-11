// The figures' animation timing inside a simulation tick.
//
// The game keeps 51 animation timers and, once per frame and by the real time (anim_timers_update 0x413330), lights
// flag i in anim_due 0x6668ec when i * 20 ms have passed since the timer last fired; the flags are cleared again on
// the next frame. Many figures' actions, which run in the simulation tick, advance their animation only when the
// flag of their picture's animation speed is lit (anim_advance 0x5af8c0). On a fast computer a frame takes a
// millisecond or two and a tick runs only every 20 ms or more, so the frame that lights a flag rarely has a tick:
// those animations hardly move (gods appearing slowly, urchin collectors at an irregular pace).
//
// The fix (keep each picture's own speed): for the length of each tick the flags are replaced by
// flags of the same rule on a clock of the game's time, which moves on by one tick's length per tick; after the tick
// the frame's own flags are put back for the drawing. This is what the game does on a computer that draws exactly
// one frame per tick.
//
// No Windows or game dependency: tested offline (test_anim_clock.cpp).
#pragma once

#include <cstdint>

namespace zeus::anim {

constexpr int kTimers = 51;         // anim_timer 0x666820 (51 dwords), anim_due 0x6668ec (51 bytes)
constexpr std::uint32_t kStepMs = 20; // timer i fires every i * 20 ms (anim_timers_update)

struct Clock {
    std::uint32_t now = 0;           // the game's time in ms, moved on by Advance
    std::uint32_t last[kTimers] = {}; // when each timer last fired (as anim_timer)
};

// Moves the clock on by stepMs and fills due[i] with 1 for the timers that fire now (the game's rule: now >=
// last[i] + i * 20, then last[i] = now), 0 for the others. Timer 0 fires every time, as in the game.
void Advance(Clock &c, std::uint32_t stepMs, unsigned char due[kTimers]);

} // namespace zeus::anim
