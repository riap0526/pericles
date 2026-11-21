// Pericles's own texts for players in the options window (a text check scans every string literal here).
#include "games/zeus/options_text.h"

#include "core/options/option_text.h"
#include "core/ui/hotkey.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace zeus::options_text {
namespace {

using core::options::option_text::ChoiceText;
using core::options::option_text::Text;

#define CHOICES(name, ...) const ChoiceText name[] = {__VA_ARGS__}
#define N(a) static_cast<int>(sizeof(a) / sizeof((a)[0]))

CHOICES(kIntro,
        {"play", "The game's intro movie plays when the game starts."},
        {"skip", "The game starts at the main menu."});

CHOICES(kMenus,
        {"city", "In a fret border, with your city blurred around it. A picture of your own named "
                 "menu_background.png or .bmp in Pericles's folder or the game folder is used instead of the city."},
        {"panel", "In a fret border on the game's own panel pattern."},
        {"framed", "In a fret border, black around it."},
        {"original", "The game's own picture, black around it, as the game shows it on a wide screen."});

CHOICES(kWorldMap,
        {"scaled", "Larger than its own size (see the scale below), in a fret border, over the background below."},
        {"native", "At its own size, centered, over the background below."});

CHOICES(kWorldMapBackground,
        {"map", "The same map, blurred and darkened.", "Blurred map"},
        {"city", "Your city as it was when you opened the map, standing still, as if the map were a window over it."});

const Text kTexts[] = {
    {"ui", "intro_movie", "Whether the game plays its intro movie when it starts.", kIntro, N(kIntro), true},
    {"video", "menu_screens", "What is around the main menu, the choice of leader and adventure, briefings, high "
                                "scores, victory and defeat.",
     kMenus, N(kMenus)},
    {"video", "world_map", "How the world map is shown on a screen larger than the map.", kWorldMap, N(kWorldMap)},
    {"video", "world_map_scale", "How large the map is, in percent of its own size. Auto: as large as fits.", nullptr,
     0},
    {"video", "world_map_background", "What is around the map.", kWorldMapBackground, N(kWorldMapBackground)},
    {"audio", "nature_sounds", "The original game calls them 3D sounds. Played the way the game designed them, "
                               "recreated in stereo: now and then in the city, one comes down from above, moves around "
                               "you for a while and rises away again, at the ambient sounds volume.",
     nullptr, 0},
};

#undef CHOICES
#undef N

} // namespace

const char *const kOptionsTitle = "Pericles options";

void Register()
{
    core::options::option_text::SetGameTexts(kTexts, static_cast<int>(sizeof(kTexts) / sizeof(kTexts[0])));
}

// Zeus's own keys (game_wndproc 0x5c4a90, cheat_key 0x42c700, the R poll); our fixed keys (F5, Alt+Enter, Shift+F7)
// are answered by the core (core/options/hotkeys_page) first. Not said: F2, F5 and F6 are also polled, without a visible
// effect; Alt+D, Alt+Z and Alt+F11 are development keys.
const char *GameKeyNote(unsigned vk, unsigned mods)
{
    using core::ui::kModAlt;
    using core::ui::kModCtrl;
    const char *const uses = "The game also uses this key; it no longer reaches the game.";
    if (mods == kModAlt && vk >= '1' && vk <= '5') { // recalling army groups 1-5
        return uses;
    }
    if (mods == (kModCtrl | kModAlt) && vk == 'C') { // the game's cheat box
        return uses;
    }
    if ((vk == VK_INSERT || vk == VK_DELETE) && mods == 0) {
        // Only for typing in the game (insert / overwrite, deleting): hardly ever used, so nothing to tell.
        return nullptr;
    }
    if (vk == 'R') { // polled by the game while placing a building, whatever we do with the key press
        return "The game also reacts to this key itself.";
    }
    if ((mods & (kModCtrl | kModAlt)) == 0) {
        const bool used = vk == 'P' || vk == 'M' || (vk >= '0' && vk <= '9') || (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) ||
                          vk == VK_OEM_4 || vk == VK_OEM_6 || vk == VK_OEM_PLUS || vk == VK_ADD ||
                          vk == VK_OEM_MINUS || vk == VK_SUBTRACT || (vk >= VK_F1 && vk <= VK_F8) || vk == VK_SPACE ||
                          vk == VK_TAB || vk == VK_HOME || vk == VK_END || vk == VK_PRIOR || vk == VK_NEXT;
        if (used) {
            return uses;
        }
    }
    return nullptr;
}

} // namespace zeus::options_text
