// Zeus: Master of Olympus + Poseidon (Zeus.exe 2.1.4.0, SHA256 382bcd28...0a57; the GOG and the Steam release have
// the same exe) -- the game-specific layer. Everything that knows a Zeus address lives under games/zeus/;
// core never does.
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* From DllMain(DLL_PROCESS_ATTACH), before the settings file is read: registers this layer's settings and the
   settings file this version ships. Kernel32 only. */
void zeus_register_settings(void);

/* From the deferred start-up work (the exe's entry point), after the settings are read. Checks that the exe is the
   known build (the bytes at every hooked address and at two more places) and installs the game hooks; on any
   mismatch it installs nothing and logs why. */
void zeus_attach(void);

/* Zeus's editor is part of Zeus.exe (no separate editor exe): always 0. */
int zeus_editor_exe(void);

#ifdef __cplusplus
}
#endif
