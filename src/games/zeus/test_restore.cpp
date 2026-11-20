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
//   mode_size_fault     a fault after mode 3's size was written: 1024x768 back (logged: 2 values put back),
//                           switching off, settings refused
//   viewport_layout              the sidebar (where the open-sidebar columns end) and both city viewports at 1920x1080 and
//                           1024x900; the game's own at 1024x768
//   layout_fault        viewport_init faults: the 1024 layout at once, the sidebar at the game's 838, the game's
//                           own size on the next loop
//   narrow_fault        the same with the sidebar open (city_viewport_narrow): the 1024 layout's 14 x 50
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
//   intro_mismatch      Attach (a smoke test asks for the skip) where the sites' bytes are not the game's: nothing
//                           installed, the log says which site, the movie plays
//
// The game's 3D sounds (games/zeus/sound):
//   sound_3d_off            sound_init: the switch (Zeus.inf's on) is 0 before the game's sound opens, logged; already
//                           off: left 0, logged as such
//   sound_3d_fault          the switch cannot be written (a fault in the guard): off, logged, the game's sound opens
//                           with the switch as it was
//
// The core's sound on Zeus's sound manager (games/zeus/sound, core/sound):
//   sound_pause          WM_KILLFOCUS and WM_ACTIVATE minimized (the game's call sites): the playing stream and
//                           voice paused instead of the game's sound_stop_all; focus back: they go on, the time away
//                           added to the timed music's start; game_main's call: the game's own stop
//   sound_3d_on          the game's 3D sounds on: the game's own stop (its looping 3D voices are not paused)
//   sound_fault          reading the sound manager faults: the game's own stop, the pause off, then always the game's
//   sound_ambient_pan    ambient_sound_random: none while quiet; sound_play: pan 128 becomes 127
//
// The core's key bindings with Zeus's actions (games/zeus/keys, core/input/keys):
//   key_bindings                 a bound key becomes the game's own (its character eaten), a game key bound to nothing is
//                           kept, Ctrl actions with key_ctrl put back, Alt characters eaten but Alt+Space, F6-F8 from
//                           the keyboard kept but the game's own posted ones (lParam 0) passed, R polled once; the
//                           editor's map only its own actions, other screens as they are; the pause banner's keys
//   key_bindings_fault           a fault while translating: the bindings off, every key the game's own from then on
//
// Esc and the pause menu (games/zeus/game_menu, core/options/pause_menu):
//   esc_and_pause                  a confirmation's No; lParam 0 the game's; the main menu asks; dialogs and menu screens a
//                           right click in the next input pass; the world map; the city (tool, company, pause menu);
//                           the pause menu's items at the next loop, Replay map's No back to the menu
//   esc_editor           the editors: 0x58 the game's, pages and dialogs a right click, the map's tool, the three
//                           editor pause menus and their File menu functions with button_param
//   esc_fault            a fault in the game's function: Esc off, the game's own from then on
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
// Our messages in the game's notice boxes (games/zeus/notice with the core's core/ui/notice):
//   notice_boxes               a text shown through city_warning_show(1000) (a __cdecl fake): '[' replaced, warnings_on 1
//                           for the call only, the 200 bytes of city_warning_text put back, the font, our box widths;
//                           five boxes full: none; the zoom box rewritten in place, a new one when its slot holds the
//                           game's own Sound Frags (also id 1000); the smoke test's notice action; a fault in
//                           city_warning_show: city_warning_text and warnings_on put back, notices off, the caption line
//
// Smooth scrolling's camera (games/zeus/scroll with the core's core/scroll/camera):
//   scroll_ok            at a city call of scroll_get_direction with the cursor on the right edge: 8 every frame,
//                           the camera moved right by time, edge_scrolling set, sound_view_decay called, the rate log's
//                           part; another caller and smooth off: the game's own direction
//   direction_fault      view_clamp faults in a smooth frame: the camera, edge_scrolling and the key flag put back,
//                           remainder 0, off, the game's own direction; cannot be switched on again
//
// Smooth scrolling's picture (games/zeus/scroll with the core's core/scroll/picture):
//   render_ok            with a remainder: drawn into our buffer (the clip two columns and a row pair larger, the
//                           viewport +2 / +2), shown offset into the gap left of the sidebar and on the last row, nothing
//                           on the sidebar or below the screen; conversions moved (a conversion while drawing: not
//                           made larger again); the Game paused width and the menus' background up to the sidebar
//   render_fault         a fault while drawing in a counting frame: put back, off, drawn again with anim_due all 0
//                           and put back (the half before the fault counts); the next counting frame counts
//   render_fault_uncounted  the same in a frame that does not count: the outer drawing puts both halves back
//   convert_fault        a fault in screen_to_grid with the origin moved: put back, off, converted again
//   drag_fault           the drag's start corner moved by the remainder and 2 px per column for the call only; a
//                           fault: put back, off, drawn again
//   city_input           the cursor on the sidebar or the top bar is no tile (the game not asked), in the gap it is;
//                           grid_to_screen moved; a drag's mouse kept to the city as shown; the smallest frame
//   city_extent               the layout's extent: the sidebar at the right edge, the gap drawn by the city (also with
//                           smooth_scroll off), a clip past the screen's bottom shown to the bottom only; 1024x768 the
//                           clip only
//   east_edge            the view at the game's east limit is pulled back to the original's east edge (the column
//                           before, 56 px of it unshown at 1920x1080), the game drawing that view; also with smooth_scroll off
//   figure_portrait             the figure portrait (city_render_2): the game's own conversions, the remainder back after it
//   scroll_attach        Attach where the sites do not match: nothing installed (logged), the game's own drawing;
//                           view_scroll_step's detour counts the game's own steps
//
// Each scenario runs in a child process of its own (a feature switched off stays off for the rest of a process):
//   test_restore.exe            runs every scenario and prints one line each
//   test_restore.exe <name>     one scenario (exit code 0 = passed)
#include "core/capture/big_picture.h"
#include "core/capture/city_picture.h"
#include "core/capture/screenshot.h"
#include "core/diag/autorun.h"
#include "core/diag/guard.h"
#include "core/log.h"
#include "core/config.h"
#include "core/ddraw/surface_memory.h"
#include "core/display/framed_screens.h"
#include "core/display/logical_size.h"
#include "core/display/city_zoom.h"
#include "core/display/map_screen.h"
#include "core/output/sdl_output.h"
#include "core/scroll/zoom.h"
#include "core/ui/info_window.h"
#include "core/display/virtual_display.h"
#include "core/input/keys.h"
#include "core/settings/settings.h"
#include "core/ui/hotkey.h"
#include "games/zeus/anim_clock.h"
#include "games/zeus/city_picture.h"
#include "games/zeus/fullscreen.h"
#include "games/zeus/game_menu.h"
#include "games/zeus/painter.h"
#include "core/options/menu.h"
#include "core/options/menu_text.h"
#include "core/options/pause_menu.h"
#include "core/ui/notice.h"
#include "games/zeus/intro.h"
#include "games/zeus/keys.h"
#include "games/zeus/sound.h"
#include "core/sound/sound.h"
#include "games/zeus/notice.h"
#include "games/zeus/resolution.h"
#include "games/zeus/scroll.h"
#include "games/zeus/site.h"
#include "games/zeus/smoke.h"
#include "games/zeus/state_check.h"
#include "games/zeus/timing.h"
#include "games/zeus/world_map.h"
#include "games/zeus/zoom.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>

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
int g_captionNotices = 0; // notice_boxes: texts on our caption line (core/ui/notice's fallback)
void SetNotice(const char *, unsigned) { ++g_captionNotices; }
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
// The core's zoom: the wheel's mouse filter, kept for the zoom scenarios to call.
bool (*g_mouseFilter)(UINT, WPARAM, int, int) = nullptr;
void SetMouseFilter(bool (*filter)(UINT, WPARAM, int, int))
{
    g_mouseFilter = filter;
}
// Drag scrolling: no game window here (the drag's own offline test is test_drag).
void HideCursor(bool) {}
HWND GameWindow() { return nullptr; }
struct Placement;
bool ModePlacement(Placement *) { return false; }
// core/sound: the focus handler (the pause's "focus back"), kept for the sound scenarios to call.
void (*g_focusHandler)() = nullptr;
void SetFocusHandler(void (*handler)())
{
    g_focusHandler = handler;
}
} // namespace core::display

namespace core::diag::autorun { // the smoke test is not started here (its own test: test_autorun)
bool Start(const Game &) { return false; }
void Frame() {}
bool g_requested = false; // intro_mismatch: as when a smoke test started the game
bool Requested() { return g_requested; }
bool IntroRequested() { return false; }
} // namespace core::diag::autorun

// city_picture: the pictures handed to the writer (not written here) and its done callback.
struct SavedPic {
    std::string kind;
    int w, h;
};
std::vector<SavedPic> g_savedPics;
void (*g_savedDone)(bool, const char *) = nullptr;

// core/input/keys: what the key bindings sent to the game (for the key scenarios), with key_ctrl at that moment.
struct KeysSent {
    UINT m;
    WPARAM w;
    int ctrl;
};
KeysSent g_keysSent[16];
int g_keysSentCount = 0;
volatile int *g_ctrlForKeys = nullptr;

namespace core::display {
LRESULT SendToGame(UINT m, WPARAM w, LPARAM)
{
    if (g_keysSentCount < 16) {
        g_keysSent[g_keysSentCount++] = KeysSent{m, w, g_ctrlForKeys ? *g_ctrlForKeys : -1};
    }
    return 0;
}
void SetKeyTranslator(KeyTranslator) {}
void SetKeyStateFilter(KeyStateFilter) {}
void AddActionKey(ActionKey *) {} // the pictures' keys (the scenarios ask through RequestCity / RequestMinimap)
void SetModalInput(const ModalInput *) {} // games/zeus/game_menu: the pause menu's input (not under test here)
} // namespace core::display

namespace core::capture {
bool SaveBig(BigPicture *pic, const BigOptions &o)
{
    g_savedPics.push_back(SavedPic{o.kind, pic->Width(), pic->Height()});
    g_savedDone = o.done;
    delete pic;
    return true;
}
int Flush(unsigned long) { return 0; }
bool SaveOne(const void *, int, int, int, const char *, const char *) { return true; } // a report's city buffer
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

// The game's memory: our copy of 0x400000 .. 0x1611000, the whole image (scrolling: the image packs' table 0x12afbec is
// above the 0x1100000 used before).
constexpr DWORD kImageBase = 0x400000, kImageSize = 0x1211000;

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

// What the drawing sees of smooth scrolling's picture (draw_buffer, the clip, the viewport), and pixels it draws.
struct PicSeen {
    BYTE *buffer = nullptr;
    int clipR = 0, clipB = 0, cols = 0, rows = 0;
    struct Mark {
        int x, y;
        WORD v;
    } marks[4];
    int markCount = 0;
    int (*mouse)() = nullptr; // asked for the cursor tile while drawing (as the drawing's markers do)
    int mouseTile = -1;
    int vx = -1, vy = -1; // the view city_render was given
} g_pic;

// As the city drawing: one tile animation byte, a building's field, a figure's field and the drawing counters
// move on by one.
void __cdecl FakeCityRender(int vx, int vy, int)
{
    ++g_calls.draws;
    g_pic.vx = vx;
    g_pic.vy = vy;
    g_pic.buffer = At<BYTE *>(0xf9c100);
    g_pic.clipR = At<int>(0x7d3b90);
    g_pic.clipB = At<int>(0x7d3b88);
    g_pic.cols = At<int>(0x7d3bbc);
    g_pic.rows = At<int>(0x7d3bb8);
    for (int i = 0; i < g_pic.markCount; ++i) {
        reinterpret_cast<WORD *>(g_pic.buffer + g_pic.marks[i].y * At<int>(0xf2a698))[g_pic.marks[i].x] =
            g_pic.marks[i].v;
    }
    if (g_pic.mouse) {
        g_pic.mouseTile = g_pic.mouse();
    }
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
    if (Fires("city_render")) { // after what it moved on (the half of a drawing that ran before the fault)
        Fault();
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

void __cdecl FakeViewportInit(int, int, int x, int top, int cols, int rows, int tileW, int tileH)
{
    ++g_res.inits;
    if (Fires("viewport_init")) {
        Fault();
    }
    g_res.top = top;
    g_res.cols = cols;
    g_res.rows = rows;
    g_res.tileW = tileW;
    // As the game's (the variables smooth scrolling's picture reads).
    At<int>(aClipBottom) = (rows - 1) * (tileH / 2) + top;
    At<int>(0x7d3b90) = (tileW + 2) * cols - 2 + x; // the clip's right
    At<int>(0x7d3b94) = x;                          // left
    At<int>(0x7d3b8c) = top;                        // top
    At<int>(0x7d3be8) = x;                          // the origin
    At<int>(0x7d3be4) = top;
    At<int>(0x7d3bbc) = cols;
    At<int>(0x7d3bb8) = rows;
    At<int>(0x7d3bb0) = tileW;
    At<int>(0x7d3ba4) = tileH / 2;
}

// The top bar, the bottom strip and the city message panel. The image functions are __thiscall: as __fastcall
// fakes (ecx, edx unused, then the stack arguments) they pop the same bytes.
constexpr DWORD fTopBar = 0x59ebe0, fImageGroup = 0x40fcf0, fImageDraw = 0x5acf50, fImageSize = 0x5aceb0,
                fFillRect = 0x509b20, fMessagePanel = 0x4f2d80;
constexpr int kMaxDraws = 4096; // image_draw calls recorded (the filler at 3840x2160: about 900)
struct CityLayoutCalls {
    int topBars = 0, draws = 0, fills = 0, panels = 0, panelClip = 0, panelClipR = 0, sidebars = 0, worldSidebars = 0;
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
    g_cityLayout.panelClipR = At<int>(0x7d3b90);
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
    Check(LogContains("2 changed value(s) put back"), "log: the two values written (mode_width, mode_height) put back");
    Res<Mode_fn>("screen_mode_size")(3);
    Check(At<int>(aModeW) == 1024, "afterwards mode 3 is the game's own 1024x768");
    Check(!zeus::resolution::SettingsChanged(&c), "a new setting is refused (the game runs at its own size)");
}

void ViewportLayout()
{
    SetupResolution();
    ToCity();
    Res<Tick_fn>("ui_set_sidebar_position")();
    Check(g_res.sidebars == 0 && At<int>(aSidebarX) == 1920 - 186 && At<int>(aSidebarX2) == 1920 - 186 + 0x78,
          "1920 wide: the sidebar against the right edge (1734; before: 1678 where 28 columns end)");
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
    Check(g_cityLayout.drawX[0] == -64 - 158 && g_cityLayout.drawX[1] == 176 - 158 && g_cityLayout.drawX[2] == 416 - 158 &&
              g_cityLayout.drawX[3] == 656 - 158 && g_cityLayout.drawX[4] == 1734 - 838 && g_cityLayout.drawY[0] == 0 && g_cityLayout.drawY[4] == 0,
          "columns 158..397 from the left (-64, 176, 416, 656; only the left side clipped), the whole bar last (896)");
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
    At<int>(aSidebarX) = 0;
    At<int>(aSidebarX2) = 0;
    Res<Tick_fn>("ui_set_sidebar_position")();
    Check(At<int>(aSidebarX) == 838 && At<int>(aSidebarX2) == 838 + 0x78,
          "the layout off: the sidebar at the game's 1024 place (838) until the size is back");
    At<unsigned char>(aGameScreen) = 0x01;
    const int switches = g_res.switches;
    Sleep(510); // the mode change is tried at most every 500 ms
    zeus::resolution::OnLoop();
    Check(g_res.switches == switches + 1, "the next loop: the game's own mode change");
}

// The same with the sidebar open (city_viewport_narrow 0x51cc00): the 1024 layout's 14 x 50 at once.
void NarrowFault()
{
    SetupResolution();
    ToCity();
    g_fault = "viewport_init";
    Res<Tick_fn>("city_viewport_narrow")();
    Check(g_res.inits == 2 && g_res.cols == 14 && g_res.rows == 50 && g_res.narrows == 0,
          "a fault: the 1024 layout's 14 x 50 at once (our call, not the game's function)");
    Check(LogContains("feature \"city layout at this resolution (games/zeus/resolution)\" switched off") &&
              LogContains("back to the game's own 1024x768"),
          "log: layout off, back to the game's own size");
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

// Attach where the game's bytes are not there: a smoke test asks for the skip, but Matches reads the sites at the
// game's own addresses (0x417b00, 0x5c3bab), which in this test program are not Zeus.exe's code (the copy of the
// game's range is elsewhere), so the bytes differ. Nothing is installed and the movie plays.
void IntroMismatch()
{
    SetupIntro(true); // as if skipping: Attach must take it back
    core::diag::autorun::g_requested = true;
    zeus::intro::Attach();
    core::diag::autorun::g_requested = false;
    Check(LogContains("intro: a smoke test: the intro movie is skipped whatever intro_movie says"),
          "log: the smoke test's skip asked for");
    Check(LogContains("intro: bink_open does not match: the intro movie plays"), "log: the site does not match");
    Check(!LogContains("bink_open could not be hooked"), "no hook tried");
    int player = 0;
    Check(GameMainIntro(&player, "Binks\\zeus_intro.bik", 112, 84) == 1 && g_bink.opens == 1 && g_bink.x == 112 &&
              g_bink.y == 84,
          "the movie plays: game_main's call opens it");
}

// The game's 3D sounds (games/zeus/sound)

constexpr DWORD fSoundInit = 0x5a3990, aSounds3d = 0xf2aa98;

struct SoundInit {
    int calls = 0;
    int switchSeen = -1; // sounds_3d_on when the game's sound_init ran
} g_soundInit;

void __cdecl FakeSoundInit()
{
    ++g_soundInit.calls;
    g_soundInit.switchSeen = At<int>(aSounds3d);
}

using SoundInit_fn = void(__cdecl *)();

SoundInit_fn SoundDetour()
{
    int n;
    const zeus::Detour *d = zeus::sound::Detours(&n);
    return reinterpret_cast<SoundInit_fn>(d[0].detour);
}

void Sound3dOff()
{
    Thunk(fSoundInit, reinterpret_cast<const void *>(&FakeSoundInit));
    At<int>(aSounds3d) = 1; // as config_load leaves it with Zeus.inf's "on"
    SoundDetour()();
    Check(g_soundInit.calls == 1 && g_soundInit.switchSeen == 0 && At<int>(aSounds3d) == 0,
          "on: the switch is 0 before the game's sound opens (the game's sound_init still runs)");
    Check(LogContains("sound: the game's 3D sounds turned off for this run (Zeus.inf had them on)"), "log: turned off");
    SoundDetour()();
    Check(g_soundInit.calls == 2 && g_soundInit.switchSeen == 0 && At<int>(aSounds3d) == 0, "already off: left 0");
    Check(LogContains("sound: the game's 3D sounds are off (as in Zeus.inf)"), "log: already off");
}

void Sound3dFault()
{
    Thunk(fSoundInit, reinterpret_cast<const void *>(&FakeSoundInit));
    At<int>(aSounds3d) = 1;
    DWORD old;
    VirtualProtect(reinterpret_cast<void *>((aSounds3d & ~0xfffu) + g_imageShift), 0x1000, PAGE_READONLY, &old);
    SoundDetour()(); // writing the switch faults inside the guard
    VirtualProtect(reinterpret_cast<void *>((aSounds3d & ~0xfffu) + g_imageShift), 0x1000, old, &old);
    Check(g_soundInit.calls == 1 && g_soundInit.switchSeen == 1 && At<int>(aSounds3d) == 1,
          "a fault: the game's sound opens with the switch as Zeus.inf had it");
    Check(LogContains("feature \"3D sounds off (games/zeus/sound)\" switched off"), "log: the feature off");
    SoundDetour()();
    Check(g_soundInit.calls == 2 && At<int>(aSounds3d) == 1, "afterwards: the switch is not touched");
}

// The core's sound on Zeus's sound manager (games/zeus/sound, core/sound). The call sites are the game's
// own (CallSite), so the detour sees the return addresses of game_wndproc and game_main.
constexpr DWORD fStopAll = 0x5a1330, fAmbient = 0x5a4790, fPlay = 0x5c80b0, aManager = 0xf9bea0, aMusic = 0xf97000,
                cKillFocus = 0x5c4bbd, cMinimized = 0x5c4be7, cGameEnd = 0x5c399f;

struct SoundFakes {
    int stopAlls = 0, ambients = 0, lastPan = -1;
    int status[32] = {}; // fake Miles: a handle is an index
    void *voices[2] = {};
} g_snd;

void __cdecl FakeStopAll()
{
    ++g_snd.stopAlls;
}
void __cdecl FakeAmbient()
{
    ++g_snd.ambients;
}
int __fastcall FakePlay(void *, void *, void *request)
{
    g_snd.lastPan = static_cast<int *>(request)[3];
    return 0;
}
int H(void *h)
{
    return static_cast<int>(reinterpret_cast<intptr_t>(h));
}
void __stdcall MilesPauseStream(void *h, int onoff)
{
    g_snd.status[H(h)] = onoff ? 16 : core::sound::kPlaying;
}
int __stdcall MilesStatus(void *h)
{
    return g_snd.status[H(h)];
}
void __stdcall MilesStop(void *h)
{
    g_snd.status[H(h)] = core::sound::kStopped;
}
void __stdcall MilesResume(void *h)
{
    g_snd.status[H(h)] = core::sound::kPlaying;
}

template <typename F> F SoundAt(const char *name)
{
    int n;
    const zeus::Detour *d = zeus::sound::Detours(&n);
    for (int i = 0; i < n; ++i) {
        if (std::strcmp(d[i].site->name, name) == 0) {
            return reinterpret_cast<F>(d[i].detour);
        }
    }
    return nullptr;
}

using Void_fn = void(__cdecl *)();

// The game's sound manager as Zeus keeps it: stream 0 (music) playing, stream 2 open and done, two voices (one
// playing); music for a while (mode 1) started at 1000.
void SetupSound()
{
    static const core::sound::Miles miles{&MilesPauseStream, &MilesStatus, &MilesStatus, &MilesStop, &MilesResume};
    core::sound::SetMilesForTest(&miles);
    zeus::sound::AttachForTest();
    Thunk(fStopAll, reinterpret_cast<const void *>(&FakeStopAll));
    Thunk(fAmbient, reinterpret_cast<const void *>(&FakeAmbient));
    Thunk(fPlay, reinterpret_cast<const void *>(&FakePlay));
    const void *stopAll = reinterpret_cast<const void *>(SoundAt<Void_fn>("sound_stop_all"));
    CallSite(cKillFocus, stopAll);
    CallSite(cMinimized, stopAll);
    CallSite(cGameEnd, stopAll);
    At<BYTE>(aManager + 0x84) = 0;
    At<DWORD>(aManager + 0x2c) = 1;
    At<DWORD>(aManager + 0x34) = 2;
    g_snd.status[1] = core::sound::kPlaying;
    g_snd.status[2] = 2;
    g_snd.voices[0] = reinterpret_cast<void *>(10);
    g_snd.voices[1] = reinterpret_cast<void *>(11);
    g_snd.status[10] = core::sound::kPlaying;
    g_snd.status[11] = 2;
    At<int>(aManager + 0x3c) = 2;
    At<DWORD>(aManager + 0xd8) = reinterpret_cast<DWORD>(g_snd.voices);
    At<int>(aSounds3d) = 0;
    At<int>(aMusic + 0x4e40) = 1;
    At<DWORD>(aMusic + 0x4e8c) = 1000;
}

void CallFrom(DWORD site)
{
    reinterpret_cast<Void_fn>(site + g_imageShift)();
}

void SoundPause()
{
    SetupSound();
    CallFrom(cKillFocus);
    Check(g_snd.stopAlls == 0, "WM_KILLFOCUS: paused instead of the game's sound_stop_all");
    Check(g_snd.status[1] == 16 && g_snd.status[2] == 2, "... the playing stream paused, the done one left");
    Check(g_snd.status[10] == core::sound::kStopped && g_snd.status[11] == 2, "... the playing voice stopped");
    Check(LogContains("sound: switched away: 1 stream(s) and 1 sound(s) paused"), "log: paused");
    Sleep(20);
    Check(core::display::g_focusHandler != nullptr, "the focus handler is the core's sound");
    core::display::g_focusHandler();
    Check(g_snd.status[1] == core::sound::kPlaying && g_snd.status[10] == core::sound::kPlaying,
          "focus back: the stream and the voice go on");
    Check(At<DWORD>(aMusic + 0x4e8c) > 1000, "... the time away is added to the timed music's start (mode 1)");
    CallFrom(cMinimized);
    Check(g_snd.stopAlls == 0 && g_snd.status[1] == 16, "WM_ACTIVATE minimized: paused too");
    core::display::g_focusHandler();
    CallFrom(cGameEnd);
    Check(g_snd.stopAlls == 1 && g_snd.status[1] == core::sound::kPlaying, "game_main's call: the game's own stop");
}

void Sound3dOn()
{
    SetupSound();
    At<int>(aSounds3d) = 1; // turned on in the game's sound options: its looping 3D voices cannot be paused by us
    CallFrom(cKillFocus);
    Check(g_snd.stopAlls == 1 && g_snd.status[1] == core::sound::kPlaying, "3D sounds on: the game's own stop");
    Check(LogContains("the game plays sounds that cannot wait"), "log: why");
}

void SoundFault()
{
    SetupSound();
    DWORD old;
    VirtualProtect(reinterpret_cast<void *>((aManager & ~0xfffu) + g_imageShift), 0x1000, PAGE_NOACCESS, &old);
    CallFrom(cKillFocus); // reading the sound manager faults inside the guard
    VirtualProtect(reinterpret_cast<void *>((aManager & ~0xfffu) + g_imageShift), 0x1000, old, &old);
    Check(g_snd.stopAlls == 1, "a fault while pausing: the game's own stop");
    Check(LogContains("feature \"sound paused while switched away (core/sound)\" switched off"), "log: the feature off");
    CallFrom(cKillFocus);
    Check(g_snd.stopAlls == 2 && g_snd.status[1] == core::sound::kPlaying, "afterwards: the game's own stop");
}

void SoundAmbientPan()
{
    SetupSound();
    const Void_fn ambient = SoundAt<Void_fn>("ambient_sound_random");
    using Play_fn = int(__fastcall *)(void *, void *, void *);
    const Play_fn play = SoundAt<Play_fn>("sound_play");
    ambient();
    Check(g_snd.ambients == 1, "ambient_sound_random: the game's");
    core::sound::SetQuiet(true);
    ambient();
    Check(g_snd.ambients == 1, "our window open (quiet): no new ambient sound");
    core::sound::SetQuiet(false);
    ambient();
    Check(g_snd.ambients == 2, "closed: the game's again");
    int request[13] = {};
    request[3] = 128;
    play(reinterpret_cast<void *>(aManager + g_imageShift), nullptr, request);
    Check(g_snd.lastPan == 127, "sound_play with pan 128 (the right edge): the game gets 127");
    request[3] = 40;
    play(reinterpret_cast<void *>(aManager + g_imageShift), nullptr, request);
    Check(g_snd.lastPan == 40, "pan 40: as it was");
}

// The game's 3D sounds kept off after the sound options' button (0x402110, __cdecl, no arguments) and on
// every pass; a fault in the layer-1 sounds switches them off and the game goes on.
constexpr DWORD fButton = 0x402110, aSoundStarted = 0x10e4230;

void __cdecl FakeButton()
{
    At<int>(aSounds3d) = 1 - At<int>(aSounds3d); // button 5: the game flips the switch
}

void Layer1KeepOff()
{
    SetupSound();
    Thunk(fButton, reinterpret_cast<const void *>(&FakeButton));
    SoundAt<Void_fn>("sound_options_toggle")();
    Check(At<int>(aSounds3d) == 0, "the 3D sounds button turned them on: put back to off at once");
    Check(LogContains("the game's 3D sounds turned on (the sound options' button): put back to off"), "log: the button");
    At<int>(aSounds3d) = 1;
    zeus::sound::OnLoop();
    Check(At<int>(aSounds3d) == 0, "found on on a pass: put back to off");
    Check(LogContains("the game's 3D sounds turned on (found on a pass): put back to off"), "log: the pass");
}

void Layer1Fault()
{
    SetupSound();
    At<int>(aSoundStarted) = 1;
    DWORD old;
    VirtualProtect(reinterpret_cast<void *>((aSoundStarted & ~0xfffu) + g_imageShift), 0x1000, PAGE_NOACCESS, &old);
    zeus::sound::OnLoop(); // reading whether the game's sound started faults inside the layer-1 guard
    VirtualProtect(reinterpret_cast<void *>((aSoundStarted & ~0xfffu) + g_imageShift), 0x1000, old, &old);
    Check(LogContains("feature \"layer-1 ambient sounds (core/sound/layer1)\" switched off"),
          "a fault in the layer-1 sounds: that feature off");
    zeus::sound::OnLoop();
    Check(true, "... and the game's pass goes on");
}

// --- The key bindings (games/zeus/keys on core/input/keys). The modifiers and held keys come from a fake
// system; what the bindings send to the game is recorded by the fake SendToGame (with key_ctrl at that moment).
unsigned g_fakeMods = 0;
unsigned g_fakeHeldVk = 0;
const core::input::keys::KeysSystem kFakeKeys{[] { return g_fakeMods; },
                                              [](unsigned vk) { return vk != 0 && vk == g_fakeHeldVk; }};
constexpr DWORD aGameScreenK = 0xf2a6dd, aEditorModeK = 0x665e2c, aKeyCtrl = 0xf9c108;
const char *__cdecl FakeLangKeys(int group, int index) // lang_get_string: __cdecl(group, index), the caller pops
{
    return group == 13 && index == 2 ? "Game paused ('P' key continues)" : "the game's text";
}

void SetupKeys()
{
    namespace k = core::input::keys;
    k::SetSystem(&kFakeKeys);
    zeus::keys::RegisterSettings();
    g_ctrlForKeys = reinterpret_cast<volatile int *>(aKeyCtrl + g_imageShift);
    At<int>(aKeyCtrl) = 0;
    At<int>(aEditorModeK) = 0;
    At<unsigned char>(aGameScreenK) = 1;
    Thunk(0x507bf0, reinterpret_cast<void *>(&FakeLangKeys)); // lang_get_string
}

void BindKey(const char *key, const char *v)
{
    core::settings::Request(core::settings::Find("keys", key), v, false);
    core::settings::RunPending();
}

bool SentOne(UINT m, WPARAM w)
{
    return g_keysSentCount == 1 && g_keysSent[0].m == m && g_keysSent[0].w == w;
}

void KeyBindings()
{
    namespace k = core::input::keys;
    SetupKeys();
    Check(k::ActionCount() == 63, "keys: 63 actions (19 overlays, no F9, army groups 1-5, Tab)");
    g_keysSentCount = 0;
    Check(k::Translate(WM_KEYDOWN, VK_UP, 1) && SentOne(WM_KEYDOWN, VK_UP), "keys: Up (the default) sent as Up");
    BindKey("scroll_up", "W");
    g_keysSentCount = 0;
    Check(k::Translate(WM_KEYDOWN, 'W', 1) && SentOne(WM_KEYDOWN, VK_UP), "keys: W bound to scrolling up: Up");
    Check(k::Translate(WM_CHAR, 'w', 1) && g_keysSentCount == 1, "keys: ... and not the character w");
    g_keysSentCount = 0;
    Check(k::Translate(WM_KEYDOWN, VK_UP, 1) && g_keysSentCount == 0, "keys: Up bound to nothing: kept from the game");
    g_fakeHeldVk = 'W';
    Check(k::Held(VK_UP), "keys: smooth scrolling: W held counts as Up");
    g_fakeHeldVk = VK_UP;
    Check(!k::Held(VK_UP), "keys: ... and Up (bound to nothing) does not");
    g_fakeHeldVk = 0;
    g_keysSentCount = 0;
    Check(k::Translate(WM_KEYDOWN, VK_SPACE, 1) && SentOne(WM_CHAR, ' '), "keys: Space: the character space");
    g_keysSentCount = 0;
    Check(k::Translate(WM_KEYDOWN, VK_TAB, 1) && SentOne(WM_KEYDOWN, VK_TAB), "keys: Tab: the sidebar's Overview key");
    g_keysSentCount = 0;
    Check(k::Translate(WM_KEYDOWN, 'P', 1) && SentOne(WM_CHAR, 'p'), "keys: P: the character p (pause)");
    g_fakeMods = core::ui::kModCtrl;
    g_keysSentCount = 0;
    Check(k::Translate(WM_KEYDOWN, '3', 1) && SentOne(WM_KEYDOWN, '3') && g_keysSent[0].ctrl == 1 &&
              At<int>(aKeyCtrl) == 0,
          "keys: Ctrl+3: 3 with key_ctrl 1, put back after");
    g_keysSentCount = 0;
    Check(!k::Translate(WM_KEYDOWN, '6', 1) && g_keysSentCount == 0,
          "keys: Ctrl+6 (no group 6 in Zeus): to the game (which does nothing with it)");
    Check(k::Translate(WM_KEYDOWN, VK_LEFT, 1) && g_keysSentCount == 0, "keys: Ctrl+Left: kept from the game");
    g_fakeMods = core::ui::kModAlt;
    g_keysSentCount = 0;
    Check(k::Translate(WM_SYSKEYDOWN, '2', 1 | (1 << 29)) && SentOne(WM_SYSKEYDOWN, '2'), "keys: Alt+2: recall 2");
    Check(k::Translate(WM_SYSCHAR, '2', 1 | (1 << 29)), "keys: ... its character eaten");
    g_keysSentCount = 0;
    Check(!k::Translate(WM_SYSKEYDOWN, '7', 1 | (1 << 29)) && g_keysSentCount == 0, "keys: Alt+7: to the game");
    Check(k::Translate(WM_SYSCHAR, '7', 1 | (1 << 29)), "keys: ... its character eaten (no beep)");
    Check(!k::Translate(WM_SYSCHAR, ' ', 1 | (1 << 29)), "keys: Alt+Space's character: to Windows (the window menu)");
    g_fakeMods = 0;
    g_keysSentCount = 0;
    Check(k::Translate(WM_KEYDOWN, VK_F6, 1) && g_keysSentCount == 0 && LogContains("keys: the options window is not "
                                                                                     "available"),
          "keys: F6 (Open Options): taken, no window yet");
    Check(k::Translate(WM_KEYDOWN, VK_F7, 1) && k::Translate(WM_KEYDOWN, VK_F8, 1) && g_keysSentCount == 0,
          "keys: F7, F8 from the keyboard never reach the game");
    Check(!k::Translate(WM_KEYDOWN, VK_F6, 0) && !k::Translate(WM_KEYDOWN, VK_F7, 0) && g_keysSentCount == 0,
          "keys: F6, F7 the game posted itself (lParam 0, the Display options buttons): the game's");
    Check(!k::Translate(WM_KEYDOWN, VK_ESCAPE, 1), "keys: Esc: the game's");
    g_keysSentCount = 0;
    Check(k::Translate(WM_KEYDOWN, 'R', 1) && SentOne(WM_KEYDOWN, 'R') &&
              k::KeyState('R', 0) == static_cast<SHORT>(0x8001) && k::KeyState('R', 0) == 0,
          "keys: R: the game gets R, and its poll says pressed once");
    // The editor's map: only its own actions.
    At<int>(aEditorModeK) = 1;
    g_keysSentCount = 0;
    Check(k::Translate(WM_KEYDOWN, 'W', 1) && SentOne(WM_KEYDOWN, VK_UP), "keys: editor: W scrolls up there too");
    Check(k::Translate(WM_KEYDOWN, VK_UP, 1) && g_keysSentCount == 1, "keys: editor: Up (bound to nothing) kept");
    g_keysSentCount = 0;
    Check(k::Translate(WM_KEYDOWN, VK_PRIOR, 1) && SentOne(WM_KEYDOWN, VK_PRIOR), "keys: editor: PgUp rotates the map");
    g_keysSentCount = 0;
    Check(k::Translate(WM_KEYDOWN, VK_F2, 1) && SentOne(WM_KEYDOWN, VK_F2), "keys: editor: F2 (bookmark 2)");
    g_keysSentCount = 0;
    Check(!k::Translate(WM_KEYDOWN, 'P', 1) && !k::Translate(WM_CHAR, 'p', 1) && !k::Translate(WM_KEYDOWN, VK_SPACE, 1) &&
              !k::Translate(WM_KEYDOWN, VK_TAB, 1) && g_keysSentCount == 0,
          "keys: editor: P, Space, Tab are the editor's own");
    Check(k::Translate(WM_KEYDOWN, VK_F7, 1), "keys: editor: F7 never reaches it");
    At<unsigned char>(aGameScreenK) = 0x58;
    g_keysSentCount = 0;
    Check(k::Translate(WM_KEYDOWN, 'W', 1) && SentOne(WM_KEYDOWN, VK_UP), "keys: editor screen 0x58: translated");
    At<unsigned char>(aGameScreenK) = 0x27;
    g_keysSentCount = 0;
    Check(!k::Translate(WM_KEYDOWN, 'W', 1) && !k::Translate(WM_CHAR, 'w', 1) && g_keysSentCount == 0,
          "keys: another editor screen: the keys as they are (typing)");
    // Not a city screen.
    At<int>(aEditorModeK) = 0;
    At<unsigned char>(aGameScreenK) = 0x0d;
    g_keysSentCount = 0;
    Check(!k::Translate(WM_KEYDOWN, 'W', 1) && !k::Translate(WM_CHAR, 'w', 1) && g_keysSentCount == 0,
          "keys: the save dialog: the keys as they are");
    Check(k::Translate(WM_KEYDOWN, VK_F7, 1) && g_keysSentCount == 0, "keys: ... F7 never reaches the game");
    // The pause banner (lang_get_string's detour).
    int n = 0;
    const zeus::Detour *d = zeus::keys::Detours(&n);
    auto lang = reinterpret_cast<const char *(__cdecl *)(int, int)>(d[0].detour);
    Check(n == 1 && std::strcmp(lang(13, 2), "Game paused ('P' key continues)") == 0, "keys: the pause banner: P");
    BindKey("pause", "Space, P");
    Check(std::strcmp(lang(13, 2), "Game paused ('Space' or 'P' key continues)") == 0,
          "keys: the pause banner: both keys");
    BindKey("pause", "");
    Check(std::strcmp(lang(13, 2), "Game paused") == 0, "keys: the pause banner: no key");
    Check(std::strcmp(lang(21, 7), "the game's text") == 0, "keys: other texts: the game's");
}

// A fault while translating (game_screen unreadable): the bindings off, the keys the game's own from then on.
void KeyBindingsFault()
{
    namespace k = core::input::keys;
    SetupKeys();
    const DWORD page = (aGameScreenK & ~0xfffu) + g_imageShift;
    DWORD old;
    VirtualProtect(reinterpret_cast<void *>(page), 0x1000, PAGE_NOACCESS, &old);
    g_keysSentCount = 0;
    Check(!k::Translate(WM_KEYDOWN, VK_UP, 1) && g_keysSentCount == 0, "keys: a fault: the key goes to the game");
    VirtualProtect(reinterpret_cast<void *>(page), 0x1000, old, &old);
    Check(LogContains("feature \"key bindings (core/input/keys)\" switched off"), "keys: ... the bindings off");
    BindKey("scroll_up", "W");
    Check(!k::Translate(WM_KEYDOWN, 'W', 1) && !k::Translate(WM_KEYDOWN, VK_F7, 1) && g_keysSentCount == 0,
          "keys: ... and every key the game's own from then on");
    g_fakeHeldVk = VK_UP;
    Check(k::Held(VK_UP), "keys: ... smooth scrolling follows the arrow key itself");
    g_fakeHeldVk = 0;
    Check(k::KeyState('R', static_cast<SHORT>(0x8000)) == static_cast<SHORT>(0x8000), "keys: ... R polled as it is");
}

// --- The overlay keys (games/zeus/keys OverlayKey). The fakes do what the game's three functions do
// (checked by disassembly), each __cdecl as the game's: overlay_select and sidebar_set_category without
// arguments (button_param), category_available(category) switching there and back.
constexpr DWORD aButtonParam = 0xf135f8, aBuildCategory = 0xfa0004, aOverlayShown = 0xfa05f0,
                aOverlayBefore = 0xfa05f4, aCategoryOverlay = 0xfa042c, aSidebarDialog = 0xfa0428,
                aCategoryOverlays = 0x63e2e0;
const int kZeusCategoryOverlays[55] = {1, 0, 0, 0, 0, 2, 0, 0, 0, 0, 3, 0, 0, 0, 0, 4, 0, 0, 0, 0, 5, 6, 7, 8, 0,
                                       9, 0, 0, 0, 0, 12, 13, 14, 11, 10, 15, 0, 0, 0, 0, 16, 0, 0, 0, 0, 17, 0, 0, 0,
                                       0, 18, 19, 0, 0, 0}; // the game's table, read from Zeus.exe
constexpr DWORD aAllCategories = 0x665df0, aCategoryOpen = 0xfeae06; // what category_available 0x516f50 reads
int g_overviewsOpened = 0;
bool g_selectFaults = false;

void __cdecl FakeOverlaySelect() // 0x516cd0
{
    if (g_selectFaults) {
        Fault();
    }
    const int o = At<int>(aButtonParam);
    At<int>(aOverlayBefore) = At<int>(aOverlayShown);
    if (o == At<int>(aOverlayShown)) {
        At<int>(aOverlayShown) = 0;
        return;
    }
    At<int>(aOverlayShown) = o;
    At<int>(aCategoryOverlay + 4 * At<int>(aBuildCategory)) = o;
}

void __cdecl FakeSetCategory() // 0x516d50: the category from button_param; an overlay shown becomes the new one's
{
    const int c = At<int>(aButtonParam);
    At<int>(aBuildCategory) = c;
    if (At<int>(aOverlayShown) != 0) {
        volatile int &own = At<int>(aCategoryOverlay + 4 * c);
        if (own == 0) {
            own = kZeusCategoryOverlays[c * 5];
        }
        At<int>(aOverlayShown) = own;
    }
    if (At<unsigned char>(aGameScreenK) == 7) {
        ++g_overviewsOpened; // the game opens the new category's overview here
    }
}

void SetupOverlayKeys()
{
    SetupKeys();
    Thunk(0x516cd0, reinterpret_cast<void *>(&FakeOverlaySelect));
    Thunk(0x516d50, reinterpret_cast<void *>(&FakeSetCategory));
    for (int i = 0; i < 55; ++i) {
        At<int>(aCategoryOverlays + 4 * i) = kZeusCategoryOverlays[i];
    }
    At<int>(aAllCategories) = 0;
    for (int c = 0; c < 11; ++c) {
        At<unsigned char>(aCategoryOpen + c) = 1; // every category usable (category_available's byte)
    }
    zeus::keys::OverlaySitesForTest();
    At<int>(aBuildCategory) = 0;
    At<int>(aOverlayShown) = 0;
    At<int>(aButtonParam) = 77; // the last button's: put back after each key
}

void OverlayKeys()
{
    namespace k = core::input::keys;
    SetupOverlayKeys();
    g_keysSentCount = 0;
    Check(k::Translate(WM_KEYDOWN, 'S', 1) && At<int>(aOverlayShown) == 1 && At<int>(aBuildCategory) == 0 &&
              At<int>(aButtonParam) == 77 && g_keysSentCount == 0 && k::Translate(WM_CHAR, 's', 1),
          "overlay keys: S shows supplies (its category already), button_param put back, the character eaten");
    Check(k::Translate(WM_KEYDOWN, 'W', 1) && At<int>(aBuildCategory) == 4 && At<int>(aOverlayShown) == 5 &&
              At<int>(aCategoryOverlay + 4 * 4) == 5 && At<int>(aButtonParam) == 77,
          "overlay keys: W switches the sidebar to category 4 and shows water (Space's overlay there too)");
    Check(k::Translate(WM_KEYDOWN, 'W', 1) && At<int>(aOverlayShown) == 0, "overlay keys: W again hides it");
    // Another category while one of its overlays is shown: the new one shown, not turned off.
    At<int>(aCategoryOverlay + 4 * 6) = 0;
    Check(k::Translate(WM_KEYDOWN, 'E', 1) && At<int>(aBuildCategory) == 6 && At<int>(aOverlayShown) == 10,
          "overlay keys: E: culture category, all culture shown");
    Check(k::Translate(WM_KEYDOWN, 'H', 1) && At<int>(aBuildCategory) == 4 && At<int>(aOverlayShown) == 6,
          "overlay keys: H with an overlay shown: hygiene shown (not turned off by the switch)");
    // A building menu's overview open (screen 7): back on the city, no overview of the new category.
    At<unsigned char>(aGameScreenK) = 7;
    At<unsigned char>(aSidebarDialog) = 1;
    g_overviewsOpened = 0;
    Check(k::Translate(WM_KEYDOWN, 'S', 1) && At<unsigned char>(aGameScreenK) == 1 &&
              At<unsigned char>(aSidebarDialog) == 0 && g_overviewsOpened == 0 && At<int>(aOverlayShown) == 1,
          "overlay keys: screen 7: back on the city first, supplies shown, no overview opened");
    At<unsigned char>(aGameScreenK) = 0x20;
    Check(k::Translate(WM_KEYDOWN, 'W', 1) && At<unsigned char>(aGameScreenK) == 0x20 && At<int>(aOverlayShown) == 5 &&
              At<int>(aBuildCategory) == 4,
          "overlay keys: warships selected (0x20): stays, water shown (as an army selected)");
    At<unsigned char>(aGameScreenK) = 0x48;
    Check(k::Translate(WM_KEYDOWN, 'S', 1) && At<unsigned char>(aGameScreenK) == 0x48 && At<int>(aOverlayShown) == 1 &&
              At<int>(aBuildCategory) == 0,
          "overlay keys: an army selected (0x48): stays, supplies shown (as Yongle)");
    Check(k::Translate(WM_KEYDOWN, 'S', 1) && At<int>(aOverlayShown) == 0, "overlay keys: ... S again hides it");
    // A category the player cannot use: nothing changes, the menu stays open; the current category too.
    At<unsigned char>(aGameScreenK) = 7;
    At<unsigned char>(aCategoryOpen + 2) = 0;
    Check(k::Translate(WM_KEYDOWN, 'I', 1) && At<int>(aBuildCategory) == 0 && At<int>(aOverlayShown) == 0 &&
              At<unsigned char>(aGameScreenK) == 7 && At<int>(aButtonParam) == 77 &&
              LogContains("keys: overlay 3: its category cannot be used now"),
          "overlay keys: a category that cannot be used: nothing changed, logged");
    At<unsigned char>(aCategoryOpen) = 0;
    At<unsigned char>(aGameScreenK) = 1;
    Check(k::Translate(WM_KEYDOWN, 'S', 1) && At<int>(aOverlayShown) == 0,
          "overlay keys: the current category (0) cannot be used: S does nothing either");
    At<int>(aAllCategories) = 1;
    Check(k::Translate(WM_KEYDOWN, 'I', 1) && At<int>(aBuildCategory) == 2 && At<int>(aOverlayShown) == 3,
          "overlay keys: every category usable (0x665df0): I shows industry");
    At<int>(aAllCategories) = 0;
    At<unsigned char>(aCategoryOpen) = 1;
    At<unsigned char>(aCategoryOpen + 2) = 1;
    // Rebound: the default letter goes to the game (Zeus does nothing with it); no default: only once bound.
    BindKey("overlay_supplies", "F5");
    Check(!k::Translate(WM_KEYDOWN, 'S', 1) && !k::Translate(WM_CHAR, 's', 1),
          "overlay keys: S bound to nothing: to the game as it is");
    BindKey("overlay_roads", "Q");
    Check(k::Translate(WM_KEYDOWN, 'Q', 1) && At<int>(aBuildCategory) == 10 && At<int>(aOverlayShown) == 19,
          "overlay keys: roads (no default) bound to Q: shown");
    // The editor: not translated.
    At<int>(aEditorModeK) = 1;
    const int shown = At<int>(aOverlayShown);
    Check(!k::Translate(WM_KEYDOWN, 'W', 1) && At<int>(aOverlayShown) == shown,
          "overlay keys: the editor's map: W is the editor's own");
    At<int>(aEditorModeK) = 0;
}

// The game's overlay_select faults: everything the key changed back, the overlay keys off, the other keys work.
void OverlayKeysFault()
{
    namespace k = core::input::keys;
    SetupOverlayKeys();
    At<unsigned char>(aGameScreenK) = 7;
    g_selectFaults = true;
    Check(k::Translate(WM_KEYDOWN, 'W', 1), "overlay keys: a fault: the key taken");
    g_selectFaults = false;
    Check(At<int>(aBuildCategory) == 0 && At<int>(aButtonParam) == 77 && At<unsigned char>(aGameScreenK) == 7 &&
              At<int>(aOverlayShown) == 0,
          "overlay keys: ... category, button_param, screen and overlay back");
    Check(LogContains("feature \"overlay keys (games/zeus/keys)\" switched off"), "overlay keys: ... switched off");
    Check(k::Translate(WM_KEYDOWN, 'W', 1) && At<int>(aBuildCategory) == 0 && LogContains("keys: the overlay keys are off"),
          "overlay keys: ... afterwards W does nothing");
    g_keysSentCount = 0;
    At<unsigned char>(aGameScreenK) = 1;
    Check(k::Translate(WM_KEYDOWN, 'P', 1) && SentOne(WM_CHAR, 'p'), "overlay keys: ... the other keys work (P)");
}

// --- Esc and the pause menu (games/zeus/game_menu with core/options/pause_menu). The fakes are thunks at
// the game's functions, each __cdecl as the game's: modal_dialog_run(title, text,
// buttons) answers what the scenario says; the File menu's functions and city_cancel_tool count their calls and read
// button_param; input_update_mouse clears the right-button edge as the game does each pass.
constexpr DWORD aRightUpM = 0xf2a6fc, aModalDoneM = 0x10df2e8, aModalChoiceM = 0x10df2ec, aModalButtonsM = 0x10df2f0,
                aMapSelM = 0x903120, aMapSel2M = 0x5fb7d0, aQuitM = 0xf1389d, aBuildTypeM = 0x665dcc,
                aBuildingM = 0x665dc8, aEditorToolM = 0xf135ec, aDialogXM = 0xf2a6ac, aDialogYM = 0xf2a6a8;
int g_modalAnswer = 7, g_modalCalls = 0, g_cancelCalls = 0, g_mapToCityCalls = 0, g_loadCalls = 0,
    g_replayCalls = 0, g_leaveCalls = 0, g_editorCalls = 0, g_editorParam = -1, g_loadParam = -1;
std::string g_modalTitle, g_modalText;
bool g_cancelFaults = false;

int __cdecl FakeModal(const char *title, const char *text, int)
{
    ++g_modalCalls;
    g_modalTitle = title;
    g_modalText = text;
    return g_modalAnswer;
}
char __cdecl FakeCityCancel()
{
    if (g_cancelFaults) {
        Fault();
    }
    ++g_cancelCalls;
    At<int>(aBuildTypeM) = 0;
    return 1;
}
void __cdecl FakeMapToCity() { ++g_mapToCityCalls; }
void __cdecl FakeInputMouseM() { At<char>(aRightUpM) = 0; }
void __cdecl FakeLoadM()
{
    ++g_loadCalls;
    g_loadParam = At<int>(aButtonParam);
}
void __cdecl FakeReplayM() { ++g_replayCalls; }
void __cdecl FakeLeaveM() { ++g_leaveCalls; }
void __cdecl FakeEditorItem()
{
    ++g_editorCalls;
    g_editorParam = At<int>(aButtonParam);
}
void __cdecl FakeUiSound(int) {}
// image_group_get: __thiscall (ecx = a pak's groups), 1 argument, ret 4 -- reached as __fastcall (edx unused, the
// callee cleans); the entry's +4 is the group's first image (here the group number, for the font).
BYTE g_groupEntry[8];
BYTE *__fastcall FakeImageGroupM(void *, void *, int group)
{
    *reinterpret_cast<int *>(g_groupEntry + 4) = group;
    return g_groupEntry;
}
// text_get_width: __cdecl (text, font): 7 pixels a character in the confirmation's text font (group 1), 9 in the
// title's measuring font (group 6), as the real fonts differ.
int __cdecl FakeTextWidthM(const char *text, int font)
{
    return static_cast<int>(std::strlen(text)) * (font == 6 ? 9 : 7);
}

void SetupGameMenu()
{
    SetupKeys(); // lang_get_string's fake ("the game's text"), screen 1, not the editor
    Thunk(0x40fcf0, reinterpret_cast<void *>(&FakeImageGroupM));
    Thunk(0x507da0, reinterpret_cast<void *>(&FakeTextWidthM));
    Thunk(0x5a0980, reinterpret_cast<void *>(&FakeModal));
    Thunk(0x404c70, reinterpret_cast<void *>(&FakeCityCancel));
    Thunk(0x408bb0, reinterpret_cast<void *>(&FakeMapToCity));
    Thunk(0x50b470, reinterpret_cast<void *>(&FakeInputMouseM));
    Thunk(0x4014d0, reinterpret_cast<void *>(&FakeLoadM));
    Thunk(0x421730, reinterpret_cast<void *>(&FakeReplayM));
    Thunk(0x401170, reinterpret_cast<void *>(&FakeLeaveM));
    Thunk(0x5a4c00, reinterpret_cast<void *>(&FakeUiSound));
    for (DWORD a : {0x4081f0ul, 0x408250ul, 0x408600ul, 0x51ebe0ul, 0x51ec50ul, 0x401cd0ul, 0x425120ul, 0x427100ul,
                    0x427140ul, 0x425190ul, 0x42a8e0ul}) {
        Thunk(a, reinterpret_cast<void *>(&FakeEditorItem));
    }
    At<int>(aDialogXM) = 0;
    At<int>(aDialogYM) = 0;
    At<int>(aBuildTypeM) = 0;
    At<int>(aBuildingM) = 0;
    At<char>(aEditorToolM) = 0;
    At<int>(aMapSelM) = 0;
    At<int>(aMapSel2M) = -1;
    At<char>(aQuitM) = 0;
    zeus::game_menu::AttachForTest();
}

bool EscM(LPARAM l = 1) // a WM_KEYDOWN of Esc (repeat count 1); held: bit 30
{
    return zeus::game_menu::Escape(l);
}

std::string Trimmed(const std::string &t)
{
    const size_t a = t.find_first_not_of(' '), b = t.find_last_not_of(' ');
    return a == std::string::npos ? std::string() : t.substr(a, b - a + 1);
}

// A click on the pause menu's item i (n items), in the game's dialog coordinates (dialog_offset 0, 0), with Pericles'
// look (the game's Display options).
void ClickPauseItem(int i, int n)
{
    core::options::Menu m;
    m.SetLook(zeus::painter::MenuLook());
    m.Set("Pause", std::vector<std::string>(static_cast<size_t>(n), "x"));
    const int x = m.ButtonX() + 20, y = m.ButtonY(i) + 10;
    core::options::pause_menu::HandleMessage(WM_LBUTTONDOWN, 0, 0, x, y);
    core::options::pause_menu::HandleMessage(WM_LBUTTONUP, 0, 0, x, y);
}

void EscAndPause()
{
    namespace pm = core::options::pause_menu;
    SetupGameMenu();
    // A confirmation: its last button; a held Esc does nothing more.
    At<unsigned char>(aGameScreenK) = 0x54;
    At<int>(aModalButtonsM) = 2;
    At<int>(aModalChoiceM) = 0;
    At<char>(aModalDoneM) = 0;
    Check(EscM() && At<int>(aModalChoiceM) == 1 && At<char>(aModalDoneM) == 1, "esc: a confirmation: No (its last button)");
    At<char>(aModalDoneM) = 0;
    Check(EscM(1 | (1 << 30)) && At<char>(aModalDoneM) == 0, "esc: ... held: taken, nothing more");
    // Posted by the game (lParam 0): the game's own.
    At<unsigned char>(aGameScreenK) = 1;
    Check(!EscM(0) && !pm::IsOpen(), "esc: posted by the game (lParam 0): the game's own Esc");
    // The main menu: our Quit / Exit Game? box; No: nothing; Yes: the game quits.
    At<unsigned char>(aGameScreenK) = 0;
    g_modalAnswer = 7;
    Check(EscM() && g_modalCalls == 1 && At<char>(aQuitM) == 0 && !pm::IsOpen(),
          "esc: the main menu: asks (No: stays, no pause menu)");
    Check(Trimmed(g_modalTitle) == "the game's text" && Trimmed(g_modalText) == "the game's text" &&
              std::max(FakeTextWidthM(g_modalText.c_str(), 1), FakeTextWidthM(g_modalTitle.c_str(), 6)) >=
                  FakeTextWidthM("the game's text", 6),
          "esc: ... Quit / Exit Game?, the box at least as wide as the text in the title's font");
    g_modalAnswer = 6;
    Check(EscM() && At<char>(aQuitM) == 1, "esc: ... Yes: quit_request");
    At<char>(aQuitM) = 0;
    // A dialog and a menu screen: a right click for the next input pass (after input_update_mouse's own edges).
    for (int s : {0x05, 0x09, 0x21, 0x3b, 0x02, 0x1f, 0x2f, 0x35}) {
        At<unsigned char>(aGameScreenK) = static_cast<unsigned char>(s);
        const bool taken = EscM();
        zeus::game_menu::InputMouseForTest();
        char what[96];
        std::snprintf(what, sizeof what, "esc: screen 0x%02x: a right click in the next input pass", s);
        Check(taken && At<char>(aRightUpM) == 1, what);
        zeus::game_menu::InputMouseForTest();
        Check(At<char>(aRightUpM) == 0, "esc: ... only once");
    }
    // Screens that do not read a right click (but 0x35): the game's own Esc.
    for (int s : {0x2d, 0x4c}) {
        At<unsigned char>(aGameScreenK) = static_cast<unsigned char>(s);
        char what[96];
        std::snprintf(what, sizeof what, "esc: screen 0x%02x: the game's own Esc", s);
        Check(!EscM(), what);
    }
    // The timed picture, ratings, scoreboard, victory: Esc taken, nothing else (no right click).
    for (int s : {0x04, 0x33, 0x37, 0x3a, 0x42, 0x43}) {
        At<unsigned char>(aGameScreenK) = static_cast<unsigned char>(s);
        char what[96];
        std::snprintf(what, sizeof what, "esc: screen 0x%02x: taken, nothing happens", s);
        Check(EscM() && g_modalCalls == 2 && !pm::IsOpen(), what);
    }
    zeus::game_menu::InputMouseForTest();
    Check(At<char>(aRightUpM) == 0, "esc: ... and no right click");
    // The world map: the selection let go first, then back to the city.
    At<unsigned char>(aGameScreenK) = 0x14;
    At<int>(aMapSelM) = 5;
    Check(EscM() && At<int>(aMapSelM) == 0 && At<int>(aMapSel2M) == -1 && g_mapToCityCalls == 0,
          "esc: the world map: the selection let go");
    Check(EscM() && g_mapToCityCalls == 1, "esc: ... then back to the city");
    // The city: a tool dropped (city_cancel_tool), a company / ship let go (a right click), else the pause menu.
    At<unsigned char>(aGameScreenK) = 1;
    At<int>(aBuildTypeM) = 3;
    Check(EscM() && g_cancelCalls == 1 && !pm::IsOpen(), "esc: the city with a tool: dropped, no pause menu");
    for (int s : {0x48, 0x20}) {
        At<unsigned char>(aGameScreenK) = static_cast<unsigned char>(s);
        Check(EscM() && !pm::IsOpen(), "esc: a company (0x48) / ship (0x20) selected: taken");
        zeus::game_menu::InputMouseForTest();
        Check(At<char>(aRightUpM) == 1, "esc: ... as the game's right click");
    }
    At<unsigned char>(aGameScreenK) = 1;
    Check(EscM() && pm::IsOpen() && zeus::game_menu::IsOpen() && LogContains("pause menu: opened"),
          "esc: the city with nothing to cancel: the pause menu");
    // Its items: Load (2 of 7) closes the menu and runs at the next loop with button_param 0.
    At<int>(aButtonParam) = 77;
    ClickPauseItem(2, 7);
    Check(!pm::IsOpen() && g_loadCalls == 0, "pause menu: Load: closed, not run in the window procedure");
    Check(zeus::game_menu::OnLoop() && g_loadCalls == 1 && g_loadParam == 0 && !zeus::game_menu::OnLoop(),
          "pause menu: ... run once at the next loop, button_param 0 (as the File menu)");
    // Replay map: asks with our text and the button's name; No: the pause menu again.
    EscM();
    g_modalAnswer = 7;
    ClickPauseItem(1, 7);
    Check(zeus::game_menu::OnLoop() && zeus::game_menu::IsOpen() && g_replayCalls == 0 &&
              Trimmed(g_modalTitle) == core::options::menu_text::kMenuReplayMap &&
              Trimmed(g_modalText) == core::options::menu_text::kLeaveGame,
          "pause menu: Replay map: asked (\"Replay map\", our text); No: the pause menu again");
    g_modalAnswer = 6;
    ClickPauseItem(5, 7); // Exit to main menu
    Check(zeus::game_menu::OnLoop() && !zeus::game_menu::IsOpen() && g_leaveCalls == 1,
          "pause menu: Exit to main menu: Yes: the game's New adventure (to the game choice)");
    EscM();
    ClickPauseItem(0, 7);
    Check(!pm::IsOpen() && !zeus::game_menu::OnLoop(), "pause menu: Continue: closed, nothing to run");
}

void EscEditor()
{
    namespace pm = core::options::pause_menu;
    SetupGameMenu();
    At<int>(aEditorModeK) = 1;
    At<unsigned char>(aGameScreenK) = 0x58;
    Check(!EscM() && !pm::IsOpen(), "esc editor: the city editor 0x58: the game's own Esc (no pause menu)");
    for (int s : {0x18, 0x29, 0x46, 0x0d, 0x03, 0x1f}) { // 0x1f: the editor's first screen (choose an adventure)
        At<unsigned char>(aGameScreenK) = static_cast<unsigned char>(s);
        const bool taken = EscM();
        zeus::game_menu::InputMouseForTest();
        Check(taken && At<char>(aRightUpM) == 1, "esc editor: a page / dialog that goes back with a right click");
    }
    At<unsigned char>(aGameScreenK) = 0x01;
    At<char>(aEditorToolM) = 1;
    Check(EscM() && !pm::IsOpen(), "esc editor: the map with a tool held: taken ...");
    zeus::game_menu::InputMouseForTest();
    Check(At<char>(aRightUpM) == 1, "esc editor: ... as the game's right click (drops the tool)");
    At<char>(aEditorToolM) = 0;
    At<int>(aBuildTypeM) = 28; // a brush (as a test log showed)
    Check(EscM() && !pm::IsOpen(), "esc editor: the map with a brush (build_tool 28): taken ...");
    zeus::game_menu::InputMouseForTest();
    Check(At<char>(aRightUpM) == 1, "esc editor: ... as the game's right click (drops the brush)");
    At<int>(aBuildTypeM) = 0;
    Check(EscM() && pm::IsOpen(), "esc editor: the map: the editor pause menu");
    ClickPauseItem(3, 6); // Save map
    Check(zeus::game_menu::OnLoop() && g_editorCalls == 1 && g_editorParam == 0 && LogContains("editor_menu_save_map"),
          "esc editor: ... Save map: the File menu's function, button_param 0");
    At<unsigned char>(aGameScreenK) = 0x57;
    Check(EscM() && pm::IsOpen(), "esc editor: the adventure editor: its pause menu");
    ClickPauseItem(5, 7); // Create Adventure
    Check(zeus::game_menu::OnLoop() && g_editorCalls == 2 && g_editorParam == 1 &&
              LogContains("editor_menu_create_adventure"),
          "esc editor: ... Create Adventure with button_param 1 (its File menu item's)");
    At<unsigned char>(aGameScreenK) = 0x15;
    Check(EscM() && pm::IsOpen(), "esc editor: the world map editor: its pause menu");
    ClickPauseItem(0, 5);
    At<int>(aEditorModeK) = 0;
}

// city_cancel_tool faults: Esc off (the game's own from then on), nothing of ours open.
void EscFault()
{
    namespace pm = core::options::pause_menu;
    SetupGameMenu();
    At<unsigned char>(aGameScreenK) = 1;
    At<int>(aBuildTypeM) = 3;
    g_cancelFaults = true;
    Check(!EscM() && !pm::IsOpen(), "esc: a fault in the game's function: the game's own Esc");
    g_cancelFaults = false;
    Check(LogContains("feature \"Esc and the pause menu (games/zeus/game_menu)\" switched off") &&
              LogContains("game menu: off after an error"),
          "esc: ... switched off, logged");
    At<int>(aBuildTypeM) = 0;
    Check(!EscM() && !pm::IsOpen(), "esc: ... afterwards Esc is the game's (no pause menu)");
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

// --- The notice boxes (games/zeus/notice). city_warning_show(id, arg, 0) is __cdecl (3 arguments, ret, the caller
// cleans), as are text_get_width (2), text_length (1) and text_copy (3): the fakes are the same.
constexpr DWORD aSlots = 0xfd3438, aWarningText = 0x916af8, aWarningsOn = 0xf2a7c9, aFrameTime = 0xf9c0f4;
constexpr DWORD aGroupTable = 0x12afbec, aGroupCount = 0x12bc820;
constexpr DWORD aFontGroups = 0x1600000; // two fake groups of 8 bytes near the image's end (not used by the tests)
struct ShowCall {
    int calls = 0, id = 0, warningsOn = -1;
    char text[200] = {};
};
ShowCall g_show;
int g_widthFont = 0;

int __cdecl FakeTextWidth(const char *text, int font)
{
    g_widthFont = font;
    return static_cast<int>(std::strlen(text)) * 7;
}
short __cdecl FakeTextLength(const char *text)
{
    return static_cast<short>(std::strlen(text));
}
void __cdecl FakeTextCopy(const char *text, char *to, int count)
{
    std::memcpy(to, text, count);
}

// As the game's: warnings off or screen 0 -> nothing; the first slot not in use; id 1000 takes city_warning_text.
void __cdecl FakeShow(int id, int, int)
{
    ++g_show.calls;
    g_show.id = id;
    g_show.warningsOn = At<BYTE>(aWarningsOn);
    std::memcpy(g_show.text, reinterpret_cast<const void *>(aWarningText + g_imageShift), sizeof(g_show.text));
    if (Fires("city_warning_show")) {
        Fault();
    }
    if (At<BYTE>(aWarningsOn) == 0 || At<unsigned char>(0xf2a6dd) == 0) {
        return;
    }
    for (int k = 0; k < 5; ++k) {
        const DWORD slot = aSlots + k * 0x78;
        if (At<short>(slot + 6) != 0) {
            continue;
        }
        At<short>(slot + 6) = 1;
        At<short>(slot) = static_cast<short>(id);
        const char *text = reinterpret_cast<const char *>(aWarningText + g_imageShift);
        const int n = static_cast<int>(std::strlen(text));
        const int w = n * 7;
        At<short>(slot + 0xa) = static_cast<short>(w < 0x65 ? 200 : w < 0xc9 ? 300 : w > 300 ? 460 : 400);
        At<short>(slot + 8) = static_cast<short>(n);
        At<int>(slot + 0xc) = At<int>(aFrameTime);
        char *to = reinterpret_cast<char *>(slot + 0x12 + g_imageShift);
        std::memset(to, 0, 100);
        std::memcpy(to, text, n);
        return;
    }
}

const char *SlotText(int k)
{
    return reinterpret_cast<const char *>(aSlots + k * 0x78 + 0x12 + g_imageShift);
}

short SlotShort(int k, int offset)
{
    return At<short>(aSlots + k * 0x78 + offset);
}

void FillGameText()
{
    char *game = reinterpret_cast<char *>(aWarningText + g_imageShift);
    for (int i = 0; i < 200; ++i) {
        game[i] = static_cast<char>('a' + i % 26); // the game's own text there (a city message, a save's block)
    }
}

bool GameTextKept()
{
    const char *game = reinterpret_cast<const char *>(aWarningText + g_imageShift);
    for (int i = 0; i < 200; ++i) {
        if (game[i] != static_cast<char>('a' + i % 26)) {
            return false;
        }
    }
    return true;
}

void SetupNotice()
{
    Thunk(0x5231e0, reinterpret_cast<const void *>(&FakeShow));
    Thunk(0x507da0, reinterpret_cast<const void *>(&FakeTextWidth));
    Thunk(0x507f00, reinterpret_cast<const void *>(&FakeTextLength));
    Thunk(0x507d10, reinterpret_cast<const void *>(&FakeTextCopy));
    At<DWORD>(aGroupTable) = aFontGroups + g_imageShift; // the game keeps a pointer there
    At<int>(aFontGroups + 4) = 0x1111;                    // group 0's first image
    At<int>(aFontGroups + 8 + 4) = 0x2222;                // group 1's
    At<int>(aGroupCount) = 0;
    At<unsigned char>(0xf2a6dd) = 0x01;
    At<BYTE>(aWarningsOn) = 0; // the player's warnings off: ours are shown all the same
    At<int>(aFrameTime) = 5000;
    FillGameText();
    zeus::notice::InstallForTest();
}

void NoticeBoxes()
{
    SetupNotice();
    core::ui::notice::Post("Game [city] text [x");
    core::ui::notice::OnFrame();
    Check(g_show.calls == 1 && g_show.id == 1000 && std::strcmp(g_show.text, "Game (city] text (x") == 0,
          "city_warning_show(1000) with our text in city_warning_text, '[' replaced");
    Check(g_show.warningsOn == 1 && At<BYTE>(aWarningsOn) == 0, "warnings_on 1 for the call only");
    Check(GameTextKept(), "city_warning_text's 200 bytes put back");
    Check(SlotShort(0, 6) == 1 && std::strcmp(SlotText(0), "Game (city] text (x") == 0 && SlotShort(0, 0xa) == 300 &&
              g_widthFont == 0x1111,
          "the box: slot 0, 133 px -> 300 (the game's rule), measured with group 0's first image");
    At<int>(aGroupCount) = 3;
    core::ui::notice::Post("Minimap screenshot saved in the shots"); // 37 x 7 = 259 px
    core::ui::notice::OnFrame();
    Check(SlotShort(1, 0xa) == 400 && g_widthFont == 0x2222,
          "259 px -> 400; group 1's first image when the entry count is above 0");
    const std::string mid(45, 'm'); // 315 px: the game says 460, ours 400 up to 320
    core::ui::notice::Post(mid.c_str());
    core::ui::notice::OnFrame();
    Check(SlotShort(2, 0xa) == 400, "315 px: our 400 box (the game would take 460)");
    const std::string wide(47, 'w'); // 329 px
    core::ui::notice::Post(wide.c_str());
    core::ui::notice::OnFrame();
    Check(SlotShort(3, 0xa) == 460, "329 px: 460");
    At<short>(aSlots + 3 * 0x78 + 6) = 0; // its time is up

    // The zoom box: made, then rewritten in place.
    At<int>(aFrameTime) = 6000;
    core::ui::notice::Zoom(50);
    core::ui::notice::OnFrame();
    Check(SlotShort(3, 6) == 1 && std::strcmp(SlotText(3), "Zoom: 50%") == 0, "zoom 50: a box in slot 3");
    const int calls = g_show.calls;
    At<int>(aFrameTime) = 7000;
    core::ui::notice::Zoom(125);
    core::ui::notice::OnFrame();
    Check(g_show.calls == calls && std::strcmp(SlotText(3), "Zoom: 125%") == 0 &&
              At<int>(aSlots + 3 * 0x78 + 0xc) == 7000 && SlotShort(3, 8) == 10 && SlotShort(4, 6) == 0,
          "zoom 125: slot 3 rewritten in place (text, length, time), no new box");

    // Five boxes full: ours not passed to the game.
    core::ui::notice::Post("fifth");
    core::ui::notice::OnFrame();
    const int callsFull = g_show.calls;
    core::ui::notice::Post("sixth");
    core::ui::notice::OnFrame();
    Check(SlotShort(4, 6) == 1 && g_show.calls == callsFull, "five boxes shown: the sixth not passed to the game");

    // The zoom box gone and its slot taken by the game's own Sound Frags (also id 1000): a new zoom box elsewhere.
    for (int k = 0; k < 5; ++k) {
        At<short>(aSlots + k * 0x78 + 6) = 0;
    }
    At<short>(aSlots + 3 * 0x78 + 6) = 1;
    At<short>(aSlots + 3 * 0x78) = 1000;
    std::strcpy(const_cast<char *>(SlotText(3)), "Sound Frags 3");
    core::ui::notice::Zoom(75);
    core::ui::notice::OnFrame();
    Check(std::strcmp(SlotText(3), "Sound Frags 3") == 0 && std::strcmp(SlotText(0), "Zoom: 75%") == 0,
          "the game's Sound Frags box (id 1000) left alone; a new zoom box in the first free slot");
    Check(GameTextKept(), "city_warning_text still the game's after all of them");

    // The smoke test's notice action posts to the same queue; red is shown as the others.
    char why[200] = "";
    const int before = g_show.calls;
    Check(zeus::smoke::Action("notice_red", why, sizeof(why)), "smoke: notice_red");
    core::ui::notice::OnFrame();
    Check(g_show.calls == before + 1 && std::strcmp(g_show.text, "Red test: Zeus shows it in the plain font.") == 0 &&
              SlotShort(1, 0) == 1000,
          "smoke: the red text shown through id 1000 as the others");
    Check(LogContains("zeus: notice (red): Red test"), "log: the red text");

    // The box widths are logged for the first 20 boxes only (the zoom box is opened again and
    // again); then one line says so.
    Check(!LogContains("the box widths are not logged any more"), "fewer than 21 boxes: each width logged");
    for (int i = 0; i < 16; ++i) {
        for (int k = 0; k < 5; ++k) {
            At<short>(aSlots + k * 0x78 + 6) = 0;
        }
        core::ui::notice::Post("box");
        core::ui::notice::OnFrame();
    }
    Check(LogContains("zeus: notice: the box widths are not logged any more"), "the 21st box: one line, then no more");
    for (int k = 0; k < 5; ++k) {
        At<short>(aSlots + k * 0x78 + 6) = 0;
    }

    // A fault inside city_warning_show: city_warning_text and warnings_on put back, notices off, the caption line.
    g_fault = "city_warning_show";
    core::ui::notice::Post("faults");
    core::ui::notice::Post("not reached");
    const int callsFault = g_show.calls;
    core::ui::notice::OnFrame();
    Check(g_show.calls == callsFault + 1 && std::strcmp(g_show.text, "faults") == 0,
          "the fault: in the call with our text (the second text not tried)");
    Check(GameTextKept() && At<BYTE>(aWarningsOn) == 0, "after the fault: city_warning_text and warnings_on back");
    Check(LogContains("feature \"notices in the game's boxes (core/ui/notice)\" switched off"), "log: notices off");
    const int caption = core::output::g_captionNotices;
    core::ui::notice::Post("after");
    core::ui::notice::OnFrame();
    Check(core::output::g_captionNotices == caption + 1 && g_show.calls == callsFault + 1,
          "afterwards: our caption line, the game not called");
}

// ---------------------------------------------------------------------------------------------
// Smooth scrolling's camera (games/zeus/scroll with the core's core/scroll/camera)

constexpr DWORD fGetDirection = 0x50b7b0, fViewClamp = 0x51cd60, fSoundDecay = 0x42e560;
constexpr DWORD cCityScroll = 0x5149ae;  // a city call of scroll_get_direction (returns to 0x5149b3)
constexpr DWORD cOtherScroll = 0x516213; // another caller's (returns to 0x516218): the game's own
constexpr DWORD aViewX = 0xda2f64, aViewY = 0xda2f68, aTileW = 0x7d3bb0, aTileHalfH = 0x7d3ba4,
                aCursorX = 0xf2a6c8, aCursorY = 0xf2a6c4, aDesktopW = 0xf2a6bc, aDesktopH = 0xf2a6b8,
                aKeyUp = 0xf9c120, aScrollSpeed = 0xf2a778, aFrameCount = 0xf2aac0;

struct ScrollCalls {
    int directions = 0, clamps = 0, sounds = 0;
} g_scroll;

int __cdecl FakeGetDirection() // the game's own: "right" (3), clears nothing here
{
    ++g_scroll.directions;
    return 3;
}

void __cdecl FakeViewClamp() // the map's range 0..200 x 0..400, view_y even
{
    ++g_scroll.clamps;
    if (Fires("view_clamp")) {
        Fault();
    }
    volatile int &x = At<int>(aViewX), &y = At<int>(aViewY);
    x = x < 0 ? 0 : x > 200 ? 200 : x;
    y = y < 0 ? 0 : y > 400 ? 400 : y;
    y = y & ~1;
}

void __cdecl FakeSoundDecay()
{
    ++g_scroll.sounds;
}

using Dir_fn = int(__cdecl *)();

int ScrollCall(DWORD site) // a game frame's call of scroll_get_direction at site
{
    return reinterpret_cast<Dir_fn>(site + g_imageShift)();
}

void SetupScroll()
{
    int n = 0;
    const zeus::Detour *d = zeus::scroll::Detours(&n);
    void *direction = nullptr;
    for (int i = 0; i < n; ++i) {
        if (std::strcmp(d[i].site->name, "scroll_get_direction") == 0) {
            direction = d[i].detour;
        }
    }
    if (!direction) {
        std::printf("no detour scroll_get_direction\n");
        ExitProcess(3);
    }
    Thunk(fGetDirection, reinterpret_cast<void *>(&FakeGetDirection));
    Thunk(fViewClamp, reinterpret_cast<void *>(&FakeViewClamp));
    Thunk(fSoundDecay, reinterpret_cast<void *>(&FakeSoundDecay));
    CallSite(cCityScroll, direction);
    CallSite(cOtherScroll, direction);
    At<int>(aViewX) = 50;
    At<int>(aViewY) = 100;
    At<int>(aTileW) = 58; // a column is 60 px
    At<int>(aTileHalfH) = 15;
    At<int>(aDesktopW) = 1920;
    At<int>(aDesktopH) = 1080;
    At<int>(aCursorX) = 960;
    At<int>(aCursorY) = 540;
    At<int>(aScrollSpeed) = 50;
    zeus::scroll::StartForTest(true);
}

// Frames of about 4 ms at the city's call for ms milliseconds; true when every one returned 8 (no direction).
bool ScrollFrames(int ms)
{
    bool all8 = true;
    const DWORD end = GetTickCount() + ms;
    while (static_cast<LONG>(end - GetTickCount()) > 0) {
        At<int>(aFrameCount) = At<int>(aFrameCount) + 1;
        all8 = ScrollCall(cCityScroll) == 8 && all8;
        Sleep(4);
    }
    return all8;
}

void ScrollOk()
{
    SetupScroll();
    At<int>(aCursorX) = 1919; // the right edge
    const bool all8 = ScrollFrames(400);
    int px = 0, py = 0;
    zeus::scroll::Remainder(&px, &py);
    Check(all8 && g_scroll.directions == 0, "city call, smooth: 8 every frame (the game's own step does nothing)");
    Check(At<int>(aViewX) > 52 && At<int>(aViewY) == 100 && px >= 0 && px < 60 && py == 0,
          "the camera moved right by time (whole columns into view_x, a remainder)");
    Check(At<int>(aEdgeScrolling) == 1, "edge_scrolling set at the edge (no simulation tick meanwhile, as the game)");
    Check(g_scroll.sounds > 0, "sound_view_decay called while scrolling");
    zeus::scroll::OnFrame(1000);
    char stats[256];
    zeus::scroll::TakeStats(stats, sizeof(stats));
    Check(std::strstr(stats, "at scroll speed 50%") != nullptr &&
              std::strstr(stats, "(expected 720 px/s per axis)") != nullptr,
          "rate log: the scrolling part, expected 720 px/s at 50% (Augustus's table)");
    Check(ScrollCall(cOtherScroll) == 3 && g_scroll.directions == 1, "another caller: the game's own direction");
    Check(zeus::scroll::SetSmooth(false), "switched off");
    At<int>(aFrameCount) = At<int>(aFrameCount) + 1;
    Check(ScrollCall(cCityScroll) == 3 && g_scroll.directions == 2, "off: the game's own direction at the city call");
    zeus::scroll::Remainder(&px, &py);
    Check(px == 0 && py == 0, "off: the view on its whole tile");
    Check(zeus::scroll::Working() && !LogContains("switched off after"), "no feature switched off");
}

void DirectionFault()
{
    SetupScroll();
    At<int>(aCursorX) = 1919;
    ScrollFrames(100); // under way
    At<int>(aCursorX) = 0;       // now the left edge
    At<int>(aKeyUp) = 1;         // and the up arrow's flag (the game's window procedure)
    At<int>(aEdgeScrolling) = 7; // a sentinel
    const int x = At<int>(aViewX), y = At<int>(aViewY);
    g_fault = "view_clamp";
    int r = 8, frames = 0;
    while (std::strcmp(g_fault, "view_clamp") == 0 && frames < 200) {
        At<int>(aFrameCount) = At<int>(aFrameCount) + 1;
        r = ScrollCall(cCityScroll);
        ++frames;
        if (std::strcmp(g_fault, "view_clamp") == 0) {
            At<int>(aEdgeScrolling) = 7;
            At<int>(aKeyUp) = 1;
            Sleep(4);
        }
    }
    int px = 0, py = 0;
    zeus::scroll::Remainder(&px, &py);
    Check(!zeus::scroll::Working(), "fault in view_clamp during a smooth frame: smooth scrolling off");
    Check(At<int>(aViewX) == x && At<int>(aViewY) == y && At<int>(aEdgeScrolling) == 7 && At<int>(aKeyUp) == 1,
          "the camera, edge_scrolling and the key flag put back to the frame's start");
    Check(px == 0 && py == 0, "remainder 0 (whole tiles)");
    Check(r == 3, "that call returns the game's own direction");
    At<int>(aFrameCount) = At<int>(aFrameCount) + 1;
    Check(ScrollCall(cCityScroll) == 3, "afterwards the game's own scrolling");
    Check(!zeus::scroll::SetSmooth(true), "cannot be switched on again from the panel");
    Check(LogContains("feature \"smooth scrolling (games/zeus/scroll)\" switched off") &&
              LogContains("scroll: smooth scrolling is off after an error"),
          "log: the feature off, the fallback");
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
    Res<Tick_fn>("ui_set_sidebar_position")(); // 1734: against the right edge
    Res<Tick_fn>("sidebar_draw")();
    const int x = 1734, pictureY = 1080 - 168;
    Check(g_cityLayout.sidebars == 1 && PixelAt(x + 50, pictureY) == 600 && PixelAt(x + 185, 1079) == 767,
          "1920x1080: the backdrop's rows 600..767 (frame, picture) moved to the bottom (912..1079)");
    Check(PixelAt(x, 600) == 640 && PixelAt(x + 10, 659) == 699 && PixelAt(x + 3, 660) == 640 &&
              PixelAt(x + 10, pictureY - 1) == 640 + (pictureY - 1 - 600) % 60,
          "the left edge (11 columns: the edge and its plain blue) carried on from the picture's rows 640..699");
    const int inset = 0x1e4 + 0x4000;
    Check(DrawsOf(inset + 0, x + 11, 600) == 1 && DrawsOf(inset + 6, x + 186 - 16, 600) == 1 &&
              DrawsOf(inset + 42, x + 11, pictureY - 16) == 1 && DrawsOf(inset + 48, x + 186 - 16, pictureY - 16) == 1,
          "the inset panel edge to edge: its four corners at (1745, 600), (1904, 600), (1745, 896), (1904, 896)");
    Check(DrawsOf(inset + 8, x + 11 + 16, 616) == 1 && DrawsOf(inset + 13, x + 186 - 16, 616) == 1,
          "an inner tile and a right edge tile where the game's inner_panel_draw would put them");
    const int strip = 0x4d2 + 0x4000;
    Check(DrawsOf(strip, 1920, 0) == 0 && DrawsOf(strip, 1678, 30) == 0,
          "the sidebar against the right edge: nothing right of it; no viewport yet: no gap filled");
    At<int>(aCityRight) = 1678; // the city's clip (28 columns) left of the sidebar
    g_cityLayout.draws = 0;
    Res<Tick_fn>("sidebar_draw")();
    Check(DrawsOf(strip, 1678, 30) == 1 && DrawsOf(strip, 1726, 630) == 1 && DrawsOf(strip, 1742, 30) == 0,
          "smooth scrolling not working: the gap 1678 .. 1734 below the top bar (here the strip, without the pack)");
    zeus::scroll::StartForTest(true);
    g_cityLayout.draws = 0;
    Res<Tick_fn>("sidebar_draw")();
    Check(DrawsOf(strip, 1678, 30) == 0, "smooth scrolling working: the gap is the city's (not filled)");
    g_cityLayout.draws = 0;
    Res<Units_fn>("sidebar_draw_units")(0x48);
    Check(g_cityLayout.sidebars == 4 && DrawsOf(inset + 0, x + 11, 600) == 1, "the army's sidebar: the same filler");
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
    Check(PixelAt(x + 50, 600) == 600 && DrawsOf(inset + 0, x + 11, 600) == 0 && g_cityLayout.draws == 0,
          "768 high: the picture where the game drew it, no inset panel, nothing on the right");
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
              DrawsOf(strip, 1920, 0) == 0 && DrawsOf(strip, 1678, 30) == 0,
          "... the inset panel right of the edge; nothing right of the sidebar, no city gap on the world map");
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
    Res<Tick_fn>("ui_set_sidebar_position")(); // 1734 at 1920x1080 (against the right edge)
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
    const int shift = (1734 - 838) / 2;
    Check(At<int>(aInfoX) == 0xbc + shift && At<int>(aInfoY) == 1080 - 8 - 0x14 * 16,
          "original: the game's 768 rule at 1080, moved with the city area's centre (636, 752)");
    At<int>(aMouseY) = 700;
    place();
    Check(At<int>(aInfoY) == 0x20, "original, the cursor in the lower half: at the top (0x20)");
    core::ui::info_window::Set(Mode::Centered);
    place();
    Check(At<int>(aInfoX) == (1734 - 464) / 2 && At<int>(aInfoY) == 30 + (1080 - 30 - 320) / 2,
          "centered: in the middle of the city area (635, 395)");
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
    Check(At<int>(aInfoX) == (1734 - 464) / 2, "opened on the world map: in the middle whatever the setting");
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
    At<int>(aSidebarX) = w > 1024 ? w - 186 : 838; // against the right edge
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

    // 2560x1440, sidebar at 2374 (against the right edge): the map area is 2374 x 1410 below the top bar. The first frame after the city with
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
    const int nx = (2374 - 838) / 2, ny = 30 + (1410 - 738) / 2; // 768, 366
    Check(g_wm.draws == 1 && g_wm.drawBuffer != At<BYTE *>(aDrawBuffer) && g_wm.drawHeight == 768,
          "the first map: drawn into our layer with the game's own height 768");
    Check(At<int>(aDdHeight2) == 1440 && At<BYTE *>(aDrawBuffer) == reinterpret_cast<BYTE *>(g_wmScreen.data()),
          "dd_height and draw_buffer back afterwards");
    Check(Wm(nx, ny) == MapPattern(0, 0) && Wm(nx + 837, ny + 737) == MapPattern(837, 737) &&
              Wm(nx + 300, ny + 200) == MapPattern(300, 200),
          "native: the map pixel for pixel in the middle of the map area (768, 366)");
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
    const int tw = static_cast<int>(838 * s + 0.5), tx = (2374 - tw) / 2, ty = 30;
    At<int>(aMouseXw) = tx + static_cast<int>((419 + 0.5) * s);
    At<int>(aMouseYw) = ty + static_cast<int>((369 + 0.5) * s);
    hit(nullptr, nullptr);
    Check(g_wm.mouseX == 419 && g_wm.mouseY == 369 + 30, "scaled (fit): the middle of the screen's map is the map's");
    // The scaling fails -- logged, the game's drawing as it drew it, the real mouse.
    ms::FailScalingForTest(true);
    WorldScreen(2560, 1440);
    draw();
    ms::FailScalingForTest(false);
    Check(LogContains("could not be scaled to") && LogContains("drawn the game's way"),
          "the scaling fails: logged, drawn the game's way");
    hit(nullptr, nullptr);
    Check(g_wm.mouseX == At<int>(aMouseXw) && g_wm.mouseY == At<int>(aMouseYw),
          "the scaling failed: the real mouse (nothing of ours shown)");
    // 200% at 3840x2160 (fit 288%): each map pixel a 2 x 2 block.
    ms::SetModeForTest(0, 200);
    WorldScreen(3840, 2160); // sidebar 3654: area 3654 x 2130
    draw();
    const int ix = (3654 - 1676) / 2, iy = 30 + (2130 - 1476) / 2;
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
    Check(DrawsOf(0x4cf + 0x4000, 2374 - 838, 0) == 1, "campaign map wider than 1024: the top bar ending at the sidebar");
    At<int>(aMouseXw) = nx + 300;
    At<int>(aMouseYw) = ny + 200;
    g_cityLayout.draws = 0;
    Map<Tick_fn>("campaign_map_draw")();
    Check(g_wm.campaign == 1 && g_wm.mouseX == 300 && g_wm.mouseY == 230, "campaign map: hover in map coordinates");
    Check(Wm(nx + 100, ny + 100) == 0x0001, "campaign map: the marker shown on the map");
    Check(Wm(2374 + 5, 5) == 0x0002, "campaign map: the sidebar drawn where it belongs");
    const int inset = 0x1e4 + 0x4000;
    Check(DrawsOf(inset + 0, 2374 + 11, 600) == 1 && Wm(2374 + 50, 1440 - 168) == 600,
          "campaign map: the sidebar's filler below it (the gold edge's width, the picture at the bottom)");
    const WORD below = Wm(2374 + 93, 700);
    g_wm.noSidebar = true;
    g_cityLayout.draws = 0;
    Map<Tick_fn>("campaign_map_draw")();
    Check(DrawsOf(inset + 0, 2374 + 11, 600) == 0 && Wm(2374 + 93, 700) == below && Wm(2374 + 50, 1440 - 168) == 600,
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
        check(w <= 1024 ? sx == 838 : sx + 186 == w, "the sidebar against the right edge (the game's 838 at 1024)");
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

// ---------------------------------------------------------------------------------------------
// Smooth scrolling's picture (games/zeus/scroll with the core's core/scroll/picture)

constexpr DWORD fScreenToGrid = 0x44a460, fMouseToGrid = 0x44a270, fScreenToGridRaw = 0x433590,
                fGridToScreen = 0x56bf20, fCityWidth = 0x44a7c0, fDragStart = 0x433380, fDragDraw = 0x433400,
                fDragFinish = 0x433700, fCityRender2 = 0x44a930;
constexpr DWORD aOriginXz = 0x7d3be8, aOriginYz = 0x7d3be4, aColsZ = 0x7d3bbc, aRowsZ = 0x7d3bb8,
                aClipRightZ = 0x7d3b90, aMouseMode = 0xf2a6d9;
constexpr WORD kBackSentinel = 0x0f0f;

// What the game's conversions saw (origin, viewport, arguments, the mouse).
struct ConvSeen {
    int calls = 0, ox = 0, oy = 0, cols = 0, rows = 0, x = 0, y = 0, third = 0;
} g_conv, g_mouseConv;
int g_dragDraws = 0, g_dragSeen[2] = {}, g_portraits = 0, g_portraitTile = -1, g_portraitXy[2] = {};

void SeeOrigin(ConvSeen &s)
{
    ++s.calls;
    s.ox = At<int>(aOriginXz);
    s.oy = At<int>(aOriginYz);
    s.cols = At<int>(aColsZ);
    s.rows = At<int>(aRowsZ);
}

int __cdecl FakeScreenToGridZ(int x, int y, int third)
{
    SeeOrigin(g_conv);
    g_conv.x = x;
    g_conv.y = y;
    g_conv.third = third;
    if (Fires("screen_to_grid")) {
        Fault();
    }
    return 1234;
}

// As mouse_to_grid 0x44a270: no tile (0) outside origin + viewport, else a tile.
int __cdecl FakeMouseToGridZ()
{
    SeeOrigin(g_mouseConv);
    const int x = At<int>(aMouseX), y = At<int>(aMouseY);
    g_mouseConv.x = x;
    g_mouseConv.y = y;
    if (x < g_mouseConv.ox || x >= g_mouseConv.ox + 60 * g_mouseConv.cols || y < g_mouseConv.oy ||
        y >= g_mouseConv.oy + 15 * g_mouseConv.rows) {
        return 0;
    }
    return 1234;
}

int __cdecl FakeScreenToGridRawZ(int x, int y, int)
{
    SeeOrigin(g_conv);
    g_conv.x = x;
    g_conv.y = y;
    return 4321;
}

void __cdecl FakeGridToScreenZ(int, int, int *x, int *y) // the origin (as the game's for the view's first tile)
{
    *x = At<int>(aOriginXz);
    *y = At<int>(aOriginYz);
}

int __cdecl FakeCityWidthZ()
{
    return 60 * At<int>(aColsZ);
}

void __fastcall FakeDragNothing(int *) {}

void __fastcall FakeDragDrawZ(int *drag)
{
    ++g_dragDraws;
    g_dragSeen[0] = drag[2];
    g_dragSeen[1] = drag[3];
    if (Fires("drag_draw")) {
        Fault();
    }
}

template <typename F> F Scr(const char *name)
{
    int n;
    const zeus::Detour *d = zeus::scroll::Detours(&n);
    return Detour<F>(d, n, name);
}

using Mouse_fn = int(__cdecl *)();
using S2g_fn = int(__cdecl *)(int, int, int);
using G2s_fn = void(__cdecl *)(int, int, int *, int *);
using DragZ_fn = void(__fastcall *)(int *);
using Render2_fn = void(__cdecl *)(int, int, int, int);

// The figure portrait (city_render_2): asks for the cursor tile and a tile's position while it draws.
void __cdecl FakeCityRender2(int, int, int, int)
{
    ++g_portraits;
    g_portraitTile = Scr<Mouse_fn>("mouse_to_grid")();
    Scr<G2s_fn>("grid_to_screen")(1, 2, &g_portraitXy[0], &g_portraitXy[1]);
}

std::vector<WORD> g_back;

WORD Back(int x, int y)
{
    return reinterpret_cast<WORD *>(At<BYTE *>(aDrawBuffer))[y * (At<int>(aPitch) / 2) + x];
}

void BackBuffer(int w, int h)
{
    g_back.assign(static_cast<size_t>(w) * (h + 64), kBackSentinel);
    At<BYTE *>(aDrawBuffer) = reinterpret_cast<BYTE *>(g_back.data());
    At<int>(aPitch) = w * 2;
    At<int>(aDdWidth) = w;
    At<int>(aDdHeight2) = h;
}

void PictureFakes()
{
    Thunk(fScreenToGrid, reinterpret_cast<void *>(&FakeScreenToGridZ));
    Thunk(fMouseToGrid, reinterpret_cast<void *>(&FakeMouseToGridZ));
    Thunk(fScreenToGridRaw, reinterpret_cast<void *>(&FakeScreenToGridRawZ));
    Thunk(fGridToScreen, reinterpret_cast<void *>(&FakeGridToScreenZ));
    Thunk(fCityWidth, reinterpret_cast<void *>(&FakeCityWidthZ));
    Thunk(fDragStart, reinterpret_cast<void *>(&FakeDragNothing));
    Thunk(fDragDraw, reinterpret_cast<void *>(&FakeDragDrawZ));
    Thunk(fDragFinish, reinterpret_cast<void *>(&FakeDragNothing));
    Thunk(fCityRender2, reinterpret_cast<void *>(&FakeCityRender2));
    Thunk(fViewClamp, reinterpret_cast<void *>(&FakeViewClamp)); // the east edge asks the game's limit (camera.h)
}

// A 1920x1080 city with our layout: 28 columns x 71 half rows from (0, 30) (clip (0,30)-(1678,1080)), the sidebar
// against the right edge at 1734, the city seen to it and to the bottom; smooth scrolling attached and on.
void SetupPicture()
{
    PictureFakes();
    BackBuffer(1920, 1080);
    FakeViewportInit(0xe4, 0xe4, 0, 30, 28, 71, 58, 30);
    At<int>(aSidebarX) = 1920 - 186;
    zeus::scroll::StartForTest(true);
    zeus::scroll::SetCityExtent(1920 - 186, 1080);
}

bool ViewportIsGame()
{
    return At<int>(aOriginXz) == 0 && At<int>(aOriginYz) == 30 && At<int>(aColsZ) == 28 && At<int>(aRowsZ) == 71 &&
           At<int>(aClipRightZ) == 1678 && At<int>(aClipBottom) == 1080;
}

void RenderOk()
{
    SetupPicture();
    zeus::scroll::SetRemainderForTest(5, 6);
    BYTE *back = At<BYTE *>(aDrawBuffer);
    g_pic.marks[0] = {1700 + 5, 500 + 6, 0x1111}; // in the gap left of the sidebar
    g_pic.marks[1] = {100 + 5, 1079 + 6, 0x2222}; // the screen's last row
    g_pic.markCount = 2;
    At<int>(aMouseX) = 500;
    At<int>(aMouseY) = 500;
    g_pic.mouse = Scr<Mouse_fn>("mouse_to_grid");
    Timing<Render_fn>("city_render")(0, 0, 0);
    g_pic.mouse = nullptr;
    Check(g_calls.draws == 1 && g_pic.buffer != back, "while the game draws: draw_buffer is our city buffer");
    Check(g_pic.clipR == 1678 + 120 && g_pic.clipB == 1080 + 30 && g_pic.cols == 30 && g_pic.rows == 73,
          "while the game draws: the clip 2 columns (the remainder and the 56 px gap to the sidebar) and a row pair "
          "larger, the viewport +2 / +2");
    Check(g_mouseConv.ox == -5 && g_mouseConv.oy == 24 && g_mouseConv.cols == 30 && g_pic.mouseTile == 1234,
          "a conversion while the game draws: the origin moved by the remainder, the viewport not made larger again");
    Check(At<BYTE *>(aDrawBuffer) == back && ViewportIsGame(), "afterwards: draw_buffer, clip and viewport the game's");
    Check(Back(1700, 500) == 0x1111 && Back(100, 1079) == 0x2222,
          "shown with the remainder: drawn at (1705, 506) and (105, 1085), on the screen at (1700, 500) in the gap and "
          "(100, 1079) on the last row");
    Check(Back(1734, 500) == kBackSentinel && Back(100, 1080) == kBackSentinel && Back(5, 29) == kBackSentinel,
          "nothing written on the sidebar, below the screen or on the top bar");
    const int r = Scr<S2g_fn>("screen_to_grid")(10, 50, 77);
    Check(r == 1234 && g_conv.ox == -5 && g_conv.oy == 24 && g_conv.cols == 30 && g_conv.rows == 73 &&
              g_conv.third == 77,
          "screen_to_grid: the origin moved, the viewport 2 columns (5 + the 56 px gap) and a row pair larger, the "
          "third argument passed on");
    Check(ViewportIsGame(), "afterwards: origin and viewport the game's");
    Check(Scr<Int_fn>("city_view_width_px")() == 1734, "the Game paused banner: the city's width up to the sidebar");
    int left, top, right, bottom;
    const char *why = nullptr;
    Check(zeus::scroll::DrawnCity(&left, &top, &right, &bottom, &why) && left == 0 && top == 30 && right == 1734 &&
              bottom == 1080,
          "the menus' background: the city as shown (0, 30) .. (1734, 1080)");
    Check(LogContains("scroll: the city is shown past its clip to (1734,1080): 56 px on the right"),
          "log: the city shown past its clip");
    Check(zeus::scroll::Working(), "smooth scrolling still on");
}

// A fault while the game draws into our buffer, in a frame whose drawing counts: the half drawing before the
// fault is the frame's counting one; the drawing again never counts.
void RenderFault()
{
    SetupPicture();
    if (!zeus::timing::PrepareForTest()) {
        Check(false, "the copy buffer");
        return;
    }
    Timing<Frame_fn>("game_frame")(); // the first frame: its drawing counts
    const Drawn before = ReadDrawn();
    zeus::scroll::SetRemainderForTest(5, 6);
    BYTE *back = At<BYTE *>(aDrawBuffer);
    g_fault = "city_render";
    Timing<Render_fn>("city_render")(0, 0, 0);
    Check(!zeus::scroll::Working(), "fault while the game draws into our buffer: smooth scrolling off");
    Check(At<BYTE *>(aDrawBuffer) == back && ViewportIsGame(), "draw_buffer, clip and viewport put back");
    Check(g_calls.draws == 2 && g_pic.buffer == back && g_pic.clipR == 1678 && g_pic.cols == 28,
          "the city drawn again the game's own way (its buffer, its clip)");
    Check(g_calls.drawFlagsZero, "the drawing again: anim_due all 0 (an extra drawing never counts)");
    const Drawn after = ReadDrawn();
    Check(after.tile == before.tile + 1 && after.counter == before.counter + 1,
          "the half drawing before the fault counts (the frame's counting one), the drawing again put back");
    Check(DueIsFrame(&At<unsigned char>(aAnimDue)), "the frame's own anim_due back");
    int px, py;
    zeus::scroll::Remainder(&px, &py);
    Check(px == 0 && py == 0, "the view snaps to its whole tile (remainder 0)");
    Check(LogContains("feature \"smooth scrolling (games/zeus/scroll)\" switched off") &&
              LogContains("6 changed value(s) put back") && LogContains("smooth scrolling is off after an error"),
          "log: switched off, the 6 changed values put back (draw_buffer, the clip's right and bottom, the viewport, the drawing flag)");
    Sleep(60);
    Timing<Frame_fn>("game_frame")(); // the next counting frame
    Timing<Render_fn>("city_render")(0, 0, 0);
    Check(g_calls.draws == 3 && g_pic.buffer == back && ReadDrawn().tile == after.tile + 1,
          "afterwards: drawn the game's way, and a counting frame counts (not an extra drawing)");
    Check(zeus::timing::DrawnWorking() && !zeus::scroll::SetSmooth(true),
          "drawn-frame animations still on; smooth scrolling cannot be switched on again");
}

// The same in a frame whose drawing does not count: the outer drawing puts everything back, both halves.
void RenderFaultUncounted()
{
    SetupPicture();
    if (!zeus::timing::PrepareForTest()) {
        Check(false, "the copy buffer");
        return;
    }
    CountingThenNot();
    const Drawn counted = ReadDrawn();
    zeus::scroll::SetRemainderForTest(5, 6);
    g_fault = "city_render";
    Timing<Render_fn>("city_render")(0, 0, 0);
    Check(!zeus::scroll::Working() && g_calls.draws == 3 && g_calls.drawFlagsZero,
          "fault: off, drawn again (anim_due all 0, as the whole frame's drawings)");
    Check(Same(ReadDrawn(), counted), "the half drawing and the drawing again both put back (the frame does not count)");
    Check(DueIsFrame(&At<unsigned char>(aAnimDue)) && ViewportIsGame(), "the frame's anim_due and the viewport back");
    Check(zeus::timing::DrawnWorking(), "drawn-frame animations still on");
}

void ConvertFault()
{
    SetupPicture();
    zeus::scroll::SetRemainderForTest(5, 6);
    g_fault = "screen_to_grid";
    const int r = Scr<S2g_fn>("screen_to_grid")(10, 50, 0);
    Check(!zeus::scroll::Working(), "fault in screen_to_grid with the origin moved: smooth scrolling off");
    Check(ViewportIsGame(), "origin and viewport put back");
    Check(r == 1234 && g_conv.calls == 2 && g_conv.ox == 0 && g_conv.oy == 30,
          "converted again the game's own way, its result returned");
    BYTE *back = At<BYTE *>(aDrawBuffer);
    Timing<Render_fn>("city_render")(0, 0, 0);
    Check(g_calls.draws == 1 && g_pic.buffer == back && g_pic.clipR == 1678, "city_render passes through");
}

void DragFault()
{
    SetupPicture();
    int drag[16] = {0, 0, 300, 400}; // the game's drag structure (0x7818b0) reaches +0x30
    zeus::scroll::SetRemainderForTest(0, 0);
    Scr<DragZ_fn>("drag_select_start")(drag);
    zeus::scroll::SetRemainderForTest(7, 4);
    Scr<DragZ_fn>("drag_select_draw")(drag);
    Check(g_dragDraws == 1 && g_dragSeen[0] == 293 && g_dragSeen[1] == 396 && drag[2] == 300 && drag[3] == 400,
          "drag: the start corner moved by the remainder for the call only");
    At<int>(aViewX) = 2; // two columns scrolled since the start: the game moves the corner 2 x 58, a column is 60
    zeus::scroll::SetRemainderForTest(7, 4);
    Scr<DragZ_fn>("drag_select_draw")(drag);
    Check(g_dragSeen[0] == 300 - 7 - 4 && drag[2] == 300, "... and 2 px per column scrolled");
    g_fault = "drag_draw";
    Scr<DragZ_fn>("drag_select_draw")(drag);
    Check(!zeus::scroll::Working() && drag[2] == 300 && drag[3] == 400, "fault: the start corner put back");
    Check(g_dragDraws == 4 && g_dragSeen[0] == 300 && g_dragSeen[1] == 400, "drawn again the game's own way");
    Scr<DragZ_fn>("drag_select_finish")(drag);
    Check(drag[2] == 300, "afterwards the game's own (finish passes through)");
}

// Clicks and the cursor on the sidebar or the top bar never reach the city (Yongle's rc2_city_input), the drag
// selection's mouse kept to the city as shown, positions worked out outside the drawing moved with the city.
void CityInput()
{
    SetupPicture();
    zeus::scroll::SetRemainderForTest(5, 6);
    auto mouse = Scr<Mouse_fn>("mouse_to_grid");
    At<int>(aMouseX) = 1750; // on the sidebar (the viewport moved and made larger reaches 1735)
    At<int>(aMouseY) = 500;
    Check(mouse() == 0 && g_mouseConv.calls == 0, "the cursor on the sidebar: no tile (the game not asked)");
    At<int>(aMouseX) = 500;
    At<int>(aMouseY) = 25; // on the top bar (the origin moved up by 6 reaches 24)
    Check(mouse() == 0 && g_mouseConv.calls == 0, "the cursor on the top bar: no tile");
    At<int>(aMouseX) = 1700; // in the gap left of the sidebar: the city is shown there
    At<int>(aMouseY) = 500;
    Check(mouse() == 1234 && g_mouseConv.calls == 1 && g_mouseConv.ox == -5 && g_mouseConv.cols == 30,
          "the cursor in the gap: a tile, the origin moved and the viewport larger");
    Check(Scr<S2g_fn>("screen_to_grid")(1750, 500, 0) == 0 && g_conv.calls == 0, "screen_to_grid on the sidebar: 0");
    int sx = 0, sy = 0;
    Scr<G2s_fn>("grid_to_screen")(1, 2, &sx, &sy);
    Check(sx == -5 && sy == 24 && ViewportIsGame(), "grid_to_screen outside the drawing: moved with the city");
    zeus::scroll::SetRemainderForTest(0, 0);
    At<int>(aMouseX) = 1750;
    Check(mouse() == 0 && g_mouseConv.calls == 1, "no remainder, our layout: the sidebar is still no tile");
    // The gap at rest: the city is shown past the grid clip, so the viewport is made larger without a remainder too
    // (before, the game's own viewport said no tile there).
    At<int>(aMouseX) = 1700;
    Check(mouse() == 1234 && g_mouseConv.ox == 0 && g_mouseConv.cols == 29,
          "the gap at rest: a tile (the viewport a column larger, the origin not moved)");
    // A drag selection pressed on the city: the mouse kept to the city as shown for the corner's tile.
    zeus::scroll::SetRemainderForTest(5, 6);
    int drag[16] = {};
    At<int>(aMouseX) = 500;
    At<int>(aMouseY) = 500;
    Scr<DragZ_fn>("drag_select_start")(drag);
    Check(drag[11] == 8 + 2 && drag[12] == 8 + 2, "a drag from the city: the smallest frame 8 px (+ 2, as the game's)");
    At<unsigned char>(aMouseMode) = 0x4d;
    At<int>(aMouseX) = 1850; // dragged onto the sidebar
    At<int>(aMouseY) = 600;
    Check(mouse() == 1234 && g_mouseConv.x == 1733 && g_mouseConv.y == 600 && At<int>(aMouseX) == 1850,
          "dragged onto the sidebar: the game sees the mouse at the city's edge (1733), the real mouse afterwards");
    int sideDrag[16] = {};
    At<unsigned char>(aMouseMode) = 0;
    Scr<DragZ_fn>("drag_select_start")(sideDrag); // pressed on the sidebar
    Check(sideDrag[11] == 4 && sideDrag[12] == 4, "a drag from the sidebar: the game's own smallest frame");
    Check(zeus::scroll::Working(), "smooth scrolling still on");
}

// The layout gives the extent: the sidebar against the right edge, the city drawn up to it and to the screen's bottom
// (also with smooth_scroll off); a clip past the screen's bottom (an uneven height) shown to the bottom only; the
// game's own sizes: the clip only.
void CityExtent()
{
    SetupResolution();
    PictureFakes();
    ToCity();
    BackBuffer(1920, 1080);
    zeus::scroll::StartForTest(true);
    Res<Tick_fn>("ui_set_sidebar_position")();
    Res<Tick_fn>("city_viewport_narrow")();
    int left, top, right, bottom;
    const char *why = nullptr;
    Check(At<int>(aSidebarX) == 1734 && zeus::scroll::DrawnCity(&left, &top, &right, &bottom, &why) && left == 0 &&
              top == 30 && right == 1734 && bottom == 1080,
          "1920x1080: the sidebar at 1734, the city seen to it and to the screen's bottom");
    zeus::scroll::SetRemainderForTest(0, 0);
    g_pic.marks[0] = {1720, 500, 0x3333};
    g_pic.markCount = 1;
    Timing<Render_fn>("city_render")(0, 0, 0);
    Check(Back(1720, 500) == 0x3333 && Back(1734, 500) == kBackSentinel,
          "no remainder: the gap 1678 .. 1734 drawn by the city, the sidebar untouched");
    Check(zeus::scroll::SetSmooth(false), "smooth_scroll off");
    g_pic.marks[0].v = 0x4444;
    Timing<Render_fn>("city_render")(0, 0, 0);
    Check(Back(1720, 500) == 0x4444, "smooth_scroll off: the city still drawn to the sidebar");
    zeus::scroll::SetSmooth(true);
    At<int>(aModeH) = 1013; // a window of an uneven height: the clip 7 rows past the screen's bottom
    BackBuffer(1920, 1013);
    Res<Tick_fn>("city_viewport_narrow")();
    Check(At<int>(aClipBottom) == 1020 && zeus::scroll::DrawnCity(&left, &top, &right, &bottom, &why) &&
              bottom == 1013,
          "1920x1013: the clip at 1020, the city shown to 1013");
    zeus::scroll::SetRemainderForTest(5, 6);
    g_pic.marks[0] = {100 + 5, 1012 + 6, 0x5555};
    g_pic.marks[1] = {100 + 5, 1013 + 6, 0x6666};
    g_pic.markCount = 2;
    const int draws = g_calls.draws;
    Timing<Render_fn>("city_render")(0, 0, 0);
    Check(g_calls.draws == draws + 1 && g_pic.buffer != At<BYTE *>(aDrawBuffer) && g_pic.clipB == 1020 + 30,
          "with a remainder: drawn into our buffer although the clip passes the screen (one row pair more)");
    Check(Back(100, 1012) == 0x5555 && Back(100, 1013) == kBackSentinel,
          "the screen's last row from the city, nothing written below the screen");
    At<int>(aModeW) = 1024; // the game's own size: the clip only
    At<int>(aModeH) = 768;
    Res<Tick_fn>("ui_set_sidebar_position")();
    Res<Tick_fn>("city_viewport_narrow")();
    BackBuffer(1024, 768);
    zeus::scroll::DrawnCity(&left, &top, &right, &bottom, &why);
    const int clipR = At<int>(aClipRightZ) < 1024 ? At<int>(aClipRightZ) : 1024;
    Check(right == clipR && bottom == 765 && Scr<Int_fn>("city_view_width_px")() == 60 * At<int>(aColsZ),
          "1024x768: the game's layout, the city to its clip (no extent), the game's banner width");
    Check(zeus::scroll::Working(), "smooth scrolling still on");
}

// The east edge: 1920x1080, the clip at 1678 (the original's east edge at the
// game's limit), the city shown to 1734 (56 px past it), so the camera stops 56 px before the game's east limit (the
// fake view_clamp's 200): view_x 199 with a remainder of 60 - 56 = 4.
void EastEdge()
{
    SetupPicture();
    At<int>(aViewX) = 200; // the game's east limit (a minimap click, say)
    At<int>(aViewY) = 100;
    zeus::scroll::SetRemainderForTest(0, 0);
    g_pic.marks[0] = {1700 + 4, 500, 0x7777};
    g_pic.markCount = 1;
    Timing<Render_fn>("city_render")(200, 100, 0);
    int px, py;
    zeus::scroll::Remainder(&px, &py);
    Check(At<int>(aViewX) == 199 && At<int>(aViewY) == 100 && px == 4 && py == 0,
          "at the game's east limit: the view pulled back to column 199 with a 4 px remainder");
    Check(g_pic.vx == 199 && g_pic.vy == 100, "the game draws the view as pulled back");
    Check(Back(1700, 500) == 0x7777, "shown with the remainder (drawn at 1704, on the screen at 1700)");
    Timing<Render_fn>("city_render")(199, 100, 0);
    zeus::scroll::Remainder(&px, &py);
    Check(At<int>(aViewX) == 199 && px == 4 && g_pic.vx == 199, "the next frame: unchanged");
    At<int>(aViewX) = 150;
    zeus::scroll::SetRemainderForTest(0, 0);
    Timing<Render_fn>("city_render")(150, 100, 0);
    zeus::scroll::Remainder(&px, &py);
    Check(At<int>(aViewX) == 150 && px == 0 && g_pic.vx == 150, "inside the map: nothing changed");
    Check(zeus::scroll::SetSmooth(false), "smooth_scroll off");
    At<int>(aViewX) = 200; // the game's own whole-tile step to its limit
    Timing<Render_fn>("city_render")(200, 100, 0);
    zeus::scroll::Remainder(&px, &py);
    Check(At<int>(aViewX) == 199 && px == 4 && g_pic.vx == 199, "smooth_scroll off: pulled back the same way");
    Check(zeus::scroll::Working(), "smooth scrolling (the feature) still on");
}

// The figure portrait (city_render_2) with a remainder: its conversions are the game's own (no origin moved), the
// remainder back afterwards.
void FigurePortrait()
{
    SetupPicture();
    zeus::scroll::SetRemainderForTest(5, 6);
    At<int>(aMouseX) = 500;
    At<int>(aMouseY) = 500;
    Timing<Render2_fn>("city_render_2")(0, 0, 0, 0);
    Check(g_portraits == 1 && g_portraitTile == 1234 && g_mouseConv.ox == 0 && g_mouseConv.oy == 30 &&
              g_mouseConv.cols == 28,
          "while the portrait draws: mouse_to_grid sees the game's origin and viewport");
    Check(g_portraitXy[0] == 0 && g_portraitXy[1] == 30, "... and grid_to_screen gives the game's position");
    int px, py;
    zeus::scroll::Remainder(&px, &py);
    Check(px == 5 && py == 6 && ViewportIsGame(), "afterwards: the remainder (5, 6) back, the viewport the game's");
    int sx = 0, sy = 0;
    Scr<G2s_fn>("grid_to_screen")(1, 2, &sx, &sy);
    Check(sx == -5 && sy == 24, "and conversions outside it moved again");
    Check(zeus::scroll::Working(), "smooth scrolling still on");
}

int g_steps = 0;
void __cdecl FakeNothingStep(int)
{
    ++g_steps;
}

// Attach (the install flow): the sites are not the game's here, so nothing is installed and the log says which; the
// game's own drawing and scrolling then. view_scroll_step's detour counts the game's own steps for the rate log.
void ScrollAttach()
{
    PictureFakes();
    BackBuffer(1920, 1080);
    FakeViewportInit(0xe4, 0xe4, 0, 30, 28, 71, 58, 30);
    Check(!zeus::scroll::Attach(true) && !zeus::scroll::Working(), "Attach where the sites do not match: false, off");
    Check(LogContains("zeus: scroll_get_direction at 0x50b7b0 does not match; smooth scrolling not installed"),
          "log: the first site that does not match");
    BYTE *back = At<BYTE *>(aDrawBuffer);
    Timing<Render_fn>("city_render")(0, 0, 0);
    Check(g_calls.draws == 1 && g_pic.buffer == back && g_pic.clipR == 1678, "not installed: the game's own drawing");
    zeus::scroll::StartForTest(false); // installed, smooth off: the game's own steps counted
    char stats[256];
    zeus::scroll::TakeStats(stats, sizeof(stats));
    using Step_fn = void(__cdecl *)(int);
    Thunk(0x51cad0, reinterpret_cast<void *>(&FakeNothingStep));
    auto step = Scr<Step_fn>("view_scroll_step");
    step(1); // right and up
    step(2); // right
    step(8); // none
    At<int>(aEdgeScrolling) = 1;
    zeus::scroll::OnFrame(100);
    zeus::scroll::TakeStats(stats, sizeof(stats));
    Check(std::strstr(stats, "x 1200 px/s, y 600 px/s, 2 steps") != nullptr && g_steps == 3,
          "view_scroll_step: the game's own steps counted (2 with a direction: 120 px across, 60 px down in 100 ms), "
          "each passed to the game");
}

// The notice boxes not hooked (Attach fails: the sites are not the game's here): our messages on the caption line,
// logged with Pericles's prefix (before, the core's default "notice").
void NoticePrefix()
{
    Check(!zeus::notice::Attach(), "Attach where the sites do not match: false");
    const int caption = core::output::g_captionNotices;
    core::ui::notice::Post("unhooked");
    Check(core::output::g_captionNotices == caption + 1 && LogContains("zeus: notice: unhooked"),
          "the caption line, logged as zeus: notice");
}

// ---------------------------------------------------------------------------------------------
// Zoom: the core's zoom (core/scroll/zoom) with Zeus's layer -- the wheel and the middle
// button on the city screens only, the layout's view for the scale (Zeus's bottom margin rule), the message panel and
// the bottom strip in the city as shown, the zoomed drawing, conversions and the drag's lines (Yongle's zoom_conversions), and
// the zoom switched off when the scaling fails or smooth scrolling faults.

constexpr DWORD aMapColMin = 0xfa05b4, aMapColMax = 0xfa05b0, aMapRowMin = 0xfa05ac, aMapRowMax = 0xfa05a8,
                aBottomMargin = 0x8f9820, aDirection = 0xda2f88, aViewDrawnX = 0x7d3bd0, aViewDrawnY = 0x7d3bcc;
constexpr DWORD fLineDraw = 0x509700;

struct LineSeen {
    int calls = 0, x1 = 0, y1 = 0, x2 = 0, y2 = 0, ddH = 0;
} g_line;

void __cdecl FakeLineDraw(int x1, int y1, int x2, int y2, int)
{
    ++g_line.calls;
    g_line.x1 = x1;
    g_line.y1 = y1;
    g_line.x2 = x2;
    g_line.y2 = y2;
    g_line.ddH = At<int>(aDdHeight2);
}

using Line_fn = void(__cdecl *)(int, int, int, int, int);

bool Wheel(int delta, int x, int y, WORD keys = 0)
{
    return core::display::g_mouseFilter(WM_MOUSEWHEEL, MAKEWPARAM(keys, static_cast<WORD>(static_cast<short>(delta))),
                                        x, y);
}

bool Middle(int x, int y)
{
    return core::display::g_mouseFilter(WM_MBUTTONDOWN, 0, x, y);
}

// One wheel movement (delta) at (x, y) and the zoom's step (whole tiles: the target at once).
void WheelTo(int delta, int x = 500, int y = 500)
{
    Wheel(delta, x, y);
    core::scroll::zoom::CameraStep();
}

// The view the core's rule gives for a percent on the 1920x1080 city (1734 x 1050 px, columns of 60, half rows of 15)
// and the current map (Zeus's clamp: the map's half rows less the margin and 4).
core::display::ZoomLayout Expected(int percent)
{
    const int cols = At<int>(aMapColMax) - At<int>(aMapColMin), rows = At<int>(aMapRowMax) - At<int>(aMapRowMin);
    return core::display::ZoomFor({percent, 1734, 1050, 60, 15, cols, rows, rows - (At<unsigned char>(aBottomMargin) + 4),
                                   150});
}

bool ViewIs(const core::display::ZoomLayout &l)
{
    return At<int>(aColsZ) == l.cols && At<int>(aRowsZ) == l.rows;
}

// 1920x1080, our layout, a map of 200 x 400 (bottom margin 0), smooth scrolling installed (whole tiles: the wheel reaches its target in one step)
// and the zoom given Zeus's view (without view_clamp's hook: the fake view_clamp is called).
void SetupZoom()
{
    SetupResolution();
    PictureFakes();
    Thunk(fViewClamp, reinterpret_cast<void *>(&FakeViewClamp));
    Thunk(fLineDraw, reinterpret_cast<void *>(&FakeLineDraw));
    ToCity();
    BackBuffer(1920, 1080);
    At<int>(aMapColMin) = 0;
    At<int>(aMapColMax) = 200;
    At<int>(aMapRowMin) = 0;
    At<int>(aMapRowMax) = 400;
    At<unsigned char>(aBottomMargin) = 0;
    At<int>(aDirection) = 0;
    zeus::scroll::StartForTest(false);
    zeus::zoom::InstallForTest();
    Res<Tick_fn>("ui_set_sidebar_position")();
    Res<Tick_fn>("city_viewport_narrow")();
}

// city_bottom_and_sidebar's fill_rect call at 0x58e320 (returning to 0x58e325), as in message_panel.
Tick_fn BottomStrip()
{
    using Fill_fn = void(__cdecl *)(int, int, int, int, int);
    BYTE *stub = reinterpret_cast<BYTE *>(0x58e300 + g_imageShift);
    const DWORD args[5] = {0, 6, 1920, 1074, 0}; // pushed in this order: colour, h, w, y, x
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
    return reinterpret_cast<Tick_fn>(stub);
}

void ZoomWheel()
{
    SetupZoom();
    Check(core::scroll::zoom::Percent() == 100 && At<int>(aColsZ) == 28 && At<int>(aRowsZ) == 71,
          "at start: 100%, the layout's 28 x 71");
    Check(Wheel(-120, 500, 500), "the wheel over the city: taken");
    core::scroll::zoom::CameraStep();
    Check(core::scroll::zoom::Percent() == 125 && ViewIs(Expected(125)),
          "a notch towards the player: 125%, the view the core's rule gives");
    Check(At<int>(aViewDrawnX) == At<int>(aViewX) && At<int>(aViewDrawnY) == At<int>(aViewY),
          "the drawn view written with the view after the relayout (viewport_init set it to the map's middle)");
    Check(LogContains("zoom: wheel at (500,500): 100% -> 125%"), "log: the wheel");
    Check(!Wheel(-120, 1800, 500) && core::scroll::zoom::Percent() == 125, "the wheel over the sidebar: the game's");
    At<unsigned char>(aGameScreen) = 0x07;
    Check(!Wheel(-120, 500, 500), "a sidebar panel's dialog (0x07): the game's");
    At<unsigned char>(aGameScreen) = 0x06;
    Check(!Wheel(-120, 500, 500), "the build submenu (0x06): the game's (it does nothing with it)");
    At<unsigned char>(aGameScreen) = 0x09;
    Check(!Wheel(-120, 500, 500), "the information window (0x09): the game's");
    At<unsigned char>(aGameScreen) = 0x48;
    Check(Wheel(0, 500, 500), "the city with a company selected (0x48): ours");
    At<unsigned char>(aGameScreen) = 0x01;
    Check(Wheel(-120, 500, 500, MK_LBUTTON), "dragging with the left button: taken ...");
    core::scroll::zoom::CameraStep();
    Check(core::scroll::zoom::Percent() == 125, "... but no zoom (the drag works in its scale)");
    At<unsigned char>(aMouseMode) = 0x4d;
    Check(Wheel(-120, 500, 500), "a military drag selection: taken ...");
    core::scroll::zoom::CameraStep();
    Check(core::scroll::zoom::Percent() == 125, "... but no zoom");
    At<unsigned char>(aMouseMode) = 0;
    WheelTo(-120);
    Check(core::scroll::zoom::Percent() == 150 && ViewIs(Expected(150)), "another notch: 150%");
    WheelTo(-120);
    Check(core::scroll::zoom::Percent() == 150 && LogContains("150% -> 150% (nothing to do)"),
          "at the cap (150): nothing to do");
    Check(Middle(500, 500), "the middle button over the city: taken");
    core::scroll::zoom::CameraStep();
    Check(core::scroll::zoom::Percent() == 100 && At<int>(aColsZ) == 28 && At<int>(aRowsZ) == 71,
          "the middle button: back to 100%, the layout's view");
    WheelTo(240);
    Check(core::scroll::zoom::Percent() == 50 && ViewIs(Expected(50)), "two notches away: 50%");
    // The adventure editor's city (screen 0x01 at our resolution, editor_mode 1): no zoom, the wheel the game's.
    At<int>(0x665e2c) = 1;
    Res<Tick_fn>("city_viewport_narrow")();
    Check(core::scroll::zoom::Percent() == 100 && At<int>(aColsZ) == 28 && At<int>(aRowsZ) == 71 &&
              !Wheel(-120, 500, 500) && !Middle(500, 500),
          "the editor's city: 100% (the layout's view), the wheel and the middle button the game's");
    At<int>(0x665e2c) = 0;
    Res<Tick_fn>("city_viewport_narrow")();
    Check(core::scroll::zoom::Percent() == 50, "out of the editor: the zoom again (50%)");
    At<int>(aModeW) = 1024; // the game's own size: not zoomed, the wheel the game's
    At<int>(aModeH) = 768;
    Res<Tick_fn>("ui_set_sidebar_position")();
    Res<Tick_fn>("city_viewport_narrow")();
    Check(core::scroll::zoom::Percent() == 100 && !Wheel(-120, 300, 300),
          "1024x768: 100% (the game's layout), the wheel the game's");
    Check(zeus::scroll::Working(), "smooth scrolling still on");
}

void ZoomLayoutScenario()
{
    SetupZoom();
    Tick_fn strip = BottomStrip();
    WheelTo(-240);
    Check(core::scroll::zoom::Percent() == 150 && At<int>(aClipRightZ) > 1734 && At<int>(aClipBottom) > 1080,
          "150%: the clip (the zoomed buffer's) past the sidebar and the screen's bottom");
    const int clipR = At<int>(aClipRightZ), clipB = At<int>(aClipBottom);
    Res<Tick_fn>("city_message_layout")();
    Check(g_cityLayout.panelClip == 1080 && g_cityLayout.panelClipR == 1734,
          "150%: the message panel laid out on the screen's bottom, as wide as the city up to the sidebar");
    Check(At<int>(aClipRightZ) == clipR && At<int>(aClipBottom) == clipB, "afterwards the clip is the zoomed one again");
    WheelTo(480);
    Check(core::scroll::zoom::Percent() == 50 && At<int>(aClipBottom) < 1080, "50%: the clip above the screen's bottom");
    Res<Tick_fn>("city_message_layout")();
    Check(g_cityLayout.panelClip == 1080 && g_cityLayout.panelClipR == 1734, "50%: the panel on the screen's bottom, up to the sidebar");
    strip();
    Check(g_cityLayout.fills == 0, "50%: the bottom strip not blanked (the city is shown to the bottom)");
    Middle(500, 500);
    core::scroll::zoom::CameraStep();
    Res<Tick_fn>("city_message_layout")();
    Check(core::scroll::zoom::Percent() == 100 && g_cityLayout.panelClip == 1080 && g_cityLayout.panelClipR == 1734,
          "100%: the panel up to the sidebar too (as Yongle's; the clip ends at 1678)");
    // The build cost text (seen in testing: cut off below 100%) and a loaded game (back to the starting zoom).
    struct Cost {
        static void __cdecl Draw()
        {
            g_cityLayout.panelClip = At<int>(aClipBottom);
            g_cityLayout.panelClipR = At<int>(0x7d3b90);
        }
        static int __cdecl Load(int, int, int, int)
        {
            return 7;
        }
    };
    Thunk(0x4542b0, reinterpret_cast<void *>(&Cost::Draw));
    Thunk(0x50d6e0, reinterpret_cast<void *>(&Cost::Load));
    int nz;
    const zeus::Detour *dz = zeus::zoom::Detours(&nz);
    WheelTo(240); // 100 -> 50
    Detour<Tick_fn>(dz, nz, "build_cost_text_draw")();
    Check(core::scroll::zoom::Percent() == 50 && g_cityLayout.panelClip == 1080 && g_cityLayout.panelClipR == 1734 &&
              At<int>(aClipBottom) < 1080,
          "50%: the build cost text clipped to the city as shown (the screen's bottom, the sidebar), the clip back");
    using Load_fn = int(__cdecl *)(int, int, int, int);
    Check(Detour<Load_fn>(dz, nz, "save_load")(1, 2, 3, 4) == 7 && core::scroll::zoom::Percent() == 100 &&
              At<int>(aColsZ) == 28 && At<int>(aRowsZ) == 71,
          "a game loaded (save_load): the starting zoom (100%) and the layout's view at once, the result passed on");
    // Another map (loading a game): the view laid out again before the game clamps (view_clamp's hook).
    WheelTo(-240);
    At<int>(aMapColMax) = 60;
    At<int>(aMapRowMax) = 100;
    At<unsigned char>(aBottomMargin) = 3;
    int n;
    const zeus::Detour *d = zeus::zoom::Detours(&n);
    Detour<Tick_fn>(d, n, "view_clamp")();
    const core::display::ZoomLayout small = Expected(150);
    Check(LogContains("zoom: the city view is laid out again (the map's range changed)") &&
              core::scroll::zoom::Percent() == small.percent && ViewIs(small) && small.percent < 150,
          "another map (60 x 100): laid out again at once, capped by the map");
    Check(LogContains("map range 60 x 100 (the clamp holds 93 half rows)"),
          "Zeus's clamp: the map's half rows less the margin (3) and 4");
    Check(LogContains("zoom: the bottom margin for direction 0 is 3 half row(s)"), "log: the bottom margin");
    Check(zeus::scroll::Working(), "smooth scrolling still on");
}

// The east edge zoomed: the zoomed city ends cols x 60 - 2 - bufW px short of the
// original's east edge, so a view put at the game's limit (the fake view_clamp's 200) goes that far past it; scrolling
// east stops there too.
void ZoomEastEdge()
{
    SetupZoom();
    WheelTo(-240); // 150%
    zeus::scroll::SetSmooth(true);
    int bufW = 0, bufH = 0;
    core::scroll::zoom::BufferSize(&bufW, &bufH);
    const int cols = At<int>(aColsZ), viewY = At<int>(aViewY), e = cols * 60 - 2 - bufW;
    At<int>(aViewX) = 200; // a minimap click at the east edge
    zeus::scroll::SetRemainderForTest(0, 0);
    Timing<Render_fn>("city_render")(200, viewY, 0);
    int px, py;
    zeus::scroll::Remainder(&px, &py);
    Check(core::scroll::zoom::Percent() == 150 && e > 0 && e < 60 && At<int>(aViewX) == 200 && px == e &&
              g_pic.vx == 200,
          "150%, at the game's east limit: the remainder put on the original's east edge (past the limit)");
    Timing<Render_fn>("city_render")(200, viewY, 0);
    zeus::scroll::Remainder(&px, &py);
    Check(At<int>(aViewX) == 200 && px == e, "the next frame: kept there");
    // A zoom step there (smooth scrolling off: whole tiles, one step): the new layout's line at once, before drawing.
    zeus::scroll::SetSmooth(false);
    WheelTo(120); // 125%
    core::scroll::zoom::BufferSize(&bufW, &bufH);
    const int e125 = At<int>(aColsZ) * 60 - 2 - bufW;
    zeus::scroll::Remainder(&px, &py);
    Check(core::scroll::zoom::Percent() == 125 && e125 > 0 && e125 < 60 && e125 != e && At<int>(aViewX) == 200 &&
              px == e125,
          "a zoom step at the east edge: on the new layout's line right after the step");
    Check(zeus::scroll::Working() && core::scroll::zoom::Percent() == 125, "smooth scrolling and the zoom still on");
}

// As Zeus's view_clamp: the largest view is the map's range less the view (view_clamp 0x51cd60: view_x at most
// 0xfa05b0 - columns, view_y at most 0xfa05a8 - margin - 4 - half rows, even), so a zoom step's layout moves the limits.
void __cdecl FakeViewClampLayout()
{
    ++g_scroll.clamps;
    volatile int &x = At<int>(aViewX), &y = At<int>(aViewY);
    const int maxX = At<int>(aMapColMax) - At<int>(aColsZ);
    const int maxY = (At<int>(aMapRowMax) - At<unsigned char>(aBottomMargin) - 4 - At<int>(aRowsZ)) & ~1;
    x = x < At<int>(aMapColMin) ? At<int>(aMapColMin) : x > maxX ? maxX : x;
    y = y < At<int>(aMapRowMin) ? At<int>(aMapRowMin) : y > maxY ? maxY : y;
    y = y & ~1;
}

// The camera's position against the east / south lines for the layout as it is:
// the original's edges, the game's limit + (the view's columns / half rows less the zoomed buffer shown). South: the
// map's limit before the clamp makes it even (view_y in half rows of 15 px; the position counts view_y x 15).
bool OnCornerLines(int *dx, int *dy)
{
    int bufW = 0, bufH = 0;
    core::scroll::zoom::BufferSize(&bufW, &bufH);
    const int cols = At<int>(aColsZ), rows = At<int>(aRowsZ);
    const int maxX = At<int>(aMapColMax) - cols;
    const int maxY = At<int>(aMapRowMax) - At<unsigned char>(aBottomMargin) - 4 - rows; // not made even
    const int limX = maxX * 60 + (cols * 60 - 2 - bufW), limY = maxY * 15 + ((rows - 1) * 15 - bufH);
    int px, py;
    zeus::scroll::Remainder(&px, &py);
    *dx = At<int>(aViewX) * 60 + px - limX;
    *dy = At<int>(aViewY) / 2 * 30 + py - limY;
    return *dx == 0 && *dy == 0;
}

// Zooming out at the south-east corner, smooth: every step of the zoom's movement
// keeps the camera on the new layout's east and south lines (pinned there, as the west and north edges are), not
// moved away from them by the game's clamp and the anchor's shift both.
void ZoomCorner()
{
    SetupZoom();
    Thunk(fViewClamp, reinterpret_cast<void *>(&FakeViewClampLayout));
    WheelTo(240); // 50% (whole tiles: at once)
    zeus::scroll::SetSmooth(true);
    At<int>(aViewX) = 10000; // far past the south-east corner: the drawing puts it on the lines
    At<int>(aViewY) = 10000;
    zeus::scroll::SetRemainderForTest(0, 0);
    Timing<Render_fn>("city_render")(10000, 10000, 0);
    int dx = 0, dy = 0;
    Check(core::scroll::zoom::Percent() == 50 && OnCornerLines(&dx, &dy), "50% at the south-east corner: on the lines");
    Wheel(-120, 1000, 600); // one notch out, the cursor in the middle of the city
    int steps = 0, off = 0, worstX = 0, worstY = 0;
    for (int i = 0; i < 400 && core::scroll::zoom::CameraMoving(); ++i) {
        core::scroll::zoom::CameraStep();
        ++steps;
        if (!OnCornerLines(&dx, &dy)) {
            int px, py;
            zeus::scroll::Remainder(&px, &py);
            std::printf("    step %d: view %d x %d, view_y %d remainder %d, off %d, %d px\n", steps, At<int>(aColsZ),
                        At<int>(aRowsZ), At<int>(aViewY), py, dx, dy);
            ++off;
            worstX = std::abs(dx) > std::abs(worstX) ? dx : worstX;
            worstY = std::abs(dy) > std::abs(worstY) ? dy : worstY;
        }
        Sleep(2);
    }
    char what[160];
    std::snprintf(what, sizeof(what),
                  "zooming out at the corner: on the east and south lines after every step (%d steps, %d off, worst "
                  "%d, %d px)",
                  steps, off, worstX, worstY);
    Check(steps > 1 && off == 0 && core::scroll::zoom::Percent() == 75, what);
    Check(zeus::scroll::Working(), "smooth scrolling still on");
}

void ZoomRender()
{
    SetupZoom();
    WheelTo(-240); // whole tiles: the target at once
    zeus::scroll::SetSmooth(true);
    zeus::scroll::SetRemainderForTest(5, 6);
    int bufW = 0, bufH = 0;
    core::scroll::zoom::BufferSize(&bufW, &bufH);
    const int cols = At<int>(aColsZ), rows = At<int>(aRowsZ);
    BYTE *back = At<BYTE *>(aDrawBuffer);
    Timing<Render_fn>("city_render")(0, 0, 0);
    Check(core::scroll::zoom::Percent() == 150 && g_calls.draws == 1 && g_pic.buffer != back &&
              g_pic.cols == cols + 1 && g_pic.rows == rows + 2,
          "150%: drawn once into our buffer, the view a column and a row pair larger meanwhile");
    Check(At<BYTE *>(aDrawBuffer) == back && At<int>(aColsZ) == cols && At<int>(aDdHeight2) == 1080,
          "afterwards: draw_buffer, the viewport and dd_height the game's");
    Check(LogContains("zoom: city buffer"), "log: the zoom's buffer");
    int left, top, right, bottom;
    const char *why = nullptr;
    Check(zeus::scroll::DrawnCity(&left, &top, &right, &bottom, &why) && left == 0 && top == 30 && right == 1734 &&
              bottom == 1080,
          "the menus' background: the city as shown on the screen (not the zoomed buffer)");
    Check(Scr<Int_fn>("city_view_width_px")() == 1734, "the Game paused banner: the city's width on the screen");
    namespace cd = core::display;
    const double fx = static_cast<double>(bufW) / 1734, fy = static_cast<double>(bufH) / 1050;
    Scr<S2g_fn>("screen_to_grid")(100, 140, 0);
    Check(g_conv.x == cd::ZoomToBuffer(100, 0, fx) && g_conv.y == cd::ZoomToBuffer(140, 30, fy) && g_conv.ox == -5 &&
              g_conv.oy == 24,
          "screen_to_grid: the buffer pixel the point shows, the origin moved by the remainder");
    At<int>(aMouseX) = 1750;
    At<int>(aMouseY) = 500;
    Check(Scr<Mouse_fn>("mouse_to_grid")() == 0, "the cursor on the sidebar: no tile (the zoomed view reaches past it)");
    // The drag selection's frame: drag_select_draw's limits as buffer pixels, its lines drawn on the screen.
    struct DragLine {
        static void __fastcall Draw(int *drag)
        {
            g_dragSeen[0] = At<int>(aSidebarX);
            g_dragSeen[1] = At<int>(aDdHeight2);
            Scr<Line_fn>("line_draw")(drag[2], drag[3], drag[2] + 100, drag[3], 0);
        }
    };
    Thunk(fDragDraw, reinterpret_cast<void *>(&DragLine::Draw));
    At<int>(aMouseX) = 500;
    At<int>(aMouseY) = 500;
    int drag[16] = {0, 0, 300, 400};
    Scr<DragZ_fn>("drag_select_start")(drag);
    zeus::scroll::SetRemainderForTest(5, 6);
    Scr<DragZ_fn>("drag_select_draw")(drag);
    Check(g_dragSeen[0] == cd::ZoomToBuffer(1734, 0, fx) && g_dragSeen[1] == cd::ZoomToBuffer(1080, 30, fy),
          "drag_select_draw: sidebar_x and dd_height as buffer pixels while it runs");
    Check(g_line.calls == 1 && g_line.x1 == cd::ZoomToScreen(300, 0, fx) && g_line.x2 == cd::ZoomToScreen(400, 0, fx) &&
              g_line.y1 == cd::ZoomToScreen(400, 30, fy) && g_line.ddH == 1080,
          "line_draw: the frame's buffer pixels drawn on the screen, with the screen's dd_height");
    Check(At<int>(aSidebarX) == 1734 && At<int>(aDdHeight2) == 1080, "afterwards sidebar_x and dd_height the game's");
    Scr<Line_fn>("line_draw")(1, 2, 3, 4, 0);
    Check(g_line.calls == 2 && g_line.x1 == 1 && g_line.y2 == 4, "line_draw elsewhere: as the game's");
    // The scaling fails: the zoom off for the rest of the run, laid out at 100 at once; the game's
    // drawing is not made again (it would count).
    core::scroll::zoom::FailScalingForTest(true);
    Timing<Render_fn>("city_render")(0, 0, 0);
    Check(g_calls.draws == 2 && core::scroll::zoom::Percent() == 100 && At<int>(aColsZ) == 28 && At<int>(aRowsZ) == 71,
          "the scaling fails: drawn once, the zoom at 100% and the layout's view at once");
    Check(LogContains("zoom: off for the rest of the run (the ") && LogContains("could not be shrunk"),
          "log: why the zoom is off");
    core::scroll::zoom::FailScalingForTest(false);
    Check(!Wheel(-120, 500, 500), "afterwards the wheel is the game's");
    Timing<Render_fn>("city_render")(0, 0, 0);
    Check(g_calls.draws == 3 && g_pic.cols != cols + 1, "the next drawing at 100% (smooth scrolling's own)");
    Check(zeus::scroll::Working(), "smooth scrolling still on");
}

// A fault in smooth scrolling's drawing while zoomed: the zoom off and laid out at 100% before the game draws again
// its own way (straight into the back buffer: a zoomed view there would overrun it).
void ZoomFault()
{
    SetupZoom();
    WheelTo(-240); // whole tiles: the target at once
    zeus::scroll::SetSmooth(true);
    BYTE *back = At<BYTE *>(aDrawBuffer);
    g_fault = "city_render";
    Timing<Render_fn>("city_render")(0, 0, 0);
    Check(!zeus::scroll::Working() && core::scroll::zoom::Percent() == 100, "fault: smooth scrolling and the zoom off");
    Check(g_calls.draws == 2 && g_pic.buffer == back && g_pic.cols == 28 && g_pic.rows == 71,
          "drawn again the game's way with the layout's 100% view");
    Check(LogContains("zoom: off for the rest of the run (smooth scrolling was switched off after an error)"),
          "log: the zoom off with the reason");
}

// ---------------------------------------------------------------------------------------------
// city_picture (games/zeus/city_picture, core/capture/city_picture): the city in
// strips inside timing's drawing that never counts, drawn only in a counting frame; the game's variables, the water
// marks, the ambient counts, city_render's own and minimap_draw's put back; a fault in a strip; the put-back off.

constexpr DWORD fMinimapDraw = 0x454980, fPlayerCity = 0x4621e0, fLangGetString = 0x507bf0;
constexpr DWORD aOverlay = 0xfa05f0, aPreview = 0xda2fa8, aMarkers = 0x160ac80, aWater = 0x83ad6c,
                aAmbient = 0x10e41f0, aRhythmFlag = 0x7d3b1c, aDrawClip = 0xf139b0, aMinimapState = 0x7d3b08,
                aMinimapRow = 0x7d3b30, aCityCache = 0x903128, aCities = 0x8fbac0,
                aScreenZ = 0xf2a6dd;

struct PictureSeen {
    bool on = false;          // count the drawings into our picture buffer
    int strips = 0;           // drawings into a buffer that is not the back buffer
    int tileFirst = -1;       // the tile byte each of them saw on entry (all the same: put back in between)
    bool tilesSame = true;
    bool held = true;         // overlay, preview and markers 0, anim_due 0 in every strip
    int faultAt = -1;         // fault in this strip (0-based)
    bool touchOutside = false; // every strip changes 0x7d3f00, which nothing puts back (city_picture_check)
    int listFirst = -1;        // each tile's first figure (save block 19) as each strip saw it: the same in all
    bool listSame = true;
    int minimaps = 0;
    bool minimapHeld = true;  // overlay 0, our buffer, anim_due 0
} g_ps;

void __cdecl FakeCityRenderPicture(int x, int y, int z)
{
    // A strip: the picture's clip starts at the buffer's top (the game's layout has the top bar above it).
    const bool strip = g_ps.on && At<int>(0x7d3b8c) == 0;
    if (strip) {
        const int tile = At<unsigned char>(aTiles + 7);
        if (g_ps.tileFirst < 0) {
            g_ps.tileFirst = tile;
        }
        g_ps.tilesSame = g_ps.tilesSame && tile == g_ps.tileFirst;
        const int list = At<short>(aTileFigures + 2 * 777);
        if (g_ps.listFirst < 0) {
            g_ps.listFirst = list;
        }
        g_ps.listSame = g_ps.listSame && list == g_ps.listFirst;
        At<short>(aTileFigures + 2 * 777) = static_cast<short>(list + 1); // as the drawing relinks a tile's figures
        bool zero = true;
        for (int i = 0; i < zeus::anim::kTimers; ++i) {
            zero = zero && At<unsigned char>(aAnimDue + i) == 0;
        }
        g_ps.held = g_ps.held && zero && At<int>(aOverlay) == 0 && At<int>(aPreview) == 0 && At<int>(aMarkers) == 0;
        if (g_ps.strips++ == g_ps.faultAt) {
            Fault();
        }
        if (g_ps.touchOutside) {
            At<int>(0x7d3f00) = At<int>(0x7d3f00) + 1;
        }
    }
    // What city_render and the drawing of the whole map change besides kDrawnState.
    At<unsigned char>(aWater + 9) = At<unsigned char>(aWater + 9) + 1;
    At<int>(aAmbient) = At<int>(aAmbient) + 1;
    At<int>(aRhythmFlag) = 1;
    At<int>(aDrawClip) = 777;
    FakeCityRender(x, y, z);
}

void __cdecl FakeMinimapDraw(int x, int y, int, int)
{
    ++g_ps.minimaps;
    bool zero = true;
    for (int i = 0; i < zeus::anim::kTimers; ++i) {
        zero = zero && At<unsigned char>(aAnimDue + i) == 0;
    }
    BYTE *buf = At<BYTE *>(aDrawBuffer);
    g_ps.minimapHeld = g_ps.minimapHeld && zero && At<int>(aOverlay) == 0 &&
                       buf != reinterpret_cast<BYTE *>(g_back.data());
    // As the game's: every cell of the view table painted (outside the map a filler), from 4 half rows above y and
    // 4 columns (8 px) right of x; a few map cells drawn.
    const int pitchPx = At<int>(aPitch) / 2;
    for (int r = 0; r < 0x1c9; ++r) {
        for (int c = 0; c < 2 * 0xe7; ++c) {
            reinterpret_cast<WORD *>(buf)[(y - 4 + r) * pitchPx + x + 8 + c] = 0x5555;
        }
    }
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 6; ++c) {
            reinterpret_cast<WORD *>(buf)[(y - 4 + 30 + r) * pitchPx + x + 8 + 30 + c] = static_cast<WORD>(0x300 + c);
        }
    }
    At<int>(aMinimapState) = x; // its own variables
    At<int>(aMinimapRow) = 5;
    At<int>(aDrawClip) = 555;
    At<unsigned char>(aTiles + 7) = At<unsigned char>(aTiles + 7) + 1; // something put back by the drawing state
}

int __cdecl FakePlayerCity()
{
    At<int>(aCityCache) = 3;
    return 3;
}

const char *__cdecl FakeLangGetString(int group, int index)
{
    return group == 21 && index == 7 ? "Mount Olympus" : "?";
}

// A 1920x1080 city (SetupPicture) on a map of columns 10..50 and half rows 20..140 (no bottom margin): 2 strips of
// 64 half rows. The pictures installed, the copy buffer ready, the first frame (counting) run.
bool SetupCityPicture()
{
    SetupPicture();
    if (!zeus::timing::PrepareForTest()) {
        Check(false, "the copy buffer");
        return false;
    }
    Thunk(fCityRender, reinterpret_cast<void *>(&FakeCityRenderPicture));
    Thunk(fMinimapDraw, reinterpret_cast<void *>(&FakeMinimapDraw));
    Thunk(fPlayerCity, reinterpret_cast<void *>(&FakePlayerCity));
    Thunk(fLangGetString, reinterpret_cast<void *>(&FakeLangGetString));
    At<int>(aMapColMin) = 10;
    At<int>(aMapColMax) = 50;
    At<int>(aMapRowMin) = 20;
    At<int>(aMapRowMax) = 140;
    At<unsigned char>(aBottomMargin) = 0;
    At<int>(aDirection) = 0;
    At<unsigned char>(aScreenZ) = 0x01;
    At<int>(aEditorMode) = 0;
    At<int>(aCityCache) = 9;
    At<unsigned char>(aCities + 3 * 0x23c + 3) = 7;
    At<int>(aOverlay) = 4;
    At<int>(aPreview) = 1;
    At<int>(aMarkers) = 1;
    zeus::city_picture::InstallForTest();
    g_ps = PictureSeen{};
    g_ps.on = true;
    g_savedPics.clear();
    return true;
}

struct PictureState {
    int overlay, preview, markers, water, ambient, rhythm, clip, minimap, minimapRow, cache, cols, origin;
    BYTE *buffer;
};

PictureState ReadPictureState()
{
    return PictureState{At<int>(aOverlay),       At<int>(aPreview),       At<int>(aMarkers),
                        At<unsigned char>(aWater + 9), At<int>(aAmbient), At<int>(aRhythmFlag),
                        At<int>(aDrawClip),      At<int>(aMinimapState),  At<int>(aMinimapRow),
                        At<int>(aCityCache),     At<int>(aColsZ),         At<int>(aOriginYz),
                        At<BYTE *>(aDrawBuffer)};
}

bool SameState(const PictureState &a, const PictureState &b)
{
    return std::memcmp(&a, &b, sizeof(a)) == 0;
}

void CityPicture()
{
    if (!SetupCityPicture()) {
        return;
    }
    Timing<Frame_fn>("game_frame")(); // a counting frame
    core::capture::city_picture::RequestCity();
    Check(LogContains("picture: city picture asked for"), "F10 in the city: asked for");
    // The frame's drawing: the outer drawing moves its state on by one; the strips put theirs back.
    const PictureState before = ReadPictureState();
    const Drawn drawn0 = ReadDrawn();
    const Random random0 = ReadRandom();
    g_calls.touchRandom = true;
    Timing<Render_fn>("city_render")(0, 0, 0);
    g_calls.touchRandom = false;
    Check(g_savedPics.size() == 1 && g_savedPics[0].kind == "Mount-Olympus" && g_savedPics[0].w == 2398 &&
              g_savedPics[0].h == 115 * 15,
          "drawn in the same frame: one picture, the player's city's name, 40 columns x 115 half rows");
    Check(g_ps.strips == 2 && g_ps.tilesSame && g_ps.tileFirst == drawn0.tile + 1,
          "2 strips, each starting from the state after the frame's own drawing (tile animation put back between)");
    Check(g_ps.listSame && At<short>(aTileFigures + 2 * 777) == g_ps.listFirst,
          "save block 19 (each tile's first figure): the same in every strip, put back after the picture");
    Check(g_ps.held, "every strip: anim_due 0, the overlay, the building preview and the markers 0");
    const Drawn drawn1 = ReadDrawn();
    Check(drawn1.tile == drawn0.tile + 1 && drawn1.counter == drawn0.counter + 1 &&
              drawn1.building == drawn0.building + 1,
          "afterwards: only the frame's own (counting) drawing moved the animations on");
    const Random random1 = ReadRandom();
    Check(random1.index == random0.index + 1, "the random numbers: only the frame's own drawing turned them");
    const PictureState after = ReadPictureState();
    Check(after.overlay == 4 && after.preview == 1 && after.markers == 1 && after.buffer == before.buffer &&
              after.cols == before.cols && after.origin == before.origin && ViewportIsGame(),
          "afterwards: the overlay, the preview, the markers, draw_buffer and the viewport the game's");
    Check(after.water == before.water + 1 && after.ambient == before.ambient + 1,
          "the water marks and the ambient counts: as after the frame's own drawing (the picture's put back)");
    Check(after.cache == 9, "the player's city's cache put back");
    Check(DueIsFrame(&At<unsigned char>(aAnimDue)), "the frame's anim_due back");
    Check(zeus::timing::DrawnWorking() && zeus::scroll::Working(), "drawn-frame put-back and smooth scrolling on");

    // The minimap in the next counting frame.
    g_savedDone(true, "shots\\x.png");
    Sleep(60);
    Timing<Frame_fn>("game_frame")();
    core::capture::city_picture::RequestMinimap();
    const PictureState mBefore = ReadPictureState();
    Timing<Render_fn>("city_render")(0, 0, 0);
    const PictureState mAfter = ReadPictureState();
    Check(g_ps.minimaps == 2 && g_ps.minimapHeld, "the minimap: drawn twice into our buffer, overlay 0, anim_due 0");
    // The map's part (columns 10..50, half rows 20..138, minimap_center's row 5 as the fake leaves it): x 8 + 8 + 20 - 1
    // .. + 100 + 1, y 8 - 4 + 20 - 1 .. 8 - 4 + 138 - 1: 83 x 119 (the filler around it cut away).
    Check(g_savedPics.size() == 2 && g_savedPics[1].kind == "Mount-Olympus_minimap" && g_savedPics[1].w == 83 &&
              g_savedPics[1].h == 119,
          "the minimap: saved, cropped to the map's part (no filler around it)");
    Check(mAfter.minimap == mBefore.minimap && mAfter.minimapRow == mBefore.minimapRow &&
              mAfter.clip == mBefore.clip && mAfter.buffer == mBefore.buffer &&
              ReadDrawn().tile == drawn1.tile + 1,
          "the minimap: its variables and draw_buffer put back, the tile byte it moved put back");
    Check(LogContains("picture: the player's city is 3 of the world, name text 21 / 7: \"Mount Olympus\""),
          "log: the player's city and its name");
}

// Asked in a frame that does not count: not drawn until the next counting frame.
void CityPictureWait()
{
    if (!SetupCityPicture()) {
        return;
    }
    CountingThenNot();
    core::capture::city_picture::RequestCity();
    Timing<Render_fn>("city_render")(0, 0, 0);
    Check(g_savedPics.empty() && g_ps.strips == 0, "a frame that does not count: not drawn, the request kept");
    Sleep(60);
    Timing<Frame_fn>("game_frame")();
    Timing<Render_fn>("city_render")(0, 0, 0);
    Check(g_savedPics.size() == 1 && g_ps.strips == 2 && g_ps.tilesSame, "the next counting frame: drawn");
    // Only the city screens without a window: a sidebar panel's dialog (0x07) and a
    // building's information window (0x09) are refused, the city with a company selected (0x48) is not; the editors
    // are refused.
    g_savedDone(true, "a");
    At<unsigned char>(aScreenZ) = 0x07;
    core::capture::city_picture::RequestCity();
    Check(LogContains("picture: city picture key pressed outside the city (screen 0x07)"), "a sidebar dialog: refused");
    At<unsigned char>(aScreenZ) = 0x09;
    At<unsigned char>(aInfoPrev) = 0x01;
    core::capture::city_picture::RequestCity();
    Check(LogContains("picture: city picture key pressed outside the city (screen 0x09)"),
          "a building's information window: refused");
    At<unsigned char>(aScreenZ) = 0x48;
    core::capture::city_picture::RequestCity();
    Sleep(60);
    Timing<Frame_fn>("game_frame")();
    Timing<Render_fn>("city_render")(0, 0, 0);
    Check(g_savedPics.size() == 2 && !LogContains("outside the city (screen 0x48)"),
          "the city with a company selected: drawn");
    g_savedDone(true, "b");
    At<unsigned char>(aScreenZ) = 0x01;
    At<int>(aEditorMode) = 1;
    core::capture::city_picture::RequestCity();
    Check(LogContains("picture: city picture key pressed outside the city (screen 0x01)"), "the editor: refused");
}

// A fault in the second strip: that drawing's put-back still runs, the game's variables back, the pictures off.
void CityPictureFault()
{
    if (!SetupCityPicture()) {
        return;
    }
    Timing<Frame_fn>("game_frame")();
    core::capture::city_picture::RequestCity();
    g_ps.faultAt = 1;
    const PictureState before = ReadPictureState();
    const Drawn drawn0 = ReadDrawn();
    Timing<Render_fn>("city_render")(0, 0, 0);
    const PictureState after = ReadPictureState();
    Check(g_savedPics.empty() && g_ps.strips == 2, "fault in the second strip: not saved, no strip after it");
    Check(ReadDrawn().tile == drawn0.tile + 1 && ReadDrawn().counter == drawn0.counter + 1,
          "the faulting strip's drawing state put back (only the frame's own drawing counted)");
    Check(after.overlay == 4 && after.preview == 1 && after.markers == 1 && after.buffer == before.buffer &&
              after.water == before.water + 1 && after.ambient == before.ambient + 1 && ViewportIsGame(),
          "the game's variables, the water marks and the ambient counts put back");
    Check(DueIsFrame(&At<unsigned char>(aAnimDue)), "the frame's anim_due back");
    Check(zeus::timing::DrawnWorking() && zeus::scroll::Working(), "drawn-frame put-back and smooth scrolling still on");
    Check(LogContains("feature \"city picture (core/capture/city_picture)\" switched off") &&
              LogContains("picture: the city picture is off for the rest of the run"),
          "log: the pictures off");
    core::capture::city_picture::RequestCity();
    Check(LogContains("picture: city picture key pressed, but the pictures are off"), "afterwards: unavailable");
}

// The drawn-frame put-back switched off by a fault: refused before the key; dropped when it happens after the key.
void CityPictureDrawnOff()
{
    if (!SetupCityPicture()) {
        return;
    }
    CountingThenNot();
    core::capture::city_picture::RequestCity(); // asked in a frame that does not count: waits
    g_calls.protectAfterDraw = true;           // this frame's put-back faults: the put-back switched off
    Timing<Render_fn>("city_render")(0, 0, 0);
    g_calls.protectAfterDraw = false;
    ProtectCounters(PAGE_READWRITE);
    Check(!zeus::timing::DrawnWorking() && g_savedPics.empty(), "the put-back off; nothing drawn yet");
    Sleep(60);
    Timing<Frame_fn>("game_frame")();
    Timing<Render_fn>("city_render")(0, 0, 0);
    Check(g_savedPics.empty() && g_ps.strips == 0 &&
              LogContains("picture: the picture asked for is not drawn: the city drawing's hooks were switched off"),
          "the next frame: dropped, not saved");
    core::capture::city_picture::RequestCity();
    Check(LogContains("picture: city picture key pressed, but the pictures are off"), "a new key: unavailable");
}

// [debug] state_check on: each picture checked as one (labelled), what our put-back leaves found.
void CityPictureCheck()
{
    if (!SetupCityPicture()) {
        return;
    }
    if (!zeus::state_check::EnableForTest(0x7d3000, 0x1000)) { // the city's and the minimap's variables
        Check(false, "the check's copy");
        return;
    }
    Timing<Frame_fn>("game_frame")();
    core::capture::city_picture::RequestCity();
    Timing<Render_fn>("city_render")(0, 0, 0);
    Check(g_savedPics.size() == 1 &&
              LogContains("zeus: state check (the city picture): ") &&
              LogContains("place(s) changed by the drawing, 0 still changed after our put-back"),
          "the city picture checked: our changes found after the drawing, none left after the put-back");
    g_savedDone(true, "a");
    Sleep(60);
    Timing<Frame_fn>("game_frame")();
    core::capture::city_picture::RequestMinimap();
    Timing<Render_fn>("city_render")(0, 0, 0);
    Check(LogContains("zeus: state check (the minimap picture): ") && g_savedPics.size() == 2,
          "the minimap picture checked");
    Check(!LogContains("still changed after our put-back (by"), "nothing left by either");
    g_savedDone(true, "b");
    Sleep(60);
    Timing<Frame_fn>("game_frame")();
    g_ps.touchOutside = true; // the strips change a place nothing puts back
    core::capture::city_picture::RequestCity();
    Timing<Render_fn>("city_render")(0, 0, 0);
    g_ps.touchOutside = false;
    Check(LogContains("zeus: state check: new region 0x7d3f00") &&
              LogContains("changed by the city picture; still changed after our put-back") &&
              LogContains("1 still changed after our put-back"),
          "a place the put-back misses: reported as left, with the picture's label");
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
    {"narrow_fault", &NarrowFault},
    {"map_flags", &MapFlags},
    {"map_height", &MapHeight},
    {"draw_only", &DrawOnly},
    {"intro_skip", &IntroSkip},
    {"intro_fault", &IntroFault},
    {"intro_mismatch", &IntroMismatch},
    {"sound_3d_off", &Sound3dOff},
    {"sound_3d_fault", &Sound3dFault},
    {"sound_pause", &SoundPause},
    {"sound_3d_on", &Sound3dOn},
    {"sound_fault", &SoundFault},
    {"sound_ambient_pan", &SoundAmbientPan},
    {"layer1_keep_off", &Layer1KeepOff},
    {"layer1_fault", &Layer1Fault},
    {"key_bindings", &KeyBindings},
    {"key_bindings_fault", &KeyBindingsFault},
    {"overlay_keys", &OverlayKeys},
    {"esc_and_pause", &EscAndPause},
    {"esc_editor", &EscEditor},
    {"esc_fault", &EscFault},
    {"overlay_keys_fault", &OverlayKeysFault},
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
    {"notice_boxes", &NoticeBoxes},
    {"scroll_ok", &ScrollOk},
    {"direction_fault", &DirectionFault},
    {"render_ok", &RenderOk},
    {"render_fault", &RenderFault},
    {"render_fault_uncounted", &RenderFaultUncounted},
    {"convert_fault", &ConvertFault},
    {"drag_fault", &DragFault},
    {"city_input", &CityInput},
    {"city_extent", &CityExtent},
    {"east_edge", &EastEdge},
    {"figure_portrait", &FigurePortrait},
    {"scroll_attach", &ScrollAttach},
    {"notice_prefix", &NoticePrefix},
    {"zoom_wheel", &ZoomWheel},
    {"zoom_layout", &ZoomLayoutScenario},
    {"zoom_render", &ZoomRender},
    {"zoom_east_edge", &ZoomEastEdge},
    {"zoom_corner", &ZoomCorner},
    {"zoom_fault", &ZoomFault},
    {"city_picture", &CityPicture},
    {"city_picture_wait", &CityPictureWait},
    {"city_picture_fault", &CityPictureFault},
    {"city_picture_drawn_off", &CityPictureDrawnOff},
    {"city_picture_check", &CityPictureCheck},
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
