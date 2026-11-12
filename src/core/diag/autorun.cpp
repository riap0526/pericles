// Unattended smoke test (autorun.h).
#include "core/diag/autorun.h"

#include "core/diag/guard.h"
#include "core/log.h"
#include "core/output/sdl_output.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <atomic>
#include <cstring>

namespace core::diag::autorun {
namespace {

constexpr int kHoldFrames = 10;         // a screen counts once it stayed this many frames in a row
constexpr unsigned long kShotWaitMs = 15000; // a shot or report not saved by then fails the step

Game g_game{};
Script g_script;
Hooks g_hooks{};
bool g_on = false;
const char *g_state = "off";
std::size_t g_step = 0;
bool g_stepStarted = false;
long g_stepFrames = 0, g_stepTicks = 0, g_stepShots = 0;
int g_held = 0;
unsigned long g_startMs = 0, g_stepMs = 0;
long g_frames = 0;
bool g_quitAsked = false;
Feature g_feature{"autorun (core/diag)"};

// The watchdog (Watch, on another thread): the game thread publishes these; the watchdog only reads them, and acts
// only when the game's frames have stopped (the game is in a loop of its own, e.g. a picture waiting for a click).
constexpr unsigned long kWatchGraceMs = 5000; // past the timeout by this much with no frame: the watchdog acts
std::atomic<bool> g_watchRunning{false};
std::atomic<unsigned long> g_watchStart{0}, g_watchLastFrame{0}, g_watchTimeoutMs{0};
std::atomic<int> g_watchStage{0}; // 0 watching, 1 shot asked, 2 done (quit asked)
unsigned long g_watchShotAsked = 0;
long g_watchShotsBefore = 0;

unsigned long RealNow()
{
    return GetTickCount();
}
void OutputShot()
{
    core::output::RequestScreenshot();
}
void OutputReport()
{
    core::output::RequestReport();
}
long OutputSaved()
{
    return core::output::ScreenshotsSaved();
}
bool OutputThreaded()
{
    return core::output::Threaded(); // known once the game made its first surface, before its first frame
}
bool g_firstFrame = true;

std::string Trim(const std::string &s)
{
    std::size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) {
        ++a;
    }
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) {
        --b;
    }
    return s.substr(a, b - a);
}

bool Number(const std::string &s, long *out)
{
    if (s.empty()) {
        return false;
    }
    char *end = nullptr;
    const long v = std::strtol(s.c_str(), &end, 0);
    if (*end != 0 || v < 0) {
        return false;
    }
    *out = v;
    return true;
}

void Quit(const char *why)
{
    if (g_quitAsked) {
        return;
    }
    g_quitAsked = true;
    core_log("AUTORUN: quit (%s)", why);
    if (g_game.quit) {
        g_game.quit();
    }
}

void Finish(const char *state, const char *why)
{
    g_state = state;
    g_watchRunning = false;
    core_log("AUTORUN: %s: %s (step %u of %u, %ld frames, %lu ms)", state, why, static_cast<unsigned>(g_step + 1),
             static_cast<unsigned>(g_script.steps.size()), g_frames, g_hooks.nowMs() - g_startMs);
}

// One frame of the current step; true when it is done.
bool RunStep(const Step &s)
{
    const unsigned long now = g_hooks.nowMs();
    if (!g_stepStarted) {
        g_stepStarted = true;
        g_stepFrames = 0;
        g_stepTicks = g_game.ticks ? g_game.ticks() : 0;
        g_stepShots = g_hooks.savedShots ? g_hooks.savedShots() : 0;
        g_held = 0;
        g_stepMs = now;
        // Asked here, not earlier: a game may present (a loading picture) before the output thread exists.
        if ((s.op == Op::Shot || s.op == Op::Report) && g_hooks.canShoot && !g_hooks.canShoot()) {
            core_log("AUTORUN: note: present is not display or cap60 (no output thread): %s cannot be taken",
                     s.op == Op::Shot ? "a shot" : "a report");
            g_hooks.requestShot = nullptr;
            g_hooks.requestReport = nullptr;
        }
        if (s.op == Op::Shot && g_hooks.requestShot) {
            g_hooks.requestShot();
        }
        if (s.op == Op::Report && g_hooks.requestReport) {
            g_hooks.requestReport();
        }
    }
    ++g_stepFrames;
    switch (s.op) {
    case Op::WaitScreen:
        g_held = g_game.screen && g_game.screen() == s.value ? g_held + 1 : 0;
        return g_held >= kHoldFrames;
    case Op::WaitFrames:
        return g_stepFrames > s.value;
    case Op::WaitMs:
        return now - g_stepMs >= static_cast<unsigned long>(s.value);
    case Op::WaitTicks:
        return g_game.ticks && g_game.ticks() - g_stepTicks >= s.value;
    case Op::Shot:
    case Op::Report:
        if (g_hooks.savedShots && g_hooks.savedShots() > g_stepShots) {
            return true;
        }
        if (now - g_stepMs > kShotWaitMs) {
            Finish("error", "the picture was not saved in time");
            Quit("error");
        }
        return false;
    case Op::Quit:
        Quit("script");
        return true;
    }
    return true;
}

void FrameGuarded(void *)
{
    ++g_frames;
    if (std::strcmp(g_state, "running") != 0) {
        return;
    }
    if (g_watchStage.load() != 0) { // the watchdog took over while the game was in a loop of its own
        g_state = "timeout";
        return;
    }
    g_watchLastFrame = g_hooks.nowMs();
    if (g_firstFrame) {
        g_firstFrame = false;
        const unsigned long now = g_hooks.nowMs();
        core_log("AUTORUN: first game frame, %lu ms after the start; the timeout counts from here", now - g_startMs);
        g_startMs = now;
        g_watchStart = now;
        g_watchTimeoutMs = static_cast<unsigned long>(g_script.timeoutSeconds) * 1000ul;
        g_watchRunning = true;
    }
    if (g_hooks.nowMs() - g_startMs > static_cast<unsigned long>(g_script.timeoutSeconds) * 1000ul) {
        Finish("timeout", "the script took longer than its timeout");
        Quit("timeout");
        return;
    }
    while (g_step < g_script.steps.size()) {
        const Step &s = g_script.steps[g_step];
        if (!RunStep(s)) {
            return;
        }
        if (std::strcmp(g_state, "running") != 0) {
            return;
        }
        core_log("AUTORUN: step %u of %u done: %s (frame %ld, %lu ms)", static_cast<unsigned>(g_step + 1),
                 static_cast<unsigned>(g_script.steps.size()), s.text.c_str(), g_frames, g_hooks.nowMs() - g_startMs);
        ++g_step;
        g_stepStarted = false;
        if (s.op != Op::Shot && s.op != Op::Report && s.op != Op::Quit) {
            return; // the next wait starts with the next frame
        }
    }
    g_state = "done";
    g_watchRunning = false;
    core_log("AUTORUN: done: all %u steps (%ld frames, %lu ms)", static_cast<unsigned>(g_script.steps.size()), g_frames,
             g_hooks.nowMs() - g_startMs);
}

} // namespace

bool Parse(const char *text, int mainMenuScreen, int cityScreen, Script *out, std::string *error)
{
    out->steps.clear();
    out->timeoutSeconds = 120;
    int lineNo = 0;
    const char *p = text;
    while (*p) {
        const char *e = std::strchr(p, '\n');
        std::string line = e ? std::string(p, e) : std::string(p);
        p = e ? e + 1 : p + line.size();
        ++lineNo;
        const std::size_t c = line.find(';');
        if (c != std::string::npos) {
            line.resize(c);
        }
        line = Trim(line);
        if (line.empty()) {
            continue;
        }
        std::vector<std::string> w;
        std::size_t i = 0;
        while (i < line.size()) {
            while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) {
                ++i;
            }
            const std::size_t b = i;
            while (i < line.size() && line[i] != ' ' && line[i] != '\t') {
                ++i;
            }
            if (i > b) {
                w.push_back(line.substr(b, i - b));
            }
        }
        Step s{Op::Quit, 0, line};
        long n = 0;
        bool ok = true;
        if (w[0] == "wait" && w.size() == 2 && w[1] == "main_menu") {
            s.op = Op::WaitScreen, s.value = mainMenuScreen;
        }
        else if (w[0] == "wait" && w.size() == 2 && w[1] == "city") {
            s.op = Op::WaitScreen, s.value = cityScreen;
        }
        else if (w[0] == "wait" && w.size() == 3 && w[1] == "screen" && Number(w[2], &n)) {
            s.op = Op::WaitScreen, s.value = n;
        }
        else if (w[0] == "wait" && w.size() == 3 && w[1] == "frames" && Number(w[2], &n)) {
            s.op = Op::WaitFrames, s.value = n;
        }
        else if (w[0] == "wait" && w.size() == 3 && w[1] == "ms" && Number(w[2], &n)) {
            s.op = Op::WaitMs, s.value = n;
        }
        else if (w[0] == "wait" && w.size() == 3 && w[1] == "ticks" && Number(w[2], &n)) {
            s.op = Op::WaitTicks, s.value = n;
        }
        else if (w[0] == "shot" && w.size() == 1) {
            s.op = Op::Shot;
        }
        else if (w[0] == "report" && w.size() == 1) {
            s.op = Op::Report;
        }
        else if (w[0] == "quit" && w.size() == 1) {
            s.op = Op::Quit;
        }
        else if (w[0] == "timeout" && w.size() == 2 && Number(w[1], &n) && n > 0) {
            out->timeoutSeconds = n;
            continue;
        }
        else {
            ok = false;
        }
        if (!ok) {
            char buf[160];
            std::snprintf(buf, sizeof(buf), "line %d not understood: %s", lineNo, line.c_str());
            *error = buf;
            return false;
        }
        out->steps.push_back(s);
    }
    if (out->steps.empty()) {
        *error = "no steps";
        return false;
    }
    return true;
}

bool StartWith(const Game &game, const char *scriptText, const Hooks &hooks)
{
    g_game = game;
    g_hooks = hooks;
    if (!g_hooks.nowMs) {
        g_hooks.nowMs = &RealNow;
    }
    std::string error;
    g_step = 0;
    g_stepStarted = false;
    g_frames = 0;
    g_quitAsked = false;
    g_firstFrame = true;
    g_watchRunning = false;
    g_watchStage = 0;
    g_watchLastFrame = 0;
    if (!Parse(scriptText, game.mainMenuScreen, game.cityScreen, &g_script, &error)) {
        g_on = false;
        g_state = "error";
        core_log("AUTORUN: error: the script: %s; the game runs as without it", error.c_str());
        return false;
    }
    g_on = true;
    g_state = "running";
    g_startMs = g_hooks.nowMs();
    core_log("AUTORUN: start: %u steps, timeout %ld s", static_cast<unsigned>(g_script.steps.size()),
             g_script.timeoutSeconds);
    return true;
}

bool Start(const Game &game)
{
    wchar_t path[MAX_PATH];
    const DWORD n = GetEnvironmentVariableW(L"IMPRESSIONS_AUTORUN", path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) {
        return false; // the normal case: a player's start
    }
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) {
        core_log("AUTORUN: error: the script %ls cannot be opened (error %lu); the game runs as without it", path,
                 GetLastError());
        g_state = "error";
        return false;
    }
    std::string text;
    char buf[4096];
    DWORD got = 0;
    while (ReadFile(f, buf, sizeof(buf), &got, nullptr) && got > 0 && text.size() < 65536) {
        text.append(buf, got);
    }
    CloseHandle(f);
    core_log("AUTORUN: script %ls (%u bytes)", path, static_cast<unsigned>(text.size()));
    return StartWith(game, text.c_str(), Hooks{&RealNow, &OutputShot, &OutputReport, &OutputSaved, &OutputThreaded});
}

void Frame()
{
    if (!g_on) {
        return;
    }
    if (!Guarded(g_feature, &FrameGuarded, nullptr)) {
        g_on = false;
        g_state = "error";
        core_log("AUTORUN: error: a fault in the autorun; its steps stop, the game goes on");
    }
}

void Watch()
{
    if (!g_watchRunning.load()) {
        return;
    }
    const unsigned long now = g_hooks.nowMs();
    const int stage = g_watchStage.load();
    if (stage == 0) {
        const unsigned long timeout = g_watchTimeoutMs.load();
        if (now - g_watchStart.load() < timeout + kWatchGraceMs || now - g_watchLastFrame.load() < kWatchGraceMs) {
            return; // within the timeout, or the game's frames still run (Frame ends it then)
        }
        char state[200] = "";
        if (g_game.describe) {
            g_game.describe(state, sizeof(state));
        }
        core_log("AUTORUN: timeout (watchdog): no game frame for %lu ms; the game is in a loop of its own (%s). A "
                 "screenshot of where it stopped, then the game is ended",
                 now - g_watchLastFrame.load(), state[0] ? state : "no state from the game layer");
        g_watchShotAsked = now;
        g_watchShotsBefore = g_hooks.savedShots ? g_hooks.savedShots() : 0;
        if (g_hooks.requestShot) {
            g_hooks.requestShot();
        }
        g_watchStage = 1;
        return;
    }
    if (stage == 1) {
        const bool saved = g_hooks.savedShots && g_hooks.savedShots() > g_watchShotsBefore;
        if (!saved && now - g_watchShotAsked < kShotWaitMs) {
            return;
        }
        core_log("AUTORUN: quit (watchdog%s)", saved ? "" : "; the screenshot was not saved");
        g_watchStage = 2;
        g_watchRunning = false;
        if (g_game.quit) {
            g_game.quit(); // a flag the game's own loops read (its main loop and its dialog loop)
        }
    }
}

bool Active()
{
    return g_on && std::strcmp(g_state, "running") == 0;
}

const char *State()
{
    return g_state;
}

} // namespace core::diag::autorun
