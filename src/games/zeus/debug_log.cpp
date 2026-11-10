// Zeus's own log messages: reading the game's text safely and limiting repeats (debug_log.h).
#include "games/zeus/debug_log.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstring>
#include <string>
#include <unordered_map>

namespace zeus::debug_log {

namespace {

constexpr DWORD kReadable = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ |
                            PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;

SRWLOCK g_lock = SRWLOCK_INIT;
std::unordered_map<std::string, int> *g_counts = nullptr; // made on first use (no static constructor in the DLL)
int g_total = 0;

// The game's messages are English: at least 3 characters, at least 2 ASCII letters, and at most a quarter outside
// ASCII. The game sometimes passes a pointer to numbers, whose bytes can look like Windows-1252 letters ("\xC0\xC1A",
// a lone "D"); those are not taken.
bool LooksLikeText(const char *s, int n)
{
    if (n < 3) {
        return false;
    }
    int letters = 0, high = 0;
    for (int i = 0; i < n; ++i) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        letters += (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
        high += c >= 0x80;
    }
    return letters >= 2 && high * 4 <= n;
}

} // namespace

int ReadMessage(const void *p, char *out, int size)
{
    if (size < 2 || reinterpret_cast<ULONG_PTR>(p) < 0x10000) {
        return -1;
    }
    const char *s = static_cast<const char *>(p);
    int n = 0;
    while (n < size - 1) {
        // The text may cross into the next region: look each region up before reading from it.
        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQuery(s + n, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT || !(mbi.Protect & kReadable) ||
            (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS))) {
            return -1;
        }
        const char *end = static_cast<const char *>(mbi.BaseAddress) + mbi.RegionSize;
        while (n < size - 1 && s + n < end) {
            const unsigned char c = static_cast<unsigned char>(s[n]);
            if (c == 0) {
                while (n > 0 && (out[n - 1] == ' ' || out[n - 1] == '\t' || out[n - 1] == '\r' || out[n - 1] == '\n')) {
                    --n;
                }
                out[n] = '\0';
                return LooksLikeText(out, n) ? n : -1;
            }
            if (c < 0x20 && c != '\t' && c != '\r' && c != '\n') {
                return -1;
            }
            out[n++] = static_cast<char>(c == '\r' || c == '\n' || c == '\t' ? ' ' : c);
        }
    }
    return -1; // no NUL within size - 1 characters: not taken as a message
}

bool ShouldLog(const char *message, int *times)
{
    AcquireSRWLockExclusive(&g_lock);
    if (!g_counts) {
        g_counts = new std::unordered_map<std::string, int>();
    }
    int &count = (*g_counts)[message];
    ++count;
    *times = count;
    const bool log = count <= kPerMessage && g_total < kTotal;
    if (log) {
        ++g_total;
    }
    ReleaseSRWLockExclusive(&g_lock);
    return log;
}

void ResetForTest()
{
    AcquireSRWLockExclusive(&g_lock);
    if (g_counts) {
        g_counts->clear();
    }
    g_total = 0;
    ReleaseSRWLockExclusive(&g_lock);
}

} // namespace zeus::debug_log
