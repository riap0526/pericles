# Zeus's own offline tests, dot-sourced by proxy\ddraw\run_tests.ps1 -Game zeus after the core's tests ($repo, $dir
# and Run come from it; none of these is the game):
#   test_debug_log games/zeus/debug_log: the game's texts read safely (numbers, unreadable memory, control
#               characters, a text crossing into a page that cannot be read), the repeat limit (no DLL)
#   test_anim_clock games/zeus/anim_clock: the game's animation timer rule on the game-time clock (which timers
#               fire at 90%, 100% capped and 50%, the clock's wrap-around)
#   test_restore the failure fallbacks of the game layer: real detours on a copy of the game's address range with
#               fake game functions that fault on purpose; one child process per scenario (no DLL)
Run 'test_debug_log' 'emulated' @()
Run 'test_anim_clock' 'emulated' @()
Run 'test_restore' 'emulated' @()
