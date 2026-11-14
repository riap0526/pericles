// Our messages in the game's own notice boxes (the boxes at the top of the city, with their look, place and 15 s on
// screen): Zeus's boxes for core/ui/notice, which keeps the queue (Post and Zoom, any thread). Shown on the game
// thread from the game's notice drawing (city_warnings_draw 0x522ec0, hooked). Zeus has one notice font, so a red
// text is shown in it as the others.
#pragma once

namespace zeus::notice {

// After the game's hooks: hooks city_warnings_draw and installs core/ui/notice with Zeus's boxes. False (logged) when
// a site does not match or the hook cannot be installed; core::ui::notice::Post then shows the text on our own
// caption line (core/output SetNotice).
bool Attach();

// For the offline test: installs core/ui/notice with Zeus's boxes without the hook (core::ui::notice::OnFrame then
// stands for the detour's first half).
void InstallForTest();

} // namespace zeus::notice
