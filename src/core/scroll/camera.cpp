// The camera of smooth scrolling (camera.h). Moved from games/emperor/scroll.cpp (Yongle's) for Zeus; the
// behavior is Yongle's, except the speeds, which are Augustus's now (StepMs) instead of the game's own timing.
#include "core/scroll/camera.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "core/log.h"
#include "core/scroll/smooth_scroll.h"

#include <cstdio>
#include <cstdlib>

namespace core::scroll::camera {

namespace {

int g_px = 0, g_py = 0; // pixel remainder: 0 <= g_px < tile_width + 2, 0 <= g_py < 2 x tile_half_height
int g_lastViewX = 0, g_lastViewY = 0;
bool g_haveLast = false;
int g_sinceDecayPx = 0; // movement since the last sound step
bool g_smooth = false;
SmoothScroll g_scroller;
int g_lastFrame = -1;
DragTake g_dragTake = nullptr;
DragLoop g_dragLoop = nullptr;
bool g_dragFrame = false; // this frame's view was moved by a drag (the game's own step gets no direction)
int g_edge = 1;           // edge scrolling: pixels from the screen's edge

// Statistics for the rate log
double g_scrollingMs = 0;          // time with view_scrolling set
double g_movedX = 0, g_movedY = 0; // pixels moved (smooth) or stepped (the game's own)
long g_steps = 0;                  // the game's view_scroll_step calls with a direction
long g_carries = 0;                // smooth: tile changes
long g_logged = 0;
constexpr long kMoveLogs = 12;

// The cruise speed by the game's speed index (100 - speed) / 10, as the time of one step (one column of
// tile_width + 2 px across, 4 half rows down). The speeds are Augustus's SCROLL_STEP for the city (src/input/scroll.c,
// https://github.com/Keriew/augustus, AGPL-3.0; the numbers only): px per 16.67 ms frame
// for a 60 px column (Caesar 3's tile, Zeus's column), 100% .. 10%. As the time of a column, every game moves the same
// number of columns a second (Emperor's column is 80 px). Chosen after comparing it with the original
// game's own timing (100% was far too fast) and two other tables in both games.
constexpr int kPerFrame[] = {60, 44, 30, 20, 16, 12, 10, 8, 6, 4};

double StepMs(int speed)
{
    int idx = (100 - speed) / 10;
    idx = idx < 0 ? 0 : idx > 9 ? 9 : idx; // 10 (speed 0) does not scroll at all (the callers check)
    return 1000.0 / kPerFrame[idx];
}

// The largest view_x / view_y the game's clamp allows right now. (The camera is in the frame's Restore list: if the
// clamp faults here, the guard puts the camera of the frame back.)
void Limits(const CameraGame &g, int *maxX, int *maxY)
{
    const CameraVars &v = g.vars;
    const int sx = *v.viewX, sy = *v.viewY;
    *v.viewX = 1 << 20;
    *v.viewY = 1 << 20;
    g.clampView();
    *maxX = *v.viewX;
    *maxY = *v.viewY;
    *v.viewX = sx;
    *v.viewY = sy;
}

// A held arrow key: the game sets its flag on WM_KEYDOWN (only where it allows keyboard scrolling) and its own
// direction function clears it on every step, the key repeat setting it again. Per frame that would flicker, so a key
// counts as held from its first flag until the key is released.
struct HeldKey {
    IntVar CameraVars::*flag;
    unsigned vk;
    bool held = false;
    bool Poll(const CameraGame &g)
    {
        const IntVar &f = g.vars.*flag;
        if (*f) {
            *f = 0;
            held = true;
        }
        if (held && !g.keyHeld(vk)) {
            held = false;
        }
        return held;
    }
};
HeldKey g_left{&CameraVars::keyLeft, VK_LEFT}, g_right{&CameraVars::keyRight, VK_RIGHT},
    g_up{&CameraVars::keyUp, VK_UP}, g_down{&CameraVars::keyDown, VK_DOWN};

// The edge scrolling and arrow keys of one frame.
void SmoothFrame(const CameraGame &g)
{
    const CameraVars &v = g.vars;
    const int x = *v.cursorX, y = *v.cursorY;
    const bool keyLeft = g_left.Poll(g), keyRight = g_right.Poll(g), keyUp = g_up.Poll(g), keyDown = g_down.Poll(g);
    const bool left = keyLeft || x < g_edge;
    const bool right = keyRight || x >= *v.desktopWidth - g_edge;
    const bool up = keyUp || y < g_edge;
    const bool down = keyDown || y >= *v.desktopHeight - g_edge;
    int dirX = (right ? 1 : 0) - (left ? 1 : 0);
    int dirY = (down ? 1 : 0) - (up ? 1 : 0);
    *v.viewScrolling = (left || right || up || down) ? 1 : 0; // as scroll_get_direction: the simulation pauses
    if ((100 - *v.scrollSpeed) / 10 >= 10) {
        dirX = dirY = 0; // the game does not scroll at all then
    }
    const double stepMs = StepMs(*v.scrollSpeed);
    double vx = (*v.tileWidth + 2) / stepMs, vy = (4 * *v.tileHalfHeight) / stepMs;
    double fx = 1, fy = 1;
    if (g.zoomScale(&fx, &fy)) { // zoomed: the camera moves in the game's pixels, on the screen as fast as at 100%
        vx *= fx;
        vy *= fy;
    }
    int dx = 0, dy = 0;
    g_scroller.Update(dirX, dirY, vx, vy, &dx, &dy);
    g_movedX += std::abs(dx);
    g_movedY += std::abs(dy);
    Move(g, dx, dy);
}

// Once per game frame: the zoom's step, then smooth scrolling. r: the camera, view_scrolling and the key flags as they
// were before the frame (put back if the frame is abandoned; kept when it completes).
struct FrameCall {
    const CameraGame *g;
    core::diag::Restore r;
};

void Frame(FrameCall &c)
{
    const CameraGame &g = *c.g;
    const CameraVars &v = g.vars;
    if (*v.frameCount == g_lastFrame) {
        return;
    }
    g_lastFrame = *v.frameCount;
    c.r.Save(v.viewX.ptr());
    c.r.Save(v.viewY.ptr());
    c.r.Save(v.viewScrolling.ptr());
    c.r.Save(v.keyLeft.ptr());
    c.r.Save(v.keyRight.ptr());
    c.r.Save(v.keyUp.ptr());
    c.r.Save(v.keyDown.ptr());
    SyncView(g);
    if (g.zoomMoving()) {
        g.zoomFrame();
    }
    int dx = 0, dy = 0;
    const bool wasDrag = g_dragFrame;
    g_dragFrame = g_dragTake && g_dragTake(g, g_smooth, &dx, &dy);
    if (wasDrag && !g_dragFrame && !g_smooth) {
        *v.viewScrolling = 0; // the drag set it; without smooth scrolling nothing of ours writes it again
    }
    if (g_dragFrame) {
        // A drag: the mouse moves the view; the edges and the arrow keys wait (the speed from rest afterwards).
        g_scroller.Reset();
        *v.viewScrolling = (dx != 0 || dy != 0) ? 1 : 0; // as edge scrolling: the simulation setting decides
        g_movedX += std::abs(dx);
        g_movedY += std::abs(dy);
        Move(g, dx, dy);
        return;
    }
    if (g_smooth) {
        SmoothFrame(g);
    }
}

} // namespace

void Start(const CameraGame &g, bool smooth)
{
    g_smooth = smooth;
    if (smooth) {
        core_log("%s: smooth scrolling on (the camera by time at Augustus's speed for each scroll speed setting, "
                 "Augustus easing %u ms; guarded: on an error the game's whole tiles)",
                 g.log, SmoothScroll::kEaseMs);
    }
    else {
        core_log("%s: smooth scrolling off (gameplay.smooth_scroll): the game's tile steps (hooks installed, passing "
                 "through, so it can be switched on from the panel)",
                 g.log);
    }
}

bool Smooth()
{
    return g_smooth;
}

bool SetSmooth(const CameraGame &g, bool smooth)
{
    if (smooth && !core::diag::Enabled(*g.feature)) {
        core_log("%s: smooth scrolling cannot be switched on: it was switched off after an error", g.log);
        return false;
    }
    if (smooth == g_smooth) {
        return true;
    }
    g_smooth = smooth;
    if (!smooth) {
        g_px = g_py = 0; // the view snaps to the whole tile it is on
    }
    g_scroller.Reset();
    core_log("%s: smooth scrolling switched %s", g.log, smooth ? "on" : "off");
    return true;
}

bool CityDirection(const CameraGame &g)
{
    if (!(g_smooth || g.zoomMoving() || g_dragTake) || !core::diag::Enabled(*g.feature)) {
        return false;
    }
    FrameCall c{&g, {}};
    if (core::diag::Guarded(*g.feature, [](void *p) { Frame(*static_cast<FrameCall *>(p)); }, &c, &c.r)) {
        return g_smooth || g_dragFrame; // the camera has been moved already (smooth, or by a drag)
    }
    g_dragFrame = false;
    FallBack(g);
    return false;
}

void CountStep(const CameraGame &g, int dir)
{
    if (dir >= 0 && dir < 8) {
        ++g_steps;
        const int colPx = *g.vars.tileWidth + 2, rowPx = 4 * *g.vars.tileHalfHeight;
        if (dir != 0 && dir != 4) {
            g_movedX += colPx; // 1, 2, 3 right; 5, 6, 7 left
        }
        if (dir != 2 && dir != 6) {
            g_movedY += rowPx; // 7, 0, 1 up; 3, 4, 5 down
        }
    }
}

int GameDirection(const CameraGame &g, int (*call)())
{
    const CameraVars &v = g.vars;
    const int x = *v.cursorX, y = *v.cursorY, w = *v.desktopWidth, h = *v.desktopHeight;
    if (g_edge == 1 || w < 3 || h < 3) {
        return call(); // the game's own 1 px
    }
    // The game's test: x <= 0 or x >= width - 1 (y the same).
    auto seen = [](int c, int size) {
        if (g_edge == 0) {
            return c < 1 ? 1 : c > size - 2 ? size - 2 : c; // never on an edge
        }
        return c < g_edge ? 0 : c >= size - g_edge ? size - 1 : c;
    };
    *v.cursorX = seen(x, w);
    *v.cursorY = seen(y, h);
    const int dir = call();
    *v.cursorX = x;
    *v.cursorY = y;
    return dir;
}

void SetDrag(DragTake take, DragLoop loop)
{
    g_dragTake = take;
    g_dragLoop = loop;
}

void SetEdgeWidth(int px)
{
    g_edge = px < 0 ? 0 : px; // 0: no edge scrolling (x < 0 and x >= width never hold)
}

void OnLoop(const CameraGame &g, double ms)
{
    if (g_dragLoop) {
        g_dragLoop();
    }
    if (*g.vars.viewScrolling) {
        g_scrollingMs += ms;
    }
}

void Resume()
{
    g_scroller.Reset();
}

void TakeStats(const CameraGame &g, char *out, std::size_t size)
{
    if (g_scrollingMs < 1) {
        out[0] = 0;
    }
    else {
        const double s = g_scrollingMs / 1000.0;
        const int speed = *g.vars.scrollSpeed;
        const double expected = (100 - speed) / 10 >= 10 ? 0.0 // 0%: the view does not scroll at all
                                                          : (*g.vars.tileWidth + 2) * 1000.0 / StepMs(speed);
        std::snprintf(out, size,
                      "; scrolling %.0f ms at scroll speed %d%%: x %.0f px/s, y %.0f px/s, %ld steps, %ld tile changes "
                      "(expected %.0f px/s per axis)",
                      g_scrollingMs, speed, g_movedX / s, g_movedY / s, g_steps, g_carries, expected);
    }
    g_scrollingMs = 0;
    g_movedX = g_movedY = 0;
    g_steps = 0;
    g_carries = 0;
}

void FallBack(const CameraGame &g)
{
    static bool logged = false;
    g_px = g_py = 0;
    g_scroller.Reset();
    if (!logged) {
        logged = true;
        core_log("scroll: smooth scrolling is off after an error: the game's whole-tile scrolling from now on");
    }
    g.fallBack();
}

void SyncView(const CameraGame &g)
{
    const int vx = *g.vars.viewX, vy = *g.vars.viewY;
    if (!g_haveLast || vx != g_lastViewX || vy != g_lastViewY) {
        g_px = g_py = 0;
        g_scroller.Reset();
        g_lastViewX = vx;
        g_lastViewY = vy;
        g_haveLast = true;
    }
}

void Move(const CameraGame &g, int dx, int dy)
{
    if (dx == 0 && dy == 0) {
        return;
    }
    const CameraVars &v = g.vars;
    const int colPx = *v.tileWidth + 2;      // one view_x step
    const int rowPx = 2 * *v.tileHalfHeight; // one view_y step of two half rows (view_y stays even)
    if (colPx <= 0 || rowPx <= 0) {
        return; // no layout yet (never in the city)
    }
    g_px += dx;
    g_py += dy;
    int vx = *v.viewX, vy = *v.viewY;
    while (g_px >= colPx) { g_px -= colPx; ++vx; }
    while (g_px < 0) { g_px += colPx; --vx; }
    while (g_py >= rowPx) { g_py -= rowPx; vy += 2; }
    while (g_py < 0) { g_py += rowPx; vy -= 2; }
    const int oldX = *v.viewX, oldY = *v.viewY;
    const bool moved = vx != oldX || vy != oldY;
    *v.viewX = vx;
    *v.viewY = vy;
    g.clampView();
    if (*v.viewX != vx) { g_px = 0; g_scroller.ResetX(); } // hit a map edge: stop there on a whole tile
    if (*v.viewY != vy) { g_py = 0; g_scroller.ResetY(); }
    if (g_px != 0 || g_py != 0) {
        int maxX, maxY;
        Limits(g, &maxX, &maxY);
        // At the right/bottom limit a remainder would show what lies beyond the map.
        if (*v.viewX >= maxX && g_px != 0) { g_px = 0; g_scroller.ResetX(); }
        if (*v.viewY >= maxY && g_py != 0) { g_py = 0; g_scroller.ResetY(); }
    }
    if (moved) {
        // The game decays the ambient sound weights once per scroll step (one step = colPx either way, diagonals
        // included); keep that rate instead of decaying on every (half as large) vertical carry.
        const int ax = std::abs(*v.viewX - oldX) * colPx, ay = std::abs(*v.viewY - oldY) * *v.tileHalfHeight;
        g_sinceDecayPx += ax > ay ? ax : ay;
        if (g_sinceDecayPx >= colPx) {
            g.soundStep();
            g_sinceDecayPx = 0;
        }
        ++g_carries;
    }
    if (moved && ++g_logged <= kMoveLogs) {
        core_log("scroll: view (%d,%d) remainder (%d,%d) px", static_cast<int>(*v.viewX), static_cast<int>(*v.viewY),
                 g_px, g_py);
    }
    g_lastViewX = *v.viewX;
    g_lastViewY = *v.viewY;
}

void NoteView(const CameraGame &g)
{
    g_lastViewX = *g.vars.viewX;
    g_lastViewY = *g.vars.viewY;
}

void Remainder(int *px, int *py)
{
    *px = g_px;
    *py = g_py;
}

void ClearRemainder()
{
    g_px = g_py = 0;
}

void ZeroRemainder(core::diag::Restore &r)
{
    r.Set(&g_px, 0);
    r.Set(&g_py, 0);
}

void SetRemainderForTest(const CameraGame &g, int px, int py)
{
    g_px = px;
    g_py = py;
    g_haveLast = true;
    NoteView(g);
}

bool IsCall(const void *ret, const DWORD *calls, int n)
{
    const DWORD r = core::site::GameAddress(ret);
    for (int i = 0; i < n; ++i) {
        if (r == calls[i]) {
            return true;
        }
    }
    return false;
}

bool CallsTarget(const char *log, const DWORD *calls, int n, const core::site::Site &target)
{
    for (int i = 0; i < n; ++i) {
        const DWORD ret = calls[i];
        const BYTE *call = reinterpret_cast<const BYTE *>(ret - 5);
        const DWORD to = ret + *reinterpret_cast<const DWORD *>(call + 1);
        if (call[0] != 0xe8 || to != target.address) {
            core_log("%s: city scroll call before 0x%lx is not a call of %s; smooth scrolling not installed", log,
                     ret, target.name);
            return false;
        }
    }
    return true;
}

} // namespace core::scroll::camera
