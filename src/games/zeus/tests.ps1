# Zeus's own offline tests, dot-sourced by proxy\ddraw\run_tests.ps1 -Game zeus after the core's tests ($repo, $dir,
# $GamePath, $notRun and Run come from it; none of these is the game):
#   test_debug_log games/zeus/debug_log: the game's texts read safely (numbers, unreadable memory, control
#               characters, a text crossing into a page that cannot be read), the repeat limit (no DLL)
#   test_anim_clock games/zeus/anim_clock: the game's animation timer rule on the game-time clock (which timers
#               fire at 90%, 100% capped and 50%, the clock's wrap-around)
#   test_restore the failure fallbacks of the game layer: real detours on a copy of the game's address range with
#               fake game functions that fault on purpose; one child process per scenario (no DLL)
#   test_sg3    core/assets/sg3 on the player's own DATA\Zeus_Interface.sg3, if present (no DLL; nothing
#               written)
Run 'test_debug_log' 'emulated' @()
Run 'test_anim_clock' 'emulated' @()
Run 'test_restore' 'emulated' @()
# The framing's panel tiles from the player's own DATA\Zeus_Interface.sg3 (version 213; run_tests.ps1 -GamePath):
# they must decode exactly as the Python reader decodes them (hashes from the Python reader, GOG Zeus 2.1.4.0;
# nothing written).
$pack = if ($GamePath) { Join-Path $GamePath 'DATA\Zeus_Interface.sg3' } else { '' }
if (-not ($pack -and (Test-Path $pack))) { $script:notRun += "test_sg3: no DATA\Zeus_Interface.sg3" }
else {
    "  the game's pack: $pack"
    Run 'test_sg3' 'emulated' @((Resolve-Path $pack).Path, '340', '144', 'd0d8c6a1') # the dialog panel (group 0x83)
    Run 'test_sg3' 'emulated' @((Resolve-Path $pack).Path, '484', '49', 'a106b497')  # the inset panel (group 0x84)
}
