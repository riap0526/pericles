#include "games/zeus/intro.h"

#include "core/diag/autorun.h"
#include "core/diag/guard.h"
#include "core/log.h"
#include "core/settings/intro_movie.h"
#include "core/settings/settings.h"

// Evidence (Zeus.exe 2.1.4.0, SHA256 382bcd28...0a57): game_main 0x5c3b44..0x5c3bbd
// plays the intro with bink_open (0x5c3bab) only when cmdline_file is empty and bink_player is not 0; with no player
// it goes to 0x5c3bb0 directly, which is where the call returns to (bink_open's return value is not used there).
namespace zeus::intro {
namespace {

// bink_open: __thiscall (ecx = bink_player, mov esi, ecx), 3 arguments (file, x, y), ret 0xc; returns al (0: the
// movie could not be opened, the screen unchanged). Called from game_main (the intro) and 0x421ac8 (other movies).
const Site kBinkOpen{"bink_open", 0x417b00, {0x81, 0xec, 0xc8, 0x00, 0x00, 0x00, 0x56, 0x8b}};
// game_main's call of it: call 0x417b00, then mov al, [0xf1389d] at 0x5c3bb0.
const Site kIntroCall{"game_main's intro movie call", 0x5c3bab, {0xe8, 0x50, 0x3f, 0xe5, 0xff, 0xa0, 0x9d, 0x38}};
constexpr DWORD kIntroReturn = 0x5c3bb0;

// __thiscall reached as __fastcall: ecx = the player, edx unused, the 3 arguments on the stack, the callee cleans.
using BinkOpen_fn = char(__fastcall *)(void *, void *, const char *, int, int);
BinkOpen_fn o_BinkOpen;

core::diag::Feature g_feature{"intro movie setting (games/zeus/intro)"};
bool g_skip = false; // decided by Attach

void Skip(void *file)
{
    // The game's string is read here first: a bad pointer faults in this guard, not inside the log's lock.
    const char *f = static_cast<const char *>(file);
    for (volatile const char *c = f; *c; ++c) {
    }
    core_log("intro: the intro movie (%s) is skipped: straight to the main menu (the game's files are not changed)",
             static_cast<const char *>(file));
}

char __fastcall BinkOpen_detour(void *player, void *edx, const char *file, int x, int y)
{
    if (g_skip && GameAddress(__builtin_return_address(0)) == kIntroReturn &&
        core::diag::Guarded(g_feature, &Skip, const_cast<char *>(file))) {
        return 0; // as when the game has no Bink player (game_main does not look at it)
    }
    // Other movies; or a fault above (the feature is off): the game plays the movie.
    return Original(o_BinkOpen, kBinkOpen)(player, edx, file, x, y);
}

} // namespace

void RegisterSettings()
{
    using core::settings::Effect;
    using core::settings::Item;
    using core::settings::Type;
    namespace im = core::settings::intro_movie;
    const int i = core::settings::Register(Item{
        "ui", "intro_movie", "Intro movie",
        "Whether the game plays its intro movie at start-up. Takes effect the next time the game starts.",
        Type::Choice, "play", Effect::Restart, true, true, 0, 0, 0, nullptr, im::kChoices, im::kChoiceLabels,
        im::kChoiceCount});
    // No old key to move: Zeus never had one. No sentence about the game's own settings: Zeus has none for its intro.
    im::Watch(i);
}

void Attach()
{
    namespace autorun = core::diag::autorun;
    if (autorun::Requested() && !autorun::IntroRequested()) {
        g_skip = true;
        core_log("intro: a smoke test: the intro movie is skipped whatever intro_movie says");
    }
    else {
        g_skip = core::settings::intro_movie::Skip();
        if (!g_skip) {
            core_log("intro: intro_movie = play: the game plays its intro movie");
            return; // nothing installed
        }
        core_log("intro: intro_movie = skip");
    }
    if (!Matches(kBinkOpen) || !Matches(kIntroCall)) {
        g_skip = false;
        core_log("intro: %s does not match: the intro movie plays", Matches(kBinkOpen) ? kIntroCall.name : kBinkOpen.name);
        return;
    }
    if (!Install(kBinkOpen, &BinkOpen_detour, o_BinkOpen)) {
        g_skip = false;
        core_log("intro: bink_open could not be hooked: the intro movie plays");
    }
}

const Detour *Detours(int *count)
{
    static const Detour kDetours[] = {
        {&kBinkOpen, reinterpret_cast<void *>(&BinkOpen_detour)},
    };
    *count = static_cast<int>(sizeof(kDetours) / sizeof(kDetours[0]));
    return kDetours;
}

void SkipForTest(bool skip)
{
    g_skip = skip;
}

} // namespace zeus::intro
