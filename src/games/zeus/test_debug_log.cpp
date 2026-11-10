// Offline test of games/zeus/debug_log (no game, no DLL): reading the game's texts safely and the repeat limit.
#include "games/zeus/debug_log.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdio>
#include <cstring>

namespace {

int g_failed = 0, g_cases = 0;

void Check(bool ok, const char *what)
{
    ++g_cases;
    if (!ok) {
        ++g_failed;
    }
    std::printf("%s %s\n", ok ? "  ok    " : "  FAILED", what);
}

} // namespace

int main()
{
    namespace dl = zeus::debug_log;
    char out[64];

    std::printf("-- reading the game's texts\n");
    Check(dl::ReadMessage(reinterpret_cast<const void *>(0x14), out, sizeof(out)) == -1, "a small number: not a text");
    Check(dl::ReadMessage(nullptr, out, sizeof(out)) == -1, "null: not a text");
    static const char kOk[] = "OK :DD Set resolution.";
    Check(dl::ReadMessage(kOk, out, sizeof(out)) == static_cast<int>(std::strlen(kOk)) && std::strcmp(out, kOk) == 0,
          "a text in the exe's data: copied");
    static const char kTrail[] = "ERR:DD surface lock error:  \r\n";
    Check(dl::ReadMessage(kTrail, out, sizeof(out)) > 0 && std::strcmp(out, "ERR:DD surface lock error:") == 0,
          "trailing white space and line ends removed");
    static const char kInner[] = "line one\r\nline two";
    Check(dl::ReadMessage(kInner, out, sizeof(out)) > 0 && std::strcmp(out, "line one  line two") == 0,
          "line ends inside the text become spaces");
    static const char kControl[] = {'A', 'B', 0x01, 'C', 0};
    Check(dl::ReadMessage(kControl, out, sizeof(out)) == -1, "a control character: not a text");
    static const char kBlank[] = "   ";
    Check(dl::ReadMessage(kBlank, out, sizeof(out)) == -1, "only spaces: not a text");
    char longText[200];
    std::memset(longText, 'x', sizeof(longText) - 1);
    longText[sizeof(longText) - 1] = '\0';
    Check(dl::ReadMessage(longText, out, sizeof(out)) == -1, "no NUL within the buffer: not taken");
    static const char kHigh[] = "Caf\xe9 de Ath\xe8nes";
    Check(dl::ReadMessage(kHigh, out, sizeof(out)) == static_cast<int>(std::strlen(kHigh)),
          "a few Windows-1252 letters in a text are fine");
    // Seen in the first game test: pointers to numbers whose bytes looked like letters.
    static const char kBytes1[] = "\xc0\xc1" "A";
    static const char kBytes2[] = "\xa0\x20\x3d\xdd\xa6\xb1";
    static const char kBytes3[] = "\xb6,^f";
    Check(dl::ReadMessage(kBytes1, out, sizeof(out)) == -1 && dl::ReadMessage(kBytes2, out, sizeof(out)) == -1 &&
              dl::ReadMessage(kBytes3, out, sizeof(out)) == -1,
          "bytes that only look like text (mostly outside ASCII): not taken");
    Check(dl::ReadMessage("D", out, sizeof(out)) == -1 && dl::ReadMessage("OK", out, sizeof(out)) == -1,
          "one or two characters: not taken");
    Check(dl::ReadMessage("123 456", out, sizeof(out)) == -1, "no letters: not taken");
    Check(dl::ReadMessage("OK :Exited Normally", out, sizeof(out)) == 19, "a short game message: taken");

    // A page that can be read followed by one that cannot.
    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    const DWORD page = si.dwPageSize;
    char *two = static_cast<char *>(VirtualAlloc(nullptr, page * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    DWORD old = 0;
    VirtualProtect(two + page, page, PAGE_NOACCESS, &old);
    char *edge = two + page - 6;
    std::memcpy(edge, "abcdef", 6); // no NUL before the page that cannot be read
    Check(dl::ReadMessage(edge, out, sizeof(out)) == -1, "a text running into a page that cannot be read: not taken");
    std::memcpy(edge, "abcd\0", 5);
    Check(dl::ReadMessage(edge, out, sizeof(out)) == 4, "a text ending just before that page: copied");
    Check(dl::ReadMessage(two + page, out, sizeof(out)) == -1, "a pointer into a page that cannot be read");
    char *reserved = static_cast<char *>(VirtualAlloc(nullptr, page, MEM_RESERVE, PAGE_NOACCESS));
    Check(dl::ReadMessage(reserved, out, sizeof(out)) == -1, "a pointer into reserved memory");
    VirtualFree(reserved, 0, MEM_RELEASE);
    VirtualFree(two, 0, MEM_RELEASE);

    std::printf("-- the repeat limit\n");
    dl::ResetForTest();
    int times = 0;
    bool all = true;
    for (int k = 1; k <= dl::kPerMessage; ++k) {
        all = all && dl::ShouldLog("OK :Active.", &times) && times == k;
    }
    Check(all, "the first kPerMessage times are logged, counted");
    Check(!dl::ShouldLog("OK :Active.", &times) && times == dl::kPerMessage + 1, "then not logged, still counted");
    Check(dl::ShouldLog("OK :Not active.", &times) && times == 1, "another text is counted on its own");
    dl::ResetForTest();
    char text[32];
    bool under = true;
    for (int k = 0; k < dl::kTotal; ++k) {
        std::snprintf(text, sizeof(text), "message %d", k);
        under = under && dl::ShouldLog(text, &times);
    }
    Check(under, "kTotal different texts are logged");
    Check(!dl::ShouldLog("one more", &times), "then nothing more is logged");

    std::printf("  %d cases, %d failed\n  RESULT: %s (%d failed)\n", g_cases, g_failed, g_failed ? "FAIL" : "ok",
                g_failed);
    return g_failed ? 1 : 0;
}
