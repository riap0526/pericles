// Zeus's own log messages. The game calls an empty function all over (debug_log_stub, Zeus.exe 0x401000: the release
// build compiled its log out) with "OK :DD Set resolution."-style texts, numbers or other values; we hook it and put
// the texts in our log. These are the parts that do not need the game (the offline test runs them).
#pragma once

namespace zeus::debug_log {

// The game's message at p when p points to readable text: characters (no control characters other than tab, CR,
// LF) up to a NUL, at most size - 1 of them, copied to out with trailing white space removed; it must look like a
// message (at least 3 characters, at least 2 ASCII letters, at most a quarter outside ASCII). Returns its length,
// or -1 when p is a small number (the game also passes numbers), not readable, or bytes that only look like text.
int ReadMessage(const void *p, char *out, int size);

// Whether a message is logged this time: the first kPerMessage times each text, at most kTotal lines in all.
// times gets how often this text has come so far (including this time). Thread-safe.
constexpr int kPerMessage = 5;
constexpr int kTotal = 2000;
bool ShouldLog(const char *message, int *times);

// Offline tests: forget the counts.
void ResetForTest();

} // namespace zeus::debug_log
