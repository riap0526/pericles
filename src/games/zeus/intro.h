// [ui] intro_movie = play | skip in Zeus (intro.cpp). The game plays Binks\zeus_intro.bik (or the HiQ one) from
// game_main before its main loop, unless it was started with /sg=, /map= or /custom=; it has no setting of its own for
// it. With skip, game_main's call of the Bink player is left out: the game then goes on as when it has no Bink player
// (straight to the main menu). Other movies (e.g. after a won mission) are not touched. No file of the game is
// written. In a smoke test (core/diag/autorun.h Requested) the intro is always skipped unless the test runner asks for
// it (IntroRequested: then the setting decides).
#pragma once

#include "games/zeus/site.h"

namespace zeus::intro {

// From zeus_register_settings (DllMain): [ui] intro_movie.
void RegisterSettings();

// From zeus_attach (the exe's entry point, before game_main runs): decides skip or play and, only for skip, checks
// the call site and installs the hook; when they do not match nothing is installed (logged): the movie plays.
void Attach();

// For the offline tests (src/games/zeus/test_restore.cpp): the detours by site, and skip as if decided by Attach.
const Detour *Detours(int *count);
void SkipForTest(bool skip);

} // namespace zeus::intro
