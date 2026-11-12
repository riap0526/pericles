// Offline test of core/diag/autorun (not the game): the script is read as written, its steps run in order on a fake
// game (screen, ticks, quit) and a fake clock, shots wait until they are saved, the timeout ends the game, a bad
// script changes nothing, and without the environment variable nothing runs at all.
#include "core/diag/autorun.h"
#include "core/log.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdio>
#include <string>

// (core::diag::autorun::Watch is called directly below, as core/output's thread would.)
// core/output's side, which this test does not link (autorun.cpp calls these only through Start).
namespace core::output {
void RequestScreenshot() {}
void RequestReport() {}
long ScreenshotsSaved()
{
    return 0;
}
bool Threaded()
{
    return true;
}
} // namespace core::output

namespace ar = core::diag::autorun;

namespace {

int g_failures = 0;
void Check(bool ok, const char *what)
{
    std::printf("  %-70s %s\n", what, ok ? "ok" : "FAILED");
    if (!ok) {
        ++g_failures;
    }
}

int g_screen = 0x44;
long g_ticks = 1000;
int g_quits = 0;
unsigned long g_now = 0;
int g_shotRequests = 0, g_reportRequests = 0;
long g_saved = 0;
bool g_saveAtOnce = true;

int Screen()
{
    return g_screen;
}
long Ticks()
{
    return g_ticks;
}
void Quit()
{
    ++g_quits;
}
unsigned long Now()
{
    return g_now;
}
void Shot()
{
    ++g_shotRequests;
    if (g_saveAtOnce) {
        ++g_saved;
    }
}
void Report()
{
    ++g_reportRequests;
    if (g_saveAtOnce) {
        ++g_saved;
    }
}
long Saved()
{
    return g_saved;
}

const ar::Game kGame{&Screen, 0x00, 0x01, &Ticks, &Quit};
bool g_canShoot = true;
bool CanShoot()
{
    return g_canShoot;
}

const ar::Hooks kHooks{&Now, &Shot, &Report, &Saved, &CanShoot};

void Reset()
{
    g_screen = 0x44, g_ticks = 1000, g_quits = 0, g_now = 0, g_shotRequests = 0, g_reportRequests = 0, g_saved = 0;
    g_saveAtOnce = true;
    g_canShoot = true;
}

void Frames(int n)
{
    for (int i = 0; i < n; ++i) {
        g_now += 16;
        ar::Frame();
    }
}

} // namespace

int main()
{
    char dir[MAX_PATH];
    GetModuleFileNameA(nullptr, dir, MAX_PATH);
    core_log_open(nullptr, "test_autorun.log");
    std::printf("test_autorun: core/diag/autorun\n");

    // --- the script
    {
        ar::Script s;
        std::string e;
        const bool ok = ar::Parse("; a comment\n\nwait main_menu\nwait screen 0x20 ; inline\nwait frames 5\r\n"
                                  "wait ticks 300\nshot\nreport\nwait city\ntimeout 90\nquit\n",
                                  0x00, 0x01, &s, &e);
        Check(ok && s.steps.size() == 8, "a script of 8 steps is read (comments, blank lines, CRLF)");
        Check(ok && s.steps[0].op == ar::Op::WaitScreen && s.steps[0].value == 0x00, "wait main_menu = screen 0");
        Check(ok && s.steps[1].value == 0x20, "wait screen 0x20 (hex)");
        Check(ok && s.steps[3].op == ar::Op::WaitTicks && s.steps[3].value == 300, "wait ticks 300");
        Check(ok && s.steps[6].op == ar::Op::WaitScreen && s.steps[6].value == 0x01, "wait city = screen 1");
        Check(ok && s.timeoutSeconds == 90, "timeout 90 (not a step)");
        Check(!ar::Parse("wait sometimes\n", 0, 1, &s, &e) && e.find("line 1") != std::string::npos,
              "an unknown line is refused, with its number");
        Check(!ar::Parse("wait ticks -3\n", 0, 1, &s, &e), "a negative count is refused");
        Check(!ar::Parse("; nothing\n", 0, 1, &s, &e), "a script without steps is refused");
    }

    // --- a run: main menu held, shot, quit
    {
        Reset();
        Check(ar::StartWith(kGame, "wait main_menu\nshot\nquit\n", kHooks), "the script starts");
        Frames(20);
        Check(g_quits == 0 && g_shotRequests == 0, "nothing happens before the main menu");
        g_screen = 0x00;
        Frames(5);
        Check(g_shotRequests == 0, "the main menu must be held for a few frames");
        g_screen = 0x44;
        Frames(3);
        g_screen = 0x00;
        Frames(9);
        Check(g_shotRequests == 0, "a screen that went away starts the count again");
        Frames(2);
        Check(g_shotRequests == 1 && g_quits == 1, "held 10 frames: shot, then quit at once (saved)");
        Check(std::string(ar::State()) == "done", "the state is done");
        Frames(5);
        Check(g_quits == 1 && g_shotRequests == 1, "nothing more after the script");
    }

    // --- ticks, a shot saved later, a report
    {
        Reset();
        g_saveAtOnce = false;
        ar::StartWith(kGame, "wait city\nwait ticks 30\nshot\nreport\nquit\n", kHooks);
        g_screen = 0x01;
        Frames(10);
        Frames(1); // the tick count starts with the step's first frame
        g_ticks += 29;
        Frames(1);
        Check(g_shotRequests == 0, "29 of 30 ticks: still waiting");
        g_ticks += 1;
        Frames(1);
        Check(g_shotRequests == 0, "30 ticks: the wait is done (the next step starts with the next frame)");
        Frames(1);
        Check(g_shotRequests == 1, "  then the shot is asked for");
        Frames(10);
        Check(g_quits == 0 && g_reportRequests == 0, "the next step waits until the shot is saved");
        ++g_saved;
        Frames(1);
        Check(g_reportRequests == 1 && g_quits == 0, "saved: the report is asked for");
        ++g_saved;
        Frames(1);
        Check(g_quits == 1 && std::string(ar::State()) == "done", "the report saved: quit");
    }

    // --- a shot never saved, the timeout
    {
        Reset();
        g_saveAtOnce = false;
        ar::StartWith(kGame, "shot\nquit\n", kHooks);
        Frames(1);
        g_now += 16000;
        Frames(1);
        Check(g_quits == 1 && std::string(ar::State()) == "error", "a shot not saved in 15 s: error, the game is ended");

        Reset();
        ar::StartWith(kGame, "timeout 5\nwait city\nquit\n", kHooks);
        g_now += 130000; // an intro movie before the first game frame: not counted
        Frames(1);
        Check(g_quits == 0 && std::string(ar::State()) == "running",
              "the time before the first game frame does not count");
        Frames(10);
        Check(g_quits == 0, "within the timeout: still waiting");
        g_now += 6000;
        Frames(1);
        Check(g_quits == 1 && std::string(ar::State()) == "timeout", "past the timeout: the game is ended");
    }

    // --- the watchdog: the game's frames stop (a loop of its own); past the timeout: a shot, then quit
    {
        Reset();
        g_saveAtOnce = false;
        ar::StartWith(kGame, "timeout 10\nwait main_menu\nquit\n", kHooks);
        ar::Watch();
        Check(g_shotRequests == 0, "watchdog: nothing before the first frame");
        Frames(3); // then the game stops calling Frame
        g_now += 9000;
        ar::Watch();
        Check(g_shotRequests == 0 && g_quits == 0, "watchdog: within the timeout, nothing");
        g_now += 7000; // 16 s: past the timeout and the grace, no frame for 16 s
        ar::Watch();
        Check(g_shotRequests == 1 && g_quits == 0, "watchdog: past the timeout with no frame: a shot is asked for");
        ar::Watch();
        Check(g_quits == 0, "  and it waits for the shot to be saved");
        ++g_saved;
        ar::Watch();
        Check(g_quits == 1, "  saved: the game is ended");
        ar::Watch();
        Check(g_quits == 1 && g_shotRequests == 1, "  only once");
        Frames(1);
        Check(std::string(ar::State()) == "timeout", "  a frame after it: the state is timeout");

        Reset();
        ar::StartWith(kGame, "timeout 10\nwait main_menu\nquit\n", kHooks);
        for (int i = 0; i < 1500; ++i) { // 24 s of frames that never reach the main menu
            Frames(1);
            ar::Watch();
        }
        Check(g_quits == 1 && g_shotRequests == 0, "watchdog: while frames run, the script's own timeout ends it");
    }

    // --- wait ms
    {
        Reset();
        ar::StartWith(kGame, "wait ms 500\nquit\n", kHooks);
        Frames(30); // 480 ms
        Check(g_quits == 0, "wait ms 500: not after 480 ms");
        Frames(3); // 528 ms: 512 ms since the step's first frame
        Check(g_quits == 0, "  done after 500 ms (the next step starts with the next frame)");
        Frames(1);
        Check(g_quits == 1, "  then quit");
    }

    // --- whether shots can be taken is asked when the shot step starts, not on the first frame
    {
        Reset();
        g_canShoot = false; // e.g. a loading picture presented before the output thread exists
        ar::StartWith(kGame, "wait frames 3\nshot\nquit\n", kHooks);
        Frames(2);
        g_canShoot = true;
        Frames(5);
        Check(g_shotRequests == 1 && g_quits == 1, "no output thread on the first frames, there by the shot: taken");
    }

    // --- no output thread: a shot cannot be taken, the script ends with an error instead of waiting
    {
        Reset();
        g_canShoot = false;
        ar::StartWith(kGame, "shot\nquit\n", kHooks);
        Frames(1);
        Check(g_shotRequests == 0, "no output thread: the shot is not asked for");
        g_now += 16000;
        Frames(1);
        Check(g_quits == 1 && std::string(ar::State()) == "error", "  and the step fails after its wait, the game ends");
    }

    // --- a bad script, and no environment variable
    {
        Reset();
        Check(!ar::StartWith(kGame, "jump\n", kHooks) && !ar::Active(), "a bad script: off");
        Frames(5);
        Check(g_quits == 0 && g_shotRequests == 0, "  and the game is left alone");
        SetEnvironmentVariableW(L"IMPRESSIONS_AUTORUN", nullptr);
        Check(!ar::Start(kGame) && !ar::Active(), "without IMPRESSIONS_AUTORUN: not started");
        SetEnvironmentVariableW(L"IMPRESSIONS_AUTORUN", L"C:\\no such folder\\script.txt");
        Check(!ar::Start(kGame) && !ar::Active(), "a script that cannot be opened: not started");
        Frames(5);
        Check(g_quits == 0, "  and the game is left alone");
    }

    std::printf("  RESULT: %s (%d failure(s))\n", g_failures ? "FAIL" : "PASS", g_failures);
    return g_failures ? 1 : 0;
}
