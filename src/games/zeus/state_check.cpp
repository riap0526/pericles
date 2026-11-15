// [debug] state_check (state_check.h).
#include "games/zeus/state_check.h"

#include "core/diag/guard.h"
#include "core/log.h"
#include "core/settings/settings.h"
#include "games/zeus/site.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdio>
#include <cstring>

namespace {

using zeus::At;

// The game's own save descriptor table (save_game_table): only to name
// the regions (which block of a saved game they are in).
constexpr DWORD kTable = 0x63c2d0;
constexpr int kDescriptors = 0x8b; // save_load 0x50d6e0 reads this many
struct Descriptor {
    DWORD flag, address, size, count, conversion;
};
static_assert(sizeof(Descriptor) == 20, "the game's descriptor");

constexpr DWORD kEveryMs = 1000;
constexpr int kStatsEvery = 60;    // a one-line summary every 60 checks (about a minute)
constexpr int kTableEvery = 600;   // the whole table every 600 checks (about ten minutes) and at the end
constexpr DWORD kMergeGap = 256;   // outside the save blocks, differences closer than this are one region
constexpr int kMaxRegions = 1024;
constexpr DWORD kPage = 4096;

bool g_on = false; // [debug] state_check
core::diag::Feature g_feature{"drawing check (games/zeus/state_check)"};
DWORD g_low = 0, g_size = 0;       // the game's addresses compared (the exe's .data)
unsigned char *g_copy = nullptr;
DWORD g_last = 0;
bool g_pending = false;            // Before ran; AfterDraw / AfterPutBack compare
const char *g_label = nullptr;     // the picture whose drawing is checked (BeginPicture; nullptr: a drawing that
                                   // does not count)
int g_drawRuns = 0;                // AfterDraw's differences (the picture's log line)
long g_checks = 0, g_checksChanged = 0, g_checksLeft = 0;
long g_untracked = 0;              // differences found once the region table was full

struct Region {
    DWORD low, high;               // the game's addresses [low, high): a whole save block, or merged differences
    int block;                     // the save block (-1: not in the save)
    DWORD first;                   // the first address found changed
    DWORD leftLow, leftHigh;       // the lowest and highest address still changed after our put-back (0: none yet)
    long changed, left;            // checks in which the drawing changed it / it was still changed after the put-back
    long markChanged, markLeft;    // the check that counted it last (each check counts a region once)
    bool logged, loggedLeft;
};
Region g_regions[kMaxRegions];
int g_regionCount = 0;

// The save block an address is in (-1: none), and its range.
int SaveBlock(DWORD address, DWORD *low, DWORD *high)
{
    const Descriptor *d = At<const Descriptor>(kTable);
    for (int i = 0; i < kDescriptors; ++i) {
        const DWORD bytes = d[i].size * d[i].count;
        if (d[i].size && d[i].count && bytes <= 0x2000000 && address >= d[i].address && address - d[i].address < bytes) {
            *low = d[i].address;
            *high = d[i].address + bytes;
            return i;
        }
    }
    return -1;
}

// Where an address of a region is: the record and field of its save block, or the address itself.
void Place(const Region &r, DWORD address, char *out, std::size_t size)
{
    const Descriptor *d = At<const Descriptor>(kTable);
    if (r.block >= 0 && d[r.block].size) {
        const DWORD at = address - d[r.block].address;
        std::snprintf(out, size, "0x%lx (record %lu +0x%lx)", static_cast<unsigned long>(address),
                      static_cast<unsigned long>(at / d[r.block].size), static_cast<unsigned long>(at % d[r.block].size));
    }
    else {
        std::snprintf(out, size, "0x%lx", static_cast<unsigned long>(address));
    }
}

// "; still changed from A to B" (the leftover's first and last byte), or "".
void Leftover(const Region &r, char *out, std::size_t size)
{
    out[0] = 0;
    if (r.leftHigh == 0) {
        return;
    }
    char a[64], b[64];
    Place(r, r.leftLow, a, sizeof(a));
    Place(r, r.leftHigh - 1, b, sizeof(b));
    std::snprintf(out, size, "; still changed from %s to %s", a, b);
}

void Describe(const Region &r, char *out, std::size_t size)
{
    const Descriptor *d = At<const Descriptor>(kTable);
    if (r.block >= 0 && d[r.block].size) {
        const DWORD at = r.first - d[r.block].address;
        std::snprintf(out, size, "save block %d, %lu x %lu, first in record %lu at +0x%lx", r.block,
                      static_cast<unsigned long>(d[r.block].size), static_cast<unsigned long>(d[r.block].count),
                      static_cast<unsigned long>(at / d[r.block].size), static_cast<unsigned long>(at % d[r.block].size));
    }
    else {
        std::snprintf(out, size, "not in the save, first at 0x%lx", static_cast<unsigned long>(r.first));
    }
}

Region *Find(DWORD low, DWORD high)
{
    DWORD blockLow = 0, blockHigh = 0;
    const int block = SaveBlock(low, &blockLow, &blockHigh);
    if (block >= 0) { // a save block is one region
        for (int i = 0; i < g_regionCount; ++i) {
            if (g_regions[i].block == block) {
                return &g_regions[i];
            }
        }
        if (g_regionCount == kMaxRegions) {
            ++g_untracked;
            return nullptr;
        }
        g_regions[g_regionCount] = Region{blockLow, blockHigh, block, low, 0, 0, 0, 0, 0, 0, false, false};
        return &g_regions[g_regionCount++];
    }

    for (int i = 0; i < g_regionCount; ++i) {
        Region &r = g_regions[i];
        if (r.block < 0 && low <= r.high + kMergeGap && high + kMergeGap >= r.low) {
            r.low = low < r.low ? low : r.low;
            r.high = high > r.high ? high : r.high;
            return &r;
        }
    }
    if (g_regionCount == kMaxRegions) {
        ++g_untracked;
        return nullptr;
    }
    g_regions[g_regionCount] = Region{low, high, -1, low, 0, 0, 0, 0, 0, 0, false, false};
    return &g_regions[g_regionCount++];
}

// One difference [low, high) found at stage 0 (after the drawing) or 1 (after the put-back).
void Note(DWORD low, DWORD high, int stage)
{
    Region *r = Find(low, high);
    if (!r) {
        return;
    }
    const long check = g_checks + 1;
    if (stage == 0 && r->markChanged != check) {
        r->markChanged = check;
        ++r->changed;
    }
    if (stage == 1) {
        if (r->markLeft != check) {
            r->markLeft = check;
            ++r->left;
        }
        r->leftLow = r->leftHigh == 0 || low < r->leftLow ? low : r->leftLow;
        r->leftHigh = high > r->leftHigh ? high : r->leftHigh;
    }
}

// The differences between the copy and the game's memory now, as runs merged across small gaps. Pages first, so
// unchanged memory costs one memcmp per page.
int Compare(int stage)
{
    int runs = 0;
    const unsigned char *now = At<const unsigned char>(g_low);
    DWORD runLow = 0, runHigh = 0;
    bool open = false;
    for (DWORD page = 0; page < g_size; page += kPage) {
        const DWORD n = g_size - page < kPage ? g_size - page : kPage;
        if (std::memcmp(now + page, g_copy + page, n) == 0) {
            continue;
        }
        for (DWORD i = page; i < page + n; ++i) {
            if (now[i] == g_copy[i]) {
                continue;
            }
            const DWORD a = g_low + i;
            if (open && a <= runHigh + kMergeGap) {
                runHigh = a + 1;
                continue;
            }
            if (open) {
                Note(runLow, runHigh, stage);
                ++runs;
            }
            runLow = a;
            runHigh = a + 1;
            open = true;
        }
    }
    if (open) {
        Note(runLow, runHigh, stage);
        ++runs;
    }
    return runs;
}

void BeforeBody(void *)
{
    std::memcpy(g_copy, At<const void>(g_low), g_size);
}

void AfterDrawBody(void *p)
{
    *static_cast<int *>(p) = Compare(0);
}

void AfterPutBackBody(void *p)
{
    *static_cast<int *>(p) = Compare(1);
}

void LogNew()
{
    for (int i = 0; i < g_regionCount; ++i) {
        Region &r = g_regions[i];
        char where[96], left[160];
        Leftover(r, left, sizeof(left));
        if (!r.logged && r.changed) {
            r.logged = true;
            Describe(r, where, sizeof(where));
            core_log("zeus: state check: new region 0x%lx (+%lu B, %s) changed by %s%s%s",
                     static_cast<unsigned long>(r.low), static_cast<unsigned long>(r.high - r.low), where,
                     g_label ? g_label : "a drawing that does not count",
                     r.left ? "; still changed after our put-back" : "; undone by our put-back", left);
            r.loggedLeft = r.left != 0;
        }
        else if (r.logged && !r.loggedLeft && r.left) {
            r.loggedLeft = true;
            Describe(r, where, sizeof(where));
            core_log("zeus: state check: region 0x%lx (+%lu B, %s) now also still changed after our put-back%s%s%s%s",
                     static_cast<unsigned long>(r.low), static_cast<unsigned long>(r.high - r.low), where, left,
                     g_label ? " (by " : "", g_label ? g_label : "", g_label ? ")" : "");
        }
    }
}

void Summary()
{
    int left = 0;
    for (int i = 0; i < g_regionCount; ++i) {
        left += g_regions[i].left ? 1 : 0;
    }
    core_log("zeus: state check: %ld drawing(s) checked: %ld changed the game's memory, %ld left something changed after "
             "our put-back; %d region(s), %d of them left at least once%s",
             g_checks, g_checksChanged, g_checksLeft, g_regionCount, left,
             g_untracked ? " (the region table is full: some differences are not counted)" : "");
}

bool Allocate(DWORD low, DWORD size)
{
    g_low = low;
    g_size = size;
    g_copy = static_cast<unsigned char *>(VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    return g_copy != nullptr;
}

// The exe's .data section (the game's globals, initialized and not), from the loaded image's section table.
bool DataSection(DWORD *low, DWORD *size)
{
    const BYTE *base = reinterpret_cast<const BYTE *>(GetModuleHandleW(nullptr));
    const auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(base);
    if (!base || dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return false;
    }
    const auto *nt = reinterpret_cast<const IMAGE_NT_HEADERS *>(base + dos->e_lfanew);
    const IMAGE_SECTION_HEADER *s = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        if (std::memcmp(s[i].Name, ".data", 6) == 0) {
            *low = reinterpret_cast<DWORD>(base) + s[i].VirtualAddress;
            *size = s[i].Misc.VirtualSize;
            return true;
        }
    }
    return false;
}

} // namespace

namespace zeus::state_check {

void RegisterSettings()
{
    using core::settings::Effect;
    using core::settings::Item;
    using core::settings::Type;
    const int i = core::settings::Register(Item{
        "debug", "state_check", "Drawing check",
        "For development: the city is drawn more often than the game was made for, and the extra drawings must not "
        "change the game. on = about once a second the log checks what one of them changed in the game's memory and "
        "whether all of it was put back. Slows the game a little. Read at start-up.",
        Type::Bool, "off", Effect::Restart, true, true, 0, 0, 0, nullptr, nullptr, nullptr, 0});
    core::settings::OnChange(i, [](int, const char *v, bool) {
        g_on = std::strcmp(v, "on") == 0;
        return true;
    });
}

void Attach()
{
    if (!g_on) {
        return;
    }
    DWORD low = 0, size = 0;
    if (!DataSection(&low, &size) || !Allocate(low, size)) {
        core::diag::Disable(g_feature, "no .data section or no memory for the copy");
        return;
    }
    core_log("zeus: state check on: the exe's .data (0x%lx, %lu bytes) is compared around one drawing that does not "
             "count every %lu ms: after the drawing and after our put-back", static_cast<unsigned long>(low),
             static_cast<unsigned long>(size), static_cast<unsigned long>(kEveryMs));
}

bool Wanted()
{
    if (!g_copy || !core::diag::Enabled(g_feature)) {
        return false;
    }
    const DWORD now = GetTickCount();
    if (g_last != 0 && now - g_last < kEveryMs) {
        return false;
    }
    g_last = now;
    return true;
}

void Before()
{
    g_pending = core::diag::Guarded(g_feature, &BeforeBody, nullptr);
}

void AfterDraw()
{
    if (!g_pending) {
        return;
    }
    int runs = 0;
    if (!core::diag::Guarded(g_feature, &AfterDrawBody, &runs)) {
        g_pending = false;
        return;
    }
    g_drawRuns = runs;
    if (runs) {
        ++g_checksChanged;
    }
}

void AfterPutBack()
{
    if (!g_pending) {
        return;
    }
    g_pending = false;
    int runs = 0;
    if (!core::diag::Guarded(g_feature, &AfterPutBackBody, &runs)) {
        return;
    }
    if (runs) {
        ++g_checksLeft;
    }
    ++g_checks;
    LogNew();
    if (g_label) {
        core_log("zeus: state check (%s): %d place(s) changed by the drawing, %d still changed after our put-back",
                 g_label, g_drawRuns, runs);
        g_label = nullptr;
    }
    if (g_checks == 1 || g_checks % kStatsEvery == 0) {
        Summary();
    }
    if (g_checks % kTableEvery == 0) {
        Report("");
    }
}

void Cancel()
{
    g_pending = false;
    g_label = nullptr;
}

bool BeginPicture(const char *label)
{
    if (!g_copy || !core::diag::Enabled(g_feature)) {
        return false;
    }
    if (g_pending) { // not expected: the pictures are drawn only in counting frames, whose drawings are not checked
        core_log("zeus: state check: a check was under way when the %s was drawn; that check is not counted", label);
        Cancel();
    }
    g_label = label;
    g_drawRuns = 0;
    Before();
    if (!g_pending) {
        g_label = nullptr;
    }
    return g_pending;
}

void Report(const char *why)
{
    if (!g_copy || g_checks == 0) {
        return;
    }
    Summary();
    core_log("zeus: state check regions%s (address, size, where; checks changed by the drawing / still changed after "
             "our put-back):", why);
    for (int i = 0; i < g_regionCount; ++i) {
        const Region &r = g_regions[i];
        char where[96], left[160];
        Describe(r, where, sizeof(where));
        Leftover(r, left, sizeof(left));
        core_log("zeus: state check:   0x%lx +%lu B (%s): %ld / %ld%s", static_cast<unsigned long>(r.low),
                 static_cast<unsigned long>(r.high - r.low), where, r.changed, r.left, left);
    }
}

bool EnableForTest(unsigned long low, unsigned long size)
{
    g_on = true;
    return Allocate(static_cast<DWORD>(low), static_cast<DWORD>(size));
}

bool Working()
{
    return core::diag::Enabled(g_feature);
}

} // namespace zeus::state_check
