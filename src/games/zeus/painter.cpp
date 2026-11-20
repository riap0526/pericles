#include "games/zeus/painter.h"

#include "core/display/tile_layout.h"
#include "core/options/menu.h"
#include "core/log.h"
#include "games/zeus/diag.h"
#include "games/zeus/site.h"

// Evidence (Zeus.exe 2.1.4.0, SHA256 382bcd28...0a57):
// calling conventions checked by disassembly.
namespace zeus::painter {
namespace {

namespace co = core::options;
namespace tl = core::display::tiles;

// image_group_get 0x40fcf0: __thiscall (ecx = a pak's image groups: image_paks 0x12afbe8 + pak x 0xcc44), 1 argument
// (group), ret 4; returns the group's entry (entry 0 when the group is past the pak's count [this + 0xcc38]), whose
// +4 is the group's first image in the pak; image_draw's id adds pak << 14.
const Site kImageGroup{"image_group_get", 0x40fcf0, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x91, 0x38, 0xcc}};
// image_draw 0x5acf50: __thiscall (ecx = the image manager 0x12aebe8), 6 arguments (id, x, y, 0, 0, 0), ret 0x18.
// Checked by resolution (or hooked by diag's draw census, which changes its first bytes): see Attach.
const Site kImageDraw{"image_draw", 0x5acf50, {0x81, 0xec, 0x14, 0x01, 0x00, 0x00, 0x8b, 0x84}};
// text_draw 0x508800: __cdecl (text, x, y, font, 0, 0), the caller pops (0x42504d..0x425069); the first bytes are
// Emperor's text_draw's (its push ecx only keeps a local).
const Site kTextDraw{"text_draw", 0x508800, {0x51, 0x8a, 0x44, 0x24, 0x1c, 0x84, 0xc0, 0x74}};
// text_get_width 0x507da0: __cdecl (text, font), the caller pops.
const Site kTextWidth{"text_get_width", 0x507da0, {0x83, 0xec, 0x08, 0x8b, 0x15, 0x20, 0xc8, 0x2b}};
// screen_clip_reset 0x59fcf0: __cdecl, no arguments; the clip back to the whole screen (Emperor's 0x5bf860, the same
// instructions).
const Site kClipReset{"screen_clip_reset", 0x59fcf0, {0x8b, 0x0d, 0xb4, 0xa6, 0xf2, 0x00, 0x8b, 0x15}};

using ImageGroup_fn = BYTE *(__thiscall *)(void *, int);
using ImageDraw_fn = void(__thiscall *)(void *, int, int, int, int, int, int);
using TextDraw_fn = int(__cdecl *)(const char *text, int x, int y, int font, int, int);
using TextWidth_fn = int(__cdecl *)(const char *text, int font);
using Void_fn = void(__cdecl *)();

const DWORD kImagePaks = 0x12afbe8, kImages = 0x12aebe8;
constexpr DWORD kPakSize = 0xcc44;
const Var<int> kDialogX{0xf2a6ac}; // dialog_offset_x
const Var<int> kDialogY{0xf2a6a8};

// Art: Zeus_Interface is pak 1. The panel as the game's own dialogs draw it (0x59ee90 /
// 0x59f0f0) [confirmed]. The text button bar, 25 high: group 0x0e's first six images, two sets of left, middle, right:
// style 0 raised, style 1 darker and sunk (the pressed or hovered look) [confirmed: images 205-210 of Zeus_Interface,
// build\mockups\zeus\z5-pause\round1\bars.png]; the game's text_button_bar_draw 0x59ed50 draws them. (The Display
// options dialog's buttons are the 20-high set after them, 0x1a + 3 x style, 0x59fba0; the pause menu uses the
// 25-high buttons with the darker one under the mouse.) The hover frame (not used by the pause menu) group 0x7b + 8
// [inferred].
constexpr int kInterfacePak = 1;
constexpr int kPanel = 0x83, kButton = 0x0e, kFrame = 0x7b;
// Fonts: pak 0 groups. The Display options dialog draws its title in group 6 and its buttons' text in group 1 with the
// colour 0x02bf, 0xfac1 under the mouse (0x512b50's colour, text_draw's fifth argument); the confirmation box draws
// its text in group 1 and sizes the box by the title in group 6, drawing the title in group 7 (modal_dialog_draw
// 0x5a04f0) [confirmed: decompilation].
constexpr int kFontPak = 0;
constexpr int kFontNormal = 1, kFontTitle = 6, kFontConfirmTitle = 7, kFontTitleWidth = 6;
constexpr int kColourItem = 0x02bf, kColourItemHover = 0xfac1;

int Group(int pak, int group)
{
    BYTE *entry = Function<ImageGroup_fn>(kImageGroup)(
        reinterpret_cast<void *>(kImagePaks + g_imageShift + pak * kPakSize), group);
    return entry ? *reinterpret_cast<int *>(entry + 4) : 0;
}

int ArtId(int group)
{
    return Group(kInterfacePak, group) + (kInterfacePak << 14);
}

int FontId(int group)
{
    return Group(kFontPak, group); // pak 0: no pak bits (the game passes the entry's +4 as it is: 0x425044)
}

int FontOf(co::Font f)
{
    return FontId(f == co::Font::Title ? kFontTitle : kFontNormal); // Gold, Small, Disabled: with the options window
}

int ColourOf(co::Font f)
{
    return f == co::Font::MenuItem ? kColourItem : f == co::Font::MenuItemHover ? kColourItemHover : 0;
}

void Image(int id, int x, int y)
{
    Function<ImageDraw_fn>(kImageDraw)(reinterpret_cast<void *>(kImages + g_imageShift), id, x, y, 0, 0, 0);
}

class ZeusPainter : public co::Painter {
public:
    int ox = 0, oy = 0;

    void Panel(int x, int y, int w, int h) override
    {
        const int base = ArtId(kPanel);
        tl::WalkNine(tl::kPanel, ox + x, oy + y, w, h, true, [&](int i, int xx, int yy) { Image(base + i, xx, yy); });
    }
    void ButtonBar(int x, int y, int w, int style) override
    {
        const int base = ArtId(kButton) + style * 3;
        tl::WalkPieces(ox + x, oy + y, w, [&](int i, int xx, int yy) { Image(base + i, xx, yy); });
    }
    void Frame(int x, int y, int w, int h) override
    {
        const int base = ArtId(kFrame) + 8; // the darker set, as Emperor's button_frame_draw
        tl::WalkFrame(ox + x, oy + y, w, h, [&](int i, int xx, int yy) { Image(base + i, xx, yy); });
    }
    // y is where the game's own dialogs put their text (text_draw's y: Display options' title at the panel's top +
    // 0x0e, a button's text at its top + 4; the menu's look gives those).
    void Text(const char *t, int x, int y, co::Font f) override
    {
        Function<TextDraw_fn>(kTextDraw)(t, ox + x, oy + y, FontOf(f), ColourOf(f), 0);
    }
    int TextWidth(const char *t, co::Font f) override { return Function<TextWidth_fn>(kTextWidth)(t, FontOf(f)); }

    // Not drawn until the options window: the pause menu does not use them, and their art is not checked
    // yet.
    void Inset(int, int, int, int) override {}
    void Strip(int, int, int) override {}
    void Check(int, int, bool) override {}
    void ArrowButton(int, int, co::Arrow, co::ArrowState) override {}
    void Knob(int, int) override {}
    void Thumb(int, int) override {}
    void Line(int, int, int) override {}
    void TipBox(int, int, int, int) override {}
};

ZeusPainter g_painter;

} // namespace

bool Attach()
{
    // image_draw: checked here, or (draw census on) already checked and hooked by games/zeus/diag.
    if (!Matches(kImageDraw) && !zeus::diag::ImageDrawHooked()) {
        core_log("painter: image_draw at 0x%lx does not match; our windows are not available", kImageDraw.address);
        return false;
    }
    const Site *sites[] = {&kImageGroup, &kTextDraw, &kTextWidth, &kClipReset};
    for (const Site *s : sites) {
        if (!Matches(*s)) {
            core_log("painter: %s at 0x%lx does not match; our windows are not available", s->name, s->address);
            return false;
        }
    }
    return true;
}

co::Painter &Get()
{
    return g_painter;
}

co::MenuLook MenuLook()
{
    co::MenuLook l; // the sizes and spacing of Yongle's menu
    l.labelDown = 8; // a button's text in the middle of its 25-high bar (as an earlier build had it)
    l.barStyle = 0;
    l.hoverBarStyle = 1;
    l.hoverFrame = false;
    l.label = co::Font::MenuItem;
    l.hoverLabel = co::Font::MenuItemHover;
    return l;
}

void BeginDraw()
{
    Function<Void_fn>(kClipReset)();
    g_painter.ox = *kDialogX;
    g_painter.oy = *kDialogY;
}

void DialogOrigin(int *x, int *y)
{
    *x = *kDialogX;
    *y = *kDialogY;
}

int ConfirmTitleWidth(const char *text)
{
    return Function<TextWidth_fn>(kTextWidth)(text, FontId(kFontTitleWidth));
}

int ConfirmTitleDrawWidth(const char *text)
{
    return Function<TextWidth_fn>(kTextWidth)(text, FontId(kFontConfirmTitle));
}

int ConfirmTextWidth(const char *text)
{
    return Function<TextWidth_fn>(kTextWidth)(text, FontId(kFontNormal));
}

} // namespace zeus::painter
