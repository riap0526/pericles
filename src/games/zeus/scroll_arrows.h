// Held scroll arrows at the original pace (core/ui/scroll_repeat): the up / down arrows of the game's list
// scroll bars (messages, the city's messages, the file dialog, the family list, adventures and episodes...) scroll one
// line every 50 ms while held, counted from the press, instead of one line per drawn frame. The hook is on the game's
// image buttons function; nothing is saved.
#pragma once

namespace zeus::scroll_arrows {

// From zeus_attach: checks the function's bytes and hooks it; when they do not match nothing is installed (logged)
// and the arrows scroll as the game has them.
void Attach();

} // namespace zeus::scroll_arrows
