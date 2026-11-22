#include "games/zeus/side_panel_layout.h"

namespace zeus::side_panel_layout {

bool BuildButton(int hover)
{
    return hover >= kFirstBuildButton && hover <= kLastBuildButton;
}

bool Ours(int hover)
{
    return (hover >= kFirstSidebarButton && hover <= kLastSidebarButton) || BuildButton(hover) ||
           hover == kReportButton;
}

int BuildingText(int building, bool poseidon)
{
    if (poseidon) {
        if (building == kBuildingA) {
            return kBuildingAPoseidon;
        }
        if (building == kBuildingB) {
            return kBuildingBPoseidon;
        }
    }
    return building;
}

bool NameFor(int hover, const Facts &f, Name *out)
{
    if (hover == kReportButton) {
        *out = Name{kGroupReport, f.reportOpen ? kReportHide : kReportShow};
        return true;
    }
    if (hover >= kFirstSidebarButton && hover <= kLastSidebarButton) {
        int text = hover - kFirstSidebarButton;
        if (text == kCultureCategory && f.poseidon) {
            text = kScienceText;
        }
        *out = Name{kGroupSidebar, text};
        return true;
    }
    if (BuildButton(hover) && f.buildEnabled) {
        *out = Name{kGroupBuildings, BuildingText(f.building, f.poseidon)};
        return true;
    }
    return false;
}

bool AnswerInReport(const ReportFacts &f)
{
    return f.balloonHelp != 0 && Ours(f.hover);
}

int TipMemo::Remember(int frame, const Name &n)
{
    on_ = true;
    frame_ = frame;
    name_ = n;
    return n.text != 0 ? n.text : kStandIn;
}

bool TipMemo::GroupFor(int frame, int text, int *group) const
{
    if (!on_ || frame != frame_ || text != name_.text) {
        return false;
    }
    *group = name_.group;
    return true;
}

bool TipMemo::RealText(int value, int *text) const
{
    if (!on_ || value != kStandIn || name_.text != 0) {
        return false;
    }
    *text = name_.text;
    return true;
}

} // namespace zeus::side_panel_layout
