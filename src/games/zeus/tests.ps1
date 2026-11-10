# Zeus's own offline tests, dot-sourced by proxy\ddraw\run_tests.ps1 -Game zeus after the core's tests ($repo, $dir
# and Run come from it; none of these is the game):
#   test_debug_log games/zeus/debug_log: the game's texts read safely (numbers, unreadable memory, control
#               characters, a text crossing into a page that cannot be read), the repeat limit (no DLL)
Run 'test_debug_log' 'emulated' @()
