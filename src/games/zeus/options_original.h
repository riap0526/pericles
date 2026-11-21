// The game's own options in the options window (core/options/game_options): game and scroll speed, autosave, the
// message kinds, and the four sound switches and volumes. Here: where Zeus keeps them (the Zeus.inf block, written
// back by the game at exit) and how its own options dialogs change them; and a new player's starting speeds, given
// once Zeus.inf has been read.
#pragma once

namespace zeus::options_original {

// Start-up (zeus_attach, after resolution and keys::Attach, whose lang_get_string hook gives the game's texts):
// hooks the new player's check, checks the game's functions and gives them to the core's rows. false: they are not as
// expected (the game's own options are left out of the window).
bool Attach();

// Game thread: the music, speech and ambient volumes handed to the sound manager again, as the game does it (the
// master volume changed).
void ReapplyVolumes();

// Game thread, once Zeus.inf has been read (config_load returned; resolution's detour calls it when resolution hooks
// that function): a new player gets our starting speeds.
void AfterInfRead();

// For the offline test (src/games/zeus/test_restore.cpp): the rows given to the core without checking the game's
// functions (the test's fake game is at another address).
void AttachForTest();

// Game thread: the message kinds as the game has them now (its count and each kind's name and value) written to the
// log, once per call. The count is read where the game's Messages dialog reads it (message_kinds_count).
void LogMessageKinds();

} // namespace zeus::options_original
