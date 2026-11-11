// A hooked or called function of Zeus.exe 2.1.4.0 (image base 0x400000, no relocations): its address and the first
// bytes it has in the known build. Every hook is installed only after Matches() confirmed those bytes.
//
// Game variables are reached through Var<T> and game functions through Function(site) or Original(...). Emperor's
// layer has the same helpers (games/emperor/site.h); the two are kept apart until both layers are settled (the
// layers do not include each other).
#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstring>

#include "core/hook/hook.h"

namespace zeus {

// A variable of the game (e.g. const Var<int> kGameSpeed{0xf2a774}; int s = *kGameSpeed;).
template <typename T> struct Var {
    DWORD address;
    volatile T *ptr() const
    {
        return reinterpret_cast<volatile T *>(address);
    }
    volatile T &operator*() const
    {
        return *ptr();
    }
};

struct Site {
    const char *name;
    DWORD address;
    BYTE expect[8];
};

inline bool Matches(const Site &s)
{
    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQuery(reinterpret_cast<void *>(s.address), &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT) {
        return false;
    }
    return std::memcmp(reinterpret_cast<const void *>(s.address), s.expect, sizeof(s.expect)) == 0;
}

template <typename F> bool Install(const Site &s, F detour, F &original)
{
    return core_hook_install(s.name, reinterpret_cast<void *>(s.address), reinterpret_cast<void *>(detour),
                             reinterpret_cast<void **>(&original)) != 0;
}

template <typename F> F Function(const Site &s)
{
    return reinterpret_cast<F>(s.address);
}

// A return address in the game's code, as a number (for comparing with known call sites).
inline DWORD GameAddress(const void *p)
{
    return reinterpret_cast<DWORD>(p);
}

} // namespace zeus
