// Frames, the simulation's pace and the figures' animation timing in Zeus (timing.h; the addresses are
// in the Site and Var declarations below).
#include "games/zeus/timing.h"

#include "core/capture/screenshot.h"
#include "core/config.h"
#include "core/diag/environment.h"
#include "core/diag/guard.h"
#include "core/diag/perf.h"
#include "core/diag/report.h"
#include "core/diag/stall.h"
#include "core/log.h"
#include "core/output/sdl_output.h"
#include "core/platform/timing.h"
#include "core/settings/settings.h"
#include "core/ui/panel.h"
#include "games/zeus/anim_clock.h"
#include "games/zeus/diag.h"
#include "games/zeus/site.h"

#include <mmsystem.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

using zeus::Function;
using zeus::GameAddress;
using zeus::Install;
using zeus::Matches;
using zeus::Site;
using zeus::Var;

// ---------------------------------------------------------------------------------------------
// Addresses (Zeus.exe 2.1.4.0)

const Site kGameFrame{"game_frame", 0x513580, {0x8b, 0x0d, 0xc0, 0xaa, 0xf2, 0x00, 0x83, 0xec}};
const Site kScreenPresent{"screen_present", 0x50c1b0, {0xa1, 0x40, 0xc0, 0x63, 0x00, 0x83, 0xec, 0x20}};
const Site kSimDue{"sim_tick_due", 0x5168a0, {0x56, 0x57, 0xff, 0x15, 0x24, 0x82, 0x5d, 0x00}};
const Site kSimTick{"sim_tick", 0x513a30, {0xa1, 0x24, 0x36, 0xf1, 0x00, 0x8b, 0x0d, 0xd4}};
// Called, not hooked: what game_frame runs after every tick (0x51365d), for the ticks made up.
const Site kAfterTick{"sim_after_tick", 0x5123d0, {0xa1, 0x2c, 0x5e, 0x66, 0x00, 0x83, 0xec, 0x64}};
// Checked only, so that the tables below are the ones the game uses: anim_timers_update clearing anim_due (mov edi,
// 0x6668ec; mov esi, [frame_time]) and sim_tick_due reading the tick table (mov edx, [edx*4 + 0x63da1c]; add edx, 2).
const Site kAnimDueUse{"anim_timers_update", 0x413339, {0xbf, 0xec, 0x68, 0x66, 0x00, 0x8b, 0x35, 0xf4}};
const Site kTickTableUse{"sim_tick_due table", 0x516928, {0x8b, 0x14, 0x95, 0x1c, 0xda, 0x63, 0x00, 0x83}};

constexpr DWORD kDueFromFrame = 0x5135a9;  // game_frame's return address from sim_tick_due
constexpr DWORD kTickFromFrame = 0x5135c8; // ... and from sim_tick

const Var<char> kRedrawRequest{0xf2a6cc};  // screen_present presents at once while set
const Var<DWORD> kSimLastTick{0xf9f998};   // sim_last_tick_time
const Var<int> kTickCount{0xf9f9a4};       // game_frame counts each tick here before sim_tick
const Var<int> kEdgeScrolling{0xf2a6d0};   // set by scroll_get_direction at the screen edge; no tick meanwhile
const Var<char> kSimPaused{0xf2a768};      // the game's pause (P), toggled in game_wndproc 0x5c56ac
const Var<char> kFastForward{0xf2aa9c};    // game_frame runs ticks until a right-button release while set
const Var<int> kTickBlock{0x665dc8};       // no tick while set (sim_tick_due 0x516910)
const Var<int> kGameSpeed{0xf2a774};       // game_speed, percent
const Var<unsigned char> kGameScreen{0xf2a6dd};
const Var<int> kModeWidth{0xf2a6a0}, kModeHeight{0xf2a69c}; // dd_width, dd_height

// Where the time goes (core/diag/perf.h; the panel's Performance section and the overlay): a sample is one
// game_frame. The same idea as Yongle's "game pass" and "simulation".
namespace perf = core::diag::perf;
const int kFramePhase = perf::Register("game frame", perf::Thread::Game, true);
const int kSimPhase = perf::Register("simulation (sim_tick)", perf::Thread::Game);
const int kRenderPhase = perf::Register("city drawing (city_render)", perf::Thread::Game);
const int kPutBackPhase = perf::Register("drawn-frame animations (copy, put back)", perf::Thread::Game);
bool g_measured = false; // the output's frames per second measured once, in the city
constexpr DWORD kTickTable = 0x63da1c;     // sim_tick_delay (ms, + 2 in sim_tick_due), by (100 - speed) / 10
constexpr DWORD kAnimDue = 0x6668ec;       // anim_due, 51 bytes

constexpr DWORD kPresentGateMs = 50; // screen_present presents when more than 49 ms have accumulated
constexpr DWORD kRateLogMs = 10000;

using GameFrame_fn = void(__cdecl *)();
using ScreenPresent_fn = void(__cdecl *)(int);
using SimDue_fn = int(__cdecl *)();
using SimTick_fn = void(__cdecl *)();
GameFrame_fn o_GameFrame;
ScreenPresent_fn o_ScreenPresent;
SimDue_fn o_SimDue;
SimTick_fn o_SimTick;

// ---------------------------------------------------------------------------------------------
// Settings (games/emperor/emperor.cpp has the same three; the same defaults)

bool g_catchUp = true;           // gameplay.keep_speed
bool g_fullSpeedCapped = true;   // gameplay.full_speed
bool g_simWhileScrolling = true; // gameplay.simulate_while_scrolling

// ---------------------------------------------------------------------------------------------
// Counters and the rate log (game thread only)

long g_frames = 0, g_ticks = 0, g_animTicks = 0;
DWORD g_since = 0;
int g_lastSpeed = -1;
bool g_paused = false;

// The simulation pace (DueBody below; the same as Emperor's layer).
struct Pace {
    DWORD last = 0;    // our tick clock: the time the ticks run so far were owed at
    bool valid = false;
    int extra = 0;     // ticks owed beyond the game's own one in this frame
    long caughtUp = 0; // extra ticks run (for the rate log)
    int mostInOnePass = 0;
    DWORD lastPass = 0; // falling behind: the time between frames, smoothed
    double passMs = 0;
    bool behind = false;
    long dropped = 0;
    long droppedSpell = 0;
    long gaps = 0;
};
Pace g_pace;

// ---------------------------------------------------------------------------------------------
// The frame cap. The game's main loop never rests: game_frame ran 500-15000 times a second although at most one
// frame per screen refresh can be shown (measured). Each extra frame redraws the city (and, for the drawn-frame
// animations below, copies and puts back about a megabyte) for nothing, and the per-frame chances in game_frame
// (random ambient sounds 0x5a4790, the sound fades 0x5a24f0) follow the frame count. game_frame now runs at most
// twice per screen refresh (cap60: 120 a second): the simulation is timed by sim_tick_due and keep_speed makes up
// what a frame owes, the animations follow the clock, so nothing in the game's timing changes. Only with
// present = display / cap60 (the output thread knows the refresh rate).
bool g_frameCapOn = false; // Attach: present = display / cap60
double g_nextFrameMs = 0;

double NowMs()
{
    static LARGE_INTEGER freq{};
    if (freq.QuadPart == 0) {
        QueryPerformanceFrequency(&freq);
    }
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return static_cast<double>(c.QuadPart) * 1000.0 / static_cast<double>(freq.QuadPart);
}

// Frames a second allowed now (0: no cap, e.g. the refresh rate is not known yet).
int FrameCap()
{
    if (!g_frameCapOn) {
        return 0;
    }
    int hz = core::output::RefreshHz();
    if (hz <= 0) {
        return 0;
    }
    if (core_config_get()->present == CORE_PRESENT_CAP60 && hz > 60) {
        hz = 60;
    }
    // Twice the shown rate: the game's frames are not in step with the screen's refresh, and at exactly the refresh
    // rate some refreshes would find no new frame (the same picture twice, a judder when scrolling).
    return 2 * hz;
}

// True when game_frame may run now; otherwise waits a little (the main loop calls again).
bool FrameDue()
{
    const int cap = FrameCap();
    if (cap == 0) {
        return true;
    }
    const double interval = 1000.0 / cap;
    const double now = NowMs();
    if (g_nextFrameMs != 0 && now < g_nextFrameMs) {
        // At least 1.5 ms left: Sleep(1) (1 ms with high_resolution_timer; otherwise do not sleep, the cap still
        // holds by returning at once).
        if (g_nextFrameMs - now >= 1.5 && core_timer_resolution_us() <= 2000) {
            Sleep(1);
        }
        return false;
    }
    // An even rhythm; after a pause or a slow frame, start again from now.
    g_nextFrameMs = (g_nextFrameMs == 0 || now - g_nextFrameMs > 4 * interval) ? now + interval : g_nextFrameMs + interval;
    return true;
}

void DecideCountingFrame();                         // drawn-frame animations (below)
void TakeDrawnCounts(long *counted, long *reverted); // ... for the rate log
void TakeCopyStats(long long *bytes, DWORD *buildings);

void LogRates(const char *why)
{
    const DWORD now = GetTickCount();
    const DWORD ms = now - g_since;
    if (ms == 0) {
        return;
    }
    long presented = 0, shown = 0;
    core::output::TakeCounts(&presented, &shown);
    char paceNote[160] = "";
    size_t len = 0;
    if (g_pace.caughtUp > 0) {
        len += std::snprintf(paceNote + len, sizeof(paceNote) - len, "; %ld ticks made up (at most %d in one frame)",
                             g_pace.caughtUp, g_pace.mostInOnePass);
        g_pace.caughtUp = 0;
    }
    g_pace.mostInOnePass = 0;
    if (g_pace.dropped > 0 && len < sizeof(paceNote)) {
        len += std::snprintf(paceNote + len, sizeof(paceNote) - len, "; %ld ticks dropped (falling behind)",
                             g_pace.dropped);
        g_pace.dropped = 0;
    }
    if (g_pace.gaps > 0 && len < sizeof(paceNote)) {
        std::snprintf(paceNote + len, sizeof(paceNote) - len, "; %ld pause(s) not made up", g_pace.gaps);
        g_pace.gaps = 0;
    }
    static const char *const kPresent[] = {"original", "display", "cap60"};
    const double s = ms / 1000.0;
    long counted = 0, reverted = 0;
    TakeDrawnCounts(&counted, &reverted);
    long long copied = 0;
    DWORD buildings = 0;
    TakeCopyStats(&copied, &buildings);
    long multi = 0, torn = 0;
    core::diag::TakeIntegrity(&multi, &torn);
    core_log("rates%s: frames %.0f/s (at most %d/s), sim ticks %.1f/s (%ld with game-time animation), city drawings "
             "%.1f/s counting + %.1f/s put back (%.0f MB/s, buildings 0..%lu), game presents %.1f/s, frames shown "
             "%.1f/s over %lu ms (present=%s, display %d Hz, game speed %d%%, screen %d, mode %dx%d, timer %u us); "
             "frame check: %ld game frames with 2+ presents, %ld torn%s",
             why, g_frames / s, FrameCap(), g_ticks / s, g_animTicks, counted / s, reverted / s,
             copied / s / (1024.0 * 1024.0), static_cast<unsigned long>(buildings), presented / s, shown / s, ms,
             kPresent[core_config_get()->present], core::output::RefreshHz(), *kGameSpeed,
             static_cast<int>(*kGameScreen), *kModeWidth, *kModeHeight, core_timer_resolution_us(), multi, torn,
             paceNote);
    // Where the time goes (perf phases), how evenly frames were shown, processor time given to each thread -- the
    // same lines as Yongle's rate log.
    char times[768], longest[768];
    core::diag::perf::TakeWindow(times, sizeof(times), longest, sizeof(longest));
    if (times[0]) {
        core_log("time%s (ms): %s", why, times);
        core_log("time max%s (ms): %s", why, longest);
    }
    char intervals[512];
    core::output::TakeIntervals(intervals, sizeof(intervals));
    if (intervals[0]) {
        core_log("intervals%s (ms): %s", why, intervals);
    }
    char cpu[512];
    core::diag::env::TakeWindow(cpu, sizeof(cpu));
    if (cpu[0]) {
        core_log("cpu%s: %s", why, cpu);
    }
    g_frames = g_ticks = g_animTicks = 0;
    g_since = now;
}

void ResetWindow()
{
    g_since = GetTickCount();
    g_frames = g_ticks = g_animTicks = 0;
    // The output's counts and the drawing counts of the time before (e.g. the panel open: the output thread went on
    // showing frames) belong to no window: dropped, so the next line is not inflated (seen in testing: "frames shown
    // 1343/s" after the panel).
    long a = 0, b = 0;
    core::output::TakeCounts(&a, &b);
    TakeDrawnCounts(&a, &b);
}

// ---------------------------------------------------------------------------------------------
// game_frame

void __cdecl GameFrame_detour()
{
    if (core::ui::IsOpen()) {
        if (!g_paused) {
            g_paused = true;
            LogRates(" (panel opened: game paused)");
        }
        Sleep(5);
        return;
    }
    if (g_paused) {
        g_paused = false;
        core_log("zeus: panel closed: game resumed");
        ResetWindow();
        g_nextFrameMs = 0;
    }
    if (!FrameDue()) {
        return;
    }
    ++g_frames;
    DecideCountingFrame();
    zeus::diag::FrameBegin();
    core::diag::GameFrameBegin(); // frame check: presents within one frame
    core::diag::env::NoteCore(core::diag::env::Thread::Game);
    // A city frame of a second or more is sampled (other screens load files in one frame).
    const bool city = *kGameScreen == 1;
    if (city && !g_measured) {
        g_measured = true;
        core::output::MeasureFromNow(); // the output's frames per second, measured in the city (once)
    }
    if (city) {
        core::diag::stall::Begin(core::diag::stall::Thread::Game);
    }
    const std::int64_t t0 = perf::Now();
    o_GameFrame();
    perf::Add(kFramePhase, perf::Now() - t0);
    if (city) {
        core::diag::stall::End(core::diag::stall::Thread::Game);
    }
    perf::EndSample(perf::Thread::Game);
    core::diag::GameFrameEnd();
    zeus::diag::FrameEnd();
    if (g_since == 0) {
        g_since = GetTickCount();
    }
    if (int report = core::diag::TakeReportRequest()) { // the game's part of a problem report
        char why[40];
        std::snprintf(why, sizeof(why), " (report #%d)", report);
        LogRates(why);
        char state[256];
        zeus::diag::Describe(state, sizeof(state));
        core_log("report #%d: %s", report, state);
        zeus::diag::CensusForReport();
    }
    const int speed = *kGameSpeed;
    if (speed != g_lastSpeed) {
        if (g_lastSpeed >= 0) {
            LogRates(" (before speed change)");
        }
        core_log("zeus: game speed %d%% (was %d%%)", speed, g_lastSpeed);
        g_lastSpeed = speed;
        ResetWindow();
    }
    if (GetTickCount() - g_since >= kRateLogMs) {
        LogRates("");
    }
}

// ---------------------------------------------------------------------------------------------
// screen_present

void __cdecl ScreenPresent_detour(int param)
{
    if (param == 1 || *kRedrawRequest != 0) {
        core::output::HoldNextFrame(kPresentGateMs);
    }
    else if (core::output::FrameWanted()) {
        param = 1; // present the frame just drawn: the output thread is ready for it
    }
    o_ScreenPresent(param);
}

// ---------------------------------------------------------------------------------------------
// Simulation pace (as Emperor's layer). The game runs at most one tick per frame and sets
// its tick time to "now" when it runs one, so a slow frame stretches the interval to the next tick. As Augustus
// does: the time owed is kept and the ticks owed run in the same frame, at most 20, the rest carried over.
//   sim_tick_due still decides whether a tick may run (pause, screen, dialogs); only the time test is ours: its tick
//   time is set so that its own test agrees with ours, and set back to our clock afterwards.
//   100% (2 ms, "as fast as it can"): 16 ms unless gameplay.full_speed = original.

constexpr int kMaxTicksPerFrame = 20;
constexpr double kBehindMs = 50.0;   // fewer than 20 frames a second: at most kBehindTicks per frame
constexpr double kCaughtUpMs = 35.0;
constexpr int kBehindTicks = 2;
constexpr DWORD kGapMs = 250;        // a longer gap between frames is a pause (panel, loading): not made up
constexpr DWORD kCappedMs = 16;
core::diag::Feature g_paceFeature{"simulation pace (games/zeus/timing)"};

// The length of one tick at the current speed, as the pace runs it (0: the game's own pace, no fixed length).
DWORD TickInterval()
{
    int idx = (100 - *kGameSpeed) / 10;
    idx = idx < 0 ? 0 : idx;
    if (idx >= 10) {
        return 0;
    }
    if (idx == 0) {
        return g_fullSpeedCapped ? kCappedMs : 0;
    }
    return static_cast<DWORD>(reinterpret_cast<const int *>(kTickTable)[idx] + 2);
}

struct DueCall {
    int result;
    core::diag::Restore r;
};

void DueBody(DueCall &c)
{
    g_pace.extra = 0;
    if (g_simWhileScrolling) {
        c.r.Set(kEdgeScrolling.ptr(), 0);
    }
    const DWORD interval = TickInterval();
    if (!g_catchUp || interval == 0) {
        g_pace.valid = false;
        c.result = o_SimDue();
        c.r.Undo();
        return;
    }
    const DWORD now = timeGetTime();
    if (!g_pace.valid) {
        g_pace.last = *kSimLastTick;
        g_pace.valid = true;
    }
    if (g_pace.lastPass != 0) {
        double dt = static_cast<double>(now - g_pace.lastPass);
        dt = dt > 200 ? 200 : dt;
        g_pace.passMs = g_pace.passMs * 0.9 + dt * 0.1;
        if (!g_pace.behind && g_pace.passMs > kBehindMs) {
            g_pace.behind = true;
            g_pace.droppedSpell = 0;
            core_log("zeus: falling behind (a frame every %.0f ms): at most %d ticks per frame, the rest is dropped "
                     "(the game runs slower instead of jumping)", g_pace.passMs, kBehindTicks);
        }
        else if (g_pace.behind && g_pace.passMs < kCaughtUpMs) {
            g_pace.behind = false;
            core_log("zeus: no longer behind (a frame every %.0f ms): ticks are made up again; %ld dropped",
                     g_pace.passMs, g_pace.droppedSpell);
        }
    }
    if (g_pace.lastPass != 0 && now - g_pace.lastPass > kGapMs) {
        g_pace.last = now - interval; // after a pause: one tick as soon as allowed, nothing made up
        ++g_pace.gaps;
    }
    g_pace.lastPass = now;
    if (static_cast<int>(now - g_pace.last) < 0) {
        g_pace.last = now - interval; // the clock went back: one tick owed
    }
    const bool owed = now - g_pace.last >= interval;
    // The game's own time test agrees with ours: far in the past when a tick is owed, now when not.
    c.r.Set(kSimLastTick.ptr(), owed ? now - 100000 : now);
    c.result = o_SimDue();
    c.r.Undo();
    if (c.result > 0) {
        DWORD n = (now - g_pace.last) / interval;
        const DWORD cap = g_pace.behind ? kBehindTicks : kMaxTicksPerFrame;
        if (n > cap) {
            if (g_pace.behind) {
                g_pace.dropped += static_cast<long>(n - cap);
                g_pace.droppedSpell += static_cast<long>(n - cap);
            }
            n = cap;
            g_pace.last = now - (now - g_pace.last) % interval;
        }
        else {
            g_pace.last += n * interval;
        }
        g_pace.extra = *kFastForward ? 0 : static_cast<int>(n) - 1;
        if (g_pace.extra + 1 > g_pace.mostInOnePass) {
            g_pace.mostInOnePass = g_pace.extra + 1;
        }
    }
    else if (owed) {
        g_pace.last = now - interval; // held back (pause, another screen, a dialog): one tick as soon as allowed
    }
    *kSimLastTick = g_pace.last;
}

int __cdecl SimDue_detour()
{
    if (GameAddress(__builtin_return_address(0)) != kDueFromFrame || !core::diag::Enabled(g_paceFeature)) {
        return o_SimDue();
    }
    DueCall c{0, {}};
    if (core::diag::Guarded(g_paceFeature, [](void *p) { DueBody(*static_cast<DueCall *>(p)); }, &c, &c.r)) {
        return c.result;
    }
    g_pace = Pace{};
    core_log("zeus: simulation pace back to the game's own after an error");
    return o_SimDue();
}

// ---------------------------------------------------------------------------------------------
// The animation flags of the game's time for each tick (anim_clock.h)

core::diag::Feature g_animFeature{"figure animation timing (games/zeus/timing)"};
zeus::anim::Clock g_animClock;
DWORD g_lastTickWall = 0;

struct AnimSwap {
    unsigned char frame[zeus::anim::kTimers]; // the frame's own flags, put back after the tick
    bool swapped = false;
};

// One tick's length on the game-time clock: the pace's interval, or (the game's own pace, or 100% "original") the
// real time since the previous tick, at most 1 s (a pause, loading).
DWORD AnimStep()
{
    const DWORD now = timeGetTime();
    const DWORD wall = g_lastTickWall == 0 ? zeus::anim::kStepMs : now - g_lastTickWall;
    g_lastTickWall = now;
    const DWORD interval = g_catchUp ? TickInterval() : 0;
    if (interval != 0) {
        return interval;
    }
    return wall > 1000 ? 1000 : wall;
}

void AnimBefore(void *p)
{
    AnimSwap &s = *static_cast<AnimSwap *>(p);
    unsigned char *due = reinterpret_cast<unsigned char *>(kAnimDue);
    std::memcpy(s.frame, due, sizeof(s.frame));
    unsigned char tick[zeus::anim::kTimers];
    zeus::anim::Advance(g_animClock, AnimStep(), tick);
    std::memcpy(due, tick, sizeof(tick));
    s.swapped = true;
}

// One tick with the game-time animation flags (falls back to the frame's flags, as the game, after an error).
void RunTick()
{
    AnimSwap s;
    if (core::diag::Enabled(g_animFeature) && core::diag::Guarded(g_animFeature, &AnimBefore, &s)) {
        ++g_animTicks;
    }
    {
        perf::Scope timed(kSimPhase);
        o_SimTick();
    }
    if (s.swapped) {
        std::memcpy(reinterpret_cast<unsigned char *>(kAnimDue), s.frame, sizeof(s.frame));
    }
}

// The ticks made up, right after the game's own one, each as game_frame runs one: sim_after_tick (which game_frame
// runs after every tick; for the last one it does so itself), the counter, then the tick.
void ExtraTicks(int n)
{
    const unsigned char screen = *kGameScreen;
    for (int k = 0; k < n; ++k) {
        if (*kFastForward || *kSimPaused || *kTickBlock || *kGameScreen != screen) {
            break; // the last tick opened a dialog, paused the game or changed the screen
        }
        Function<void(__cdecl *)()>(kAfterTick)();
        *kTickCount = *kTickCount + 1;
        ++g_ticks;
        ++g_pace.caughtUp;
        RunTick();
    }
}

void __cdecl SimTick_detour()
{
    ++g_ticks;
    RunTick();
    if (g_pace.extra > 0 && GameAddress(__builtin_return_address(0)) == kTickFromFrame &&
        core::diag::Enabled(g_paceFeature)) {
        int n = g_pace.extra;
        g_pace.extra = 0;
        if (!core::diag::Guarded(g_paceFeature, [](void *p) { ExtraTicks(*static_cast<int *>(p)); }, &n)) {
            g_pace = Pace{};
            core_log("zeus: simulation pace back to the game's own after an error in a tick made up");
        }
    }
}

// ---------------------------------------------------------------------------------------------
// Animations that move on every drawn frame. Parts of the city drawing advance
// their own counters each time the city is drawn: the per-tile animation bytes (e.g. 0x44c2f0: frame = byte / 2,
// + 1 per drawing), building fields (0x44df30's overlays, 0x45c710), a figure field (0x45b060) and the drawing
// counter 0x847d14 ((count + building) % 6 in 0x45d140). The game was made for a computer that draws about 20
// frames a second (its own presentation waits 50 ms); at hundreds of drawings a second those animations race (the
// hippodrome's spectators, the trading post).
// The city is still drawn on every frame, but only one drawing every kRenderAnimMs counts: after the others, what
// the drawing changed in those places is put back, as if that drawing had not happened. During a counting drawing
// anim_due holds flags of a clock that moves on by the real time since the previous counting drawing (so the
// animations that follow anim_due keep their speed); during the others it is all 0 (nothing advances).

constexpr DWORD kRenderAnimMs = 50;
const Site kCityRender{"city_render", 0x44a830, {0xa1, 0x14, 0x7d, 0x84, 0x00, 0x8b, 0x4c, 0x24}};
const Site kCityRender2{"city_render_2", 0x44a930, {0x8b, 0x4c, 0x24, 0x08, 0x8b, 0x44, 0x24, 0x04}};
// Checked only: the code that clears each array with the size used here (mov ecx, dwords; ... mov edi, base).
const Site kTileAnimClear{"tile animation clear", 0x496183, {0xb9, 0xc4, 0x32, 0x00, 0x00, 0xbf, 0x08, 0x4b}};
const Site kBuildingsClear{"buildings clear", 0x493e51, {0xb9, 0xc0, 0x45, 0x04, 0x00, 0x33, 0xc0, 0xbf}};
const Site kFiguresClear{"figures clear", 0x4d9ed1, {0xb9, 0xd0, 0xf5, 0x02, 0x00, 0x33, 0xc0, 0xbf}};
// size bytes at address, or (count > 1) size bytes at address + i * stride for each of count records.
struct Region {
    DWORD address, size, stride = 0, count = 1;
};
// Buildings: only the records up to the last one in use are copied (the drawing touches no record without a type;
// a city has a few hundred of the 4000). Found on each counting drawing, from the end of the array down.
constexpr DWORD kBuildings = 0x92e658, kBuildingSize = 0x118, kBuildingCount = 4000;
constexpr DWORD kBuildingType = 0x10; // short; 0 = no building (0x92e668)
DWORD g_buildingsUsed = kBuildingCount; // records 0 .. g_buildingsUsed - 1 are copied

void CountBuildingsInUse()
{
    DWORD n = kBuildingCount;
    while (n > 1 && *reinterpret_cast<const short *>(kBuildings + (n - 1) * kBuildingSize + kBuildingType) == 0) {
        --n;
    }
    g_buildingsUsed = n;
}
// Not the whole figure array: the drawing keeps figure data of its own there (put back whole in an earlier test
// build, figures showed in several places at once along their way). Only the one field 0x45b060 counts up.
const Region kDrawnState[] = {
    {0xcb4b08, 0x32c4 * 4},     // per-tile animation bytes (228 x 228)
    {kBuildings, 0x445c0 * 4},  // buildings (4000 x 0x118): only up to g_buildingsUsed (BuildingsSize below)
    {0xdbd7be, 2, 0x184, 2000}, // figures' field +0x36 (0x45b060: + 1 on every other drawing)
    {0x847d14, 8},              // drawing counters 0x847d14, 0x847d18
    {0x847d24, 2},              // and 0x847d24 (a word)
};
core::diag::Feature g_drawnFeature{"animations of drawn frames (games/zeus/timing)"};
unsigned char *g_drawnCopy = nullptr; // one buffer for all regions
DWORD g_drawnSize = 0;
bool g_countingFrame = true; // decided once per game_frame
DWORD g_lastCounting = 0;
zeus::anim::Clock g_renderClock;
unsigned char g_renderFlags[zeus::anim::kTimers]; // this frame's flags for its drawings (all 0 when not counting)
DWORD g_lastFlagsTime = 0;
int g_renderDepth = 0;
long g_counted = 0, g_reverted = 0;

using CityRender_fn = void(__cdecl *)(int, int, int);
using CityRender2_fn = void(__cdecl *)(int, int, int, int);
CityRender_fn o_CityRender;
CityRender2_fn o_CityRender2;

// game_frame: is this frame's city drawing a counting one?
void DecideCountingFrame()
{
    const DWORD now = timeGetTime();
    const DWORD since = now - g_lastCounting;
    g_countingFrame = g_lastCounting == 0 || since >= kRenderAnimMs;
    if (g_countingFrame) {
        // Keep an even 50 ms rhythm; after a pause (panel, loading) start again from now.
        g_lastCounting = (g_lastCounting == 0 || since >= 4 * kRenderAnimMs) ? now : g_lastCounting + kRenderAnimMs;
        const DWORD step = g_lastFlagsTime == 0 ? kRenderAnimMs : now - g_lastFlagsTime;
        g_lastFlagsTime = now;
        zeus::anim::Advance(g_renderClock, step > 1000 ? 1000 : step, g_renderFlags);
        if (g_drawnCopy) {
            CountBuildingsInUse();
        }
    }
    else {
        std::memset(g_renderFlags, 0, sizeof(g_renderFlags));
    }
}

struct RenderSwap {
    unsigned char frame[zeus::anim::kTimers];
    bool flags = false;  // anim_due replaced (put back after)
    bool copied = false; // the regions copied (put back after a drawing that does not count)
    DWORD buildings = 0; // building records copied
};
long long g_copiedBytes = 0; // put back, for the rate log

void RenderBefore(void *p)
{
    RenderSwap &s = *static_cast<RenderSwap *>(p);
    unsigned char *due = reinterpret_cast<unsigned char *>(kAnimDue);
    std::memcpy(s.frame, due, sizeof(s.frame));
    if (!g_countingFrame) {
        unsigned char *dst = g_drawnCopy;
        s.buildings = g_buildingsUsed;
        for (const Region &r : kDrawnState) {
            const DWORD size = r.address == kBuildings ? s.buildings * kBuildingSize : r.size;
            for (DWORD i = 0; i < r.count; ++i) {
                std::memcpy(dst, reinterpret_cast<const void *>(r.address + i * r.stride), size);
                dst += size;
            }
        }
        s.copied = true;
    }
    std::memcpy(due, g_renderFlags, sizeof(g_renderFlags));
    s.flags = true;
}

void RenderAfter(void *p)
{
    RenderSwap &s = *static_cast<RenderSwap *>(p);
    if (s.copied) {
        const unsigned char *src = g_drawnCopy;
        for (const Region &r : kDrawnState) {
            const DWORD size = r.address == kBuildings ? s.buildings * kBuildingSize : r.size;
            for (DWORD i = 0; i < r.count; ++i) {
                std::memcpy(reinterpret_cast<void *>(r.address + i * r.stride), src, size);
                src += size;
            }
        }
        g_copiedBytes += static_cast<long long>(src - g_drawnCopy);
    }
    if (s.flags) {
        std::memcpy(reinterpret_cast<unsigned char *>(kAnimDue), s.frame, sizeof(s.frame));
    }
}

template <typename F> void WithDrawnState(F draw)
{
    RenderSwap s;
    std::int64_t t = perf::Now();
    const bool active = g_renderDepth == 0 && g_drawnCopy && core::diag::Enabled(g_drawnFeature) &&
                        core::diag::Guarded(g_drawnFeature, &RenderBefore, &s);
    perf::Add(kPutBackPhase, perf::Now() - t);
    ++g_renderDepth;
    t = perf::Now();
    draw();
    perf::Add(kRenderPhase, perf::Now() - t);
    --g_renderDepth;
    if (active) {
        t = perf::Now();
        if (!core::diag::Guarded(g_drawnFeature, &RenderAfter, &s)) {
            core_log("zeus: drawn-frame animations off after an error: the game's own (every drawing counts)");
        }
        perf::Add(kPutBackPhase, perf::Now() - t);
        ++(s.copied ? g_reverted : g_counted);
    }
}

void __cdecl CityRender_detour(int x, int y, int z)
{
    zeus::diag::CityRender(x, y, z); // a log line when the city viewport changes (reads only)
    WithDrawnState([&] { o_CityRender(x, y, z); });
}

void __cdecl CityRender2_detour(int x, int y, int z, int w)
{
    WithDrawnState([&] { o_CityRender2(x, y, z, w); });
}

void TakeDrawnCounts(long *counted, long *reverted)
{
    *counted = g_counted;
    *reverted = g_reverted;
    g_counted = g_reverted = 0;
}

void TakeCopyStats(long long *bytes, DWORD *buildings)
{
    *bytes = g_copiedBytes;
    *buildings = g_buildingsUsed;
    g_copiedBytes = 0;
}

void AttachDrawnState()
{
    if (!Matches(kTileAnimClear) || !Matches(kBuildingsClear) || !Matches(kFiguresClear) || !Matches(kCityRender) ||
        !Matches(kCityRender2) || !Matches(kAnimDueUse)) {
        core_log("zeus: the city drawing or its arrays are not as expected: drawn-frame animations stay the game's own");
        core::diag::Disable(g_drawnFeature, "a site does not match");
        return;
    }
    for (const Region &r : kDrawnState) {
        g_drawnSize += r.size * r.count;
    }
    g_drawnCopy = static_cast<unsigned char *>(VirtualAlloc(nullptr, g_drawnSize, MEM_COMMIT | MEM_RESERVE,
                                                            PAGE_READWRITE));
    if (!g_drawnCopy) {
        core::diag::Disable(g_drawnFeature, "no memory for the copy");
        return;
    }
    bool ok = Install(kCityRender, &CityRender_detour, o_CityRender);
    ok = Install(kCityRender2, &CityRender2_detour, o_CityRender2) && ok;
    core_log("zeus: drawn-frame animations: one city drawing every %lu ms counts (%lu bytes put back after the "
             "others)%s", static_cast<unsigned long>(kRenderAnimMs), static_cast<unsigned long>(g_drawnSize),
             ok ? "" : "; a hook could not be installed");
}

const char *const kFullSpeed[] = {"capped", "original"};
const char *const kFullSpeedLabels[] = {"capped (steady, a step above 90%)", "original (as fast as the computer can)"};

} // namespace

namespace zeus::timing {

// game_main returned (games/zeus/diag.cpp): the last rate line, the screenshots still being written (e.g. F12 just
// before leaving), the system timer resolution back.
void GameEnded()
{
    core::capture::Flush(15000);
    LogRates(" (game ended)");
    core_timer_end();
}

void RegisterSettings()
{
    using core::settings::Effect;
    using core::settings::Item;
    using core::settings::Type;
    int i = core::settings::Register(Item{
        "gameplay", "keep_speed", "Keep the game speed",
        "on: ticks owed because a frame took long (large city, high resolution) are made up in the next frame, as "
        "Augustus does, so the game runs at its set speed at any resolution; off: the game's own pace (one tick per "
        "frame at most, slower when frames are slow).",
        Type::Bool, "on", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0});
    core::settings::OnChange(i, [](int, const char *v, bool) {
        g_catchUp = std::strcmp(v, "on") == 0;
        return true;
    });
    i = core::settings::Register(Item{
        "gameplay", "full_speed", "Game speed behavior",
        "Only matters at game speed 100%. capped (default): always the same pace, a step above 90% (90% runs 45 "
        "steps a second, 100% 62; Augustus does the same). original: the game's own 100% -- it runs as fast as your "
        "computer can draw, so it changes with the city and the resolution: in a small city many times faster "
        "than 90%, in a large city on a big screen only a little faster.",
        Type::Choice, "capped", Effect::Live, true, true, 0, 0, 0, nullptr, kFullSpeed, kFullSpeedLabels, 2});
    core::settings::OnChange(i, [](int, const char *v, bool) {
        g_fullSpeedCapped = std::strcmp(v, "capped") == 0;
        return true;
    });
    i = core::settings::Register(Item{
        "gameplay", "simulate_while_scrolling", "Simulate while scrolling",
        "on (default): the city goes on while the view scrolls (as in Augustus with smooth scrolling); off: it "
        "stands still while scrolling, as in the original.",
        Type::Bool, "on", Effect::Live, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0});
    core::settings::OnChange(i, [](int, const char *v, bool) {
        g_simWhileScrolling = std::strcmp(v, "on") == 0;
        return true;
    });
}

void Attach()
{
    const core_config *cfg = core_config_get();
    const bool threaded = cfg->backend == CORE_BACKEND_EMULATED && cfg->present != CORE_PRESENT_ORIGINAL;
    const Site *sites[] = {&kGameFrame, &kScreenPresent, &kSimDue, &kSimTick};
    for (const Site *s : sites) {
        if (!Matches(*s)) {
            core_log("zeus: %s at 0x%lx does not match: no frame or timing hooks (the game's own ~20 FPS and pace)",
                     s->name, static_cast<unsigned long>(s->address));
            return;
        }
    }
    if (!Matches(kAfterTick)) {
        core_log("zeus: %s at 0x%lx does not match: the game's own simulation pace", kAfterTick.name,
                 static_cast<unsigned long>(kAfterTick.address));
        g_catchUp = false;
        core::diag::Disable(g_paceFeature, "sim_after_tick does not match");
    }
    if (!Matches(kTickTableUse)) {
        core_log("zeus: the tick table is not where expected: the game's own simulation pace");
        g_catchUp = false;
        core::diag::Disable(g_paceFeature, "the tick table does not match");
    }
    if (!Matches(kAnimDueUse)) {
        core_log("zeus: the animation flags are not where expected: the game's own animation timing");
        core::diag::Disable(g_animFeature, "anim_timers_update does not match");
    }
    bool ok = Install(kGameFrame, &GameFrame_detour, o_GameFrame);
    ok = Install(kSimDue, &SimDue_detour, o_SimDue) && ok;
    ok = Install(kSimTick, &SimTick_detour, o_SimTick) && ok;
    if (threaded) {
        ok = Install(kScreenPresent, &ScreenPresent_detour, o_ScreenPresent) && ok;
        g_frameCapOn = true;
    }
    AttachDrawnState();
    core_log("zeus: frame and timing hooks %s (screen_present %s; keep_speed %s, full_speed %s, simulate while "
             "scrolling %s; figure animations on the game's time %s)",
             ok ? "installed" : "partly installed (see above)",
             threaded ? "hooked: frames follow the display" : "not hooked: the game's own ~20 FPS",
             g_catchUp ? "on" : "off", g_fullSpeedCapped ? "capped" : "original", g_simWhileScrolling ? "on" : "off",
             core::diag::Enabled(g_animFeature) ? "on" : "off");
}

} // namespace zeus::timing
