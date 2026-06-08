#include "registry.h"
#include <windows.h>

#define ENV_USER_PATH_KEY   L"Environment"
#define ENV_SYS_PATH_KEY    L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment"
#define ENV_PATH_VALUE      L"Path"

static BOOL ReadRegValue(HKEY root, const wchar_t *subkey, const wchar_t *value,
                         wchar_t **out, DWORD *out_len)
{
    HKEY hKey;
    DWORD type, size = 0;
    LONG rc;

    rc = RegOpenKeyExW(root, subkey, 0, KEY_READ, &hKey);
    if (rc != ERROR_SUCCESS)
        return FALSE;

    /* Query size first. */
    rc = RegQueryValueExW(hKey, value, NULL, &type, NULL, &size);
    if (rc != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) {
        RegCloseKey(hKey);
        return FALSE;
    }

    *out = (wchar_t *)HeapAlloc(GetProcessHeap(), 0, size + 2);
    if (!*out) {
        RegCloseKey(hKey);
        return FALSE;
    }

    rc = RegQueryValueExW(hKey, value, NULL, &type, (LPBYTE)*out, &size);
    RegCloseKey(hKey);

    if (rc != ERROR_SUCCESS) {
        HeapFree(GetProcessHeap(), 0, *out);
        *out = NULL;
        return FALSE;
    }

    /* Ensure null termination. */
    (*out)[size / 2] = L'\0';
    if (out_len) *out_len = size;
    return TRUE;
}

static BOOL WriteRegValue(HKEY root, const wchar_t *subkey, const wchar_t *value,
                          const wchar_t *data, DWORD len_bytes)
{
    HKEY hKey;
    LONG rc;
    DWORD disp;

    rc = RegCreateKeyExW(root, subkey, 0, NULL, 0, KEY_WRITE, NULL, &hKey, &disp);
    if (rc != ERROR_SUCCESS)
        return FALSE;

    rc = RegSetValueExW(hKey, value, 0, REG_SZ, (const BYTE *)data, len_bytes);
    RegCloseKey(hKey);
    return (rc == ERROR_SUCCESS);
}

BOOL Reg_ReadUserPath(HKEY root, wchar_t **out, DWORD *out_len)
{
    return ReadRegValue(root, ENV_USER_PATH_KEY, ENV_PATH_VALUE, out, out_len);
}

BOOL Reg_ReadSystemPath(wchar_t **out, DWORD *out_len)
{
    return ReadRegValue(HKEY_LOCAL_MACHINE, ENV_SYS_PATH_KEY, ENV_PATH_VALUE, out, out_len);
}

BOOL Reg_WriteUserPath(const wchar_t *value, DWORD len_bytes)
{
    return WriteRegValue(HKEY_CURRENT_USER, ENV_USER_PATH_KEY, ENV_PATH_VALUE, value, len_bytes);
}

BOOL Reg_WriteSystemPath(const wchar_t *value, DWORD len_bytes)
{
    return WriteRegValue(HKEY_LOCAL_MACHINE, ENV_SYS_PATH_KEY, ENV_PATH_VALUE, value, len_bytes);
}

void Reg_BroadcastEnvChange(void)
{
    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0,
                        (LPARAM)L"Environment", SMTO_ABORTIFHUNG, 5000, NULL);
}
