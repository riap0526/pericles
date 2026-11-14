// The logical resolution in Zeus (resolution.cpp lists the hooks and what each part falls back to).
#pragma once

#include "core/config.h"
#include "games/zeus/site.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace zeus::resolution {

// Checks the sites and installs the hooks (emulated DirectDraw only: the real one cannot set arbitrary modes).
// Returns false (nothing installed) if anything does not match the known build.
bool Attach(const core_config *cfg);

// [ui] info_window: registered before the settings file is loaded (games/zeus/zeus.cpp).
void RegisterSettings();

// The video.resolution or video.scale setting changed (game thread): the next OnLoop switches to it. False when it
// cannot be applied (resolution switching was switched off after an error).
bool SettingsChanged(const core_config *cfg);

// From game_frame's detour, before the game's frame: switches between the 1024x768 frame of the front-end screens and
// the configured resolution when the screen changes.
void OnLoop();

// From jpeg_draw_background's detour (games/zeus/diag), before the picture is drawn: a full-screen picture outside
// the city screens starts the 1024x768 frame.
void OnBackground();

// The key filter's part (games/zeus/zeus.cpp): Shift+F7 cycles the resolution until the game ends. True when the key
// is ours (the game does not see it).
bool KeyFilter(UINT message, WPARAM key, LPARAM l);

// For the offline tests (src/games/zeus/test_restore.cpp): the detours by site; the state Attach would set (without
// hooks).
const Detour *Detours(int *count);
void InstalledForTest(const core_config *cfg);
bool FrontEndForTest();

} // namespace zeus::resolution
