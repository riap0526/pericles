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
// The intro movie (games/zeus/intro):
//   intro_skip               skip: game_main's call of bink_open (seen by its return address) is left out; another
//                           caller's movie opens with its arguments; play: game_main's opens
//   intro_fault         a fault while skipping: off, the movie opens as the game does it
//
// The framed front-end screens (games/zeus/fullscreen with the core's core/display/framed_screens and
// core/display/resolution_switch; without the player's interface pack, so no background is composed):
//   framed_screens              the main menu framed whole at the city's scale, a loading picture by its 800x600, the same
//                           picture again does nothing, not framed on a video or the city screen
//   framed_setting      the panel's resolution setting and Shift+F7 on a front-end screen: no mode change, the
//                           framing follows the new scale at once (the changed listener)
//   framed_fault        a fault while the framing follows the setting: framing off (the game's own frame), the
//                           resolution switching stays on and the setting is applied
//   city_around         menu_screens = city: the panel pattern before an episode and in the editors; the
//                           drop-down menu 0x03 judged by the screen under it
//   keep_city           the city on screen kept after city_render's detour, saved when a front-end screen comes
//                           up, at most every 3 s, not on a front-end screen (the map editor) or in the editors'
//                           city (editor_mode)
//   switch_fault        the game's mode change faults while switching (the core's resolution_switch, Emperor's
//                           switch_fault): switching off, back to the game's own 1024x768, new settings refused
//   city_skipped        no city kept in the whole run (editor_mode stays 1): one summary line with the count and
//                           the last reason at the next front-end picture, not repeated
//
// The city screen's layout (games/zeus/resolution with the core's core/display/city_layout):
//   spare_rows          one more half row at an uneven height (the clip past the screen's bottom), the spare rows
//                           allowed and logged; none at 1080 and at the game's own 1024x768; the map's view range logged
//   top_bar_tiled                the top bar tiled from the sidebar to the left edge above 1024 wide; a fault: off
//   message_panel       the city message laid out on the screen's bottom, the clip back afterwards; the bottom
//                           strip not blanked (only city_bottom_and_sidebar's call); a fault: clip back, off
//   sidebar_filler              the sidebar's picture moved to the bottom, its left edge carried on, the inset panel's
//                           tiles edge to edge; both sidebars; not on the world map; 768 and 790 high; without the
//                           pack the strip inside a narrowed clip; the world map's sidebar on its own picture;
//                           a fault: the clip back, off
//   pace_behind         the pace's cap when frames come slowly (Emperor's falling_behind): at most 2 ticks per
//                           frame, logged when it starts and ends
//   info_window                [ui] info_window: original (the game's 768 rule at any height, moved with the city
//                           area), centered, cursor, the world map's window in the middle, 0x7d placed, the granary
//                           kept at the game's place; the game's own size; a fault: the game's position, off
//   fixed_window        the granary's window drawn into a copy and shown beside the cursor, the input's mouse
//                           in the game's place, its buttons shifted (by return address); a fault: draw_buffer and
//                           the mouse back, off, drawn where the game has it
//
// The world map's layout (games/zeus/world_map with the core's core/display/map_screen):
//   world_map           the first map (not loaded yet) into our layer with the game's 768, native in the middle,
//                           background city and map, the mouse in map coordinates (hover, beside the map, sidebar),
//                           scaled fit and 200%; the game's own at 1024x768 and in the editors; the campaign map (the
//                           blurred map whatever the setting, the top bar tiled, markers and clicks in map
//                           coordinates, the sidebar's filler); a fault: the game's own drawing, the real mouse, off

//
// The display scale:
//   display_scale_sizes               every logical size the common screens give at each scale (the core's ChooseLogicalSize),
//                           windows and fixed sizes: the sidebar, both viewports, the filler, the information window
//                           (three settings, four corners), the world map native and fitted with its middle clicked
//
// The smoke test's actions (games/zeus/smoke; only in a smoke test, no detour):
//   smoke_actions       win calls cheat_run(0) (a __cdecl fake) only in the city, not governing on, with the
//                           code and table as expected; lose writes the three values at their sizes; the dialog offset
//
// Each scenario runs in a child process of its own (a feature switched off stays off for the rest of a process):
//   test_restore.exe            runs every scenario and prints one line each
//   test_restore.exe <name>     one scenario (exit code 0 = passed)
#include "core/diag/autorun.h"
#include "core/diag/guard.h"
#include "core/log.h"
#include "core/config.h"
#include "core/ddraw/surface_memory.h"
#include "core/display/framed_screens.h"
#include "core/display/logical_size.h"
#include "core/display/map_screen.h"
#include "core/output/sdl_output.h"
#include "core/ui/info_window.h"
#include "games/zeus/anim_clock.h"
#include "games/zeus/fullscreen.h"
#include "games/zeus/intro.h"
#include "games/zeus/resolution.h"
#include "games/zeus/site.h"
#include "games/zeus/smoke.h"
#include "games/zeus/state_check.h"
#include "games/zeus/timing.h"
#include "games/zeus/world_map.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstring>

// ---------------------------------------------------------------------------------------------
// Stubs for the parts of the DLL these files call that are not under test

const char *volatile g_fault = ""; // a fake faults once when its name is the armed one (Fires, below)
bool Fires(const char *name);
void Fault();

// What the framing (core/display/framed_screens) handed to the output.
core::output::Framing g_framing;
int g_framings = 0, g_backgrounds = 0, g_cityPictures = 0;

namespace core::output {
bool FrameWanted() { return false; }
void HoldNextFrame(DWORD) {}
void TakeCounts(long *a, long *b) { *a = *b = 0; }
int RefreshHz() { return 60; }
void MeasureFromNow() {}
void TakeIntervals(char *out, std::size_t) { out[0] = 0; }
void SetBackground(int, int, int, int, double, const void *, int, const void *, int) { ++g_backgrounds; }
void SetCaption(const char *, int) {}
} // namespace core::output

namespace core::assets { // no player's picture
bool LoadPicture565(const wchar_t *, int, int, std::vector<WORD> *, int *, int *) { return false; }
} // namespace core::assets

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
SIZE ClientSize() { return SIZE{1920, 1080}; }
SIZE ModeSize() // the game's mode size (mode_width / mode_height of the copy)
{
    return SIZE{*reinterpret_cast<volatile int *>(0xf2a6b4 + zeus::g_imageShift),
                *reinterpret_cast<volatile int *>(0xf2a6b0 + zeus::g_imageShift)};
}
void SetFraming(const core::output::Framing &f)
{
    if (Fires("set_framing")) {
        Fault();
    }
    g_framing = f;
    ++g_framings;
}
} // namespace core::display

namespace core::diag::autorun { // the smoke test is not started here (its own test: test_autorun)
bool Start(const Game &) { return false; }
void Frame() {}
bool Requested() { return false; }
bool IntroRequested() { return false; }
} // namespace core::diag::autorun

namespace core::capture {
int Flush(unsigned long) { return 0; }
bool SaveAs(const void *, int, int, int, const char *, bool) // the city kept for the menus (menu_background_auto.png)
{
    ++g_cityPictures;
    return true;
}
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
bool GameMainMatches() { return true; }
bool PicturesHooked() { return true; }
bool ImageDrawHooked() { return false; }
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

// The log's size now, and whether text was logged after that point (the log is shared by the scenarios of a run).
long LogSize()
{
    FILE *f = std::fopen("test_restore.log", "rb");
    if (!f) {
        return 0;
    }
    std::fseek(f, 0, SEEK_END);
    const long n = std::ftell(f);
    std::fclose(f);
    return n;
}

bool LogContainsSince(long from, const char *text)
{
    FILE *f = std::fopen("test_restore.log", "rb");
    if (!f) {
        return false;
    }
    static char buf[1 << 20];
    std::fseek(f, from, SEEK_SET);
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

} // namespace

// A fake faults once when its name is the armed one (g_fault, above the stubs).
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

namespace {

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
                fViewWide = 0x51cba0, fViewNarrow = 0x51cc00, fViewportInit = 0x449e50, fAnim = 0x599460, fBmpShot = 0x50b030;
constexpr DWORD aModeW = 0xf2a6b4, aModeH = 0xf2a6b0, aScreenMode = 0xf2a770, aGameScreen = 0xf2a6dd,
                aSidebarX = 0x10dff34, aSidebarX2 = 0x10dff30;

struct ResCalls {
    int switches = 0, sidebars = 0, wides = 0, narrows = 0, inits = 0, anims = 0, bmpShots = 0;
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

int __cdecl FakeBmpShot(const char *)
{
    ++g_res.bmpShots;
    return 1;
}

void __cdecl FakeSwitchMode(int)
{
    ++g_res.switches;
    if (Fires("screen_switch_mode")) {
        Fault();
    }
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
    *reinterpret_cast<volatile int *>(0x7d3b88 + g_imageShift) = 765; // the game's clip at 768 (50 half rows)
}

constexpr DWORD aClipBottom = 0x7d3b88;

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
    At<int>(aClipBottom) = (rows - 1) * 15 + top; // as the game's
}

// The top bar, the bottom strip and the city message panel. The image functions are __thiscall: as __fastcall
// fakes (ecx, edx unused, then the stack arguments) they pop the same bytes.
constexpr DWORD fTopBar = 0x59ebe0, fImageGroup = 0x40fcf0, fImageDraw = 0x5acf50, fImageSize = 0x5aceb0,
                fFillRect = 0x509b20, fMessagePanel = 0x4f2d80;
constexpr int kMaxDraws = 4096; // image_draw calls recorded (the filler at 3840x2160: about 900)
struct CityLayoutCalls {
    int topBars = 0, draws = 0, fills = 0, panels = 0, panelClip = 0, sidebars = 0, worldSidebars = 0;
    int drawX[kMaxDraws] = {}, drawY[kMaxDraws] = {}, drawIds[kMaxDraws] = {}, drawId = 0;
} g_cityLayout;
// Each group's entry: +4 is its first image (+ 0x4000 = the id for image_draw).
BYTE g_groupBar[8] = {0, 0, 0, 0, 0xcf, 0x04, 0, 0};   // 0x6b: 1231, the top bar (838x30)
BYTE g_groupInset[8] = {0, 0, 0, 0, 0xe4, 0x01, 0, 0}; // 0x84: 484, the inset panel's 7 x 7 tiles (16x16)
BYTE g_groupStrip[8] = {0, 0, 0, 0, 0xd2, 0x04, 0, 0}; // 0x62: 1234, the 800x600 strip (16x600)

void __cdecl FakeTopBar(int, int)
{
    ++g_cityLayout.topBars;
}

BYTE *__fastcall FakeImageGroup(void *, int, int group)
{
    return group == 0x6b ? g_groupBar : group == 0x84 ? g_groupInset : group == 0x62 ? g_groupStrip : nullptr;
}

char __fastcall FakeImageSize(void *, int, int id, int *w, int *h)
{
    *w = *h = 0;
    if (id == 0x4cf + 0x4000) {
        *w = 838;
        *h = 30;
    }
    else if (id >= 0x1e4 + 0x4000 && id < 0x1e4 + 0x4000 + 49) {
        *w = *h = 16;
    }
    else if (id == 0x4d2 + 0x4000) {
        *w = 16;
        *h = 600;
    }
    return *w > 0;
}

void __fastcall FakeImageDraw(void *, int, int id, int x, int y, int, int, int)
{
    if (Fires("image_draw")) {
        Fault();
    }
    if (g_cityLayout.draws < kMaxDraws) {
        g_cityLayout.drawX[g_cityLayout.draws] = x;
        g_cityLayout.drawY[g_cityLayout.draws] = y;
        g_cityLayout.drawIds[g_cityLayout.draws] = id;
    }
    g_cityLayout.drawId = id;
    ++g_cityLayout.draws;
}

// A sidebar's backdrop: rows numbered (pixel = row) at sidebar_x, into draw_buffer; in column 93 the picture's frame
// starts at g_frameRow (a dark pixel, as the filler looks for it: Zeus's 604, Poseidon's 610 and 614) and the rows
// from 600 to it are bright.
int g_frameRow = 604;

void PaintBackdrop()
{
    WORD *buf = reinterpret_cast<WORD *>(At<BYTE *>(0xf9c100));
    const int pitch = At<int>(0xf2a698) / 2, x0 = At<int>(0x10dff34);
    for (int y = 0; y < 768; ++y) {
        for (int x = x0; x < x0 + 186; ++x) {
            buf[y * pitch + x] = static_cast<WORD>(y);
        }
    }
    for (int y = 600; y < g_frameRow; ++y) {
        buf[y * pitch + x0 + 93] = 0x001f;
    }
    buf[g_frameRow * pitch + x0 + 93] = 0x0000;
}

// The game's sidebar (both kinds).
void PaintSidebar()
{
    ++g_cityLayout.sidebars;
    PaintBackdrop();
}

void __cdecl FakeSidebarDraw()
{
    PaintSidebar();
}

void __cdecl FakeSidebarUnits(int)
{
    PaintSidebar();
}

// world_map_sidebars: the world map's sidebar picture (rows numbered as above) down to 768.
void __cdecl FakeWorldSidebars()
{
    ++g_cityLayout.worldSidebars;
    PaintBackdrop();
}

void __cdecl FakeFillRect(int, int, int, int, int)
{
    ++g_cityLayout.fills;
}

void __cdecl FakeMessagePanel()
{
    ++g_cityLayout.panels;
    g_cityLayout.panelClip = At<int>(aClipBottom);
    if (Fires("city_message_layout")) {
        Fault();
    }
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
    Thunk(fBmpShot, reinterpret_cast<void *>(&FakeBmpShot));
    Thunk(fTopBar, reinterpret_cast<void *>(&FakeTopBar));
    Thunk(fImageGroup, reinterpret_cast<void *>(&FakeImageGroup));
    Thunk(fImageSize, reinterpret_cast<void *>(&FakeImageSize));
    Thunk(fImageDraw, reinterpret_cast<void *>(&FakeImageDraw));
    Thunk(fFillRect, reinterpret_cast<void *>(&FakeFillRect));
    Thunk(fMessagePanel, reinterpret_cast<void *>(&FakeMessagePanel));
    Thunk(0x517b10, reinterpret_cast<void *>(&FakeSidebarDraw));
    Thunk(0x525280, reinterpret_cast<void *>(&FakeSidebarUnits));
    Thunk(0x597e50, reinterpret_cast<void *>(&FakeWorldSidebars));
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
    Check(LogContains("feature \"resolution switching (core/display/resolution_switch)\" switched off"), "log: switching off");
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
    using Shot_fn = int(__cdecl *)(const char *);
    Check(Res<Shot_fn>("bmp_screenshot")("shot1.bmp") == 0 && g_res.bmpShots == 0,
          "Alt+1 / Alt+2: the game's own BMP screenshot is not run (its buffer is for 1024x768)");
}

// The city reaches the screen's bottom (one more half row, into the spare rows), the map's view range in the
// viewport's log.
void SpareRows()
{
    SetupResolution();
    ToCity();
    At<int>(0xfa05b4) = 10; // a map whose view range is 40 columns x 60 half rows
    At<int>(0xfa05b0) = 50;
    At<int>(0xfa05ac) = 20;
    At<int>(0xfa05a8) = 80;
    At<int>(aModeH) = 1013; // a window of an uneven height
    Res<Tick_fn>("ui_set_sidebar_position")();
    Res<Tick_fn>("city_viewport_narrow")();
    Check(g_res.cols == 28 && g_res.rows == 67 && At<int>(aClipBottom) == 1020,
          "1920x1013: 67 half rows, the clip at 1020 (the old 66 stopped at 1005)");
    Check(core::ddraw::CanaryAllowance() == 7 && LogContains("may draw 7 row(s) below the screen"),
          "7 spare rows allowed below the screen, logged");
    Check(LogContains("current map view range 40 x 60 -- VIEWPORT LARGER THAN THE MAP"),
          "log: the map's view range, a viewport larger than it");
    At<int>(aModeH) = 1080;
    Res<Tick_fn>("city_viewport_narrow")();
    Check(g_res.rows == 71 && At<int>(aClipBottom) == 1080 && core::ddraw::CanaryAllowance() == 0,
          "1920x1080: 71 half rows to the bottom exactly, no spare rows");
    At<int>(aModeW) = 1024;
    At<int>(aModeH) = 768;
    Res<Tick_fn>("city_viewport_narrow")();
    Check(g_res.narrows == 1 && core::ddraw::CanaryAllowance() == 0, "1024x768: the game's own viewport, none");
}

// The top bar tiled from the sidebar to the left edge above 1024 wide; the game's own at 1024; a fault: off, the
// game's own (which draws nothing at this width).
void TopBarTiled()
{
    using Bar_fn = void(__cdecl *)(int, int);
    SetupResolution();
    ToCity();
    Res<Tick_fn>("ui_set_sidebar_position")();
    At<int>(0xf139bc) = 0; // the game's clip: the screen
    At<int>(0xf139b8) = 1920;
    Res<Bar_fn>("top_bar_draw")(0, 1920);
    Check(g_cityLayout.topBars == 0 && g_cityLayout.draws == 5 && g_cityLayout.drawId == 0x4cf + 0x4000,
          "1920 wide: the top bar (group 0x6b, image 1231) once whole and 4 times its even section, not the game's");
    Check(g_cityLayout.drawX[0] == -120 - 158 && g_cityLayout.drawX[1] == 120 - 158 && g_cityLayout.drawX[2] == 360 - 158 &&
              g_cityLayout.drawX[3] == 600 - 158 && g_cityLayout.drawX[4] == 1678 - 838 && g_cityLayout.drawY[0] == 0 && g_cityLayout.drawY[4] == 0,
          "columns 158..397 from the left (-120, 120, 360, 600; only the left side clipped), the whole bar last (840)");
    Check(At<int>(0xf139bc) == 0 && At<int>(0xf139b8) == 1920, "the game's clip back afterwards");
    At<int>(aModeW) = 1024;
    At<int>(aModeH) = 768;
    Res<Bar_fn>("top_bar_draw")(0, 1024);
    Check(g_cityLayout.topBars == 1 && g_cityLayout.draws == 5, "1024 wide: the game's own");
    At<int>(aModeW) = 1920;
    At<int>(aModeH) = 1080;
    g_fault = "image_draw";
    Res<Bar_fn>("top_bar_draw")(0, 1920);
    Check(LogContains("feature \"top bar (games/zeus/resolution)\" switched off") &&
              LogContains("the top bar is no longer drawn after an error"),
          "a fault while drawing: the top bar off, logged");
    Check(At<int>(0xf139bc) == 0 && At<int>(0xf139b8) == 1920, "the game's clip put back by the guard");
    Res<Bar_fn>("top_bar_draw")(0, 1920);
    Check(g_cityLayout.topBars == 2, "afterwards the game's own (nothing at this width)");
}

// The city message panel laid out with the screen's bottom as the clip's; the bottom strip not blanked while the
// city reaches the bottom (only city_bottom_and_sidebar's call); a fault: the clip back, off, the game's own layout.
void MessagePanel()
{
    using Fill_fn = void(__cdecl *)(int, int, int, int, int);
    SetupResolution();
    ToCity();
    At<int>(aModeH) = 1013;
    Res<Tick_fn>("ui_set_sidebar_position")();
    Res<Tick_fn>("city_viewport_narrow")();
    Res<Tick_fn>("city_message_layout")();
    Check(g_cityLayout.panels == 1 && g_cityLayout.panelClip == 1013 && At<int>(aClipBottom) == 1020,
          "the panel laid out on the screen's bottom (1013), the clip 1020 again afterwards");
    // city_bottom_and_sidebar's call at 0x58e320 (returning to 0x58e325): a stub at 0x58e300 pushes
    // (x 0, y 1007, w 1920, h 6, colour 0) and calls the detour there, as the game does.
    BYTE *stub = reinterpret_cast<BYTE *>(0x58e300 + g_imageShift);
    const DWORD args[5] = {0, 6, 1920, 1007, 0}; // pushed in this order: colour, h, w, y, x
    for (int i = 0; i < 5; ++i) {
        stub[i * 5] = 0x68;
        *reinterpret_cast<DWORD *>(stub + i * 5 + 1) = args[i];
    }
    for (int i = 25; i < 0x20; ++i) {
        stub[i] = 0x90;
    }
    BYTE *call = reinterpret_cast<BYTE *>(0x58e320 + g_imageShift);
    call[0] = 0xe8;
    *reinterpret_cast<DWORD *>(call + 1) =
        reinterpret_cast<DWORD>(Res<Fill_fn>("fill_rect")) - reinterpret_cast<DWORD>(call + 5);
    call[5] = 0x83; // add esp, 0x14
    call[6] = 0xc4;
    call[7] = 0x14;
    call[8] = 0xc3;
    reinterpret_cast<Tick_fn>(stub)();
    Check(g_cityLayout.fills == 0, "the bottom strip: not blanked while the city reaches the bottom");
    Res<Fill_fn>("fill_rect")(10, 10, 20, 20, 0);
    Check(g_cityLayout.fills == 1, "fill_rect from anywhere else: as always");
    g_fault = "city_message_layout";
    Res<Tick_fn>("city_message_layout")();
    Check(At<int>(aClipBottom) == 1020 &&
              LogContains("feature \"city message panel and bottom strip (games/zeus/resolution)\" switched off"),
          "a fault in the layout: the clip put back (1020), off");
    Res<Tick_fn>("city_message_layout")();
    reinterpret_cast<Tick_fn>(stub)();
    Check(g_cityLayout.panelClip == 1020 && g_cityLayout.fills == 2, "afterwards the game's own layout and strip");
}

// The game's mode change faults while switching to a new setting (Emperor's switch_fault, now the core's
// resolution_switch for both games): switching off, the next loop goes back to the game's own 1024x768 with the
// game's mode change, and a new setting is refused.
void SwitchFault()
{
    SetupResolution();
    ToCity();
    core_config c = *core_config_get();
    c.resolution = CORE_RESOLUTION_FIXED;
    c.resolution_w = 1280;
    c.resolution_h = 1024;
    Check(zeus::resolution::SettingsChanged(&c), "a new setting in the city");
    g_fault = "screen_switch_mode";
    const int switches = g_res.switches;
    Sleep(510); // the mode change is tried at most every 500 ms
    zeus::resolution::OnLoop();
    Check(g_res.switches == switches + 1, "the loop calls the game's mode change (it faults)");
    Check(LogContains("feature \"resolution switching (core/display/resolution_switch)\" switched off") &&
              LogContains("back to the game's own 1024x768"),
          "log: switching off, back to the game's own size");
    Sleep(510);
    zeus::resolution::OnLoop();
    Check(g_res.switches == switches + 2, "the next loop: the game's own mode change again (to its own size)");
    Res<Mode_fn>("screen_mode_size")(3);
    Check(At<int>(aModeW) == 1024 && At<int>(aModeH) == 768, "mode 3 is the game's own 1024x768");
    zeus::resolution::OnLoop();
    Check(LogContains("the game runs at its own 1024x768"), "log: the game at its own size");
    Check(!zeus::resolution::SettingsChanged(&c), "a new setting is refused");
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

// ---------------------------------------------------------------------------------------------
// The framed front-end screens (games/zeus/fullscreen, core/display/framed_screens)

constexpr DWORD aDrawBuffer = 0xf9c100, aPitch = 0xf2a698, aDdWidth = 0xf2a6a0, aDdHeight2 = 0xf2a69c,
                aCityLeft = 0x7d3b94, aCityTop = 0x7d3b8c, aCityRight = 0x7d3b90, aCityBottom = 0x7d3b88;

void SetupFramed()
{
    SetupResolution();
    Check(zeus::fullscreen::Attach(), "installed (the resolution is)");
}

void FramedScreens()
{
    SetupFramed();
    zeus::fullscreen::AfterPicture("data\\Poseidon_FE_MainMenu.jpg", 112, 84);
    Check(g_framing.on && g_framing.picture.left == 0 && g_framing.picture.right == 1024 &&
              g_framing.picture.bottom == 768 && g_framing.below == 0,
          "the main menu: the whole 1024x768 frame framed, no status block");
    Check(g_framing.scale > 0.99 && g_framing.scale < 1.01, "at the city's scale (1920x1080 at 100%: 1.0)");
    const int framings = g_framings;
    zeus::fullscreen::AfterPicture("data\\Poseidon_FE_MainMenu.jpg", 112, 84);
    Check(g_framings == framings, "the same picture drawn again: nothing done");
    zeus::fullscreen::AfterPicture("data\\zeus_load1.jpg", 112, 84);
    Check(g_framing.on && g_framing.picture.left == 112 && g_framing.picture.top == 84 &&
              g_framing.picture.right == 912 && g_framing.picture.bottom == 684,
          "a loading picture: its 800x600 at (112, 84)");
    Check(zeus::fullscreen::LoadingPicture("DATA\\Zeus_Load3.JPG") &&
              zeus::fullscreen::LoadingPicture("data\\poseidon_load8.jpg") &&
              !zeus::fullscreen::LoadingPicture("data\\Zeus_FE_MainMenu.jpg") &&
              !zeus::fullscreen::LoadingPicture("data\\zeus_loader") && !zeus::fullscreen::LoadingPicture(nullptr),
          "loading pictures by name: zeus_load*, poseidon_load* (.jpg)");
    At<unsigned char>(aGameScreen) = 0x4a; // a video
    zeus::fullscreen::OnLoop();
    Check(!g_framing.on, "a video: not framed");
    At<unsigned char>(aGameScreen) = 0x00;
    zeus::fullscreen::OnLoop();
    Check(g_framing.on, "back on the menu: framed");
    ToCity();
    zeus::fullscreen::OnLoop();
    Check(!g_framing.on, "the city: not framed");
    Check(LogContains("zeus: fullscreen: installed"), "log: installed");
}

// The resolution setting changed on a front-end screen (the panel, Shift+F7): no mode change (the frame stays
// 1024x768), the framing follows the new scale at once (resolution_switch's listener), even without a redraw.
void FramedSetting()
{
    SetupFramed();
    zeus::fullscreen::AfterPicture("data\\Poseidon_FE_MainMenu.jpg", 112, 84);
    const int switches = g_res.switches, framings = g_framings;
    core_config c = *core_config_get();
    c.resolution = CORE_RESOLUTION_SCALED;
    c.scale_percent = 125;
    Check(zeus::resolution::SettingsChanged(&c), "the panel's setting is taken");
    Check(g_res.switches == switches, "front-end screen: no mode change");
    Check(g_framings > framings && g_framing.on && g_framing.scale > 1.24 && g_framing.scale < 1.26,
          "the framing follows at once: scale 1.25 (1920x1080 at 125%)");
    BYTE keys[256];
    GetKeyboardState(keys);
    keys[VK_SHIFT] = 0x80;
    SetKeyboardState(keys); // GetKeyState(VK_SHIFT) on this thread: down
    const int before = g_framings;
    Check(zeus::resolution::KeyFilter(WM_KEYDOWN, VK_F7, 0), "Shift+F7 is ours");
    Check(g_res.switches == switches, "Shift+F7 on a front-end screen: no mode change");
    Check(g_framings > before && g_framing.on, "... and the framing follows at once");
    keys[VK_SHIFT] = 0;
    SetKeyboardState(keys);
}

// A fault while the framing follows the setting (inside the resolution switching's own guard): the framing is off
// (the game's own frame, scaled), the resolution switching stays on and the setting is applied.
void FramedFault()
{
    SetupFramed();
    zeus::fullscreen::AfterPicture("data\\Poseidon_FE_MainMenu.jpg", 112, 84);
    Check(g_framing.on, "framed");
    g_fault = "set_framing";
    core_config c = *core_config_get();
    c.resolution = CORE_RESOLUTION_SCALED;
    c.scale_percent = 125;
    Check(zeus::resolution::SettingsChanged(&c), "the setting is applied all the same");
    Check(LogContains("feature \"framed full-screen pictures (core/display/framed_screens)\" switched off"),
          "log: framing off");
    Check(!LogContains("feature \"resolution switching"), "the resolution switching stays on");
    Check(!g_framing.on, "the game's own look: not framed");
    zeus::fullscreen::AfterPicture("data\\Zeus_FE_Registry.jpg", 112, 84);
    zeus::fullscreen::OnLoop();
    Check(!g_framing.on, "afterwards: never framed");
    ToCity();
    Check(At<int>(aModeW) == 1536 && At<int>(aModeH) == 864, "the city at the new setting (1920x1080 at 125%)");
}

// menu_screens = city: the screens before an episode and the editors show the panel pattern; the player's own picture
// everywhere.
void CityAroundScenario()
{
    using zeus::fullscreen::CityAround;
    Check(CityAround(0x00, false) && CityAround(0x2f, false) && CityAround(0x30, false) && CityAround(0x1f, false) &&
              CityAround(0x33, false) && CityAround(0x3a, false) && CityAround(0x22, false),
          "the city around: main menu, leader and game choice, high scores, victory, adventure screens");
    Check(!CityAround(0x3d, false) && !CityAround(0x35, false) && !CityAround(0x04, false),
          "the panel pattern: before an episode");
    Check(!CityAround(0x10, false) && !CityAround(0x58, false) && !CityAround(0x15, false) &&
              !CityAround(0x2d, false) && !CityAround(0x57, false) && !CityAround(0x46, false),
          "the panel pattern: the editors");
    Check(CityAround(0x35, true) && CityAround(0x3d, true) && CityAround(0x10, true),
          "the player's own picture on every screen");
    using zeus::fullscreen::CityAroundShown;
    At<int>(0x781894) = 0x57; // the drop-down menu 0x03 over the adventure editor (seen in testing)
    Check(!CityAroundShown(0x03, false), "the drop-down menu over the editor: the panel pattern (the screen beneath)");
    At<int>(0x781894) = 0x00;
    Check(CityAroundShown(0x03, false) && !CityAroundShown(0x57, false) && CityAroundShown(0x00, false),
          "... over the main menu: the city; other screens as they are");
}

// The city on screen is kept while playing and saved when a front-end screen comes up (and at the game's end).
void KeepCity()
{
    SetupFramed();
    static WORD screen[64 * 48];
    for (int i = 0; i < 64 * 48; ++i) {
        screen[i] = static_cast<WORD>(i);
    }
    ToCity();
    At<int>(aDdWidth) = 64;
    At<int>(aDdHeight2) = 48;
    At<int>(aPitch) = 64 * 2;
    At<DWORD>(aDrawBuffer) = reinterpret_cast<DWORD>(screen);
    At<int>(aCityLeft) = 0;
    At<int>(aCityTop) = 4;
    At<int>(aCityRight) = 50;
    At<int>(aCityBottom) = 60; // below the screen: cut at its height
    const int frames = core::display::framed::CityFramesDrawn();
    Timing<Render_fn>("city_render")(0, 0, 0); // through the real detour: a drawing that counts (no drawn-state copy)
    Check(core::display::framed::CityFramesDrawn() == frames + 1, "city_render's detour hands the city on");
    Check(g_cityPictures == 0, "kept, not saved while playing");
    At<unsigned char>(aGameScreen) = 0x0d; // the file dialog over the city, then a picture outside it
    zeus::resolution::OnBackground();
    zeus::fullscreen::AfterPicture("data\\zeus_load2.jpg", 112, 84);
    Check(g_cityPictures == 1, "a front-end screen: the city seen last saved");
    zeus::fullscreen::OnGameEnd();
    Check(g_cityPictures == 1, "the game's end: nothing new to save");
    ToCity();
    Timing<Render_fn>("city_render")(0, 0, 0); // within 3 s of the last one: not kept again
    zeus::fullscreen::OnGameEnd();
    Check(g_cityPictures == 1, "kept at most every 3 s");
    At<unsigned char>(aGameScreen) = 0x58; // the map editor: its map in the 1024x768 frame
    zeus::resolution::OnBackground();
    const int before = core::display::framed::CityFramesDrawn();
    Timing<Render_fn>("city_render")(0, 0, 0);
    Check(core::display::framed::CityFramesDrawn() == before, "a front-end screen (the map editor): not kept");
    ToCity(); // the adventure editor edits a city on the city screen 0x01 at our resolution (seen in testing)
    At<int>(0x665e2c) = 1; // editor_mode
    Timing<Render_fn>("city_render")(0, 0, 0);
    Check(core::display::framed::CityFramesDrawn() == before, "the city screen in the editor (editor_mode 1): not kept");
    At<int>(0x665e2c) = 0;
    zeus::fullscreen::OnGameEnd();
    Check(!LogContains("no city kept for the menus"), "a city was kept: no summary line");
}

// No city kept in the whole run although the city was drawn (here: editor_mode stays 1): one summary line with the
// count and the last reason at the next front-end picture, not repeated at the game's end.
void CitySkipped()
{
    SetupFramed();
    ToCity();
    At<int>(aCityLeft) = 0;
    At<int>(aCityTop) = 30;
    At<int>(aCityRight) = 1678;
    At<int>(aCityBottom) = 1080;
    At<int>(0x665e2c) = 1; // editor_mode not cleared
    Timing<Render_fn>("city_render")(0, 0, 0);
    Timing<Render_fn>("city_render")(0, 0, 0);
    Check(!LogContains("no city kept for the menus"), "while playing: nothing logged yet");
    At<unsigned char>(aGameScreen) = 0x00;
    zeus::resolution::OnBackground();
    zeus::fullscreen::AfterPicture("data\\Poseidon_FE_MainMenu.jpg", 112, 84);
    Check(LogContains("no city kept for the menus in this run: 2 city drawings skipped (last: editor_mode is not 0"),
          "the menu comes up: one line with the count and the reason");
    zeus::fullscreen::OnGameEnd();
    FILE *f = std::fopen("test_restore.log", "rb");
    static char buf[1 << 20];
    const size_t n = f ? std::fread(buf, 1, sizeof(buf) - 1, f) : 0;
    if (f) {
        std::fclose(f);
    }
    buf[n] = 0;
    int lines = 0;
    for (const char *p = buf; (p = std::strstr(p, "no city kept for the menus")) != nullptr; ++p) {
        ++lines;
    }
    Check(lines == 1, "the game's end: not written again");
    At<int>(0x665e2c) = 0;
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
    Check(LogContains("feature \"draw-only frames while the panel asks (core/sim/panel_pause)\" switched off"),
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

// ---------------------------------------------------------------------------------------------
// The intro movie (games/zeus/intro)

constexpr DWORD fBinkOpen = 0x417b00, cIntroCall = 0x5c3bab; // game_main's call returns to 0x5c3bb0

struct Bink {
    int opens = 0;
    void *player = nullptr;
    const char *file = nullptr;
    int x = 0, y = 0;
} g_bink;

char __fastcall FakeBinkOpen(void *player, void *, const char *file, int x, int y)
{
    ++g_bink.opens;
    g_bink.player = player;
    g_bink.file = file;
    g_bink.x = x;
    g_bink.y = y;
    return 1;
}

using BinkOpen_fn = char(__fastcall *)(void *, void *, const char *, int, int);

BinkOpen_fn IntroDetour()
{
    int n;
    const zeus::Detour *d = zeus::intro::Detours(&n);
    return reinterpret_cast<BinkOpen_fn>(d[0].detour);
}

// As game_main: push y, push x, push file, mov ecx, player, then the call at its own address (0x5c3bab) and a ret
// where it returns to (0x5c3bb0), so the detour sees game_main's return address.
char GameMainIntro(void *player, const char *file, int x, int y)
{
    BYTE *p = reinterpret_cast<BYTE *>(cIntroCall - 20 + g_imageShift);
    BYTE *q = p;
    const DWORD imm[4] = {static_cast<DWORD>(y), static_cast<DWORD>(x), reinterpret_cast<DWORD>(file),
                          reinterpret_cast<DWORD>(player)};
    for (int i = 0; i < 4; ++i) {
        *q++ = i < 3 ? 0x68 : 0xb9; // push imm32 / mov ecx, imm32
        *reinterpret_cast<DWORD *>(q) = imm[i];
        q += 4;
    }
    CallSite(cIntroCall, reinterpret_cast<const void *>(IntroDetour()));
    using Run_fn = char(__cdecl *)();
    return reinterpret_cast<Run_fn>(p)();
}

void SetupIntro(bool skip)
{
    Thunk(fBinkOpen, reinterpret_cast<const void *>(&FakeBinkOpen));
    zeus::intro::SkipForTest(skip);
}

void IntroSkip()
{
    SetupIntro(true);
    int player = 0;
    Check(GameMainIntro(&player, "Binks\\zeus_intro.bik", 112, 84) == 0 && g_bink.opens == 0,
          "skip: game_main's call does not open the movie (returns 0, as with no Bink player)");
    Check(LogContains("intro: the intro movie (Binks\\zeus_intro.bik) is skipped"), "log: skipped");
    Check(IntroDetour()(&player, nullptr, "Binks\\victory.bik", 3, 4) == 1 && g_bink.opens == 1 &&
              g_bink.player == &player && std::strcmp(g_bink.file, "Binks\\victory.bik") == 0 && g_bink.x == 3 &&
              g_bink.y == 4,
          "another caller: the game opens its movie (the player and the 3 arguments passed on)");
    zeus::intro::SkipForTest(false);
    Check(GameMainIntro(&player, "Binks\\zeus_intro.bik", 112, 84) == 1 && g_bink.opens == 2 && g_bink.x == 112 &&
              g_bink.y == 84,
          "play: game_main's call opens the movie");
}

void IntroFault()
{
    SetupIntro(true);
    int player = 0;
    // The file name is unreadable: logging it faults inside the guard.
    const char *bad = reinterpret_cast<const char *>(static_cast<DWORD>(16));
    Check(GameMainIntro(&player, bad, 112, 84) == 1 && g_bink.opens == 1 && g_bink.file == bad,
          "a fault: the game opens the movie as without the setting");
    Check(LogContains("feature \"intro movie setting (games/zeus/intro)\" switched off"), "log: intro off");
    Check(GameMainIntro(&player, "Binks\\zeus_intro.bik", 112, 84) == 1 && g_bink.opens == 2,
          "afterwards: the game's own way");
}

// --- The smoke test's actions (games/zeus/smoke). cheat_run is __cdecl (one argument [esp+4], returns al,
// plain ret; the caller cleans): the fake is the same.
int g_cheatArg = -1;
char __cdecl FakeCheatRun(int n)
{
    g_cheatArg = n;
    if (n == 0) { // Ambrosia
        At<char>(0xf2a6db) = 1;
        At<char>(0xf2a6da) = 1;
    }
    return 1;
}

void SetupCheats()
{
    const BYTE body[8] = {0x83, 0xf8, 0x64, 0x7d, 0x3d, 0x8b, 0x15, 0xf8};
    const BYTE ambrosia[8] = {0xb0, 0x01, 0xa2, 0xdb, 0xa6, 0xf2, 0x00, 0xa2};
    for (int i = 0; i < 8; ++i) {
        At<BYTE>(0x42c5a8 + i) = body[i];
        At<BYTE>(0x42c560 + i) = ambrosia[i];
    }
    Thunk(0x42c5a0, reinterpret_cast<const void *>(&FakeCheatRun)); // the first 5 bytes only
    At<char>(0x661510) = 1;
    At<DWORD>(0x5d8734) = 0x42c560;
    At<int>(0xfeb160) = -1;
    At<short>(0xfeb058) = 0x1234; // next to the two words lose writes
}

void SmokeActions()
{
    SetupCheats();
    char why[200] = "";
    At<unsigned char>(0xf2a6dd) = 0x00;
    Check(!zeus::smoke::Action("win", why, sizeof(why)) && std::strstr(why, "not on the city") && g_cheatArg == -1,
          "win off the city screen: refused, cheat_run not called");
    At<unsigned char>(0xf2a6dd) = 0x01;
    At<int>(0xfeaf28) = 1;
    Check(!zeus::smoke::Action("win", why, sizeof(why)) && std::strstr(why, "governed on") && g_cheatArg == -1,
          "win while governing on: refused");
    At<int>(0xfeaf28) = 0;
    Check(zeus::smoke::Action("win", why, sizeof(why)) && g_cheatArg == 0 && At<char>(0xf2a6db) == 1,
          "win in the city: cheat_run(0) (__cdecl), cheat_win set");
    At<char>(0xf2a6db) = 0;
    g_cheatArg = -1;
    At<BYTE>(0x42c5a8) = 0x90;
    Check(!zeus::smoke::Action("win", why, sizeof(why)) && g_cheatArg == -1, "cheat_run not as expected: not called");
    At<BYTE>(0x42c5a8) = 0x83;
    At<DWORD>(0x5d8734) = 0x42c561;
    Check(!zeus::smoke::Action("win", why, sizeof(why)) && g_cheatArg == -1,
          "the table's entry 0 not Ambrosia: not called");

    Check(zeus::smoke::Action("lose", why, sizeof(why)) && At<int>(0xfeb160) == 0xb91 &&
              At<short>(0xfeb054) == 0xb91 && At<short>(0xfeb056) == 0xb90 && At<short>(0xfeb058) == 0x1234,
          "lose: lose_reason 0xb91 (4 bytes), the two words 0xb91 / 0xb90, nothing after them");
    Check(!zeus::smoke::Action("lose", why, sizeof(why)) && std::strstr(why, "already lost"), "lose twice: refused");
    Check(!zeus::smoke::Action("jump", why, sizeof(why)), "an unknown action: refused");

    At<int>(0xf2a6ac) = 192;
    At<int>(0xf2a6a8) = 144;
    int x = 0, y = 0;
    zeus::smoke::DialogOffset(&x, &y);
    Check(x == 192 && y == 144, "the dialog offset (dialog_offset_x / _y)");
}

struct Scenario {
    const char *name;
    void (*run)();
};
// ---------------------------------------------------------------------------------------------
// The sidebar's filler, without the player's interface pack (the
// marble is not read: the 800x600 strip right of the sidebar, as when the pack cannot be read)

WORD PixelAt(int x, int y)
{
    return reinterpret_cast<WORD *>(At<BYTE *>(aDrawBuffer))[y * (At<int>(aPitch) / 2) + x];
}

int DrawsOf(int id, int x, int y)
{
    int n = 0;
    for (int i = 0; i < g_cityLayout.draws && i < kMaxDraws; ++i) {
        n += g_cityLayout.drawIds[i] == id && g_cityLayout.drawX[i] == x && g_cityLayout.drawY[i] == y;
    }
    return n;
}

void SidebarFiller()
{
    using Units_fn = void(__cdecl *)(int);
    SetupResolution();
    ToCity();
    static std::vector<WORD> buf(1920 * (1080 + 64));
    At<BYTE *>(aDrawBuffer) = reinterpret_cast<BYTE *>(buf.data());
    At<int>(aPitch) = 1920 * 2;
    At<int>(aDdWidth) = 1920;
    At<int>(aDdHeight2) = 1080;
    Res<Tick_fn>("ui_set_sidebar_position")(); // 1678
    Res<Tick_fn>("sidebar_draw")();
    const int x = 1678, pictureY = 1080 - 168;
    Check(g_cityLayout.sidebars == 1 && PixelAt(x + 50, pictureY) == 600 && PixelAt(x + 185, 1079) == 767,
          "1920x1080: the backdrop's rows 600..767 (frame, picture) moved to the bottom (912..1079)");
    Check(PixelAt(x, 600) == 640 && PixelAt(x + 10, 659) == 699 && PixelAt(x + 3, 660) == 640 &&
              PixelAt(x + 10, pictureY - 1) == 640 + (pictureY - 1 - 600) % 60,
          "the left edge (11 columns: the edge and its plain blue) carried on from the picture's rows 640..699");
    const int inset = 0x1e4 + 0x4000;
    Check(DrawsOf(inset + 0, x + 11, 600) == 1 && DrawsOf(inset + 6, x + 186 - 16, 600) == 1 &&
              DrawsOf(inset + 42, x + 11, pictureY - 16) == 1 && DrawsOf(inset + 48, x + 186 - 16, pictureY - 16) == 1,
          "the inset panel edge to edge: its four corners at (1689, 600), (1848, 600), (1689, 896), (1848, 896)");
    Check(DrawsOf(inset + 8, x + 11 + 16, 616) == 1 && DrawsOf(inset + 13, x + 186 - 16, 616) == 1,
          "an inner tile and a right edge tile where the game's inner_panel_draw would put them");
    const int strip = 0x4d2 + 0x4000;
    Check(DrawsOf(strip, 1864, 0) == 1 && DrawsOf(strip, 1912, 600) == 1,
          "without the pack: the 800x600 strip right of the sidebar (1864 .. 1920)");
    g_cityLayout.draws = 0;
    Res<Units_fn>("sidebar_draw_units")(0x48);
    Check(g_cityLayout.sidebars == 2 && DrawsOf(inset + 0, x + 11, 600) == 1, "the army's sidebar: the same filler");
    g_cityLayout.draws = 0;
    At<unsigned char>(aGameScreen) = 0x14; // the world map
    Res<Tick_fn>("sidebar_draw")();
    Check(g_cityLayout.draws == 0, "the world map: not filled (games/zeus/world_map lays it out)");
    g_frameRow = 614; // Poseidon's world map sidebar: the frame lower
    Res<Tick_fn>("world_map_sidebars")();
    Check(PixelAt(x + 50, pictureY) == 604 && PixelAt(x + 93, pictureY + 10) == 0 && PixelAt(x + 50, 1075) == 767 &&
              PixelAt(x + 50, 1079) == 767 && PixelAt(x + 2, 1077) == 471,
          "Poseidon's world map sidebar (frame at 614, 146 rows): rows from 604 moved, the frame 10 rows below the "
          "panel (one above the middle); the last row and the edge repeated below");
    g_frameRow = 610; // Poseidon's city sidebar: the same place, its picture 1 px right
    Res<Tick_fn>("world_map_sidebars")();
    Check(PixelAt(x + 50, pictureY) == 600 && PixelAt(x + 94, pictureY + 10) == 0 &&
              PixelAt(x + 93, pictureY + 10) == 610 && PixelAt(x + 11, pictureY + 10) == 610 &&
              PixelAt(x + 50, 1079) == 767,
          "Poseidon's city sidebar (frame at 610): rows from 600, the frame 10 rows below the panel, the picture 1 px "
          "right");
    g_frameRow = 604;
    g_cityLayout.draws = 0;
    At<unsigned char>(aGameScreen) = 0x01;
    At<int>(aModeH) = 768; // 1920x768: the picture stays, only the right of the sidebar
    At<int>(aDdHeight2) = 768;
    Res<Tick_fn>("sidebar_draw")();
    Check(PixelAt(x + 50, 600) == 600 && DrawsOf(inset + 0, x + 11, 600) == 0 && DrawsOf(strip, 1864, 0) == 1,
          "768 high: the picture where the game drew it, no inset panel; the strip on the right");
    At<int>(aModeH) = 790; // 22 rows below 768: no room for the panel's two edges, plain beside the edge
    At<int>(aDdHeight2) = 790;
    At<int>(0xf139b0) = 790; // the game's clip: the screen
    At<int>(0xf139b8) = 1920;
    g_cityLayout.draws = 0;
    Res<Tick_fn>("sidebar_draw")();
    Check(PixelAt(x + 50, 790 - 168) == 600 && DrawsOf(inset + 0, x + 11, 600) == 1 &&
              DrawsOf(inset + 48, x + 186 - 16, 622 - 16) == 1 && DrawsOf(strip, x + 11, 600) == 0,
          "790 high: the picture at the bottom, the inset panel squeezed into the 22 rows (its top and bottom edges)");
    Check(At<int>(0xf139b0) == 790 && At<int>(0xf139b8) == 1920 && At<int>(0xf139b4) == 0 && At<int>(0xf139bc) == 0,
          "the strip and the squeezed panel drawn inside narrowed clips, the game's clip back afterwards");
    At<unsigned char>(aGameScreen) = 0x14; // the world map's sidebar filled the same way on its own picture
    At<int>(aModeH) = 1080;
    At<int>(aDdHeight2) = 1080;
    g_cityLayout.draws = 0;
    Res<Tick_fn>("world_map_sidebars")();
    Check(g_cityLayout.worldSidebars == 3 && PixelAt(x + 50, pictureY) == 600 && PixelAt(x + 185, 1079) == 767,
          "the world map's sidebar: its rows 600..767 moved to the bottom");
    Check(PixelAt(x, 600) == 470 && PixelAt(x + 10, 659) == 529 && PixelAt(x + 10, 660) == 470 &&
              PixelAt(x + 50, 500) == 500,
          "... its gold edge (11 columns) carried on from its rows 470..529, which stay where the game drew them");
    Check(DrawsOf(inset + 0, x + 11, 600) == 1 && DrawsOf(inset + 48, x + 186 - 16, pictureY - 16) == 1 &&
              DrawsOf(strip, 1864, 0) == 1,
          "... the inset panel right of the edge, the city's marble (here the strip) right of the sidebar");
    At<unsigned char>(aGameScreen) = 0x01;
    g_cityLayout.draws = 0;
    g_fault = "image_draw";
    Res<Tick_fn>("sidebar_draw")();
    Check(LogContains("feature \"sidebar filler (games/zeus/resolution)\" switched off") &&
              LogContains("no longer filled after an error"),
          "a fault while filling: the filler off, logged");
    Check(At<int>(0xf139b0) == 790 && At<int>(0xf139b8) == 1920 && At<int>(0xf139bc) == 0,
          "the game's clip put back by the guard (the fault was inside the narrowed clip)");
    g_cityLayout.draws = 0;
    Res<Tick_fn>("sidebar_draw")();
    Check(g_cityLayout.draws == 0, "afterwards the game's sidebar only");
}

// ---------------------------------------------------------------------------------------------
// The pace's cap when the computer cannot keep up (core/sim/pace, Emperor's
// falling_behind): the frames slow down, at most 2 ticks are run per frame and the rest dropped; logged both ways.

void PaceBehind()
{
    At<DWORD>(aSimLastTick) = timeGetTime();
    for (int i = 0; i < 20; ++i) { // a frame every 10 ms
        CallDue();
        Sleep(10);
    }
    Check(!LogContains("falling behind"), "a frame every 10 ms: not behind");
    for (int i = 0; i < 25; ++i) { // a frame every 80 ms: 3 ticks owed per frame
        CallDue();
        Sleep(80);
    }
    Check(LogContains("zeus: falling behind (a frame every") && LogContains("at most 2 ticks per frame"),
          "a frame every 80 ms: falling behind, at most 2 ticks per frame (logged)");
    for (int i = 0; i < 40; ++i) { // fast again
        CallDue();
        Sleep(10);
    }
    Check(LogContains("zeus: no longer behind (a frame every") && !LogContains("ticks are made up again; 0 dropped"),
          "fast again: no longer behind, the ticks dropped meanwhile logged");
}

// ---------------------------------------------------------------------------------------------
// The building information window ([ui] info_window; core/ui/info_window, core/display/window_move)

constexpr DWORD aInfoX = 0xffa17c, aInfoY = 0xffa178, aInfoCols = 0xffa174, aInfoRows = 0xffa170,
                aInfoKind = 0xffa13c, aInfoBuilding = 0xffa158, aInfoPrev = 0xffa180, aMouseX = 0xf2a738,
                aMouseY = 0xf2a734;

struct InfoCalls {
    int places = 0, draws = 0, fores = 0, inputs = 0, buttons = 0;
    int inputMouseX = 0, inputMouseY = 0, buttonX = 0, buttonY = 0;
    BYTE *drawnInto = nullptr;
} g_info;

void SetBuilding(int id, int type)
{
    At<int>(aInfoKind) = 2;
    At<int>(aInfoBuilding) = id;
    At<short>(0x92e668 + id * 0x118) = static_cast<short>(type);
}

void __cdecl FakeInfoPlace() // the game's rule at heights other than 480 / 600 / 768, and its fixed places
{
    ++g_info.places;
    At<int>(aInfoX) = 8;
    At<int>(aInfoY) = 0x20;
    At<int>(aInfoCols) = 0x1d;
    At<int>(aInfoRows) = 0x14;
    if (At<int>(aInfoKind) == 2) {
        const int type = At<short>(0x92e668 + At<int>(aInfoBuilding) * 0x118);
        if (type == 0x39 || type == 0x3a || type == 0x3d || type == 0x3e) {
            At<int>(aInfoX) = 0;
            At<int>(aInfoY) = 0x20;
            At<int>(aInfoCols) = 0x1e;
        }
        else if (type == 0x7d) {
            At<int>(aInfoX) = 0;
            At<int>(aInfoY) = 0x2d;
        }
    }
}

// The fixed-place window's drawing: its rectangle filled with 0x1234 where the game has it, into draw_buffer.
void __cdecl FakeFixedDraw()
{
    ++g_info.draws;
    if (Fires("info_fixed_window_draw")) {
        Fault();
    }
    g_info.drawnInto = At<BYTE *>(aDrawBuffer);
    WORD *buf = reinterpret_cast<WORD *>(g_info.drawnInto);
    const int pitch = At<int>(aPitch) / 2;
    for (int y = At<int>(aInfoY); y < At<int>(aInfoY) + At<int>(aInfoRows) * 16; ++y) {
        for (int x = At<int>(aInfoX); x < At<int>(aInfoX) + At<int>(aInfoCols) * 16; ++x) {
            buf[y * pitch + x] = 0x1234;
        }
    }
}

void __cdecl FakeFixedFore()
{
    ++g_info.fores;
}

void __cdecl FakeInfoInput()
{
    ++g_info.inputs;
    g_info.inputMouseX = At<int>(aMouseX);
    g_info.inputMouseY = At<int>(aMouseY);
}

void __cdecl FakeButtonsDraw(int x, int y, void *, int)
{
    ++g_info.buttons;
    g_info.buttonX = x;
    g_info.buttonY = y;
}

void SetupInfo()
{
    SetupResolution();
    Thunk(0x5715a0, reinterpret_cast<void *>(&FakeInfoPlace));
    Thunk(0x5b2880, reinterpret_cast<void *>(&FakeFixedDraw));
    Thunk(0x5b38c0, reinterpret_cast<void *>(&FakeFixedFore));
    Thunk(0x5726e0, reinterpret_cast<void *>(&FakeInfoInput));
    Thunk(0x4316f0, reinterpret_cast<void *>(&FakeButtonsDraw));
    ToCity();
    Res<Tick_fn>("ui_set_sidebar_position")(); // 1678 at 1920x1080
    At<int>(aInfoKind) = 1;
    At<unsigned char>(aInfoPrev) = 0x01;
}

void InfoWindow()
{
    using core::ui::info_window::Mode;
    SetupInfo();
    static Tick_fn raw = Res<Tick_fn>("info_window_place");
    auto place = [] { // a window opened anew: a loop passed since the last one closed
        zeus::resolution::OnLoop();
        raw();
    };
    At<int>(aMouseX) = 300;
    At<int>(aMouseY) = 200;
    core::ui::info_window::Set(Mode::Original);
    place();
    const int shift = (1678 - 838) / 2;
    Check(At<int>(aInfoX) == 0xbc + shift && At<int>(aInfoY) == 1080 - 8 - 0x14 * 16,
          "original: the game's 768 rule at 1080, moved with the city area's centre (608, 752)");
    At<int>(aMouseY) = 700;
    place();
    Check(At<int>(aInfoY) == 0x20, "original, the cursor in the lower half: at the top (0x20)");
    core::ui::info_window::Set(Mode::Centered);
    place();
    Check(At<int>(aInfoX) == (1678 - 464) / 2 && At<int>(aInfoY) == 30 + (1080 - 30 - 320) / 2,
          "centered: in the middle of the city area (607, 395)");
    core::ui::info_window::Set(Mode::Cursor);
    At<int>(aMouseY) = 200;
    place();
    Check(At<int>(aInfoX) == 324 && At<int>(aInfoY) == 40, "cursor: right of the cursor, centred on it (324, 40)");
    At<int>(aMouseX) = 1500; // the trading post's arrow to the next one: placed again while the window is open
    At<int>(aMouseY) = 900;
    raw();
    Check(At<int>(aInfoX) == 324 && At<int>(aInfoY) == 40, "placed again while open (the next trading post): the same place");
    At<unsigned char>(aGameScreen) = 0x01; // the window closed: the next one is placed by the cursor again
    zeus::resolution::OnLoop();
    At<int>(aMouseX) = 300;
    At<int>(aMouseY) = 200;
    At<unsigned char>(aInfoPrev) = 0x14;
    place();
    Check(At<int>(aInfoX) == (1678 - 464) / 2, "opened on the world map: in the middle whatever the setting");
    At<unsigned char>(aInfoPrev) = 0x01;
    SetBuilding(7, 0x7d);
    place();
    Check(At<int>(aInfoX) == 324 && At<int>(aInfoY) == 40, "0x7d (the game's (0, 0x2d)): simply placed");
    SetBuilding(7, 0x39);
    place();
    Check(At<int>(aInfoX) == 0 && At<int>(aInfoY) == 0x20, "the granary: the game keeps (0, 0x20) (moved on screen)");
    At<int>(aModeW) = 1024; // the game's own size: the game's rule
    At<int>(aModeH) = 768;
    At<int>(aInfoKind) = 1;
    core::ui::info_window::Set(Mode::Original);
    place();
    Check(At<int>(aInfoX) == 8 && At<int>(aInfoY) == 0x20, "original at 1024x768: the game's own (the fake's 8, 0x20)");
    At<int>(aModeW) = 1920;
    At<int>(aModeH) = 1080;
    core::ui::info_window::Set(Mode::Cursor);
    DWORD old; // a fault inside our part (after the position was saved): the sidebar's page made unreadable
    VirtualProtect(reinterpret_cast<void *>(0x10df000 + g_imageShift), 0x1000, PAGE_NOACCESS, &old);
    place();
    VirtualProtect(reinterpret_cast<void *>(0x10df000 + g_imageShift), 0x1000, PAGE_EXECUTE_READWRITE, &old);
    Check(At<int>(aInfoX) == 8 && At<int>(aInfoY) == 0x20, "fault: the game's own position put back (8, 32)");
    place();
    Check(At<int>(aInfoX) == 8 && LogContains("feature \"info window position (games/zeus/resolution)\" switched off") &&
              LogContains("keeps the game's own position after an error"),
          "afterwards the game's own position, logged");
}

// The granary's window (the game keeps (0, 0x20)) shown beside the cursor: drawn into a copy of the screen and
// composed there; the mouse in the game's place for the input; the help / close buttons drawn shifted; a fault: the
// game's own place.
void FixedWindow()
{
    using core::ui::info_window::Mode;
    SetupInfo();
    static std::vector<WORD> buf(1920 * (1080 + 64));
    At<BYTE *>(aDrawBuffer) = reinterpret_cast<BYTE *>(buf.data());
    At<int>(aPitch) = 1920 * 2;
    At<int>(aDdWidth) = 1920;
    At<int>(aDdHeight2) = 1080;
    core::ui::info_window::Set(Mode::Cursor);
    SetBuilding(7, 0x39);
    At<int>(aMouseX) = 600;
    At<int>(aMouseY) = 500;
    Res<Tick_fn>("info_window_place")();
    At<unsigned char>(aGameScreen) = 0x09;
    const int w = 0x1e * 16, h = 0x14 * 16, x = 600 + 24, y = 500 - h / 2; // 480 x 320 at (624, 340)
    Res<Tick_fn>("info_fixed_window_draw")();
    Check(g_info.draws == 1 && g_info.drawnInto != reinterpret_cast<BYTE *>(buf.data()) &&
              At<BYTE *>(aDrawBuffer) == reinterpret_cast<BYTE *>(buf.data()),
          "drawn into a copy of the screen; draw_buffer back afterwards");
    Check(buf[y * 1920 + x] == 0x1234 && buf[(y + h - 1) * 1920 + x + w - 1] == 0x1234 && buf[0x20 * 1920 + 0] != 0x1234,
          "shown beside the cursor (624, 340); nothing at the game's own place (0, 0x20)");
    Check(At<int>(aMouseX) == 600 && At<int>(aMouseY) == 500, "the mouse back afterwards");
    Res<Tick_fn>("info_fixed_window_foreground")();
    Check(g_info.fores == 1, "the foreground through the same move");
    Res<Tick_fn>("info_window_input")();
    Check(g_info.inputMouseX == 600 - x && g_info.inputMouseY == 500 - (y - 0x20) && At<int>(aMouseX) == 600,
          "the input sees the mouse in the game's own place of the window");
    // screen_draw's buttons call returning to 0x514214: a stub at 0x5141f0 pushes (x 0, y 0x20 + h - 0x28, table, 2).
    BYTE *stub = reinterpret_cast<BYTE *>(0x5141f0 + g_imageShift);
    const DWORD args[4] = {2, 0x5ef708, static_cast<DWORD>(0x20 + h - 0x28), 0}; // pushed: count, table, y, x
    for (int i = 0; i < 4; ++i) {
        stub[i * 5] = 0x68;
        *reinterpret_cast<DWORD *>(stub + i * 5 + 1) = args[i];
    }
    for (int i = 20; i < 0x1f; ++i) {
        stub[i] = 0x90;
    }
    using Buttons_fn = void(__cdecl *)(int, int, void *, int);
    BYTE *call = reinterpret_cast<BYTE *>(0x51420f + g_imageShift);
    call[0] = 0xe8;
    *reinterpret_cast<DWORD *>(call + 1) =
        reinterpret_cast<DWORD>(Res<Buttons_fn>("buttons_draw")) - reinterpret_cast<DWORD>(call + 5);
    call[5] = 0x83; // add esp, 0x10
    call[6] = 0xc4;
    call[7] = 0x10;
    call[8] = 0xc3;
    reinterpret_cast<Tick_fn>(stub)();
    Check(g_info.buttonX == x && g_info.buttonY == y + h - 0x28, "the window's buttons drawn with it");
    Res<Buttons_fn>("buttons_draw")(10, 20, nullptr, 2);
    Check(g_info.buttonX == 10 && g_info.buttonY == 20, "buttons_draw from anywhere else: as the game draws them");
    core::ui::info_window::Set(Mode::Original); // Emperor's fixed_original: original keeps the game's place
    At<unsigned char>(aGameScreen) = 0x01;
    zeus::resolution::OnLoop();
    Res<Tick_fn>("info_window_place")();
    At<unsigned char>(aGameScreen) = 0x09;
    std::fill(buf.begin(), buf.end(), 0);
    Res<Tick_fn>("info_fixed_window_draw")();
    Check(g_info.drawnInto == reinterpret_cast<BYTE *>(buf.data()) && buf[0x20 * 1920] == 0x1234 &&
              buf[y * 1920 + x] != 0x1234,
          "original: drawn where the game has it (0, 0x20), not moved");
    core::ui::info_window::Set(Mode::Cursor);
    At<unsigned char>(aGameScreen) = 0x01;
    zeus::resolution::OnLoop();
    Res<Tick_fn>("info_window_place")();
    At<unsigned char>(aGameScreen) = 0x09;
    g_fault = "info_fixed_window_draw";
    Res<Tick_fn>("info_fixed_window_draw")();
    Check(At<BYTE *>(aDrawBuffer) == reinterpret_cast<BYTE *>(buf.data()) && At<int>(aMouseX) == 600 &&
              At<int>(aMouseY) == 500 && LogContains("feature \"info window position (games/zeus/resolution)\" switched off"),
          "a fault while drawing: draw_buffer and the mouse put back, off");
    Res<Tick_fn>("info_fixed_window_draw")();
    Check(g_info.drawnInto == reinterpret_cast<BYTE *>(buf.data()) && buf[0x20 * 1920] == 0x1234,
          "afterwards drawn where the game has it");
}

// ---------------------------------------------------------------------------------------------
// The world map's layout (games/zeus/world_map with the core's core/display/map_screen), without the player's
// interface pack (no fret border: the map alone in the middle)

constexpr DWORD aOriginX = 0x5fb7b4, aOriginY = 0x5fb7b8, aScrollX = 0x8f9838, aScrollY = 0x8f9834, aMapW = 0x5fb7ac,
                aMapH = 0x5fb7b0, aMapNumber = 0x90311c, aMouseXw = 0xf2a738, aMouseYw = 0xf2a734,
                aEditorMode = 0x665e2c;
constexpr WORD kSentinel16 = 0xdead;

WORD MapPattern(int x, int y) // the map's pixel (x, y): never the sentinel, never 0
{
    return static_cast<WORD>(0x4000 + ((x * 7 + y * 13) & 0x3fff));
}

struct MapCalls {
    int draws = 0, tops = 0, hits = 0, campaign = 0, clicks = 0;
    BYTE *drawBuffer = nullptr, *topBuffer = nullptr;
    int drawHeight = 0, mouseX = 0, mouseY = 0;
    bool noSidebar = false; // the campaign map's drawing draws nothing (its adventure record missing)
} g_wm;
using Top_fn = int(__cdecl *)(int);
using Hit_fn = int(__cdecl *)(void *, void *);
Top_fn g_topHook = nullptr; // the detour world_map_draw calls (frame_top_panel's)

// world_map_draw: loads the map (838 x 738), the origin (0, 30), scroll 0; the picture into draw_buffer at (0, 30),
// as high as dd_height allows; frame_top_panel(1).
void __cdecl FakeWorldMap()
{
    ++g_wm.draws;
    g_wm.drawBuffer = At<BYTE *>(aDrawBuffer);
    g_wm.drawHeight = At<int>(aDdHeight2);
    At<int>(aMapW) = 838;
    At<int>(aMapH) = 738;
    At<int>(aOriginX) = 0;
    At<int>(aOriginY) = 30;
    if (Fires("world_map_draw")) {
        Fault();
    }
    WORD *buf = reinterpret_cast<WORD *>(g_wm.drawBuffer);
    const int pitch = At<int>(aPitch) / 2, rows = g_wm.drawHeight - 30 < 738 ? g_wm.drawHeight - 30 : 738;
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < 838; ++x) {
            buf[(y + 30) * pitch + x] = MapPattern(x, y);
        }
    }
    At<int>(aScrollX) = -1; // as world_map_draw leaves it for a map that fills its box (0x599ec1)
    g_topHook(1);
}

int __cdecl FakeTopPanel(int)
{
    ++g_wm.tops;
    g_wm.topBuffer = At<BYTE *>(aDrawBuffer);
    return 0;
}

int __cdecl FakeHit(void *, void *)
{
    ++g_wm.hits;
    g_wm.mouseX = At<int>(aMouseXw);
    g_wm.mouseY = At<int>(aMouseYw);
    return 7;
}

// The campaign map's frame: its sidebar (rows numbered, a pixel 0x0002 at sidebar_x + 5, 5) and a marker at the
// map's (100, 100).
void __cdecl FakeCampaignDraw()
{
    ++g_wm.campaign;
    g_wm.mouseX = At<int>(aMouseXw);
    g_wm.mouseY = At<int>(aMouseYw);
    if (g_wm.noSidebar) {
        return;
    }
    WORD *buf = reinterpret_cast<WORD *>(At<BYTE *>(aDrawBuffer));
    const int pitch = At<int>(aPitch) / 2, x0 = At<int>(aSidebarX);
    PaintBackdrop();
    buf[(At<int>(aOriginY) + 100) * pitch + At<int>(aOriginX) + 100] = 0x0001;
    buf[5 * pitch + x0 + 5] = 0x0002;
}

void __cdecl FakeCampaignClick()
{
    ++g_wm.clicks;
    g_wm.mouseX = At<int>(aMouseXw);
    g_wm.mouseY = At<int>(aMouseYw);
}

std::vector<WORD> g_wmScreen;

template <typename F> F Map(const char *name)
{
    int n;
    const zeus::Detour *d = zeus::world_map::Detours(&n);
    return Detour<F>(d, n, name);
}

void __cdecl FakeCampaignBack()
{
    Map<Tick_fn>("world_map_draw")();
}

void WorldScreen(int w, int h)
{
    g_wmScreen.assign(static_cast<size_t>(w) * (h + 64), kSentinel16);
    At<BYTE *>(aDrawBuffer) = reinterpret_cast<BYTE *>(g_wmScreen.data());
    At<int>(aPitch) = w * 2;
    At<int>(aDdWidth) = w;
    At<int>(aDdHeight2) = h;
    At<int>(aModeW) = w;
    At<int>(aModeH) = h;
    At<int>(aSidebarX) = 60 * ((w - 186 + 2) / 60) - 2;
    At<int>(aScrollX) = 0;
    At<int>(aScrollY) = 0;
    At<int>(aMapNumber) = 1;
    At<int>(aEditorMode) = 0;
    At<unsigned char>(aGameScreen) = 0x14;
    At<int>(0xf139b0) = h; // the game's clip: the screen
    At<int>(0xf139b8) = w;
    g_wm = MapCalls{};
}

WORD Wm(int x, int y)
{
    return g_wmScreen[static_cast<size_t>(y) * At<int>(aDdWidth) + x];
}

void WorldMap()
{
    namespace ms = core::display::map_screen;
    SetupResolution();
    ToCity();
    Thunk(0x599da0, reinterpret_cast<void *>(&FakeWorldMap));
    Thunk(0x58eb20, reinterpret_cast<void *>(&FakeTopPanel));
    Thunk(0x407360, reinterpret_cast<void *>(&FakeHit));
    Thunk(0x421c70, reinterpret_cast<void *>(&FakeCampaignDraw));
    Thunk(0x421af0, reinterpret_cast<void *>(&FakeCampaignClick));
    Thunk(0x422d40, reinterpret_cast<void *>(&FakeCampaignBack));
    g_topHook = Map<Top_fn>("frame_top_panel");
    auto draw = Map<Tick_fn>("world_map_draw");
    auto hit = Map<Hit_fn>("world_map_hit");
    zeus::world_map::LayoutForTest(true);

    // 2560x1440, sidebar at 2338: the map area is 2338 x 1410 below the top bar. The first frame after the city with
    // background = city: the city left in draw_buffer stays around the map; native: the map in the middle.
    WorldScreen(2560, 1440);
    for (WORD &p : g_wmScreen) {
        p = 0x7777; // the city
    }
    ms::SetModeForTest(1, 0);
    ms::SetBackgroundForTest(true);
    At<int>(aMapW) = 0; // no map loaded yet: the drawing loads it
    At<int>(aMapH) = 0;
    draw();
    const int nx = (2338 - 838) / 2, ny = 30 + (1410 - 738) / 2; // 750, 366
    Check(g_wm.draws == 1 && g_wm.drawBuffer != At<BYTE *>(aDrawBuffer) && g_wm.drawHeight == 768,
          "the first map: drawn into our layer with the game's own height 768");
    Check(At<int>(aDdHeight2) == 1440 && At<BYTE *>(aDrawBuffer) == reinterpret_cast<BYTE *>(g_wmScreen.data()),
          "dd_height and draw_buffer back afterwards");
    Check(Wm(nx, ny) == MapPattern(0, 0) && Wm(nx + 837, ny + 737) == MapPattern(837, 737) &&
              Wm(nx + 300, ny + 200) == MapPattern(300, 200),
          "native: the map pixel for pixel in the middle of the map area (750, 366)");
    Check(Wm(20, 700) == 0x7777 && Wm(2337, 1439) == 0x7777, "background city: the city around the map");
    Check(Wm(2400, 700) == 0x7777 && Wm(100, 10) == 0x7777, "the sidebar and the top bar are not touched");
    Check(g_wm.tops == 1 && g_wm.topBuffer == At<BYTE *>(aDrawBuffer),
          "frame_top_panel: once, after the map, on the screen (not into the layer)");
    At<int>(aMouseXw) = nx + 300;
    At<int>(aMouseYw) = ny + 200;
    Check(hit(nullptr, nullptr) == 7 && g_wm.mouseX == 300 && g_wm.mouseY == 230,
          "world_map_hit: the mouse in the game's map coordinates, its result passed on");
    Check(At<int>(aMouseXw) == nx + 300 && At<int>(aMouseYw) == ny + 200, "and the real position afterwards");
    At<int>(aMouseXw) = 100;
    At<int>(aMouseYw) = 700;
    hit(nullptr, nullptr);
    Check(g_wm.mouseX == -10000 && g_wm.mouseY == -10000, "mouse beside the map: a point where nothing is");
    At<int>(aMouseXw) = 2400;
    At<int>(aMouseYw) = 100;
    hit(nullptr, nullptr);
    Check(g_wm.mouseX == 2400 && g_wm.mouseY == 100, "mouse on the sidebar: the real position");

    // Background map: the same map blurred around it.
    WorldScreen(2560, 1440);
    ms::SetBackgroundForTest(false);
    draw();
    Check(Wm(20, 700) != kSentinel16 && Wm(20, 700) != 0x7777 && Wm(nx + 300, ny + 200) == MapPattern(300, 200),
          "background map: the blurred map around it");
    // Scaled, fit at 2560x1440 (1.91): the mouse follows the scale.
    ms::SetModeForTest(0, 0);
    WorldScreen(2560, 1440);
    draw();
    const double s = 1410.0 / 738;
    const int tw = static_cast<int>(838 * s + 0.5), tx = (2338 - tw) / 2, ty = 30;
    At<int>(aMouseXw) = tx + static_cast<int>((419 + 0.5) * s);
    At<int>(aMouseYw) = ty + static_cast<int>((369 + 0.5) * s);
    hit(nullptr, nullptr);
    Check(g_wm.mouseX == 419 && g_wm.mouseY == 369 + 30, "scaled (fit): the middle of the screen's map is the map's");
    // 200% at 3840x2160 (fit 288%): each map pixel a 2 x 2 block.
    ms::SetModeForTest(0, 200);
    WorldScreen(3840, 2160); // sidebar 3598: area 3598 x 2130
    draw();
    const int ix = (3598 - 1676) / 2, iy = 30 + (2130 - 1476) / 2;
    Check(Wm(ix + 600, iy + 400) == MapPattern(300, 200) && Wm(ix + 601, iy + 401) == MapPattern(300, 200),
          "scale 200% at 3840x2160: each map pixel a 2 x 2 block");

    // The game's own sizes and the editor's map: the game's own drawing.
    ms::SetModeForTest(1, 0);
    WorldScreen(1024, 768);
    draw();
    Check(g_wm.drawBuffer == At<BYTE *>(aDrawBuffer) && Wm(5, 35) == MapPattern(5, 5) && g_wm.drawHeight == 768,
          "1024x768: the game's own drawing, straight to the screen");
    WorldScreen(2560, 1440);
    At<int>(aEditorMode) = 1;
    draw();
    Check(g_wm.drawBuffer == At<BYTE *>(aDrawBuffer) && g_wm.drawHeight == 768 && At<int>(aDdHeight2) == 1440,
          "the editors: the game's own drawing, still with the game's own height");
    At<int>(aEditorMode) = 0;
    At<int>(aMouseXw) = 100;
    At<int>(aMouseYw) = 700;
    hit(nullptr, nullptr);
    Check(g_wm.mouseX == 100, "... and the real mouse");

    // The campaign map (0x34): the background on a redraw (always the blurred map), the top bar tiled; each frame the
    // markers into the layer and shown on the map, the sidebar's strip on the screen, then the filler below it.
    WorldScreen(2560, 1440);
    for (WORD &p : g_wmScreen) {
        p = 0x7777; // the victory screen left behind
    }
    ms::SetBackgroundForTest(true);
    At<unsigned char>(aGameScreen) = 0x34;
    g_cityLayout.draws = 0;
    Map<Tick_fn>("campaign_map_background")();
    Check(Wm(20, 700) != 0x7777 && Wm(nx + 300, ny + 200) == MapPattern(300, 200),
          "campaign map: the map in the middle over the blurred map, whatever the background setting");
    Check(DrawsOf(0x4cf + 0x4000, 2338 - 838, 0) == 1, "campaign map wider than 1024: the top bar ending at the sidebar");
    At<int>(aMouseXw) = nx + 300;
    At<int>(aMouseYw) = ny + 200;
    g_cityLayout.draws = 0;
    Map<Tick_fn>("campaign_map_draw")();
    Check(g_wm.campaign == 1 && g_wm.mouseX == 300 && g_wm.mouseY == 230, "campaign map: hover in map coordinates");
    Check(Wm(nx + 100, ny + 100) == 0x0001, "campaign map: the marker shown on the map");
    Check(Wm(2338 + 5, 5) == 0x0002, "campaign map: the sidebar drawn where it belongs");
    const int inset = 0x1e4 + 0x4000;
    Check(DrawsOf(inset + 0, 2338 + 11, 600) == 1 && Wm(2338 + 50, 1440 - 168) == 600,
          "campaign map: the sidebar's filler below it (the gold edge's width, the picture at the bottom)");
    const WORD below = Wm(2338 + 93, 700);
    g_wm.noSidebar = true;
    g_cityLayout.draws = 0;
    Map<Tick_fn>("campaign_map_draw")();
    Check(DrawsOf(inset + 0, 2338 + 11, 600) == 0 && Wm(2338 + 93, 700) == below && Wm(2338 + 50, 1440 - 168) == 600,
          "a frame that does not draw the sidebar: no filler (nothing moved twice), the screen as it was");
    g_wm.noSidebar = false;
    Map<Tick_fn>("campaign_map_click")();
    Check(g_wm.clicks == 1 && g_wm.mouseX == 300 && g_wm.mouseY == 230, "campaign map: the click in map coordinates");

    // A fault while the game draws into our layer: off, the game's own drawing again, the height still the game's.
    At<unsigned char>(aGameScreen) = 0x14;
    WorldScreen(2560, 1440);
    g_fault = "world_map_draw";
    draw();
    Check(g_wm.draws == 2 && g_wm.drawBuffer == At<BYTE *>(aDrawBuffer) && Wm(5, 35) == MapPattern(5, 5) &&
              At<int>(aDdHeight2) == 1440 && g_wm.drawHeight == 768,
          "fault: drawn again the game's way (top left, 768 high), dd_height back");
    Check(LogContains("feature \"world map layout (games/zeus/world_map)\" switched off"),
          "fault: the world map layout is switched off");
    At<int>(aMouseXw) = 100;
    At<int>(aMouseYw) = 700;
    hit(nullptr, nullptr);
    Check(g_wm.mouseX == 100, "fault: the real mouse from then on");
}

// ---------------------------------------------------------------------------------------------
// Every logical size the display scale gives: the common screens
// at each scale through the core's ChooseLogicalSize, sizes a window gives, fixed sizes. At each: the sidebar inside
// the screen, the city viewport inside the game's view grid and down to the bottom, the sidebar's filler inside the
// screen with its picture at the bottom, the information window inside the city area, the world map in the middle
// of its area and its middle clicked as the map's.

struct SizeCase {
    int w, h;
    char what[48];
};

std::vector<SizeCase> DisplayScaleSizes()
{
    std::vector<SizeCase> v;
    auto add = [&](int w, int h, const char *what) {
        for (const SizeCase &c : v) {
            if (c.w == w && c.h == h) {
                return;
            }
        }
        SizeCase c{w, h, {}};
        std::snprintf(c.what, sizeof(c.what), "%s", what);
        v.push_back(c);
    };
    const int screens[][2] = {{1920, 1080}, {1920, 1200}, {2560, 1440}, {2560, 1600}, {3440, 1440}, {3840, 2160}};
    const int scales[] = {0, 100, 125, 150, 175, 200, 250};
    for (const auto &s : screens) {
        for (int p : scales) {
            const core::display::LogicalSize l =
                core::display::ChooseLogicalSize(s[0], s[1], CORE_RESOLUTION_SCALED, 0, 0, p, 1024, 768);
            char what[48];
            std::snprintf(what, sizeof(what), "%dx%d at %s%d%%", s[0], s[1], p ? "" : "auto ", l.scale_percent);
            add(l.w, l.h, what);
        }
    }
    // Windows (the client area divided by the scale, even widths) and fixed sizes.
    add(1024, 900, "a window at the narrowest");
    add(1024, 1081, "a window at the narrowest, uneven height");
    add(2004, 768, "a window at the lowest");
    add(2064, 1081, "a window, uneven height");
    add(1366, 768, "fixed 1366x768");
    add(1280, 800, "fixed 1280x800");
    return v;
}

void DisplayScaleSizesScenario()
{
    using core::ui::info_window::Mode;
    namespace ms = core::display::map_screen;
    const long logFrom = LogSize();
    SetupInfo();
    Thunk(0x599da0, reinterpret_cast<void *>(&FakeWorldMap));
    Thunk(0x58eb20, reinterpret_cast<void *>(&FakeTopPanel));
    Thunk(0x407360, reinterpret_cast<void *>(&FakeHit));
    g_topHook = Map<Top_fn>("frame_top_panel");
    auto drawMap = Map<Tick_fn>("world_map_draw");
    auto hit = Map<Hit_fn>("world_map_hit");
    zeus::world_map::LayoutForTest(true);
    static Tick_fn place = Res<Tick_fn>("info_window_place");
    const int inset = 0x1e4 + 0x4000;
    const std::vector<SizeCase> sizes = DisplayScaleSizes();
    int checked = 0;
    for (const SizeCase &c : sizes) {
        const int w = c.w, h = c.h;
        char what[200];
        auto check = [&](bool ok, const char *part) {
            std::snprintf(what, sizeof(what), "%dx%d (%s): %s", w, h, c.what, part);
            Check(ok, what);
        };
        // The city screen at this size.
        At<unsigned char>(aGameScreen) = 0x01;
        At<int>(aModeW) = w;
        At<int>(aModeH) = h;
        std::vector<WORD> buf(static_cast<size_t>(w) * (h + 64), 0);
        At<BYTE *>(aDrawBuffer) = reinterpret_cast<BYTE *>(buf.data());
        At<int>(aPitch) = w * 2;
        At<int>(aDdWidth) = w;
        At<int>(aDdHeight2) = h;
        At<int>(0xf139b0) = h; // the game's clip: the screen
        At<int>(0xf139b4) = 0;
        At<int>(0xf139b8) = w;
        At<int>(0xf139bc) = 0;
        Res<Tick_fn>("ui_set_sidebar_position")();
        const int sx = At<int>(aSidebarX);
        check(sx + 186 <= w && (w <= 1024 ? sx == 838 : w - (sx + 186) < 60 && sx >= 838),
              "the sidebar inside the screen, less than 60 px right of it");
        Res<Tick_fn>("city_viewport_narrow")();
        check(g_res.cols >= 14 && g_res.cols <= 220 && g_res.rows <= 427 && (g_res.cols + 0) * 60 <= sx + 2 &&
                  At<int>(aClipBottom) >= h && At<int>(aClipBottom) < h + 15,
              "the viewport left of the sidebar, inside the view grid, down to the bottom (within one half row)");
        Res<Tick_fn>("city_viewport_set_640")();
        check(g_res.cols == w / 60 && g_res.cols <= 220 && g_res.rows <= 427 && At<int>(aClipBottom) >= h,
              "the viewport under the whole width (sidebar closed)");
        // The sidebar's filler (without the pack: the strip right of the sidebar).
        g_cityLayout.draws = 0;
        Res<Tick_fn>("sidebar_draw")();
        const int pictureY = h > 768 ? h - 168 : 600;
        check(PixelAt(sx + 50, pictureY) == 600 && PixelAt(sx + 185, h - 1) == 767,
              "the sidebar's picture at the bottom (its rows 600..767)");
        bool inside = g_cityLayout.draws < kMaxDraws;
        int insets = 0;
        // Fewer than 32 rows between the sidebar's buttons and its picture: the inset panel squeezed (a 32-row panel
        // from 600 and one ending at the picture, each drawn inside a clip of half the rows; sidebar_filler checks the
        // clips), so its tiles lie within 600 .. 632 and picture - 32 .. picture.
        const bool squeezed = pictureY - 600 < 32;
        for (int i = 0; i < g_cityLayout.draws && i < kMaxDraws; ++i) {
            const int id = g_cityLayout.drawIds[i], x = g_cityLayout.drawX[i], y = g_cityLayout.drawY[i];
            bool ok = x < w && y < h && x >= 0 && y >= 0;
            if (id >= inset && id < inset + 49) {
                ++insets;
                ok = ok && x >= sx && x + 16 <= sx + 186 &&
                     (squeezed ? y >= pictureY - 32 && y + 16 <= 600 + 32 : y >= 600 && y + 16 <= pictureY);
            }
            if (!ok && inside) {
                std::printf("    draw %d of %d: id %d at (%d, %d)\n", i, g_cityLayout.draws, id, x, y);
            }
            inside = inside && ok;
        }
        check(inside && (h > 768) == (insets > 0), "every filler draw inside the screen, the inset panel above the picture");
        check(At<int>(0xf139b0) == h && At<int>(0xf139b8) == w && At<int>(0xf139b4) == 0 && At<int>(0xf139bc) == 0,
              "the game's clip back after the filler");
        // The information window: each setting, the cursor at the city area's four corners.
        const int corners[][2] = {{5, 35}, {sx - 5, 35}, {5, h - 5}, {sx - 5, h - 5}};
        const Mode modes[] = {Mode::Cursor, Mode::Centered, Mode::Original};
        bool infoInside = true;
        for (Mode m : modes) {
            core::ui::info_window::Set(m);
            for (const auto &p : corners) {
                At<unsigned char>(aGameScreen) = 0x01; // the last window closed: placed anew
                zeus::resolution::OnLoop();
                At<int>(aMouseX) = p[0];
                At<int>(aMouseY) = p[1];
                At<int>(aInfoKind) = 1;
                place();
                const int x = At<int>(aInfoX), y = At<int>(aInfoY), ww = At<int>(aInfoCols) * 16,
                          wh = At<int>(aInfoRows) * 16;
                infoInside = infoInside && x >= 0 && y >= 30 && x + ww <= sx && y + wh <= h;
            }
        }
        check(infoInside, "the information window inside the city area (cursor, centered, original; four corners)");
        core::ui::info_window::Set(Mode::Cursor);
        // The world map: native, over the blurred map.
        ms::SetModeForTest(1, 0);
        ms::SetBackgroundForTest(false);
        WorldScreen(w, h);
        drawMap();
        const int nx = (sx - 838) / 2, ny = 30 + (h - 30 - 738) / 2;
        check(At<int>(aSidebarX) == sx && g_wm.drawBuffer != At<BYTE *>(aDrawBuffer) && Wm(nx, ny) == MapPattern(0, 0) &&
                  Wm(nx + 837, ny + 737) == MapPattern(837, 737),
              "the world map taken over, in the middle of the map area");
        At<int>(aMouseXw) = nx + 419;
        At<int>(aMouseYw) = ny + 369;
        hit(nullptr, nullptr);
        check(g_wm.mouseX == 419 && g_wm.mouseY == 369 + 30, "the world map's middle clicked: the map's middle");
        // Scaled fit: the middle still the map's.
        ms::SetModeForTest(0, 0);
        WorldScreen(w, h);
        drawMap();
        const double fit = std::min(sx / 838.0, (h - 30.0) / 738); // map_screen's fit without a fret border
        const double s = fit < 1.0 ? 1.0 : fit;
        const int tw = static_cast<int>(838 * s + 0.5), th = static_cast<int>(738 * s + 0.5);
        const int tx = (sx - tw) / 2, ty = 30 + (h - 30 - th) / 2;
        At<int>(aMouseXw) = tx + static_cast<int>((419 + 0.5) * s);
        At<int>(aMouseYw) = ty + static_cast<int>((369 + 0.5) * s);
        hit(nullptr, nullptr);
        check(g_wm.mouseX >= 418 && g_wm.mouseX <= 420 && g_wm.mouseY >= 398 && g_wm.mouseY <= 400,
              "scaled (fit): the middle of the screen's map is the map's");
        ++checked;
    }
    char line[96];
    std::snprintf(line, sizeof(line), "%d sizes checked", checked);
    Check(checked == static_cast<int>(sizes.size()) && checked >= 30, line);
    Check(!LogContainsSince(logFrom, "switched off"), "no feature switched off at any size");
}

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
    {"intro_skip", &IntroSkip},
    {"intro_fault", &IntroFault},
    {"framed_screens", &FramedScreens},
    {"framed_setting", &FramedSetting},
    {"framed_fault", &FramedFault},
    {"city_around", &CityAroundScenario},
    {"keep_city", &KeepCity},
    {"switch_fault", &SwitchFault},
    {"city_skipped", &CitySkipped},
    {"spare_rows", &SpareRows},
    {"top_bar_tiled", &TopBarTiled},
    {"message_panel", &MessagePanel},
    {"sidebar_filler", &SidebarFiller},
    {"pace_behind", &PaceBehind},
    {"info_window", &InfoWindow},
    {"fixed_window", &FixedWindow},
    {"world_map", &WorldMap},
    {"display_scale_sizes", &DisplayScaleSizesScenario},
    {"smoke_actions", &SmokeActions},
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
