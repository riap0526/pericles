// [debug] state_check: what a city drawing that does not count changes in the game's memory, and whether our put-back
// undoes it. About once a second, around one such drawing (games/zeus/timing.cpp), the
// whole of the exe's .data (initialized and uninitialized; the game's globals, about 16 MB) is copied before anything
// of ours runs, compared once after the drawing and once after our put-back. Every place that differs becomes a
// region (nearby differences merged); per region the log keeps two counts: in how many checks the drawing changed
// it, and in how many it was still changed after our put-back. A region inside a block of the game's save table
// (0x63c2d0; save_load 0x50d6e0 reads 139 of them) is named by that block. Only the log; nothing in the game
// changes.
// Not covered: memory the game allocates (the heap), and code paths a session does not run -- this samples what
// happened, it does not prove that nothing else can happen.
#pragma once

#include <cstddef>

namespace zeus::state_check {

void RegisterSettings(); // [debug] state_check (read at start-up)
void Attach();           // after the exe check: the copy buffer when the setting is on

// From timing.cpp around a drawing that does not count (game thread). Wanted: on, still working and a check is due
// (once a second). Before copies the memory (before our own swap); AfterDraw compares after the drawing,
// AfterPutBack after our put-back (and counts). Cancel: this drawing is not checked after all.
bool Wanted();
void Before();
void AfterDraw();
void AfterPutBack();
void Cancel();

// The table of regions with their counts (every 600 checks and at the game's end; why: e.g. " (game ended)").
void Report(const char *why);

// Offline tests only (src/games/zeus/test_restore.cpp): switch the check on for the game's addresses
// [low, low + size) without the settings file or the exe's section table.
bool EnableForTest(unsigned long low, unsigned long size);
bool Working();

} // namespace zeus::state_check
