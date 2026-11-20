// core/options' Painter drawn with Zeus's own drawing functions and art (the pause menu now; the options
// window later). Window coordinates (640 x 480) plus an origin (the game's dialog_offset).
//
// Threads: the game thread only.
#pragma once

#include "core/options/menu.h"
#include "core/options/window.h"

namespace zeus::painter {

// From zeus_attach, after resolution (image_draw's bytes are checked there, or hooked by diag's draw census). False:
// a site does not match (logged); nothing of ours that draws with it is available.
bool Attach();

core::options::Painter &Get();

// The pause menu's look: Yongle's sizes and spacing, the button under the mouse drawn with the
// game's darker bar (style 1) and another text colour instead of a frame (as Yongle's); the title in the Display
// options dialog's title font.
core::options::MenuLook MenuLook();

// Before drawing: the game's clip reset to the screen, the origin at the game's dialog_offset.
void BeginDraw();

// The game's dialog_offset (where its 640 x 480 dialogs start on the screen).
void DialogOrigin(int *x, int *y);

// Text widths in the fonts of the game's confirmation box (modal_dialog_draw 0x5a04f0 sizes its box by them).
int ConfirmTitleWidth(const char *text);
int ConfirmTitleDrawWidth(const char *text); // the font the title is drawn in (group 7)
int ConfirmTextWidth(const char *text);

} // namespace zeus::painter
