// Offline test of games/zeus/anim_clock: the game's animation timer rule on the game-time clock (no game, no DLL).
#include "games/zeus/anim_clock.h"

#include <cstdio>

namespace {

int g_failed = 0;

void Check(bool ok, const char *what)
{
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) {
        ++g_failed;
    }
}

// How many of n ticks of stepMs light timer i.
int Fires(int i, unsigned step, int n)
{
    zeus::anim::Clock c;
    unsigned char due[zeus::anim::kTimers];
    zeus::anim::Advance(c, step, due); // the first tick (not counted)
    int count = 0;
    for (int k = 0; k < n; ++k) {
        zeus::anim::Advance(c, step, due);
        count += due[i];
    }
    return count;
}

} // namespace

int main()
{
    {
        zeus::anim::Clock c;
        unsigned char due[zeus::anim::kTimers];
        zeus::anim::Advance(c, 22, due);
        bool rest = false;
        for (int i = 2; i < zeus::anim::kTimers; ++i) {
            rest = rest || due[i];
        }
        Check(due[0] == 1 && due[1] == 1 && !rest,
              "first tick (the clock and the timers start at 0): 22 ms passed, only timers 0 and 1 fire");
    }
    // Speed 90%: a tick every 22 ms.
    Check(Fires(0, 22, 100) == 100, "90%: timer 0 fires on every tick");
    Check(Fires(1, 22, 100) == 100, "90%: timer 1 (20 ms) fires on every tick");
    Check(Fires(2, 22, 100) == 50, "90%: timer 2 (40 ms) on every second tick (44 ms)");
    Check(Fires(3, 22, 99) == 33, "90%: timer 3 (60 ms) on every third tick (66 ms)");
    Check(Fires(5, 22, 100) == 20, "90%: timer 5 (100 ms) on every fifth tick (110 ms)");
    Check(Fires(50, 22, 460) == 10, "90%: timer 50 (1000 ms) on every 46th tick (1012 ms)");
    // 100% capped: a tick every 16 ms.
    Check(Fires(1, 16, 100) == 50, "100%: timer 1 (20 ms) on every second tick (32 ms)");
    Check(Fires(4, 16, 100) == 20, "100%: timer 4 (80 ms) on every fifth tick (80 ms)");
    // Slow speeds: every timer up to the tick length fires on every tick.
    Check(Fires(5, 112, 50) == 50, "50%: timer 5 (100 ms) on every tick (112 ms)");
    Check(Fires(6, 112, 50) == 25, "50%: timer 6 (120 ms) on every second tick");
    // The clock wraps around (about 49 days of game time) without a stall.
    {
        zeus::anim::Clock c;
        c.now = 0xFFFFFFF0u;
        for (unsigned &l : c.last) {
            l = 0xFFFFFFF0u;
        }
        unsigned char due[zeus::anim::kTimers];
        zeus::anim::Advance(c, 22, due); // now = 6 after the wrap: 22 ms since every timer fired
        Check(due[1] == 1 && due[2] == 0, "across the wrap-around: 22 ms after firing, timer 1 fires, timer 2 not");
    }
    std::printf("%s\n", g_failed ? "FAILED" : "all passed");
    return g_failed ? 1 : 0;
}
