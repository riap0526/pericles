// city_picture: the city and minimap pictures in Zeus. F10 saves the whole city the
// player can scroll to as one PNG at the game's own size, without the interface; Shift+F10 the minimap of the whole
// map. The pictures are the core's (core/capture/city_picture, shared with Yongle); this file gives it Zeus's
// variables, the drawing (inside timing's drawing that never counts) and the player's city.
#pragma once

namespace zeus::city_picture {

// From zeus_register_settings: the core's [ui] city_picture_key, minimap_picture_key, minimap_picture_scale and the
// two hidden [debug] ones.
void RegisterSettings();

// After games/zeus/scroll attached (its city_render path calls the pictures): the sites checked, the core's pictures
// installed. False (logged) when a site does not match or smooth scrolling's hooks are not there.
bool Attach(bool scrollAttached);

// For the offline tests (src/games/zeus/test_restore.cpp): installed without the site checks.
void InstallForTest();

} // namespace zeus::city_picture
