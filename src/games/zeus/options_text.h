// Pericles's own texts for players in the options window: its game's files and folders, the world map, the nature
// sounds as Zeus has them, and the descriptions that say something else in Zeus for now. The ones both games share are
// in core/options/option_text and core/options/menu_text.
#pragma once

namespace zeus::options_text {

// Gives the core Pericles's own table (before the options window is built).
void Register();

extern const char *const kOptionsTitle;

} // namespace zeus::options_text
