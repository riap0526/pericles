// Diagnostics and layout logs for Zeus. Only logs and the debug panel: nothing here
// changes what the game does.
//
//   State line      the game's state for problem reports (F12) and canary reports: screen, mode, dialog offset, city
//                   viewport and view.
//   Panel           the debug panel's title and its "Game" and "Performance" sections.
//   game_main       0x5c37c0 (WinMain, __stdcall, 4 arguments): when it returns, the last rate line, the screenshots
//                   still being written and the timer resolution (timing.cpp's GameEnded).
//   Screen and mode every change of game_screen 0xf2a6dd and of the mode (screen_mode, dd_width x dd_height, dialog
//                   offsets) gets a log line (compared once per frame; no hook).
//   City viewport   city_render's viewport (0x7d3b94 left, 0x7d3b8c top, 0x7d3b90 right, 0x7d3b88 bottom) and view
//                   arguments; a line when the viewport changes.
//   Backgrounds     jpeg_draw_background 0x504fa0 (__cdecl: file, x, y): a line per new picture or position.
//   Draw census     [debug] draw_census (read at start-up): image_draw 0x5acf50 (__thiscall, ecx = the image
//                   paks, 6 arguments, ret 0x18) is hooked and, on the first frame of each screen and on each problem
//                   report, what the frame draws outside the city drawing is summed up by image file (count and area,
//                   cut to the screen) with its largest pictures. A screen is told apart by game_screen, the editor,
//                   the mode and (0x07, 0x09) which dialog; a new one is summed up once it has stayed 0.4 s, and its
//                   frame saved as shots\census_screen_0x.._game_....png (the screen list; not for an empty frame or
//                   screen 0xff). F11 also sums up the next frame. Background pictures drawn outside game_frame (the
//                   loading screens) are saved once per file.
#pragma once

#include <cstddef>

namespace zeus::diag {

void RegisterSettings(); // [debug] draw_census
void Attach();           // after the exe check

// Whether game_main is where expected (checked before any hook of games/zeus/timing, as Emperor's layer installs
// nothing without it: the end of the game flushes the screenshots and ends the timer resolution there).
bool GameMainMatches();
// Whether jpeg_draw_background is hooked (Attach): the framed front-end screens need it.
bool PicturesHooked();

// image_draw 0x5acf50 was compared with its Site and hooked for the draw census (its bytes are then ours, so
// another module cannot compare them again; games/zeus/resolution uses this).
bool ImageDrawHooked();

// From timing.cpp's game_frame hook (game thread).
void FrameBegin();
void FrameEnd();
void CensusForReport(); // the next frame is summed up (a problem report)
void ScreenshotTaken(); // from the screenshot file-name tag (F11, F12): the next frame is summed up

// From timing.cpp's city_render hook (game thread), with its arguments.
void CityRender(int a, int b, int c);
// From timing.cpp around the game's city drawing: the census counts those pictures apart (the city's own tiles and
// figures are not part of a screen's layout).
void CityDrawing(bool on);

// The game's state in one line (game thread).
void Describe(char *out, std::size_t size);

} // namespace zeus::diag
