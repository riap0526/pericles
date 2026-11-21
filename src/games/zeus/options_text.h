// Pericles's own texts for players in the options window: its game's files and folders, the world map, the nature
// sounds as Zeus has them and the debug panel's note on Zeus's own keys. The ones both games
// share are in core/options/option_text and core/options/menu_text.
#pragma once

namespace zeus::options_text {

// Gives the core Pericles's own table (before the options window is built).
void Register();

extern const char *const kOptionsTitle;

// The debug panel's note for a key Zeus itself uses, chosen for one of our key settings (core::ui::SetGameKeyNote;
// registered by games/zeus/keys); nullptr when the game does not use it.
const char *GameKeyNote(unsigned vk, unsigned mods);

} // namespace zeus::options_text
