# Zeus: Master of Olympus + Poseidon -- the game's part of the build (included by the root CMakeLists.txt when
# ENGINE_GAME is zeus). Everything here is about this game only: its public name and version, its settings file,
# its game layer and the offline tests of that layer.

# The public name and version: shown in the log, the panel and the DLL's properties (version.rc.in). Test builds
# carry a suffix (e.g. "-dev") so their logs are not taken for a release; empty for a release.
set(GAME_PROJECT zeus_engine)
set(GAME_VERSION 0.1.0)
set(GAME_VERSION_SUFFIX "-dev")
set(GAME_PRODUCT "Pericles")
set(GAME_DESCRIPTION "Pericles: modern display layer for Zeus: Master of Olympus")

# The settings file this version ships (copied next to the DLL and embedded in it), and the namespace of the
# generated header that holds it (games/zeus/zeus.cpp: core::settings::SetTemplate).
set(GAME_INI_SOURCE src/games/zeus/zeus_engine.ini)
set(GAME_NAMESPACE zeus)

# The game layer, linked into ddraw.dll after the core.
set(GAME_SOURCES
    src/games/zeus/game.cpp
    src/games/zeus/zeus.cpp
    src/games/zeus/debug_log.cpp
    src/games/zeus/timing.cpp
    src/games/zeus/anim_clock.cpp
    src/games/zeus/diag.cpp
    src/games/zeus/state_check.cpp
)

# The offline tests of this game layer (run by src/games/zeus/tests.ps1 through proxy/ddraw/run_tests.ps1).
macro(game_add_tests)
    # games/zeus/debug_log: reading the game's texts safely, the repeat limit (no game, no DLL).
    add_executable(test_debug_log src/games/zeus/test_debug_log.cpp src/games/zeus/debug_log.cpp)
    target_include_directories(test_debug_log PRIVATE src)
    target_link_options(test_debug_log PRIVATE -static)
    # games/zeus/anim_clock: the figures' animation timer rule on the game-time clock (no game, no DLL).
    add_executable(test_anim_clock src/games/zeus/test_anim_clock.cpp src/games/zeus/anim_clock.cpp)
    target_include_directories(test_anim_clock PRIVATE src)
    target_link_options(test_anim_clock PRIVATE -static)

    # The failure fallbacks of the game layer: the real detours of games/zeus on a copy of the game's address range
    # with fake game functions (each feature that changes the game's state adds a scenario). The
    # parts of the DLL they call that are not under test (output, panel, games/zeus/diag) are stubbed in the test.
    add_executable(test_restore src/games/zeus/test_restore.cpp
        src/games/zeus/timing.cpp src/games/zeus/anim_clock.cpp src/games/zeus/state_check.cpp
        src/core/diag/guard.cpp src/core/diag/report.cpp src/core/diag/perf.cpp src/core/diag/environment.cpp
        src/core/diag/rate_log.cpp src/core/sim/pace.cpp
        src/core/diag/stall.cpp src/core/platform/affinity.cpp src/core/log.cpp src/core/config.cpp
        src/core/settings/settings.cpp src/core/settings/ini_text.cpp src/core/hook/hook.cpp
        src/core/platform/files.cpp src/core/game.cpp src/core/ui/hotkey.cpp)
    target_include_directories(test_restore PRIVATE src ${CMAKE_CURRENT_BINARY_DIR}/generated)
    target_compile_options(test_restore PRIVATE -fno-exceptions)
    target_link_options(test_restore PRIVATE -static)
    target_link_libraries(test_restore PRIVATE minhook winmm)
endmacro()
