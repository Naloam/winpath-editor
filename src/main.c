/*
 * WinPath Editor — Minimal Windows PATH environment variable editor
 * Pure C + Win32 API, target < 50KB executable.
 *
 * This file contains only the entry point symbol.
 * All logic lives in ui.c, path.c, registry.c.
 *
 * We define wWinMain here as a thin wrapper so that
 * the linker finds it in the primary translation unit.
 */

#include <windows.h>

/* Defined in ui.c */
extern int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int);

/*
 * If the CRT is not linked (freestanding), we need a real entry point.
 * With MSVC + /SUBSYSTEM:WINDOWS, the linker looks for wWinMain directly.
 * With MinGW, we need main/wmain as fallback — but -mwindows handles it.
 *
 * This file is intentionally minimal. All UI logic is in ui.c.
 */
