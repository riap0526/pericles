// The game layer's part of the unattended smoke test (core/diag/autorun.h) beyond the screen, ticks and quit
// (timing.cpp): the actions "win", "lose" and the notice tests, the dialog area's offset for "click dialog", and a line of state for
// the log. Only used when the process was started for a smoke test; nothing here runs otherwise.
#pragma once

namespace zeus::smoke {

// The actions' names, separated by spaces (core/diag/autorun.h Game::actions).
extern const char kActions[];

// Game thread, after the game's frame (core/diag/autorun.h Game::action):
//   win   the game's own cheat Ambrosia, as when typed into the cheat box: cheat_run(0) sets cheat_win, and the next
//         month's mission_check takes the mission as won (about 816 ticks later). Only on the city screen and not
//         while governing on after a won mission (mission_check then ignores it).
//   lose  what objectives_check writes when the city has stayed in debt too long (lose_reason 0xb91 and the two
//         words of its message; the defeat screen says the city could not be brought out of debt); mission_check takes it on its next two monthly checks. Writes the game's variables
//         (there is no lose cheat). Only on the city screen, not governing on, not already lost.
//   notice, notice_long, notice_bracket, notice_red
//         a test text posted to our notice boxes (core/ui/notice): a short one, one about 310 pixels wide (the
//         widest kept in the 400-pixel box), one with '[' (replaced before the game sees it) and a red one (Zeus has
//         no red font: shown as the others).
//   zoom_50, zoom_75, zoom_125
//         the zoom box with that percent (core::ui::notice::Zoom): made once, then rewritten in place.
// false with why when it cannot be done.
bool Action(const char *name, char *why, int size);

// dialog_offset_x / _y: where the game's 640 x 480 dialog area starts on the display mode.
void DialogOffset(int *x, int *y);

// The screen, the game's mouse (coordinates, buttons, released flags) and its edge scrolling, for the log.
void Describe(char *out, int size);

} // namespace zeus::smoke
