// The figures' animation timing inside a simulation tick (anim_clock.h).
#include "games/zeus/anim_clock.h"

namespace zeus::anim {

void Advance(Clock &c, std::uint32_t stepMs, unsigned char due[kTimers])
{
    c.now += stepMs;
    for (int i = 0; i < kTimers; ++i) {
        // Unsigned difference: correct across the clock's wrap-around (after about 49 days of game time).
        if (c.now - c.last[i] >= static_cast<std::uint32_t>(i) * kStepMs) {
            due[i] = 1;
            c.last[i] = c.now;
        }
        else {
            due[i] = 0;
        }
    }
}

} // namespace zeus::anim
