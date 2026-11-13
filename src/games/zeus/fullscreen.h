// Framed full-screen pictures in Zeus (fullscreen.cpp lists the addresses). The framing itself is the core's
// (core/display/framed_screens, shared with Emperor's layer); this layer gives it the interface pack, the tiles'
// entries, the game's variables and the screens without the city, and calls it from the game's picture function, the
// frame loop, the city drawing and the game's end. Zeus draws its loading screens' status line inside the picture
// itself: no status block of ours.
#pragma once

namespace zeus::fullscreen {

// From zeus_register_settings: [video] loading_screen, menu_screens, background_blur, background_brightness.
void RegisterSettings();

// Needs zeus::resolution installed. Returns false (nothing installed) without it.
bool Attach();

// From jpeg_draw_background's detour (games/zeus/diag): before the picture is drawn (debug.loading_stage_ms holds the
// loading picture shown last), and after it was drawn at (x, y).
void BeforePicture(const char *file);
void AfterPicture(const char *file, int x, int y);

// Once per game_frame, also the draw-only frames while the panel asks (game thread): framing on or off, the
// background composed for the current layout.
void OnLoop();

// While the panel keeps the game paused (games/zeus/timing; game thread, no game_frame): a city background finished
// meanwhile (the panel's blur or brightness), a new window size or scale is put in place.
void WhilePaused();

// After a city drawing that counts (games/zeus/timing): the city on screen, kept every few seconds (not on a front-end
// screen: the map editor's map is not the player's city).
void CityDrawn();

// game_main returned (games/zeus/diag; game thread): the city seen last is saved now if it was not yet.
void OnGameEnd();

// menu_screens = city: whether this front-end screen shows the city around it.
// The screens before a mission (0x3d adventure and episode choice, 0x35 the episode's introduction, 0x04 the defeat
// picture before trying again) and the editors' screens get the panel pattern: the next episode is usually another
// map, and the editors have nothing to do with the city played last. The player's own menu_background picture is
// shown on every screen.
inline bool CityAround(unsigned char screen, bool playerPicture)
{
    if (playerPicture) {
        return true;
    }
    switch (screen) {
    case 0x3d: case 0x35: case 0x04:                                             // before a mission
    case 0x10: case 0x15: case 0x18: case 0x1b: case 0x27: case 0x29:            // the editors
    case 0x2b: case 0x2c: case 0x2d: case 0x46: case 0x57: case 0x58:
        return false;
    default:
        return true;
    }
}

// CityAround for the screen shown: the drop-down menu 0x03 is judged by the screen under it (0x781894).
bool CityAroundShown(unsigned char screen, bool playerPicture);

// For the offline tests: whether a picture file is one of the loading pictures.
bool LoadingPicture(const char *file);

} // namespace zeus::fullscreen
