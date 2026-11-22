// The side panel's rules that do not touch the game's memory (games/zeus/side_panel reads the game and calls these;
// the offline test calls them directly): which name a sidebar button shows as a tooltip, which text group goes with
// it and for how long, and when the cost drawn under a build button has to be cleared.
//
// The names are the ones the game's own name bar shows (sidebar_name_bar_draw 0x517210, Zeus.exe 2.1.4.0): the 17
// sidebar buttons (hover 0x19..0x29: the 11 categories, Build Road, Roadblock, Clear, Undo and the two tabs) the
// sidebar's group 88; the 4 build buttons (0x32..0x35) the building's name in group 28, only while the button can be
// used; the report button (200) group 51's "See more information". The report button's place is now the title's
// (hover 200 too): while its report is open the title has no tooltip (the game's own would show above the screen's
// top edge there; Emperor's title has none then either, only its lit font says it can be clicked).
#pragma once

namespace zeus::side_panel_layout {

// The hover numbers (hover_button 0x781834) of the buttons whose names the name bar shows.
constexpr int kFirstSidebarButton = 0x19, kLastSidebarButton = 0x29;
constexpr int kFirstBuildButton = 0x32, kLastBuildButton = 0x35;
constexpr int kReportButton = 200;

// The text groups and texts.
constexpr int kGroupSidebar = 88, kGroupBuildings = 28, kGroupReport = 51;
constexpr int kReportShow = 79;
// Category 6 is Culture; Poseidon's maps call it Science (text 0x18 of the same group), as the name bar does.
constexpr int kCultureCategory = 6, kScienceText = 0x18;
// Two buildings that Poseidon's maps name differently (their texts in group 28).
constexpr int kBuildingA = 0x49, kBuildingAPoseidon = 0xd7;
constexpr int kBuildingB = 0x88, kBuildingBPoseidon = 0xa4;

// balloon_help_update draws no tooltip while the text number is 0 (game_frame 0x5137ff; balloon_help_end 0x4f6af0
// takes only numbers above 0), and Population is text 0 of group 88. A name with text 0 is therefore handed over as
// this number and swapped for the real one only while the tooltip is drawn. Above every text of the game's groups.
constexpr int kStandIn = 0x7fff;

struct Name {
    int group;
    int text;
};

// What the game says about the button under the mouse (read by games/zeus/side_panel).
struct Facts {
    bool poseidon;     // science_names(0): a Poseidon map (its names for Culture and two buildings)
    bool reportOpen;   // sidebar_dialog_open: the sidebar's report is open
    bool buildEnabled; // the hovered build button's state is 1 (it can be used)
    int building;      // the hovered build button's building type
    bool boxMessages;  // the box at the bottom shows the messages (else the minimap)
};

// The box's toggle (ours, hover number 201): the game's own words for switching to the other one: "View messages"
// (group 68, the mouse help) while the box shows the minimap, "Show Overview Map" (group 88, the old tab's) while it
// shows the messages.
constexpr int kToggleButton = 201;
constexpr int kGroupMouseHelp = 68, kViewMessages = 33, kShowOverview = 16;

// One of the buttons whose name the name bar shows, or the box's toggle.
bool Ours(int hover);
// A build button (0x32..0x35).
bool BuildButton(int hover);
// The building's name text in group 28 (Poseidon's two renamed buildings).
int BuildingText(int building, bool poseidon);
// The name for hover, as the name bar would show it; false when it shows none (not ours, a build button that
// cannot be used, or the title while its report is open).
bool NameFor(int hover, const Facts &f, Name *out);

// The game's state as balloon_help_update 0x4f6110 sees it on the report screen (game_screen 7).
struct ReportFacts {
    bool screenRedraw; // screen_redraw is not 0: the game decides nothing this frame
    int balloonHelp;   // balloon_help: 0 off
    int dialog;        // sidebar_dialog
    int hover;
};
// Whether we answer for the sidebar's buttons ahead of balloon_help_update on the report screen, where the name bar
// showed their names on every report: the game asks balloon_help_button_text there only for two kinds of report,
// and several reports set screen_redraw on every frame they draw (the trade and the gods' reports), so that the
// game never decides a tooltip while they are open. Answered on every report and whether or not screen_redraw is
// set (the tooltip is drawn after the report in the same frame; balloon_help_end then leaves screen_redraw as it
// is); only Balloon help off hides the names.
bool AnswerInReport(const ReportFacts &f);

// The group for the name handed to the game in a frame: given only in the same frame and for the same text, so
// that every other tooltip keeps the game's own group.
class TipMemo {
public:
    // Remembers n for frame; returns the number to put into balloon_help_text (the text, or kStandIn for text 0).
    int Remember(int frame, const Name &n);
    void Forget()
    {
        on_ = false;
    }
    // The group for text in frame, when it is the one remembered.
    bool GroupFor(int frame, int text, int *group) const;
    // The number to put into balloon_help_text while the tooltip is drawn, when value is our stand-in.
    bool RealText(int value, int *text) const;

private:
    bool on_ = false;
    int frame_ = 0;
    Name name_{};
};

// The cost drawn under a build button: the game's name bar set screen_redraw on every frame it drew it (the
// background drawn again cleared it), which also kept balloon_help_update from deciding anything in that frame. Ours
// asks for one redraw when the cost moves to another button or goes away.
class CostTrail {
public:
    // drawn: the hover number whose cost was drawn this frame, 0 for none. True: set screen_redraw now.
    bool Frame(int drawn)
    {
        const bool clear = last_ != 0 && drawn != last_;
        last_ = drawn;
        return clear;
    }
    void Reset()
    {
        last_ = 0;
    }
    int Last() const
    {
        return last_;
    }

private:
    int last_ = 0;
};

// ---- The column moved up ----
//
// The sidebar's right column (from the tabs down to the build buttons) without the tabs: the game draws the column in
// its own place (sidebar_panel_draw 0x517480: the title at y 0x1e, the overlay buttons from y 0x32, the information
// panel and the category's own panel above y 300), and that drawing is moved up by the tabs' 25 rows. Every frame
// starts from a copy of the column as the backdrop left it (taken when the backdrop has just been drawn), so that
// what the game draws only on some frames (its buttons) or never again (the marble under the title) is not moved a
// second time. Pixels the game left as the backdrop had them are not moved: they become the backdrop's pixels at
// the new place, so that the marble stays where it is.

// The column, in sidebar columns (x - sidebar_x) and screen rows (the 1024 layout's).
constexpr int kColumnLeft = 50, kColumnRight = 186, kColumnW = kColumnRight - kColumnLeft;
constexpr int kColumnRows = 424; // down to the build buttons' last row (421) and the first rows of their costs
constexpr int kShift = 25;       // the tabs' rows (0..24)
// The rows moved (the game's rows 25..332: the title's place down to the information panel's new bottom edge) and
// where they go (0..307); the rows below them (308..423) are the backdrop's, and the build buttons are drawn there.
constexpr int kMovedFirst = kShift, kMovedEnd = 333, kMovedRows = kMovedEnd - kMovedFirst;
// The title: centered in 0x82 px from column 50, at y 0x1e in the game's place (5 moved); clicked and hovered over
// its whole row above the overlay buttons.
constexpr int kTitleX = kColumnLeft, kTitleW = 0x82, kTitleY = 0x1e - kShift;
constexpr int kTitleRight = 182, kTitleBottom = kShift;
// The left part of the tabs' rows (columns 0..49 of rows 0..21, above the category buttons): what tells whether the
// backdrop is the one whose army panel's picture (Zeus_Interface entry 1233) has the same rows without the tabs.
constexpr int kTopCheckW = kColumnLeft, kTopCheckRows = 22;
// The information panel: the game's height for n overlay buttons, two tiles (32 rows) longer at the bottom.
struct InfoPanel {
    int top, rows; // rows of 16 px
};
InfoPanel InfoPanelFor(int overlayButtons);
// The font of a font group as the game picks it: the group table's entry g (8 bytes each) while the table has more
// than g entries, else entry 0. Returns the byte offset of the entry.
int FontEntryOffset(int group, int count);
// The title's font groups: the game's (1) and the one while it can be clicked and the mouse is on it (15).
constexpr int kTitleFont = 1, kTitleHoverFont = 15;

// The report for the category under the title: the report button's rule (sidebar_report_button_draw 0x516fa0): a
// report kind, and category 9 only on Poseidon's maps.
constexpr int kCategoryCount = 11, kPoseidonOnlyReport = 9, kOverviewCategory = 10;
bool TitleClickable(int category, int reportKind, bool poseidon);
// A point in sidebar columns and screen rows: on the title.
bool OnTitle(int col, int y);
// Whether the mouse (sidebar column, screen row) is where the game's panels are seen moved: those inputs are given
// the game's row (y + kShift).
bool InMovedArea(int col, int y);

// 16-bit pixels of the column (kColumnW x kColumnRows) and of the tabs' rows of entry 1233 (kColumnW x kShift),
// row by row.
using Px = unsigned short;

// Whether the left part of the tabs' rows on the screen (kTopCheckW x kTopCheckRows, row by row) is entry 1233's
// (entry: its full width w): at most one step of each colour channel apart (the game's 16-bit drawing and ours),
// and nearly every pixel.
bool TopFits(const Px *screenLeft, const Px *entry, int entryW);

// The column on screen (`screen`: its first pixel at the column's left edge and row 0, `pitch` pixels a row):
// rows kMovedFirst..kMovedEnd - 1 moved up by kShift, the pixels that equal the backdrop at the same place replaced
// by the backdrop at the new place (the tabs' rows from `top`, or, without it, the backdrop's next 25 rows), and the
// rows below the moved ones put back from the backdrop.
void MoveColumn(Px *screen, int pitch, const Px *backdrop, const Px *top);
// Whether the game drew anything below the moved rows (rows kMovedEnd..kColumnRows - 1 differ from the backdrop).
bool DrawnBelow(const Px *screen, int pitch, const Px *backdrop);

// The column as the backdrop left it, and where it was taken: put back only on the same screen.
class Backdrop {
public:
    struct Where {
        int sidebarX, width, height, pitch;
        bool operator==(const Where &o) const
        {
            return sidebarX == o.sidebarX && width == o.width && height == o.height && pitch == o.pitch;
        }
    };
    // Copies the column from screen (as MoveColumn's screen).
    void Take(const Px *screen, int pitch, const Where &w);
    bool Has(const Where &w) const
    {
        return has_ && where_ == w;
    }
    void PutBack(Px *screen, int pitch) const;
    void Forget()
    {
        has_ = false;
    }
    const Px *Pixels() const
    {
        return px_;
    }

private:
    bool has_ = false;
    Where where_{};
    Px px_[kColumnW * kColumnRows];
};

// ---- The box at the bottom ----
//
// Below the Go to buttons and the message count (rows 478..511, as the game has them): one box for the minimap or
// the messages (an inset panel of the game's tiles where the message box, the button row and the picture were), the
// check / gem / globe row moved under it, and, on taller screens, an inset panel below 768 (the box and the buttons
// stay where they are on the 1024 x 768 layout). In sidebar columns (x - sidebar_x) and screen rows.
constexpr int kSidebarW = 186, kLayoutH = 768;
constexpr int kLowerTop = 512; // the first row laid out again (the old message box's)
constexpr int kBoxX = 9, kBoxY = 512, kBoxW = 172, kBoxH = 218; // the inset panel, rows 512..729
// The minimap: minimap_draw(sidebar_x + 12, 515, 83 columns of 2 px, 212 rows).
constexpr int kMinimapX = 12, kMinimapY = 515, kMinimapCols = 83, kMinimapRows = 212, kMinimapW = kMinimapCols * 2;
// The toggle: the report button's picture (and the next one under the mouse), the box's top right corner.
constexpr int kToggleX = 160, kToggleY = 515, kToggleSize = 18;
// The messages: the game's text drawing (its width, its lines), one after another from the top, as many as fit.
constexpr int kTextX = 13, kTextW = 148, kTextY = 517, kTextBottom = 726, kMessageGap = 6;
// The game's message box (sidebar_messages 0x7815a0: x sidebar_x + 8, width 0x9b): its arrows at x + width and its
// table's places (y + 2, y + 31), here at rows 535 and 708 below the toggle; clicked within its x and the box's rows.
constexpr int kListX = 8, kListW = 0x9b;
constexpr int kArrowUpY = 535, kArrowDownY = 708, kArrowUpOffset = 2, kArrowDownOffset = 31;
// The check / gem / globe row (rows 563..599 of the backdrop) at rows 731..767; their buttons' y in the game's tables
// moved by the same.
constexpr int kButtonRowTop = 563, kButtonRowEnd = 600, kButtonRowShift = 168;
// The edges carried down: the left one as below 768 elsewhere (its columns 0..10, rows 640..699 repeated), the right
// one's 5 columns (rows 512..561 repeated), and the plain row 562 between them.
constexpr int kEdgeW = 11, kEdgeBandTop = 640, kEdgeBandRows = 60;
constexpr int kRightX = 181, kRightBandTop = 512, kRightBandRows = 50, kPlainRow = 562;
// Below 768: an inset panel from column 11, 172 wide.
constexpr int kTallX = 11, kTallW = 172;

// The rows from kLowerTop down to height laid out from the backdrop on screen (`sidebar`: its first pixel at the
// sidebar's left edge and row 0, `pitch` pixels a row; height at least kLayoutH): the edges and the plain row, and the
// button row at its new place. The inset panels are drawn over it afterwards (the game's tiles).
void LowerHalf(Px *sidebar, int pitch, int height);

// Where a point (sidebar column, screen row) is.
bool OnToggle(int col, int y);
bool OnMinimap(int col, int y);
bool OnList(int col, int y); // the game's message box, as clicked: its columns, the box's rows

// The lines that the game's text_draw_wrapped 0x5090a0 makes of a text in `limit` px (its rules: words of
// 0x5091b0 -- leading spaces 4 px each, '$' a break without width, characters below 0x20 none -- added while the line
// stays narrower than limit; a word that does not fit starts the next line; at most 99 lines). width: the game's
// text_char_width for a character of 0x20 or above.
int WrapLines(const unsigned char *text, int limit, int (*width)(unsigned char c, void *ctx), void *ctx);
// Whether a message of `lines` lines (each 5 + lineHeight rows, as text_draw_wrapped advances) fits from row y.
bool MessageFits(int y, int lines, int lineHeight);

// The messages drawn in the box (their index in the game's list and their rows), for a click on one of them.
class MessageList {
public:
    static constexpr int kMax = 64;
    void Begin()
    {
        n_ = 0;
    }
    void Add(int index, int top, int bottom)
    {
        if (n_ < kMax) {
            s_[n_++] = Shown{index, top, bottom};
        }
    }
    // The message under row y, -1 for none.
    int At(int y) const;
    int Count() const
    {
        return n_;
    }

private:
    struct Shown {
        int index, top, bottom;
    };
    Shown s_[kMax];
    int n_ = 0;
};

} // namespace zeus::side_panel_layout
