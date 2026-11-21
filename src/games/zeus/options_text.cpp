// Pericles's own texts for players in the options window (a text check scans every string literal here).
#include "games/zeus/options_text.h"

#include "core/options/option_text.h"

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

CHOICES(kMinimapScale, {"1", "", "1x"}, {"2", "", "2x"}, {"4", "", "4x"});

const Text kTexts[] = {
    {"ui", "intro_movie", "Whether the game plays its intro movie when it starts.", kIntro, N(kIntro), true},
    // No Hotkeys page in Pericles yet: the key is set in the settings file.
    {"ui", "minimap_picture_scale", "How large the minimap screenshot saved with its key is, in times the minimap in "
                                    "the game. It does not change the minimap beside the city.",
     kMinimapScale, N(kMinimapScale), false, "Minimap screenshot: size"},
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

} // namespace zeus::options_text
