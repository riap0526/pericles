// The side panel's rules that do not touch the game's memory (games/zeus/side_panel reads the game and calls these;
// the offline test calls them directly): which name a sidebar button shows as a tooltip, which text group goes with
// it and for how long, and when the cost drawn under a build button has to be cleared.
//
// The names are the ones the game's own name bar shows (sidebar_name_bar_draw 0x517210, Zeus.exe 2.1.4.0): the 17
// sidebar buttons (hover 0x19..0x29: the 11 categories, Build Road, Roadblock, Clear, Undo and the two tabs) the
// sidebar's group 88; the 4 build buttons (0x32..0x35) the building's name in group 28, only while the button can be
// used; the report button (200) group 51's "See more information" or, while a report is open, "Hide full report".
#pragma once

namespace zeus::side_panel_layout {

// The hover numbers (hover_button 0x781834) of the buttons whose names the name bar shows.
constexpr int kFirstSidebarButton = 0x19, kLastSidebarButton = 0x29;
constexpr int kFirstBuildButton = 0x32, kLastBuildButton = 0x35;
constexpr int kReportButton = 200;

// The text groups and texts.
constexpr int kGroupSidebar = 88, kGroupBuildings = 28, kGroupReport = 51;
constexpr int kReportShow = 79, kReportHide = 80;
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
};

// One of the buttons whose name the name bar shows.
bool Ours(int hover);
// A build button (0x32..0x35).
bool BuildButton(int hover);
// The building's name text in group 28 (Poseidon's two renamed buildings).
int BuildingText(int building, bool poseidon);
// The name for hover, as the name bar would show it; false when it shows none (not ours, or a build button that
// cannot be used).
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

} // namespace zeus::side_panel_layout
