// Offline test of the failure fallbacks of Zeus's game layer (not the game): the real detours of games/zeus run on a
// copy of the game's address range (zeus::g_imageShift, games/zeus/site.h) where the game's functions are small
// fakes. A fake faults on purpose (or a page of the copy is made unreadable under our code); the test then checks that
// the game state the detour had changed is back to the game's own values, that the feature is switched off, and what
// the detour does afterwards. The same method as src/games/emperor/test_restore.cpp; every feature of the game layer
// that changes the game's state gets a scenario here.
//
// Scenarios so far (the frame timing, games/zeus/timing):
//   pace_ok              sim_tick_due with the pace: the game's tick time and edge-scrolling flag only for the call,
//                           the ticks owed made up after the game's own one
//   pace_fault           a fault inside sim_tick_due: both values put back, pace off, the game's own answer
//   extra_tick_fault     a fault in a tick made up (sim_after_tick): pace off, the frame's animation flags back
//   drawn_put_back       a drawing that does not count: what it changed is put back, anim_due all 0 meanwhile
//   drawn_copy_fault     copying the drawn state fails: drawn-frame animations off, the game draws its own way
//   drawn_put_back_fault putting the drawn state back fails: off, anim_due is back all the same
//   drawn_put_back also: the random number state (0xf13624 .. 0xf137e7) put back after a drawing that does not
//                           count, kept after one that counts
//   state_check          [debug] state_check: what the drawing changed in the compared memory, per region, and
//                           whether our put-back undid it (a save block not put back, the random number state put back)
//                           is logged (block, record, field); the blocks put back are not
//   state_check_fault    copying the memory faults: the check off, the drawing still put back
//
// The logical resolution (games/zeus/resolution, games/zeus/world_map):
//   mode_size           Zeus.inf's mode 2 becomes 3; mode 3 is 1024x768 on front-end screens, ours in the city;
//                           a full-screen picture outside the city starts the frame, one on the city screen does not
//   mode_size_fault         a fault after mode 3's size was written: 1024x768 back, switching off, settings refused
//   viewport_layout              the sidebar (where the open-sidebar columns end) and both city viewports at 1920x1080 and
//                           1024x900; the game's own at 1024x768
//   layout_fault            viewport_init faults: the 1024 layout at once, the game's own size on the next loop
//   map_flags               the flags step once per 50 ms (anim_due[0] 0 for the call otherwise, put back after it);
//                           a fault: put back, off, the game's own step
//   map_height          the world map drawn with the game's own 768 on a higher screen, the height back after it
//   draw_only           the panel asks: frames drawn (sim_tick_due says no) and the resolution switched in them; a
//                           fault: off, the flag put back
//
// Each scenario runs in a child process of its own (a feature switched off stays off for the rest of a process):
//   test_restore.exe            runs every scenario and prints one line each
//   test_restore.exe <name>     one scenario (exit code 0 = passed)
#include "core/diag/autorun.h"
#include "core/diag/guard.h"
#include "core/log.h"
#include "core/config.h"
#include "games/zeus/anim_clock.h"
#include "games/zeus/resolution.h"
#include "games/zeus/site.h"
#include "games/zeus/state_check.h"
#include "games/zeus/timing.h"
#include "games/zeus/world_map.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

#include <cstddef>
#include <cstdio>
#include <cstring>

// ---------------------------------------------------------------------------------------------
// Stubs for the parts of the DLL these files call that are not under test

namespace core::output {
bool FrameWanted() { return false; }
void HoldNextFrame(DWORD) {}
void TakeCounts(long *a, long *b) { *a = *b = 0; }
int RefreshHz() { return 60; }
void MeasureFromNow() {}
void TakeIntervals(char *out, std::size_t) { out[0] = 0; }
} // namespace core::output

bool g_panelOpen = false, g_panelWantsFrames = false; // draw_only

namespace core::ui {
bool IsOpen() { return g_panelOpen; }
bool WantsGameFrames() { return g_panelWantsFrames; }
} // namespace core::ui

int g_nativeCalls = 0, g_nativeFaultAt = 0; // NativeSize faults on that call (mode_size_fault)

namespace core::display { // games/zeus/resolution: a 1920x1080 monitor, full screen
SIZE NativeSize()
{
    if (++g_nativeCalls == g_nativeFaultAt) {
        *static_cast<volatile int *>(nullptr) = 1;
    }
    return SIZE{1920, 1080};
}
bool Windowed() { return false; }
void SetWindowClient(int, int) {}
} // namespace core::display

namespace core::diag::autorun { // the smoke test is not started here (its own test: test_autorun)
bool Start(const Game &) { return false; }
void Frame() {}
} // namespace core::diag::autorun

namespace core::capture {
int Flush(unsigned long) { return 0; }
} // namespace core::capture

extern "C" unsigned core_timer_resolution_us(void) { return 1000; }
extern "C" void core_timer_end(void) {}

namespace zeus::diag { // games/zeus/diag: logs only (not under test)
void FrameBegin() {}
void FrameEnd() {}
void CensusForReport() {}
void CityRender(int, int, int) {}
void CityDrawing(bool) {}
void Describe(char *out, std::size_t size)
{
    std::snprintf(out, size, "test");
}
} // namespace zeus::diag

namespace {

using zeus::g_imageShift;

int g_failures = 0;

void Check(bool ok, const char *what)
{
    std::printf("  %-74s %s\n", what, ok ? "ok" : "FAILED");
    if (!ok) {
        ++g_failures;
    }
}

bool LogContains(const char *text)
{
    FILE *f = std::fopen("test_restore.log", "rb");
    if (!f) {
        return false;
    }
    static char buf[1 << 20];
    size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
    std::fclose(f);
    buf[n] = 0;
    return std::strstr(buf, text) != nullptr;
}

// The game's memory: our copy of 0x400000 .. 0x1100000 (code, the tables and every variable used below).
constexpr DWORD kImageBase = 0x400000, kImageSize = 0xd00000;

template <typename T> volatile T &At(DWORD address)
{
    return *reinterpret_cast<volatile T *>(address + g_imageShift);
}

void Thunk(DWORD address, const void *target) // jmp target at the game's address
{
    BYTE *p = reinterpret_cast<BYTE *>(address + g_imageShift);
    p[0] = 0xe9;
    *reinterpret_cast<DWORD *>(p + 1) = reinterpret_cast<DWORD>(target) - reinterpret_cast<DWORD>(p + 5);
}

// call target; ret -- at the game's call site, so that the detour sees the game's own return address.
void CallSite(DWORD address, const void *target)
{
    BYTE *p = reinterpret_cast<BYTE *>(address + g_imageShift);
    p[0] = 0xe8;
    *reinterpret_cast<DWORD *>(p + 1) = reinterpret_cast<DWORD>(target) - reinterpret_cast<DWORD>(p + 5);
    p[5] = 0xc3;
}

template <typename F> F Timing(const char *name)
{
    int n;
    const zeus::Detour *d = zeus::timing::Detours(&n);
    for (int i = 0; i < n; ++i) {
        if (std::strcmp(d[i].site->name, name) == 0) {
            return reinterpret_cast<F>(d[i].detour);
        }
    }
    std::printf("no detour %s\n", name);
    ExitProcess(3);
}

// A fake faults once when its name is the armed one.
const char *volatile g_fault = "";
bool Fires(const char *name)
{
    if (std::strcmp(g_fault, name) == 0) {
        g_fault = "";
        return true;
    }
    return false;
}
void Fault()
{
    *static_cast<volatile int *>(nullptr) = 1;
}

// ---------------------------------------------------------------------------------------------
// The game's places used below (the same as games/zeus/timing.cpp)

constexpr DWORD fGameFrame = 0x513580, fSimDue = 0x5168a0, fSimTick = 0x513a30, fAfterTick = 0x5123d0,
                fCityRender = 0x44a830;
constexpr DWORD cDueFromFrame = 0x5135a4;  // game_frame's call of sim_tick_due (returns to 0x5135a9)
constexpr DWORD cTickFromFrame = 0x5135c3; // ... and of sim_tick (returns to 0x5135c8)
constexpr DWORD aSimLastTick = 0xf9f998, aTickCount = 0xf9f9a4, aEdgeScrolling = 0xf2a6d0, aGameSpeed = 0xf2a774,
                aTickTable = 0x63da1c, aAnimDue = 0x6668ec;
constexpr DWORD aTiles = 0xcb4b08, aBuildings = 0x92e658, aBuildingSize = 0x118, aFigures = 0xdbd7be,
                aFigureSize = 0x184, aCounters = 0x847d14;
constexpr DWORD aSaveTable = 0x63c2d0, aTileFigures = 0xd0d878; // save_game_table; each tile's first figure (2 x 51984)
constexpr DWORD aRandomIndex = 0xf13624, aRandomPool = 0xf13628, aRandomIv1 = 0xf137e4; // random_generate_next's state

constexpr int kTickMs = 20 + 2; // the tick table's entry for 90% is 20 (sim_tick_due adds 2)
constexpr unsigned char kFrameFlag = 0x5a; // this frame's anim_due, as anim_timers_update left it (never a flag value)

// ---------------------------------------------------------------------------------------------
// Fakes of the game's functions

struct Calls {
    int frames = 0, dues = 0, ticks = 0, afterTicks = 0, draws = 0;
    DWORD seenLastTick = 0; // what sim_tick_due saw (its last call)
    int seenEdge = -1;
    bool tickFlagsOurs = true; // every sim_tick ran with game-time flags (not the frame's)
    bool drawFlagsZero = true; // the last drawing saw anim_due all 0
    bool drawFlagsFrame = false; // ... or the frame's own
    bool drawCounters = true;    // the drawing changes the drawing counters (off where their page is unreadable)
    bool protectAfterDraw = false;
    bool touchList = false; // the drawing changes a block the put-back does not cover (state_check)
    bool touchRandom = false; // the drawing turns the random number generator (building type 80)
} g_calls;

int CallDue(); // below
int g_frameDue = -1; // draw_only: what sim_tick_due answered inside the last frame (-1: not asked)
bool g_frameAsksDue = false;

void __cdecl FakeGameFrame()
{
    ++g_calls.frames;
    if (g_frameAsksDue) {
        g_frameDue = CallDue();
    }
    if (Fires("game_frame")) {
        Fault();
    }
}

int __cdecl FakeSimDue()
{
    ++g_calls.dues;
    g_calls.seenLastTick = At<DWORD>(aSimLastTick);
    g_calls.seenEdge = At<int>(aEdgeScrolling);
    if (Fires("sim_tick_due")) {
        Fault();
    }
    return 1;
}

bool DueIsFrame(const volatile unsigned char *due)
{
    for (int i = 0; i < zeus::anim::kTimers; ++i) {
        if (due[i] != kFrameFlag) {
            return false;
        }
    }
    return true;
}

void __cdecl FakeSimTick()
{
    ++g_calls.ticks;
    if (DueIsFrame(&At<unsigned char>(aAnimDue))) {
        g_calls.tickFlagsOurs = false;
    }
}

void __cdecl FakeAfterTick()
{
    ++g_calls.afterTicks;
    if (Fires("sim_after_tick")) {
        Fault();
    }
}

void ProtectCounters(DWORD protect)
{
    DWORD old = 0;
    VirtualProtect(reinterpret_cast<void *>(aCounters + g_imageShift), 16, protect, &old);
}

// As the city drawing: one tile animation byte, a building's field, a figure's field and the drawing counters
// move on by one.
void __cdecl FakeCityRender(int, int, int)
{
    ++g_calls.draws;
    const volatile unsigned char *due = &At<unsigned char>(aAnimDue);
    g_calls.drawFlagsZero = true;
    for (int i = 0; i < zeus::anim::kTimers; ++i) {
        g_calls.drawFlagsZero = g_calls.drawFlagsZero && due[i] == 0;
    }
    g_calls.drawFlagsFrame = DueIsFrame(due);
    At<unsigned char>(aTiles + 7) = At<unsigned char>(aTiles + 7) + 1;
    At<short>(aBuildings + 5 * aBuildingSize + 0x20) = At<short>(aBuildings + 5 * aBuildingSize + 0x20) + 1;
    At<short>(aFigures + 3 * aFigureSize) = At<short>(aFigures + 3 * aFigureSize) + 1;
    if (g_calls.drawCounters) {
        At<int>(aCounters) = At<int>(aCounters) + 1;
    }
    if (g_calls.protectAfterDraw) {
        ProtectCounters(PAGE_NOACCESS);
    }
    if (g_calls.touchList) {
        At<short>(aTileFigures + 2 * 100) = At<short>(aTileFigures + 2 * 100) + 1;
    }
    if (g_calls.touchRandom) { // as random_generate_next: a pool entry, the index, the shift register
        At<DWORD>(aRandomPool + 4 * 7) = At<DWORD>(aRandomPool + 4 * 7) + 1;
        At<DWORD>(aRandomIndex) = At<DWORD>(aRandomIndex) + 1;
        At<DWORD>(aRandomIv1) = At<DWORD>(aRandomIv1) ^ 0x5a5a;
    }
}

struct Random {
    DWORD index, pool7, iv1;
};

Random ReadRandom()
{
    return Random{At<DWORD>(aRandomIndex), At<DWORD>(aRandomPool + 4 * 7), At<DWORD>(aRandomIv1)};
}

void Setup()
{
    BYTE *block = static_cast<BYTE *>(VirtualAlloc(nullptr, kImageSize, MEM_COMMIT | MEM_RESERVE,
                                                   PAGE_EXECUTE_READWRITE));
    if (!block) {
        std::printf("cannot allocate the game's address range\n");
        ExitProcess(3);
    }
    g_imageShift = reinterpret_cast<DWORD>(block) - kImageBase;
    Thunk(fGameFrame, reinterpret_cast<void *>(&FakeGameFrame));
    Thunk(fSimDue, reinterpret_cast<void *>(&FakeSimDue));
    Thunk(fSimTick, reinterpret_cast<void *>(&FakeSimTick));
    Thunk(fAfterTick, reinterpret_cast<void *>(&FakeAfterTick));
    Thunk(fCityRender, reinterpret_cast<void *>(&FakeCityRender));
    CallSite(cDueFromFrame, Timing<void *>("sim_tick_due"));
    CallSite(cTickFromFrame, Timing<void *>("sim_tick"));
    At<int>(aGameSpeed) = 90;
    At<int>(aTickTable + 4) = 20; // the entry for 90%: (100 - 90) / 10 = 1
    for (int i = 0; i < zeus::anim::kTimers; ++i) {
        At<unsigned char>(aAnimDue + i) = kFrameFlag;
    }
    At<short>(aBuildings + 5 * aBuildingSize + 0x10) = 1; // a building in record 5 (records 0..5 copied)
}

using Due_fn = int(__cdecl *)();
using Tick_fn = void(__cdecl *)();
using Frame_fn = void(__cdecl *)();
using Render_fn = void(__cdecl *)(int, int, int);

int CallDue() // as game_frame calls sim_tick_due
{
    return reinterpret_cast<Due_fn>(cDueFromFrame + g_imageShift)();
}

void CallTick()
{
    reinterpret_cast<Tick_fn>(cTickFromFrame + g_imageShift)();
}

// ---------------------------------------------------------------------------------------------
// The simulation pace (games/zeus/timing: sim_tick_due, sim_tick)

// A tick owed 100 ms ago at 22 ms a tick: 4 ticks are owed (the game's own and 3 made up).
DWORD OweTicks()
{
    const DWORD start = timeGetTime() - 100;
    At<DWORD>(aSimLastTick) = start;
    At<int>(aEdgeScrolling) = 1; // the view scrolls at the screen edge
    return start;
}

void PaceOk()
{
    const DWORD start = OweTicks();
    const int r = CallDue();
    Check(r == 1 && g_calls.dues == 1, "sim_tick_due: the game's answer (a tick runs)");
    Check(g_calls.seenEdge == 0, "the game saw no edge scrolling (simulate while scrolling)");
    Check(timeGetTime() - g_calls.seenLastTick > 50000, "the game saw a tick time far in the past (a tick is owed)");
    Check(At<int>(aEdgeScrolling) == 1, "edge-scrolling flag back afterwards");
    Check(At<DWORD>(aSimLastTick) == start + 4 * kTickMs, "tick time: 4 ticks of 22 ms on (the ticks owed)");
    const int count = At<int>(aTickCount);
    CallTick();
    Check(g_calls.ticks == 4 && g_calls.afterTicks == 3 && At<int>(aTickCount) == count + 3,
          "sim_tick: the game's own tick and 3 made up (each after sim_after_tick, counted)");
    Check(g_calls.tickFlagsOurs, "every tick ran with the game-time animation flags");
    Check(DueIsFrame(&At<unsigned char>(aAnimDue)), "the frame's own animation flags back afterwards");
    Check(zeus::timing::PaceWorking() && zeus::timing::AnimWorking(), "pace and animation timing still on");
}

void PaceFault()
{
    const DWORD start = OweTicks();
    g_fault = "sim_tick_due";
    const int r = CallDue();
    Check(!zeus::timing::PaceWorking(), "fault inside sim_tick_due: the simulation pace off");
    Check(r == 1 && g_calls.dues == 2, "sim_tick_due asked again the game's own way, its answer returned");
    Check(g_calls.seenEdge == 1 && g_calls.seenLastTick == start, "... with the game's own tick time and edge flag");
    Check(At<int>(aEdgeScrolling) == 1 && At<DWORD>(aSimLastTick) == start, "both values put back");
    Check(LogContains("feature \"simulation pace (games/zeus/timing)\" switched off") &&
              LogContains("2 changed value(s) put back") &&
              LogContains("simulation pace back to the game's own after an error"),
          "log: switched off, the 2 changed values put back");
    CallTick();
    Check(g_calls.ticks == 1 && g_calls.afterTicks == 0, "sim_tick: only the game's own tick (nothing made up)");
    At<int>(aEdgeScrolling) = 1;
    CallDue();
    Check(g_calls.dues == 3 && g_calls.seenEdge == 1, "afterwards sim_tick_due is the game's own (edge flag kept)");
}

void ExtraTickFault()
{
    OweTicks();
    CallDue();
    const int count = At<int>(aTickCount);
    g_fault = "sim_after_tick";
    CallTick();
    Check(!zeus::timing::PaceWorking(), "fault in a tick made up (sim_after_tick): the simulation pace off");
    Check(g_calls.ticks == 1 && g_calls.afterTicks == 1 && At<int>(aTickCount) == count,
          "the game's own tick ran, the tick made up not counted");
    Check(DueIsFrame(&At<unsigned char>(aAnimDue)), "the frame's own animation flags back");
    Check(zeus::timing::AnimWorking(), "animation timing still on (not the failing feature)");
    Check(LogContains("simulation pace back to the game's own after an error in a tick made up"),
          "log: the pace back to the game's own");
    OweTicks();
    CallDue();
    CallTick();
    Check(g_calls.ticks == 2 && g_calls.afterTicks == 1, "afterwards one tick per frame (the game's own pace)");
}

// ---------------------------------------------------------------------------------------------
// Animations of drawn frames (games/zeus/timing: city_render)

struct Drawn {
    unsigned char tile;
    short building, figure;
    int counter;
};

Drawn ReadDrawn()
{
    return Drawn{At<unsigned char>(aTiles + 7), At<short>(aBuildings + 5 * aBuildingSize + 0x20),
                 At<short>(aFigures + 3 * aFigureSize), At<int>(aCounters)};
}

bool Same(const Drawn &a, const Drawn &b)
{
    return a.tile == b.tile && a.building == b.building && a.figure == b.figure && a.counter == b.counter;
}

// One frame whose drawing counts (the first), then one within 50 ms whose drawing does not.
void CountingThenNot()
{
    Timing<Frame_fn>("game_frame")();
    Timing<Render_fn>("city_render")(0, 0, 0);
    Timing<Frame_fn>("game_frame")();
}

void DrawnPutBack()
{
    if (!zeus::timing::PrepareForTest()) {
        Check(false, "the copy buffer");
        return;
    }
    const Drawn before = ReadDrawn();
    const Random random0 = ReadRandom();
    g_calls.touchRandom = true;
    CountingThenNot();
    const Drawn counted = ReadDrawn();
    const Random random1 = ReadRandom();
    Check(g_calls.frames == 2 && g_calls.draws == 1 && counted.tile == before.tile + 1 &&
              counted.counter == before.counter + 1,
          "a drawing that counts: what it moved on is kept");
    Check(random1.index == random0.index + 1 && random1.iv1 != random0.iv1,
          "... and the random numbers it turned (as the original)");
    Timing<Render_fn>("city_render")(0, 0, 0);
    Check(g_calls.draws == 2 && g_calls.drawFlagsZero, "a drawing that does not count: anim_due all 0 meanwhile");
    Check(Same(ReadDrawn(), counted), "... tile, building, figure and counters put back afterwards");
    const Random random2 = ReadRandom();
    Check(random2.index == random1.index && random2.pool7 == random1.pool7 && random2.iv1 == random1.iv1,
          "... and the random number state (index, pool, shift register)");
    Check(DueIsFrame(&At<unsigned char>(aAnimDue)), "... and the frame's own anim_due");
    Check(zeus::timing::DrawnWorking(), "drawn-frame animations still on");
}

void DrawnCopyFault()
{
    if (!zeus::timing::PrepareForTest()) {
        Check(false, "the copy buffer");
        return;
    }
    CountingThenNot();
    const Drawn counted = ReadDrawn();
    g_calls.drawCounters = false;
    ProtectCounters(PAGE_NOACCESS); // copying the drawing counters faults
    Timing<Render_fn>("city_render")(0, 0, 0);
    ProtectCounters(PAGE_READWRITE);
    Check(!zeus::timing::DrawnWorking(), "copying the drawn state faults: drawn-frame animations off");
    Check(g_calls.draws == 2 && g_calls.drawFlagsFrame, "the city still drawn, with the frame's own anim_due");
    Check(ReadDrawn().tile == counted.tile + 1, "that drawing counts (the game's own: every drawing does)");
    Check(LogContains("feature \"animations of drawn frames (games/zeus/timing)\" switched off"),
          "log: switched off");
    g_calls.drawCounters = true;
    Timing<Frame_fn>("game_frame")();
    Timing<Render_fn>("city_render")(0, 0, 0);
    Check(g_calls.draws == 3 && g_calls.drawFlagsFrame && ReadDrawn().tile == counted.tile + 2,
          "afterwards every drawing counts, nothing put back");
}

void DrawnPutBackFault()
{
    if (!zeus::timing::PrepareForTest()) {
        Check(false, "the copy buffer");
        return;
    }
    CountingThenNot();
    const Drawn counted = ReadDrawn();
    g_calls.drawCounters = false;
    g_calls.protectAfterDraw = true; // putting the drawing counters back faults
    Timing<Render_fn>("city_render")(0, 0, 0);
    ProtectCounters(PAGE_READWRITE);
    g_calls.protectAfterDraw = false;
    Check(!zeus::timing::DrawnWorking(), "putting the drawn state back faults: drawn-frame animations off");
    Check(DueIsFrame(&At<unsigned char>(aAnimDue)), "the frame's own anim_due back all the same");
    Check(ReadDrawn().tile == counted.tile && ReadDrawn().figure == counted.figure,
          "the regions before the failing one put back");
    Check(LogContains("drawn-frame animations off after an error"), "log: off, the game's own");
}

// ---------------------------------------------------------------------------------------------
// [debug] state_check (games/zeus/state_check): the save table's blocks around a drawing that does not count

void SaveEntry(int i, DWORD flag, DWORD address, DWORD size, DWORD count)
{
    const DWORD v[5] = {flag, address, size, count, 0};
    for (int k = 0; k < 5; ++k) {
        At<DWORD>(aSaveTable + 20 * i + 4 * k) = v[k];
    }
}

void SaveTable()
{
    SaveEntry(0, 1, aTiles, 1, 51984);          // put back by the drawn-frame animations
    SaveEntry(1, 1, aBuildings, 280, 4000);     // ... records 0 .. 5
    SaveEntry(2, 1, aTileFigures, 2, 51984);    // not put back
    SaveEntry(3, 0, aTiles, 1, 51984);          // listed twice: skipped
    SaveEntry(4, 0, 0x2000000, 4, 1);           // outside the game: skipped
}

// The memory compared in the test: 0x840000 .. 0xf20000 (the drawing counters, tiles, buildings, figures, each tile's
// figure list, the random number state; not anim_due 0x6668ec, which only our own swap changes).
constexpr DWORD kCheckLow = 0x840000, kCheckSize = 0xf20000 - 0x840000;

void StateCheck()
{
    if (!zeus::timing::PrepareForTest() || !zeus::state_check::EnableForTest(kCheckLow, kCheckSize)) {
        Check(false, "the copy buffers");
        return;
    }
    SaveTable();
    CountingThenNot();
    const Drawn counted = ReadDrawn();
    g_calls.touchList = true;
    g_calls.touchRandom = true;
    Timing<Render_fn>("city_render")(0, 0, 0);
    Check(Same(ReadDrawn(), counted), "the drawing that does not count: put back as before");
    Check(LogContains("new region 0xd0d878 (+103968 B, save block 2, 2 x 51984, first in record 100 at +0x0) changed "
                      "by a drawing that does not count; still changed after our put-back"),
          "log: the save block not put back, with its record and field, still changed");
    Check(LogContains("new region 0xcb4b08 (+51984 B, save block 0, 1 x 51984, first in record 7 at +0x0) changed by "
                      "a drawing that does not count; undone by our put-back"),
          "log: a save block put back: undone");
    Check(LogContains("new region 0xf137e4 (+2 B, not in the save, first at 0xf137e4) changed by a drawing that does "
                      "not count; undone by our put-back") &&
              LogContains("new region 0xf13624 (+33 B, not in the save, first at 0xf13624)"),
          "log: the random number state (not in the save): changed, undone");
    Check(LogContains("1 drawing(s) checked: 1 changed the game's memory, 1 left something changed after our put-back"),
          "log: the first check, both counts");
    zeus::state_check::Report(" (test)");
    Check(LogContains("state check:   0xd0d878 +103968 B (save block 2, 2 x 51984, first in record 100 at +0x0): 1 / 1; "
                      "still changed from 0xd0d940 (record 100 +0x0) to 0xd0d940 (record 100 +0x0)") &&
              LogContains("state check:   0xf137e4 +2 B (not in the save, first at 0xf137e4): 1 / 0"),
          "log: the table: changed in 1 check, still changed in 1 / 0, and where the leftover is");
    Check(!LogContains("0x6668"), "log: nothing outside the compared memory");
    Check(zeus::state_check::Working() && zeus::timing::DrawnWorking(), "check and put-back still on");
}

void StateCheckFault()
{
    if (!zeus::timing::PrepareForTest() || !zeus::state_check::EnableForTest(kCheckLow, kCheckSize)) {
        Check(false, "the copy buffers");
        return;
    }
    SaveTable();
    CountingThenNot();
    const Drawn counted = ReadDrawn();
    DWORD old = 0;
    VirtualProtect(reinterpret_cast<void *>(aTileFigures + g_imageShift), 16, PAGE_NOACCESS, &old);
    Timing<Render_fn>("city_render")(0, 0, 0);
    VirtualProtect(reinterpret_cast<void *>(aTileFigures + g_imageShift), 16, old, &old);
    Check(!zeus::state_check::Working(), "copying the memory faults: the check off");
    Check(g_calls.draws == 2 && Same(ReadDrawn(), counted) && zeus::timing::DrawnWorking(),
          "the city still drawn and put back (the drawn-frame animations stay on)");
    Check(LogContains("feature \"drawing check (games/zeus/state_check)\" switched off"), "log: switched off");
    Check(!LogContains("drawing(s) checked"), "log: no comparison after the failed copy");
}

// ---------------------------------------------------------------------------------------------
// The logical resolution (games/zeus/resolution) and the world map's flags (games/zeus/world_map)

constexpr DWORD fModeSize = 0x50bcc0, fConfigLoad = 0x5c44c0, fSwitchMode = 0x5c4a30, fSidebarPos = 0x58e280,
                fViewWide = 0x51cba0, fViewNarrow = 0x51cc00, fViewportInit = 0x449e50, fAnim = 0x599460;
constexpr DWORD aModeW = 0xf2a6b4, aModeH = 0xf2a6b0, aScreenMode = 0xf2a770, aGameScreen = 0xf2a6dd,
                aSidebarX = 0x10dff34, aSidebarX2 = 0x10dff30;

struct ResCalls {
    int switches = 0, sidebars = 0, wides = 0, narrows = 0, inits = 0, anims = 0;
    int cols = 0, rows = 0, top = 0, tileW = 0;
    int animDueSeen = -1;
} g_res;

void __cdecl FakeModeSize(int mode) // the game's table
{
    static const int kW[] = {0, 640, 800, 1024}, kH[] = {0, 480, 600, 768};
    if (mode >= 1 && mode <= 3) {
        At<int>(aModeW) = kW[mode];
        At<int>(aModeH) = kH[mode];
    }
}

int __cdecl FakeConfigLoad()
{
    At<int>(aScreenMode) = 2; // Zeus.inf as the GOG copy has it (800x600)
    return 1;
}

void __cdecl FakeSwitchMode(int)
{
    ++g_res.switches;
}

void __cdecl FakeSidebarPos()
{
    ++g_res.sidebars;
    At<int>(aSidebarX) = 0x346;
    At<int>(aSidebarX2) = 0x3be;
}

void __cdecl FakeViewWide()
{
    ++g_res.wides;
}

void __cdecl FakeViewNarrow()
{
    ++g_res.narrows;
}

void __cdecl FakeViewportInit(int, int, int, int top, int cols, int rows, int tileW, int)
{
    ++g_res.inits;
    if (Fires("viewport_init")) {
        Fault();
    }
    g_res.top = top;
    g_res.cols = cols;
    g_res.rows = rows;
    g_res.tileW = tileW;
}

unsigned __cdecl FakeAnim(int, unsigned frame)
{
    ++g_res.anims;
    g_res.animDueSeen = At<unsigned char>(aAnimDue);
    if (Fires("anim_advance_ui")) {
        Fault();
    }
    return frame + 1;
}

template <typename F> F Detour(const zeus::Detour *d, int n, const char *name)
{
    for (int i = 0; i < n; ++i) {
        if (std::strcmp(d[i].site->name, name) == 0) {
            return reinterpret_cast<F>(d[i].detour);
        }
    }
    std::printf("no detour %s\n", name);
    ExitProcess(3);
}

template <typename F> F Res(const char *name)
{
    int n;
    const zeus::Detour *d = zeus::resolution::Detours(&n);
    return Detour<F>(d, n, name);
}

using Mode_fn = void(__cdecl *)(int);
using Int_fn = int(__cdecl *)();
using Anim_fn = unsigned(__cdecl *)(int, unsigned);

void SetupResolution()
{
    Thunk(fModeSize, reinterpret_cast<void *>(&FakeModeSize));
    Thunk(fConfigLoad, reinterpret_cast<void *>(&FakeConfigLoad));
    Thunk(fSwitchMode, reinterpret_cast<void *>(&FakeSwitchMode));
    Thunk(fSidebarPos, reinterpret_cast<void *>(&FakeSidebarPos));
    Thunk(fViewWide, reinterpret_cast<void *>(&FakeViewWide));
    Thunk(fViewNarrow, reinterpret_cast<void *>(&FakeViewNarrow));
    Thunk(fViewportInit, reinterpret_cast<void *>(&FakeViewportInit));
    At<int>(aScreenMode) = 3;
    FakeModeSize(3);
    At<unsigned char>(aGameScreen) = 0x00; // the main menu
    zeus::resolution::InstalledForTest(core_config_get()); // resolution = scaled, 100%: 1920x1080 on the stub monitor
}

// The first city screen leaves the 1024x768 frame: one mode change of the game's own; mode 3 is then ours.
void ToCity()
{
    At<unsigned char>(aGameScreen) = 0x01;
    zeus::resolution::OnLoop();
    Res<Mode_fn>("screen_mode_size")(3);
}

void ModeSizeScenario()
{
    SetupResolution();
    Check(Res<Int_fn>("config_load")() == 1 && At<int>(aScreenMode) == 3, "Zeus.inf's mode 2: the game starts in 3");
    Res<Mode_fn>("screen_mode_size")(3);
    Check(At<int>(aModeW) == 1024 && At<int>(aModeH) == 768, "front-end screens (before the city): 1024x768");
    Res<Mode_fn>("screen_mode_size")(2);
    Check(At<int>(aModeW) == 800 && At<int>(aModeH) == 600, "mode 2 (F6): the game's own 800x600");
    At<int>(aModeW) = 1024;
    At<int>(aModeH) = 768;
    ToCity();
    Check(g_res.switches == 1 && !zeus::resolution::FrontEndForTest(), "the city screen: one mode change of the game's");
    Check(At<int>(aModeW) == 1920 && At<int>(aModeH) == 1080, "mode 3 in the city: 1920x1080");
    zeus::resolution::OnBackground(); // a picture while the city screen is up (e.g. a dialog's): no frame
    Check(!zeus::resolution::FrontEndForTest(), "a picture on a city screen: stays at the city's size");
    At<unsigned char>(aGameScreen) = 0x0d; // the file dialog over the city, then a loading picture
    zeus::resolution::OnBackground();
    Check(zeus::resolution::FrontEndForTest(), "a full-screen picture outside the city: the 1024x768 frame");
    Res<Mode_fn>("screen_mode_size")(3);
    Check(At<int>(aModeW) == 1024 && At<int>(aModeH) == 768, "mode 3 in the frame: 1024x768");
    Check(LogContains("Zeus.inf has mode 2") && LogContains("mode 3 = 1920x1080"), "log: start-up mode and the size");
}

void ModeSizeFault()
{
    SetupResolution();
    ToCity();
    const int before = g_nativeCalls;
    g_nativeFaultAt = before + 2; // the second call: after mode_width/height were written (the log line)
    At<int>(aModeW) = 1024;
    At<int>(aModeH) = 768;
    // Another size so that the log line runs: a fixed 1280x1024.
    core_config c = *core_config_get();
    c.resolution = CORE_RESOLUTION_FIXED;
    c.resolution_w = 1280;
    c.resolution_h = 1024;
    zeus::resolution::SettingsChanged(&c);
    Res<Mode_fn>("screen_mode_size")(3);
    Check(At<int>(aModeW) == 1024 && At<int>(aModeH) == 768, "a fault after the size was written: 1024x768 back");
    Check(LogContains("feature \"resolution switching (games/zeus/resolution)\" switched off"), "log: switching off");
    Res<Mode_fn>("screen_mode_size")(3);
    Check(At<int>(aModeW) == 1024, "afterwards mode 3 is the game's own 1024x768");
    Check(!zeus::resolution::SettingsChanged(&c), "a new setting is refused (the game runs at its own size)");
}

void ViewportLayout()
{
    SetupResolution();
    ToCity();
    Res<Tick_fn>("ui_set_sidebar_position")();
    Check(g_res.sidebars == 0 && At<int>(aSidebarX) == 60 * 28 - 2 && At<int>(aSidebarX2) == 60 * 28 - 2 + 0x78,
          "1920 wide: 28 columns, the sidebar where they end (1678, 56 px left on its right)");
    Res<Tick_fn>("city_viewport_set_640")();
    Check(g_res.wides == 0 && g_res.cols == 32 && g_res.rows == 71 && g_res.top == 0x1e && g_res.tileW == 0x3a,
          "1920x1080, sidebar closed: 32 x 71");
    Res<Tick_fn>("city_viewport_narrow")();
    Check(g_res.narrows == 0 && g_res.cols == 28 && g_res.rows == 71,
          "1920x1080, sidebar open: 28 x 71 (up to the sidebar)");
    At<int>(aModeW) = 1024;
    At<int>(aModeH) = 768;
    Res<Tick_fn>("ui_set_sidebar_position")();
    Res<Tick_fn>("city_viewport_set_640")();
    Res<Tick_fn>("city_viewport_narrow")();
    Check(g_res.sidebars == 1 && g_res.wides == 1 && g_res.narrows == 1, "1024x768: the game's own functions");
    At<int>(aModeW) = 1024; // 1024 wide and taller (a window): the formulas give the game's columns
    At<int>(aModeH) = 900;
    Res<Tick_fn>("city_viewport_set_640")();
    Check(g_res.cols == 17 && g_res.rows == (900 - 30) / 15 + 1, "1024x900: 17 columns, rows for the height");
}

void LayoutFault()
{
    SetupResolution();
    ToCity();
    g_fault = "viewport_init";
    Res<Tick_fn>("city_viewport_set_640")();
    Check(g_res.inits == 2 && g_res.cols == 17 && g_res.rows == 50, "a fault: the 1024 layout's 17 x 50 at once");
    Check(LogContains("feature \"city layout at this resolution (games/zeus/resolution)\" switched off"),
          "log: layout off");
    Check(LogContains("back to the game's own 1024x768"), "log: back to the game's own size");
    At<unsigned char>(aGameScreen) = 0x01;
    const int switches = g_res.switches;
    Sleep(510); // the mode change is tried at most every 500 ms
    zeus::resolution::OnLoop();
    Check(g_res.switches == switches + 1, "the next loop: the game's own mode change");
}

void RunFrame() // one game_frame through our detour (it may wait for the frame cap)
{
    const int f = g_calls.frames;
    while (g_calls.frames == f) {
        Timing<Frame_fn>("game_frame")();
    }
}

constexpr DWORD fMapDraw = 0x599da0, aDdHeight = 0xf2a69c;
int g_mapDrawHeight = -1; // dd_height as the world map's drawing saw it

void __cdecl FakeMapDraw()
{
    g_mapDrawHeight = At<int>(aDdHeight);
}

void MapHeight()
{
    Thunk(fMapDraw, reinterpret_cast<void *>(&FakeMapDraw));
    int n;
    const zeus::Detour *d = zeus::world_map::Detours(&n);
    Tick_fn draw = Detour<Tick_fn>(d, n, "world_map_draw");
    At<int>(aDdHeight) = 1440;
    draw();
    Check(g_mapDrawHeight == 768 && At<int>(aDdHeight) == 1440, "1440 high: the map drawn with 768, 1440 back after");
    At<int>(aDdHeight) = 768;
    draw();
    Check(g_mapDrawHeight == 768 && At<int>(aDdHeight) == 768, "768 high: as it is");
    At<int>(aDdHeight) = 600;
    draw();
    Check(g_mapDrawHeight == 600 && At<int>(aDdHeight) == 600, "800x600: as it is");
    Check(LogContains("world map: drawn with the game's own height 768"), "log: once");
}

void DrawOnly()
{
    SetupResolution();
    g_frameAsksDue = true;
    g_panelOpen = true;
    const int frames = g_calls.frames;
    Timing<Frame_fn>("game_frame")();
    Check(g_calls.frames == frames, "panel open, not asking: no frame (the game paused)");
    g_panelWantsFrames = true;
    At<unsigned char>(aGameScreen) = 0x01; // the city: the new resolution is applied in the frame
    const int dues = g_calls.dues;
    Timing<Frame_fn>("game_frame")();
    Check(g_calls.frames == frames + 1 && g_frameDue == 0 && g_calls.dues == dues,
          "panel asks: a frame drawn, sim_tick_due says no (the game's is not asked)");
    Check(g_res.switches == 1, "... and the resolution switched in it");
    Timing<Frame_fn>("game_frame")();
    Check(g_calls.frames == frames + 1, "at most one every 16 ms");
    Sleep(20);
    g_fault = "game_frame";
    Timing<Frame_fn>("game_frame")();
    Check(LogContains("feature \"draw-only frames while the panel asks (games/zeus/timing)\" switched off"),
          "a fault in the frame: draw-only frames off");
    Sleep(20);
    Timing<Frame_fn>("game_frame")();
    Check(g_calls.frames == frames + 2, "afterwards: no frames while the panel is open");
    g_panelOpen = false;
    g_panelWantsFrames = false;
    const int duesBefore = g_calls.dues;
    int n = g_calls.frames;
    while (g_calls.frames == n) {
        Timing<Frame_fn>("game_frame")();
    }
    Check(g_calls.dues > duesBefore, "panel closed: sim_tick_due asks the game again (the flag put back)");
}

void MapFlags()
{
    Thunk(fAnim, reinterpret_cast<void *>(&FakeAnim));
    int n;
    const zeus::Detour *d = zeus::world_map::Detours(&n);
    Anim_fn anim = Detour<Anim_fn>(d, n, "anim_advance_ui");
    RunFrame();
    At<unsigned char>(aAnimDue) = 1;
    Check(anim(5, 2) == 3 && g_res.animDueSeen == 1, "the first frame: the flags step (anim_due[0] as the game set it)");
    RunFrame();
    At<unsigned char>(aAnimDue) = 1;
    Check(anim(5, 3) == 4 && g_res.animDueSeen == 0 && At<unsigned char>(aAnimDue) == 1,
          "the next frame within 50 ms: anim_due[0] is 0 for the call, 1 again after it");
    Sleep(60);
    RunFrame();
    At<unsigned char>(aAnimDue) = 1;
    anim(5, 4);
    const int first = g_res.animDueSeen;
    anim(6, 1);
    Check(first == 1 && g_res.animDueSeen == 1, "50 ms later: every call of that frame steps");
    RunFrame();
    At<unsigned char>(aAnimDue) = 1;
    g_fault = "anim_advance_ui";
    const int calls = g_res.anims;
    anim(5, 5);
    Check(g_res.anims == calls + 2 && g_res.animDueSeen == 1 && At<unsigned char>(aAnimDue) == 1,
          "a fault: anim_due[0] back, the game's own step");
    Check(LogContains("feature \"world map flags at the original's pace (games/zeus/world_map)\" switched off"),
          "log: flags off");
    RunFrame();
    anim(5, 6);
    Check(g_res.animDueSeen == 1, "afterwards: the game's own way");
}

struct Scenario {
    const char *name;
    void (*run)();
};
const Scenario kScenarios[] = {
    {"pace_ok", &PaceOk},
    {"pace_fault", &PaceFault},
    {"extra_tick_fault", &ExtraTickFault},
    {"drawn_put_back", &DrawnPutBack},
    {"drawn_copy_fault", &DrawnCopyFault},
    {"drawn_put_back_fault", &DrawnPutBackFault},
    {"state_check", &StateCheck},
    {"state_check_fault", &StateCheckFault},
    {"mode_size", &ModeSizeScenario},
    {"mode_size_fault", &ModeSizeFault},
    {"viewport_layout", &ViewportLayout},
    {"layout_fault", &LayoutFault},
    {"map_flags", &MapFlags},
    {"map_height", &MapHeight},
    {"draw_only", &DrawOnly},
};

} // namespace

int main(int argc, char **argv)
{
    if (argc >= 2) {
        for (const Scenario &s : kScenarios) {
            if (std::strcmp(argv[1], s.name) == 0) {
                DeleteFileA("test_restore.log");
                core_log_open(GetModuleHandleW(nullptr), "test_restore.log");
                core::diag::InstallExceptionLog();
                Setup();
                s.run();
                return g_failures ? 1 : 0;
            }
        }
        std::printf("unknown scenario %s\n", argv[1]);
        return 2;
    }
    char self[MAX_PATH];
    GetModuleFileNameA(nullptr, self, MAX_PATH);
    int failed = 0;
    for (const Scenario &s : kScenarios) {
        std::printf("%s\n", s.name);
        std::fflush(stdout);
        char cmd[MAX_PATH + 64];
        std::snprintf(cmd, sizeof(cmd), "\"%s\" %s", self, s.name);
        STARTUPINFOA si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};
        DWORD code = 99;
        if (CreateProcessA(nullptr, cmd, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, 30000);
            GetExitCodeProcess(pi.hProcess, &code);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
        if (code != 0) {
            std::printf("  scenario %s FAILED (exit code %lu)\n", s.name, code);
            ++failed;
        }
    }
    std::printf("RESULT: %s (%d scenario(s) failed)\n", failed ? "FAIL" : "PASS", failed);
    return failed ? 1 : 0;
}
