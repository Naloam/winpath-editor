#ifndef REGISTRY_H
#define REGISTRY_H

#include <windows.h>

/* Read user PATH from HKCU\Environment. Caller must free *out with HeapFree. */
BOOL Reg_ReadUserPath(HKEY root, wchar_t **out, DWORD *out_len);

/* Read system PATH from HKLM\...\Environment. Caller must free *out with HeapFree. */
BOOL Reg_ReadSystemPath(wchar_t **out, DWORD *out_len);

/* Write user PATH to HKCU\Environment. */
BOOL Reg_WriteUserPath(const wchar_t *value, DWORD len_bytes);

/* Write system PATH to HKLM (requires admin). */
BOOL Reg_WriteSystemPath(const wchar_t *value, DWORD len_bytes);

/* Broadcast WM_SETTINGCHANGE so other processes refresh their env. */
void Reg_BroadcastEnvChange(void);

#endif /* REGISTRY_H */
