// Offline test of core/scroll/camera (not the game): the camera of smooth scrolling on a fake game -- its variables in
// an array of ours (through core::site::g_imageShift, as the games' own offline tests do), a fake view_clamp that keeps
// the view in a 0..100 x 0..200 map and can fault, fake keys. Checked: whole columns and row pairs carried into view_x /
// view_y (view_y even), a map edge stops that axis on a whole tile, no remainder at the right and bottom limits,
// someone else moving the view starts again on a whole tile, once per game frame, edge scrolling at the game's own
// speed (Augustus's table), an arrow key held from its flag until released, speed 0 does not move, the game's own
// steps counted and the rate log's text, and a fault in view_clamp: the camera, view_scrolling and the key flags put
// back, the feature off, the game layer's fallback called, the game's own scrolling afterwards. The scenarios run in
// order in one process (the fault last: a feature switched off stays off); about 2 s (the camera runs on real time).
#include "core/diag/guard.h"
#include "core/log.h"
#include "core/scroll/camera.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdio>
#include <cstring>

namespace camera = core::scroll::camera;

namespace {

int g_failures = 0;

void Check(bool ok, const char *what)
{
    std::printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) {
        ++g_failures;
    }
}

// The fake game's memory: one int per variable at 0x500000 + 4 x index.
enum Slot {
    kViewX, kViewY, kTileW, kTileHalfH, kCursorX, kCursorY, kDesktopW, kDesktopH, kKeyL, kKeyR, kKeyU, kKeyD,
    kScrolling, kSpeed, kFrame, kSlots
};
int g_mem[kSlots];
constexpr DWORD kBase = 0x500000;
camera::IntVar V(Slot s)
{
    return camera::IntVar{kBase + 4 * static_cast<DWORD>(s)};
}
int &M(Slot s)
{
    return g_mem[s];
}

constexpr int kMaxX = 100, kMaxY = 200;
int g_clamps = 0, g_sounds = 0, g_fallBacks = 0;
bool g_faultClamp = false;
bool g_keyDown[256] = {};
volatile int *volatile g_null = nullptr;

void FakeClamp()
{
    ++g_clamps;
    if (g_faultClamp) {
        g_faultClamp = false;
        (void)*g_null; // a fault inside the game's view_clamp
    }
    int &x = M(kViewX), &y = M(kViewY);
    x = x < 0 ? 0 : x > kMaxX ? kMaxX : x;
    y = y < 0 ? 0 : y > kMaxY ? kMaxY : y;
    y &= ~1;
}

core::diag::Feature g_feature{"smooth scrolling (test_camera)"};
const camera::CameraGame kGame{
    "test",
    &g_feature,
    camera::CameraVars{V(kViewX), V(kViewY), V(kTileW), V(kTileHalfH), V(kCursorX), V(kCursorY), V(kDesktopW),
                       V(kDesktopH), V(kKeyL), V(kKeyR), V(kKeyU), V(kKeyD), V(kScrolling), V(kSpeed), V(kFrame)},
    &FakeClamp,
    [] { ++g_sounds; },
    [](unsigned vk) { return vk < 256 && g_keyDown[vk]; },
    [] { ++g_fallBacks; },
    [] { return false; }, // no zoom in this test
    [] {},
    [](double *, double *) { return false; },
};

// One game frame at the city's call site; true when the camera moved for it (the game's step is told "none").
bool Frame()
{
    ++M(kFrame);
    return camera::CityDirection(kGame);
}

// Frames of about 4 ms for ms milliseconds; the pixels moved along x (view_x and remainder).
int RunX(int ms)
{
    int px0, py0;
    camera::Remainder(&px0, &py0);
    const int start = M(kViewX) * (M(kTileW) + 2) + px0;
    const DWORD end = GetTickCount() + ms;
    while (static_cast<LONG>(end - GetTickCount()) > 0) {
        Frame();
        Sleep(4);
    }
    int px, py;
    camera::Remainder(&px, &py);
    return M(kViewX) * (M(kTileW) + 2) + px - start;
}

void Middle()
{
    M(kCursorX) = 500;
    M(kCursorY) = 400;
}

void Moves()
{
    camera::SetRemainderForTest(kGame, 0, 0);
    camera::Move(kGame, 70, 0); // 60 px columns
    int px, py;
    camera::Remainder(&px, &py);
    Check(M(kViewX) == 51 && px == 10, "70 px right: one column into view_x, 10 px remainder");
    camera::Move(kGame, 0, -40); // 30 px row pairs
    camera::Remainder(&px, &py);
    Check(M(kViewY) == 46 && py == 20, "40 px up: two row pairs (view_y - 4, even), 20 px remainder");
    Check(g_sounds == 2, "the sound step once per column's width: the column, then the two row pairs (4 half rows)");
    camera::Move(kGame, -10, 10);
    camera::Remainder(&px, &py);
    Check(M(kViewX) == 51 && px == 0 && M(kViewY) == 48 && py == 0, "back to a whole tile: (51,48), no remainder");
    Check(g_sounds == 2, "one row pair is less than a column's width: no sound step yet");
    camera::Move(kGame, 0, 30);
    Check(M(kViewY) == 50 && g_sounds == 3, "the second row pair makes a column's width: the next sound step");
}

void Edges()
{
    M(kViewX) = kMaxX;
    M(kViewY) = 100;
    camera::SetRemainderForTest(kGame, 0, 0);
    camera::Move(kGame, 10, 0);
    int px, py;
    camera::Remainder(&px, &py);
    Check(M(kViewX) == kMaxX && px == 0, "at the right limit: no remainder (nothing beyond the map is shown)");
    camera::Move(kGame, 70, 0);
    camera::Remainder(&px, &py);
    Check(M(kViewX) == kMaxX && px == 0, "past the right edge: view_x clamped, remainder 0");
    M(kViewX) = 50;
    M(kViewY) = kMaxY;
    camera::SetRemainderForTest(kGame, 0, 0);
    camera::Move(kGame, 0, 7);
    camera::Remainder(&px, &py);
    Check(M(kViewY) == kMaxY && py == 0, "at the bottom limit: no remainder either");
    camera::Move(kGame, 0, 40);
    camera::Remainder(&px, &py);
    Check(M(kViewY) == kMaxY && py == 0, "past the bottom edge: view_y clamped, remainder 0");
    M(kViewX) = kMaxX;
    M(kViewY) = 100;
    M(kViewX) = 0;
    camera::SetRemainderForTest(kGame, 5, 0);
    camera::Move(kGame, -20, 0);
    camera::Remainder(&px, &py);
    Check(M(kViewX) == 0 && px == 0, "past the left edge: clamped to 0 on a whole tile");
    M(kViewX) = 50;
    camera::SetRemainderForTest(kGame, 0, 0);
    camera::Move(kGame, 0, 7);
    camera::Remainder(&px, &py);
    Check(M(kViewY) == 100 && py == 7, "inside the map: the remainder kept");
    M(kViewX) = 20; // someone else (a minimap click)
    camera::SyncView(kGame);
    camera::Remainder(&px, &py);
    Check(px == 0 && py == 0, "the view moved by someone else: whole tile again");
}

void EdgeScrolling()
{
    M(kViewX) = 10;
    M(kViewY) = 100;
    camera::SetRemainderForTest(kGame, 0, 0);
    Middle();
    char stats[256];
    camera::TakeStats(kGame, stats, sizeof(stats)); // a new window
    Check(Frame() && M(kScrolling) == 0, "cursor in the middle: the camera's frame, not scrolling");
    M(kCursorX) = 1023; // the right edge of 1024
    const DWORD t0 = GetTickCount();
    const int moved = RunX(1000);
    const double ms = GetTickCount() - t0;
    // speed 50%: Augustus's 12 px per 16.67 ms frame = 720 px/s (a 60 px column every 83.3 ms); the start's 75 ms
    // easing loses about 37 ms.
    const double rate = moved * 1000.0 / ms;
    char what[160];
    std::snprintf(what, sizeof(what), "right edge at 50%%: %.0f px/s (Augustus's 720, less the start's easing)",
                  rate);
    Check(rate > 720 * 0.85 && rate < 720 * 1.03, what);
    Check(M(kScrolling) == 1, "view_scrolling set while at the edge (the simulation waits, as in the game)");
    const int x = M(kViewX);
    int px, py;
    camera::Remainder(&px, &py);
    camera::CityDirection(kGame); // the same frame again
    int px2, py2;
    camera::Remainder(&px2, &py2);
    Check(M(kViewX) == x && px2 == px, "once per game frame: a second call in the same frame does not move");
    camera::OnLoop(kGame, 1000);
    camera::TakeStats(kGame, stats, sizeof(stats));
    Check(std::strstr(stats, "; scrolling 1000 ms at scroll speed 50%") != nullptr &&
              std::strstr(stats, "(expected 720 px/s per axis)") != nullptr,
          "rate log: the time scrolling and the expected speed (Augustus's table)");
    M(kSpeed) = 30;
    camera::OnLoop(kGame, 500);
    camera::TakeStats(kGame, stats, sizeof(stats));
    Check(std::strstr(stats, "(expected 480 px/s per axis)") != nullptr, "30%: expected 480 px/s");
    M(kSpeed) = 100;
    camera::OnLoop(kGame, 500);
    camera::TakeStats(kGame, stats, sizeof(stats));
    Check(std::strstr(stats, "(expected 3600 px/s per axis)") != nullptr, "100%: expected 3600 px/s");
    M(kSpeed) = 50;
    Middle();
    RunX(400); // stopped (the easing: at most 4 x 75 ms)
    Check(M(kScrolling) == 0, "back in the middle: not scrolling");
    M(kSpeed) = 0;
    M(kCursorX) = 1023;
    const int moved0 = RunX(150);
    Check(moved0 == 0 && M(kScrolling) == 1, "speed 0%: no movement (the game does not scroll either)");
    camera::OnLoop(kGame, 200);
    camera::TakeStats(kGame, stats, sizeof(stats));
    Check(std::strstr(stats, "(expected 0 px/s per axis)") != nullptr, "speed 0%: the rate log expects 0 px/s");
    M(kSpeed) = 50;
    Middle();
    RunX(20);
    camera::TakeStats(kGame, stats, sizeof(stats));
}

void Keys()
{
    Middle();
    M(kViewX) = 50;
    camera::SetRemainderForTest(kGame, 0, 0);
    M(kKeyL) = 1; // the game's window procedure on VK_LEFT
    g_keyDown[VK_LEFT] = true;
    const int moved = RunX(300);
    Check(moved < -100 && M(kKeyL) == 0, "left arrow held: moving left, the game's flag cleared as the game does");
    g_keyDown[VK_LEFT] = false;
    RunX(400); // the easing stops it within 4 x 75 ms
    const int after = RunX(100);
    Check(after == 0 && M(kScrolling) == 0, "released: stops");
}

void CountsAndOff()
{
    char stats[256];
    camera::TakeStats(kGame, stats, sizeof(stats));
    camera::CountStep(kGame, 1); // right and up
    camera::CountStep(kGame, 2); // right
    camera::CountStep(kGame, 8); // none
    M(kScrolling) = 1;
    camera::OnLoop(kGame, 100);
    camera::TakeStats(kGame, stats, sizeof(stats));
    Check(std::strstr(stats, "x 1200 px/s, y 600 px/s, 2 steps") != nullptr,
          "the game's own steps: 2 counted, 120 px across and 60 px down in 100 ms");
    Check(camera::SetSmooth(kGame, false) && !camera::Smooth(), "switched off");
    Check(!Frame(), "off: the game's own direction");
    Check(camera::SetSmooth(kGame, true), "switched on again");
}

void ClampFault()
{
    M(kViewX) = 50;
    M(kViewY) = 100;
    camera::SetRemainderForTest(kGame, 0, 0);
    M(kCursorX) = 1023;
    for (int i = 0; i < 10; ++i) { // under way, with a remainder
        Frame();
        Sleep(4);
    }
    M(kCursorX) = 0; // now the left edge (so the frame changes view_scrolling's sentinel below)
    M(kScrolling) = 7;
    M(kKeyU) = 1;
    g_keyDown[VK_UP] = true;
    const int x = M(kViewX), y = M(kViewY);
    g_faultClamp = true;
    int frames = 0;
    bool moved = true;
    while (g_faultClamp && frames < 200) { // the clamp is reached on the first carry
        moved = Frame();
        ++frames;
        if (g_faultClamp) {
            M(kScrolling) = 7;
            M(kKeyU) = 1;
            Sleep(4);
        }
    }
    int px, py;
    camera::Remainder(&px, &py);
    Check(!g_faultClamp && !moved, "the fault: that frame goes to the game's own direction");
    Check(M(kScrolling) == 7 && M(kKeyU) == 1, "view_scrolling and the key flag put back");
    Check(M(kViewX) == x && M(kViewY) == y && px == 0 && py == 0, "the camera of the frame back, remainder 0");
    Check(!core::diag::Enabled(g_feature) && g_fallBacks == 1, "feature off, the game layer's fallback once");
    Check(!Frame() && !camera::SetSmooth(kGame, true), "afterwards the game's own scrolling; cannot be switched on");
    g_keyDown[VK_UP] = false;
}

} // namespace

int main()
{
    DeleteFileA("test_camera.log");
    core_log_open(GetModuleHandleW(nullptr), "test_camera.log");
    core::diag::InstallExceptionLog();
    core::site::g_imageShift = reinterpret_cast<DWORD>(g_mem) - kBase;
    M(kViewX) = 50;
    M(kViewY) = 50;
    M(kTileW) = 58; // a column is 60 px
    M(kTileHalfH) = 15; // a row pair is 30 px
    M(kDesktopW) = 1024;
    M(kDesktopH) = 768;
    M(kSpeed) = 50;
    Middle();
    camera::Start(kGame, true);
    Moves();
    Edges();
    EdgeScrolling();
    Keys();
    CountsAndOff();
    ClampFault();
    std::printf("RESULT: %s (%d failure(s))\n", g_failures ? "FAIL" : "PASS", g_failures);
    return g_failures ? 1 : 0;
}
