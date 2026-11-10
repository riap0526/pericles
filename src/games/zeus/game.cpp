// Zeus's info for the core (core/game.h): the names, the settings file, the game's own size and this layer's entry
// points (games/zeus/zeus.h).
#include "core/game.h"
#include "games/zeus/zeus.h"

#ifndef ENGINE_VERSION
#define ENGINE_VERSION "dev" // set by CMakeLists.txt for the DLL; the offline tests build without it
#endif

namespace {

const core_game_info kZeus = {
    "zeus",
    "Pericles",
    ENGINE_VERSION,
    L"Pericles",
    L"zeus_engine.ini",
    "zeus_engine.ini",
    1024, // the largest of the game's three sizes (640x480, 800x600, 1024x768)
    768,
    &zeus_register_settings,
    &zeus_attach,
    &zeus_editor_exe,
};

} // namespace

extern "C" const core_game_info *game_info(void)
{
    return &kZeus;
}
