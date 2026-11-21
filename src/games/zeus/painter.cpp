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
// The tooltip's box, as the game's own tooltips (0x51c99d / 0x51c9c6): rect_draw 0x509870 (x, y, w, h, colour) for the
// edge, fill_rect 0x509b20 (the same arguments) inside it; both __cdecl, the caller pops. fill_rect is hooked by
// resolution (checked there; its detour passes our calls on unchanged), so its first bytes are not checked here.
const Site kRectDraw{"rect_draw", 0x509870, {0x53, 0x55, 0x56, 0x57, 0x8b, 0x7c, 0x24, 0x14}};
const Site kFillRect{"fill_rect", 0x509b20, {}};
// dd_pixel_format_index 0xf9c0e4: 2 when the surfaces are RGB565 (the tooltip picks its colours by it).
const Var<int> kPixelFormat{0xf9c0e4};

using ImageGroup_fn = BYTE *(__thiscall *)(void *, int);
using ImageDraw_fn = void(__thiscall *)(void *, int, int, int, int, int, int);
using TextDraw_fn = int(__cdecl *)(const char *text, int x, int y, int font, int, int);
using TextWidth_fn = int(__cdecl *)(const char *text, int font);
using Void_fn = void(__cdecl *)();
using Rect_fn = void(__cdecl *)(int x, int y, int w, int h, int colour);

const DWORD kImagePaks = 0x12afbe8, kImages = 0x12aebe8;
constexpr DWORD kPakSize = 0xcc44;
const Var<int> kDialogX{0xf2a6ac}; // dialog_offset_x
const Var<int> kDialogY{0xf2a6a8};

// Art: Zeus_Interface is pak 1. The panel as the game's own dialogs draw it (0x59ee90 /
// 0x59f0f0) [confirmed]. The text button bar, 25 high: group 0x0e's first six images, two sets of left, middle, right:
// style 0 raised, style 1 darker and sunk (the pressed or hovered look) [confirmed: images 205-210 of Zeus_Interface
// drawn out and viewed]; the game's text_button_bar_draw 0x59ed50 draws them. (The Display
// options dialog's buttons are the 20-high set after them, 0x1a + 3 x style, 0x59fba0; the pause menu uses the
// 25-high buttons with the darker one under the mouse.) The hover frame (not used by the pause menu) group 0x7b + 8
// [inferred].
// The options window's art (Zeus_Interface groups, each image checked by eye): the inset 0x84 (inner_panel_draw
// 0x59f2a0); the strip 20 high: group 0x0e + 0x1a (left, middle, right); the checkbox pairs of group 0x14, the lighter
// pair + 2 ticked / + 3 empty; the round arrows up 0x99, down 0x81, left 0x73, right 0x72, + 0 normal, + 1 under the
// mouse, + 2 pressed; the scroll thumb 0x0d (26 x 26, the game's own scroll bars' diamond), also the slider's knob;
// the thin line 0x1b (left, middle, right, 16 x 3).
constexpr int kInterfacePak = 1;
constexpr int kPanel = 0x83, kButton = 0x0e, kFrame = 0x7b, kInset = 0x84, kStripFirst = 0x1a, kCheck = 0x14;
constexpr int kArrowUp = 0x99, kArrowDown = 0x81, kArrowLeft = 0x73, kArrowRight = 0x72, kThumb = 0x0d, kLine = 0x1b;
constexpr int kThumbSize = 26;
// Fonts: pak 0 groups. Only group 0 (a 9 px mask) takes text_draw's colour; every other group carries its own colours
// (font_mask_draw 0x416c30 fills type 20 images only), so a colour of ours would do nothing. The groups are made for a
// light ground (the panel, the buttons, the strips) or a dark one (the inset, the tooltip): group 1 dark blue on light
// (the game's dialogs and menus), 3 light on the inset, 9 yellow and 4 gold for emphasis on light and dark, 6 the
// dialog title, 13 the tooltip's small text (the game's tooltips). The confirmation box draws its text in group 1 and
// sizes the box by the title in group 6, drawing the title in group 7 (modal_dialog_draw 0x5a04f0) [confirmed:
// decompilation].
constexpr int kFontPak = 0;
constexpr int kFontNormal = 1, kFontTitle = 6, kFontConfirmTitle = 7, kFontTitleWidth = 6;
constexpr int kFontLight = 3, kFontYellow = 9, kFontGold = 4, kFontSmall = 13;
// text_draw puts the top of a letter's image 3 pixels above its y (text_draw_glyph 0x508b50: y - 3); the options
// window's y is the top of the letters.
constexpr int kTextDown = 3;

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
    switch (f) {
    case co::Font::Title: return FontId(kFontTitle);
    case co::Font::Gold: return FontId(kFontYellow);
    case co::Font::Label: return FontId(kFontLight);
    case co::Font::Accent:
    case co::Font::Disabled: return FontId(kFontGold); // no red font the size of the others
    case co::Font::Small: return FontId(kFontSmall);
    default: return FontId(kFontNormal); // Normal, Value, MenuItem, MenuItemHover
    }
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
    // y is the top of the letters (text_draw draws 3 pixels above its y).
    void Text(const char *t, int x, int y, co::Font f) override
    {
        Function<TextDraw_fn>(kTextDraw)(t, ox + x, oy + y + kTextDown, FontOf(f), 0, 0);
    }
    int TextWidth(const char *t, co::Font f) override { return Function<TextWidth_fn>(kTextWidth)(t, FontOf(f)); }
    void Inset(int x, int y, int w, int h) override
    {
        const int base = ArtId(kInset);
        tl::WalkNine(tl::kInset, ox + x, oy + y, w, h, true, [&](int i, int xx, int yy) { Image(base + i, xx, yy); });
    }
    void Strip(int x, int y, int w) override
    {
        const int base = ArtId(kButton) + kStripFirst;
        tl::WalkPieces(ox + x, oy + y, w, [&](int i, int xx, int yy) { Image(base + i, xx, yy); });
    }
    void Check(int x, int y, bool on) override { Image(ArtId(kCheck) + (on ? 2 : 3), ox + x, oy + y); }
    void ArrowButton(int x, int y, co::Arrow a, co::ArrowState s) override
    {
        const int g = a == co::Arrow::Left ? kArrowLeft : a == co::Arrow::Right ? kArrowRight
                      : a == co::Arrow::Up ? kArrowUp : kArrowDown;
        Image(ArtId(g) + static_cast<int>(s), ox + x, oy + y);
    }
    // The knob's place is kKnobSize square; the diamond is larger, centred on it (over the strip's edges).
    void Knob(int x, int y) override
    {
        const int d = (kThumbSize - co::kKnobSize) / 2;
        Image(ArtId(kThumb), ox + x - d, oy + y - d);
    }
    void Thumb(int x, int y) override { Image(ArtId(kThumb), ox + x, oy + y); }
    void Line(int x, int y, int w) override
    {
        const int base = ArtId(kLine);
        tl::WalkLine(ox + x, oy + y, w, [&](int i, int xx, int yy) { Image(base + i, xx, yy); });
    }
    void TipBox(int x, int y, int w, int h) override // the game's tooltip colours (0x51c8cd..0x51c8de)
    {
        const bool rgb565 = *kPixelFormat == 2;
        Function<Rect_fn>(kRectDraw)(ox + x, oy + y, w, h, rgb565 ? 0x104 : 0x84);
        Function<Rect_fn>(kFillRect)(ox + x + 1, oy + y + 1, w - 2, h - 2, rgb565 ? 0x1372 : 0x9b2);
    }
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
    const Site *sites[] = {&kImageGroup, &kTextDraw, &kTextWidth, &kClipReset, &kRectDraw};
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
    l.titleY = 11;   // the title and the buttons' text where they were before Text drew letters' tops at y
    l.labelDown = 5; // a button's text in the middle of its 25-high bar
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
