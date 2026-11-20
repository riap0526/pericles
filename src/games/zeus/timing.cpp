// Frames, the simulation's pace and the figures' animation timing in Zeus (timing.h; the addresses are
// in the Site and Var declarations below).
#include "games/zeus/timing.h"

#include "core/capture/screenshot.h"
#include "core/config.h"
#include "core/diag/autorun.h"
#include "core/diag/environment.h"
#include "core/diag/guard.h"
#include "core/diag/rate_log.h"
#include "core/diag/perf.h"
#include "core/diag/report.h"
#include "core/diag/stall.h"
#include "core/log.h"
#include "core/output/sdl_output.h"
#include "core/scroll/zoom.h"
#include "core/platform/timing.h"
#include "core/settings/settings.h"
#include "core/sim/pace.h"
#include "core/sim/panel_pause.h"
#include "games/zeus/game_menu.h"
#include "core/scroll/drag.h"
#include "core/sound/layer1.h"
#include "core/sound/sound.h"
#include "core/ui/panel.h"
#include "core/ui/settings_view.h"
#include "games/zeus/anim_clock.h"
#include "games/zeus/diag.h"
#include "games/zeus/fullscreen.h"
#include "games/zeus/resolution.h"
#include "games/zeus/scroll.h"
#include "games/zeus/site.h"
#include "games/zeus/smoke.h"
#include "games/zeus/sound.h"
#include "games/zeus/state_check.h"
#include "games/zeus/zoom.h"

#include <mmsystem.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

using zeus::At;
using zeus::Function;
using zeus::GameAddress;
using zeus::Install;
using zeus::Matches;
using zeus::Original;
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
// The simulation pace's view of Zeus (core/sim/pace.h; SimDue_detour below). The pace and its three settings
// ([gameplay] keep_speed, full_speed, simulate_while_scrolling) are the core's, shared with every game layer.
core::diag::Feature g_paceFeature{"simulation pace (games/zeus/timing)"};
const core::sim::PaceGame kPace{
    "zeus",
    "frame",
    &g_paceFeature,
    [] { return kSimLastTick.ptr(); },
    [] { return kEdgeScrolling.ptr(); },
    [] { return *kFastForward != 0; },
    [] { return *kGameSpeed; },
    [](int idx) { return At<const int>(kTickTable)[idx]; },
};

// ---------------------------------------------------------------------------------------------
// Counters and the rate log (game thread only)

long g_frames = 0, g_ticks = 0, g_animTicks = 0;
unsigned g_frameNumber = 0; // every game_frame run, never reset (FrameNumber)
DWORD g_since = 0;
int g_lastSpeed = -1;


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
double g_lastFrameMs = 0; // when the last game_frame that ran started (the time spent scrolling)

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
    char scroll[256];
    zeus::scroll::TakeStats(scroll, sizeof(scroll)); // as Yongle's rate line, before the pace's note
    char paceNote[160];
    core::sim::TakeNote(kPace, paceNote, sizeof(paceNote));
    static const char *const kPresent[] = {"original", "display", "cap60"};
    const double s = ms / 1000.0;
    long counted = 0, reverted = 0;
    TakeDrawnCounts(&counted, &reverted);
    long long copied = 0;
    DWORD buildings = 0;
    TakeCopyStats(&copied, &buildings);
    long multi = 0, torn = 0;
    core::diag::TakeIntegrity(&multi, &torn);
    int mapCols = 0, mapRows = 0;
    zeus::zoom::MapRange(&mapCols, &mapRows); // as Yongle's rate line
    core_log("rates%s: frames %.0f/s (at most %d/s), sim ticks %.1f/s (%ld with game-time animation), city drawings "
             "%.1f/s counting + %.1f/s put back (%.0f MB/s, buildings 0..%lu), game presents %.1f/s, frames shown "
             "%.1f/s over %lu ms (present=%s, display %d Hz, game speed %d%%, screen %d, mode %dx%d, city zoom %d%% on "
             "map %dx%d %s, timer %u us); frame check: %ld game frames with 2+ presents, %ld torn%s%s",
             why, g_frames / s, FrameCap(), g_ticks / s, g_animTicks, counted / s, reverted / s,
             copied / s / (1024.0 * 1024.0), static_cast<unsigned long>(buildings), presented / s, shown / s, ms,
             kPresent[core_config_get()->present], core::output::RefreshHz(), *kGameSpeed,
             static_cast<int>(*kGameScreen), *kModeWidth, *kModeHeight, core::scroll::zoom::Percent(), mapCols, mapRows,
             core::scroll::zoom::AverageShrink() ? "average" : "nearest", core_timer_resolution_us(), multi, torn,
             scroll, paceNote);
    core::diag::LogRateTail(why); // time, time max, intervals, cpu (the same lines in every game)
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

// While the panel is open the game is paused (core/sim/panel_pause, shared with Emperor's layer); while it asks whether
// to keep a new resolution (core::ui::WantsGameFrames), the game draws frames (at most ~60 a second) so that the new
// size can be seen; sim_tick_due says no meanwhile (SimDue_detour), so the city does not move on.

// Our pause menu over the game (games/zeus/game_menu; later also the options window).
bool OwnWindowOpen()
{
    return zeus::game_menu::IsOpen();
}

void Paused(bool menu)
{
    // The cursor shown again before the panel or the menu takes the mouse.
    core::scroll::drag::End(menu ? "the pause menu opened" : "our panel opened");
    core::sound::SetQuiet(true); // no new ambient sound meanwhile (frames are drawn while the panel asks "Keep?")
    LogRates(menu ? " (pause menu opened: game paused)" : " (panel opened: game paused)");
}

void Resumed()
{
    core::sound::SetQuiet(false);
    core_log("zeus: panel or pause menu closed: game resumed");
    ResetWindow();
    g_nextFrameMs = 0;
    g_lastFrameMs = 0;      // the pause is not a frame's time
    zeus::scroll::Resume(); // the scroll speed from rest
}

int DrawOnlyFrame()
{
    ++g_frameNumber;
    zeus::resolution::OnLoop(); // the new resolution is applied here
    zeus::fullscreen::OnLoop(); // ... and the framing of a front-end screen follows it
    DecideCountingFrame();
    core::diag::GameFrameBegin();
    Original(o_GameFrame, kGameFrame)();
    core::diag::GameFrameEnd();
    return 0;
}

const core::sim::panel_pause::PauseGame kPause("zeus", &OwnWindowOpen, &Paused, &Resumed, &DrawOnlyFrame);

void __cdecl GameFrame_detour()
{
    int unused = 0;
    if (core::sim::panel_pause::Loop(kPause, &unused)) {
        zeus::fullscreen::WhilePaused(); // the panel's blur or brightness shows on a menu without game_frame
        core::sound::layer1::Refresh();  // our nature sounds follow a switch or volume changed meanwhile
        return;
    }
    if (zeus::game_menu::OnLoop() && zeus::game_menu::IsOpen()) {
        return; // a confirmation answered No opened the pause menu again: no pass (the game stays paused)
    }
    if (!FrameDue()) {
        return;
    }
    // Sounds paused while away go on (the safety net), the ambient sounds' N follows the passes per second (only
    // the passes that run game_frame), the 10 s counts; the game's 3D sounds kept off, our layer-1 sounds.
    zeus::sound::OnLoop();
    ++g_frames;
    ++g_frameNumber;
    const double frameStart = NowMs();
    if (g_lastFrameMs != 0) {
        zeus::scroll::OnFrame(frameStart - g_lastFrameMs); // the time spent scrolling (the rate log)
    }
    g_lastFrameMs = frameStart;
    zeus::resolution::OnLoop(); // the 1024x768 frame of the front-end screens, the configured resolution
    zeus::fullscreen::OnLoop(); // the front-end screens framed (the menus pass here too)
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
    Original(o_GameFrame, kGameFrame)();
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
        zeus::scroll::SaveCityBuffer(report);
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
    core::diag::autorun::Frame(); // the unattended smoke test's steps (only when it was started for one)
}

// ---------------------------------------------------------------------------------------------
// The unattended smoke test (core/diag/autorun.h): the game's screen, its tick count and its own way to end.
// quit_request 0xf1389d: 1 makes the main loop send WM_CLOSE, which the window procedure takes (quit_request = 3,
// then DefWindowProc and WM_DESTROY's PostQuitMessage): the game's own exit, Zeus.inf written as always, no question
// asked (Ghidra).

const Var<char> kQuitRequest{0xf1389d};

int AutorunScreen()
{
    return *kGameScreen;
}

long AutorunTicks()
{
    return *kTickCount;
}

void AutorunQuit()
{
    *kQuitRequest = 1;
}

// ---------------------------------------------------------------------------------------------
// screen_present

void __cdecl ScreenPresent_detour(int param)
{
    zeus::game_menu::OnPresent(GameAddress(__builtin_return_address(0))); // over the frame just drawn
    if (param == 1 || *kRedrawRequest != 0) {
        core::output::HoldNextFrame(kPresentGateMs);
    }
    else if (core::output::FrameWanted()) {
        param = 1; // present the frame just drawn: the output thread is ready for it
    }
    Original(o_ScreenPresent, kScreenPresent)(param);
}

// ---------------------------------------------------------------------------------------------
// Simulation pace (core/sim/pace.h): only game_frame's own calls (return addresses 0x5135a9, 0x5135c8) go through it.

int OriginalDue()
{
    return Original(o_SimDue, kSimDue)();
}

int __cdecl SimDue_detour()
{
    if (core::sim::panel_pause::DrawOnly()) {
        return 0; // a frame drawn while the panel asks: no tick
    }
    if (GameAddress(__builtin_return_address(0)) != kDueFromFrame) {
        return OriginalDue();
    }
    return core::sim::Due(kPace, &OriginalDue);
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
    const DWORD interval = core::sim::TickInterval(kPace);
    if (interval != 0) {
        return interval;
    }
    return wall > 1000 ? 1000 : wall;
}

void AnimBefore(void *p)
{
    AnimSwap &s = *static_cast<AnimSwap *>(p);
    unsigned char *due = At<unsigned char>(kAnimDue);
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
        Original(o_SimTick, kSimTick)();
    }
    if (s.swapped) {
        std::memcpy(At<unsigned char>(kAnimDue), s.frame, sizeof(s.frame));
    }
}

// The ticks made up, right after the game's own one, each as game_frame runs one: sim_after_tick (which game_frame
// runs after every tick; for the last one it does so itself), the counter, then the tick. They stop when the last
// tick opened a dialog, paused the game or changed the screen (also tick_block 0x665dc8, which Emperor has not).
unsigned char g_tickScreen = 0;

bool ExtraTickAllowed()
{
    return !*kFastForward && !*kSimPaused && !*kTickBlock && *kGameScreen == g_tickScreen;
}

void ExtraTick()
{
    Function<void(__cdecl *)()>(kAfterTick)();
    *kTickCount = *kTickCount + 1;
    ++g_ticks;
    RunTick();
}

void __cdecl SimTick_detour()
{
    ++g_ticks;
    RunTick();
    if (GameAddress(__builtin_return_address(0)) == kTickFromFrame) {
        g_tickScreen = *kGameScreen;
        core::sim::RunOwedTicks(kPace, &ExtraTickAllowed, &ExtraTick);
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
    while (n > 1 && *At<const short>(kBuildings + (n - 1) * kBuildingSize + kBuildingType) == 0) {
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
// The game's random number generator: the city drawing turns it for building
// type 80 (0x44cd10), so a drawing that does not count must put it back too, or the simulation's random numbers move
// on with every such drawing. 0xf13624 .. 0xf137e7: the pool's index, the pool (100 x 4 B), the six values taken from
// the last turn and the two shift registers (0xf137b8 .. 0xf137c3 is used by no code; 0xf137dc only by
// window_create). Put back only when random_generate_next is the known one.
const Site kRandomNext{"random_generate_next", 0x4488c0, {0xa1, 0x24, 0x36, 0xf1, 0x00, 0x8b, 0x0d, 0xd4}};
constexpr DWORD kRandomState = 0xf13624, kRandomSize = 0xf137e8 - 0xf13624;
bool g_randomPutBack = false; // AttachDrawnState: kRandomNext matches
bool g_cityRenderHooked = false; // AttachDrawnState: city_render hooked (the menus' city background needs it)
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
    bool force = false;  // a drawing that never counts (the extra drawing after an abandoned one)
    unsigned char frame[zeus::anim::kTimers];
    bool flags = false;  // anim_due replaced (put back after)
    bool copied = false; // the regions copied (put back after a drawing that does not count)
    DWORD buildings = 0; // building records copied
    bool random = false; // the random number state copied (into randomCopy)
    unsigned char randomCopy[kRandomSize];
};
long long g_copiedBytes = 0; // put back, for the rate log

void RenderBefore(void *p)
{
    RenderSwap &s = *static_cast<RenderSwap *>(p);
    unsigned char *due = At<unsigned char>(kAnimDue);
    std::memcpy(s.frame, due, sizeof(s.frame));
    if (!g_countingFrame || s.force) {
        unsigned char *dst = g_drawnCopy;
        s.buildings = g_buildingsUsed;
        for (const Region &r : kDrawnState) {
            const DWORD size = r.address == kBuildings ? s.buildings * kBuildingSize : r.size;
            for (DWORD i = 0; i < r.count; ++i) {
                std::memcpy(dst, At<const void>(r.address + i * r.stride), size);
                dst += size;
            }
        }
        s.copied = true;
        if (g_randomPutBack) {
            std::memcpy(s.randomCopy, At<const void>(kRandomState), kRandomSize);
            s.random = true;
        }
    }
    if (s.force) {
        std::memset(due, 0, sizeof(g_renderFlags)); // nothing advances (as during any drawing that does not count)
    }
    else {
        std::memcpy(due, g_renderFlags, sizeof(g_renderFlags));
    }
    s.flags = true;
}

void RenderAfter(void *p)
{
    RenderSwap &s = *static_cast<RenderSwap *>(p);
    // anim_due first: if putting a region back fails, the frame's own flags are back all the same.
    if (s.flags) {
        std::memcpy(At<unsigned char>(kAnimDue), s.frame, sizeof(s.frame));
    }
    if (s.random) {
        std::memcpy(At<void>(kRandomState), s.randomCopy, kRandomSize);
        g_copiedBytes += kRandomSize;
    }
    if (s.copied) {
        const unsigned char *src = g_drawnCopy;
        for (const Region &r : kDrawnState) {
            const DWORD size = r.address == kBuildings ? s.buildings * kBuildingSize : r.size;
            for (DWORD i = 0; i < r.count; ++i) {
                std::memcpy(At<void>(r.address + i * r.stride), src, size);
                src += size;
            }
        }
        g_copiedBytes += static_cast<long long>(src - g_drawnCopy);
    }
}

// True when this was an outermost drawing that counts (shown with the game's own state kept; the city kept for
// the menus' background is taken only from those).
// force: a drawing that never counts -- the extra city_render after smooth
// scrolling's drawing was abandoned, inside the outer drawing of the frame. Inside a counting drawing it is put back on
// its own (the outer one copied nothing, so the copy buffer is free); inside a drawing that does not count the outer
// one puts everything back at its end (the copy buffer holds the outer one's copy and must not be touched).
template <typename F> bool WithDrawnState(F draw, bool force = false)
{
    RenderSwap s;
    s.force = force;
    const bool outer = g_renderDepth == 0;
    const bool own = outer || (force && g_countingFrame); // this drawing does its own put-back (if active)
    // [debug] state_check (state_check.h): the game's memory before anything of ours or the drawing's, once a second
    // around a drawing that does not count. Not timed: only while checking.
    const bool want = g_renderDepth == 0 && g_drawnCopy && core::diag::Enabled(g_drawnFeature) && !g_countingFrame &&
                      zeus::state_check::Wanted();
    if (want) {
        zeus::state_check::Before();
    }
    std::int64_t t = perf::Now();
    const bool active = own && g_drawnCopy && core::diag::Enabled(g_drawnFeature) &&
                        core::diag::Guarded(g_drawnFeature, &RenderBefore, &s);
    perf::Add(kPutBackPhase, perf::Now() - t);
    const bool check = want && active && s.copied;
    if (want && !check) {
        zeus::state_check::Cancel();
    }
    ++g_renderDepth;
    t = perf::Now();
    zeus::diag::CityDrawing(true);
    draw();
    zeus::diag::CityDrawing(false);
    perf::Add(kRenderPhase, perf::Now() - t);
    --g_renderDepth;
    if (check) {
        zeus::state_check::AfterDraw(); // what the drawing changed (and our anim_due swap)
    }
    if (active) {
        t = perf::Now();
        const bool putBack = core::diag::Guarded(g_drawnFeature, &RenderAfter, &s);
        if (!putBack) {
            core_log("zeus: drawn-frame animations off after an error: the game's own (every drawing counts)");
        }
        perf::Add(kPutBackPhase, perf::Now() - t);
        ++(s.copied ? g_reverted : g_counted);
        if (check) {
            putBack ? zeus::state_check::AfterPutBack() : zeus::state_check::Cancel(); // what is left after ours
        }
    }
    return outer && (!active || !s.copied);
}

// The drawing goes through smooth scrolling's picture (games/zeus/scroll, core/scroll/picture), inside the
// drawn-frame state: the picture's own guard catches a fault of its drawing and puts its changes back, and the
// put-back here still runs after it. The picture calls DrawCityOriginal for the
// drawing and DrawCityNotCounting for the extra one after an abandoned drawing.
void __cdecl CityRender_detour(int x, int y, int z)
{
    zeus::diag::CityRender(x, y, z); // a log line when the city viewport changes (reads only)
    if (WithDrawnState([&] { zeus::scroll::DrawCity(x, y, z); })) {
        zeus::fullscreen::CityDrawn(); // the city on screen, kept every few seconds for the menus' background
    }
}

// The figure portrait of the information window (figure_portrait_draw 0x572f50, a 5 x 15 viewport with the view moved
// to the figure): drawn as the game draws it, without smooth scrolling's remainder.
void __cdecl CityRender2_detour(int x, int y, int z, int w)
{
    WithDrawnState([&] {
        core::diag::Restore r;
        zeus::scroll::PortraitDrawing(r);
        Original(o_CityRender2, kCityRender2)(x, y, z, w);
        r.Undo();
    });
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

// The one buffer the regions are copied into (AttachDrawnState; the offline test calls it without the site checks).
bool AllocateDrawnCopy()
{
    g_drawnSize = 0;
    for (const Region &r : kDrawnState) {
        g_drawnSize += r.size * r.count;
    }
    g_drawnCopy = static_cast<unsigned char *>(VirtualAlloc(nullptr, g_drawnSize, MEM_COMMIT | MEM_RESERVE,
                                                            PAGE_READWRITE));
    return g_drawnCopy != nullptr;
}

void AttachDrawnState()
{
    if (!Matches(kTileAnimClear) || !Matches(kBuildingsClear) || !Matches(kFiguresClear) || !Matches(kCityRender) ||
        !Matches(kCityRender2) || !Matches(kAnimDueUse)) {
        core_log("zeus: the city drawing or its arrays are not as expected: drawn-frame animations stay the game's own");
        core::diag::Disable(g_drawnFeature, "a site does not match");
        return;
    }
    if (!AllocateDrawnCopy()) {
        core::diag::Disable(g_drawnFeature, "no memory for the copy");
        return;
    }
    g_randomPutBack = Matches(kRandomNext);
    core_log("zeus: drawn-frame animations: the random number state (0x%lx, %lu bytes) %s",
             static_cast<unsigned long>(kRandomState), static_cast<unsigned long>(kRandomSize),
             g_randomPutBack ? "is put back too" : "is not put back (random_generate_next does not match)");
    bool ok = Install(kCityRender, &CityRender_detour, o_CityRender);
    g_cityRenderHooked = ok;
    ok = Install(kCityRender2, &CityRender2_detour, o_CityRender2) && ok;
    core_log("zeus: drawn-frame animations: one city drawing every %lu ms counts (%lu bytes put back after the "
             "others)%s", static_cast<unsigned long>(kRenderAnimMs), static_cast<unsigned long>(g_drawnSize),
             ok ? "" : "; a hook could not be installed");
}


} // namespace

namespace zeus::timing {

// game_main returned (games/zeus/diag.cpp): the last rate line, the screenshots still being written (e.g. F12 just
// before leaving), the system timer resolution back.
void GameEnded()
{
    core::capture::Flush(15000);
    LogRates(" (game ended)");
    zeus::state_check::Report(" (game ended)");
    core_timer_end();
}

void RegisterSettings()
{
    core::sim::RegisterSettings(); // [gameplay] keep_speed, full_speed, simulate_while_scrolling
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
    // As Emperor's layer: game_main too, where the end of the game flushes the screenshots and ends the timer
    // resolution (games/zeus/diag hooks it).
    if (!zeus::diag::GameMainMatches()) {
        core_log("zeus: game_main does not match: no frame or timing hooks (the game's own ~20 FPS and pace)");
        return;
    }
    if (!Matches(kAfterTick)) {
        core_log("zeus: %s at 0x%lx does not match: the game's own simulation pace", kAfterTick.name,
                 static_cast<unsigned long>(kAfterTick.address));
        core::sim::TurnOff(kPace, "sim_after_tick does not match");
    }
    if (!Matches(kTickTableUse)) {
        core_log("zeus: the tick table is not where expected: the game's own simulation pace");
        core::sim::TurnOff(kPace, "the tick table does not match");
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
    // The smoke test's steps run in game_frame's detour: only with it (main menu 0x00, city 0x01).
    if (ok) {
        core::diag::autorun::Start({&AutorunScreen, 0x00, 0x01, &AutorunTicks, &AutorunQuit, &smoke::Describe,
                                    smoke::kActions, &smoke::Action, &smoke::DialogOffset});
    }
    core_log("zeus: frame and timing hooks %s (screen_present %s; keep_speed %s, full_speed %s, simulate while "
             "scrolling %s; figure animations on the game's time %s)",
             ok ? "installed" : "partly installed (see above)",
             threaded ? "hooked: frames follow the display" : "not hooked: the game's own ~20 FPS",
             core::sim::KeepSpeed() ? "on" : "off", core::sim::FullSpeedCapped() ? "capped" : "original",
             core::sim::SimulateWhileScrolling() ? "on" : "off",
             core::diag::Enabled(g_animFeature) ? "on" : "off");
}

const Detour *Detours(int *count)
{
    static const Detour kDetours[] = {
        {&kGameFrame, reinterpret_cast<void *>(&GameFrame_detour)},
        {&kScreenPresent, reinterpret_cast<void *>(&ScreenPresent_detour)},
        {&kSimDue, reinterpret_cast<void *>(&SimDue_detour)},
        {&kSimTick, reinterpret_cast<void *>(&SimTick_detour)},
        {&kCityRender, reinterpret_cast<void *>(&CityRender_detour)},
        {&kCityRender2, reinterpret_cast<void *>(&CityRender2_detour)},
    };
    *count = static_cast<int>(sizeof(kDetours) / sizeof(kDetours[0]));
    return kDetours;
}

unsigned FrameNumber()
{
    return g_frameNumber;
}

bool PresentHooked()
{
    return o_ScreenPresent != nullptr;
}

bool CityDrawingHooked()
{
    return g_cityRenderHooked;
}

void DrawCityOriginal(int x, int y, int z)
{
    Original(o_CityRender, kCityRender)(x, y, z);
}

void DrawCityNotCounting(int x, int y, int z)
{
    WithDrawnState([&] { Original(o_CityRender, kCityRender)(x, y, z); }, true);
}

// city_picture: a picture is drawn only in a counting frame. Its
// outer drawing copied nothing then, so each strip (a drawing that never counts) copies and puts back on its own and
// every strip starts from the same state; in a frame that does not count the outer drawing's copy is in the buffer and
// the strips would add up (the per-building count and tile_anim of 0x44c2f0, 0x847d14).
bool PictureDrawingNow()
{
    return g_drawnCopy && core::diag::Enabled(g_drawnFeature) && g_countingFrame;
}

bool PutBackDrawing(void (*fn)(void *), void *ctx)
{
    if (!PictureDrawingNow()) {
        return false;
    }
    WithDrawnState([&] { fn(ctx); }, true);
    return core::diag::Enabled(g_drawnFeature); // a fault in its copy or put-back switched it off
}

bool PrepareForTest()
{
    g_randomPutBack = true; // no site checks in the offline test
    return AllocateDrawnCopy();
}

bool PaceWorking()
{
    return core::diag::Enabled(g_paceFeature);
}

bool AnimWorking()
{
    return core::diag::Enabled(g_animFeature);
}

bool DrawnWorking()
{
    return core::diag::Enabled(g_drawnFeature);
}

} // namespace zeus::timing
