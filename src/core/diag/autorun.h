// Unattended smoke test: a short script of steps the game runs by itself -- wait for
// a screen or for a number of ticks, take a screenshot, write a report, end the game -- so a build can be checked for
// the faults that need no judgment (it starts, its hooks are in, nothing is switched off, the screens look as before)
// before anyone plays it.
//
// Only when the process was started with the environment variable IMPRESSIONS_AUTORUN set to the script's full path
// (a test runner sets it for the game it starts; a player's shortcut or launcher never has it). Without it nothing
// here runs. Every step goes to the log as a line starting "AUTORUN:". A failure of ours stops the steps and the game
// goes on as without them (core/diag/guard); only "quit" and the timeout end the game, through its own exit.
//
// Script: one step per line; ';' starts a comment; blank lines are skipped.
//   wait main_menu       the game's main menu screen, held for a few frames in a row
//   wait city            the city screen, held for a few frames in a row
//   wait screen <n>      that screen number (decimal or 0x...), held for a few frames in a row
//   wait frames <n>      n more game frames (what a frame is depends on the game layer: Emperor counts every
//                        screen_present call, which on its main menu runs thousands of times a second)
//   wait ms <n>          n milliseconds (checked on each frame)
//   wait ticks <n>       n more simulation ticks
//   shot                 a screenshot (as the screenshot key), waited for until it is saved
//   report               a problem report (as the report key), waited for until its picture is saved
//   quit                 ask the game to end through its own exit
//   timeout <seconds>    the whole script's limit (default 120; anywhere in the script), counted from the game's first
//                        frame (an intro movie before it does not count): past it the game is ended
// Nothing here knows a game: the game layer passes the screen numbers and the functions below.
#pragma once

#include <string>
#include <vector>

namespace core::diag::autorun {

struct Game {
    int (*screen)();     // the game's current screen number
    int mainMenuScreen;  // its main menu
    int cityScreen;      // its city screen
    long (*ticks)();     // simulation ticks run so far (any start value)
    void (*quit)();      // ask the game to end through its own exit (e.g. its quit flag)
    // Optional: a line of the game's state for the watchdog's log (flags that decide whether its loop runs). Reads
    // only; called from another thread.
    void (*describe)(char *out, int size) = nullptr;
};

enum class Op { WaitScreen, WaitFrames, WaitMs, WaitTicks, Shot, Report, Quit };

struct Step {
    Op op;
    long value; // the screen number, or how many frames or ticks; 0 otherwise
    std::string text; // the line as written, for the log
};

struct Script {
    std::vector<Step> steps;
    long timeoutSeconds = 120;
};

// Reads the script text; the main menu and city names become the game's screen numbers. false with a message (line
// number and why) when a line is not understood.
bool Parse(const char *text, int mainMenuScreen, int cityScreen, Script *out, std::string *error);

// From the game layer's attach: reads IMPRESSIONS_AUTORUN and the script. Returns whether the autorun is on.
bool Start(const Game &game);

// From the game layer once per game frame, on the game thread, after the game's own frame. Does nothing when off.
void Frame();

bool Active();

// Whether the process was started for a smoke test (IMPRESSIONS_AUTORUN is set), whatever Start finds later: for
// what happens before the game's first frame (e.g. its intro movie), when Start has not run yet. A game layer skips
// its intro movie then, unless IntroRequested().
bool Requested();

// IMPRESSIONS_AUTORUN_INTRO=play: the test runner asks for the intro movie to be played as the settings say (the
// paths around the movie, e.g. its display mode switches).
bool IntroRequested();

// From another thread (core/output's thread, every pass): the watchdog. When the game's frames have stopped for a
// while and the script is past its timeout -- the game is in a loop of its own, e.g. a picture or a dialog waiting
// for a click -- a screenshot of that is asked for, then the game is ended through Game::quit. Does nothing
// otherwise, or when the autorun is off.
void Watch();

// For the offline test: the same as Start with a script given as text, and a clock in milliseconds (nullptr: the
// real one); shot and report requests go to the given functions, savedShots is a counter that grows when one is
// saved, and canShoot (asked when a shot or report step starts; nullptr: yes) says whether shots can be taken.
struct Hooks {
    unsigned long (*nowMs)();
    void (*requestShot)();
    void (*requestReport)();
    long (*savedShots)();
    bool (*canShoot)();
};
bool StartWith(const Game &game, const char *scriptText, const Hooks &hooks);
// The state after the script: "running", "done", "timeout" or "error".
const char *State();

} // namespace core::diag::autorun
