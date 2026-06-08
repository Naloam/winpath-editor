#include "path.h"
#include <shlwapi.h>
#include <shlobj.h>

/* ---- PathList core ---- */

void PathList_Init(PathList *pl, BOOL is_system)
{
    pl->count = 0;
    pl->is_system = is_system;
    ZeroMemory(pl->entries, sizeof(pl->entries));
}

void PathList_Parse(PathList *pl, const wchar_t *raw)
{
    const wchar_t *p = raw;

    pl->count = 0;
    if (!raw || !*raw)
        return;

    while (*p && pl->count < PATH_MAX_ENTRIES) {
        while (*p == L';') p++;
        if (!*p) break;

        const wchar_t *start = p;
        while (*p && *p != L';') p++;

        int len = (int)(p - start);
        if (len > 0 && len < PATH_MAX_LEN) {
            /* Trim trailing whitespace. */
            while (len > 0 && (start[len - 1] == L' ' || start[len - 1] == L'\t'))
                len--;
            if (len > 0) {
                CopyMemory(pl->entries[pl->count].path, start, len * sizeof(wchar_t));
                pl->entries[pl->count].path[len] = L'\0';
                pl->entries[pl->count].status = 0;
                pl->entries[pl->count].expanded[0] = L'\0';
                pl->count++;
            }
        }
    }
}

/* Check if a path is a UNC/network path. */
static BOOL IsNetworkPath(const wchar_t *path)
{
    return (path[0] == L'\\' && path[1] == L'\\');
}

/* Trim trailing backslash (but not for root like C:\). */
static void TrimTrailingSlash(wchar_t *buf, int len)
{
    if (len > 1 && buf[len - 1] == L'\\' &&
        !(len == 3 && buf[1] == L':')) {
        buf[len - 1] = L'\0';
    }
}

void PathList_Validate(PathList *pl)
{
    int i;
    DWORD attr;
    wchar_t expanded[PATH_MAX_LEN];

    for (i = 0; i < pl->count; i++) {
        PathEntry *pe = &pl->entries[i];
        pe->status = 0;

        /* Check if it contains environment variables. */
        if (wcschr(pe->path, L'%')) {
            pe->status |= PST_HAS_ENVVAR;
            ExpandEnvironmentStringsW(pe->path, expanded, PATH_MAX_LEN);
        } else {
            lstrcpyW(expanded, pe->path);
        }
        lstrcpyW(pe->expanded, expanded);

        /* Check for network path. */
        if (IsNetworkPath(expanded))
            pe->status |= PST_NETWORK;

        /* Check existence. */
        attr = GetFileAttributesW(expanded);
        if (attr == INVALID_FILE_ATTRIBUTES) {
            pe->status |= PST_MISSING;
        } else if (attr & FILE_ATTRIBUTE_DIRECTORY) {
            pe->status |= PST_EXISTS;
        } else {
            pe->status |= PST_NOT_DIR;
        }
    }

    /* Mark duplicates. */
    PathList_MarkDuplicates(pl);
}

wchar_t *PathList_Join(const PathList *pl)
{
    int i, total = 0;
    wchar_t *buf;

    for (i = 0; i < pl->count; i++)
        total += lstrlenW(pl->entries[i].path) + 1;

    if (total == 0) total = 1;
    buf = (wchar_t *)HeapAlloc(GetProcessHeap(), 0, (total + 1) * sizeof(wchar_t));
    if (!buf) return NULL;

    buf[0] = L'\0';
    for (i = 0; i < pl->count; i++) {
        if (i > 0) lstrcatW(buf, L";");
        lstrcatW(buf, pl->entries[i].path);
    }
    return buf;
}

BOOL PathList_Add(PathList *pl, const wchar_t *path)
{
    int len;
    if (pl->count >= PATH_MAX_ENTRIES || !path)
        return FALSE;
    len = lstrlenW(path);
    if (len == 0 || len >= PATH_MAX_LEN)
        return FALSE;
    lstrcpyW(pl->entries[pl->count].path, path);
    pl->entries[pl->count].expanded[0] = L'\0';
    pl->entries[pl->count].status = 0;
    pl->count++;
    return TRUE;
}

BOOL PathList_Remove(PathList *pl, int index)
{
    int i;
    if (index < 0 || index >= pl->count)
        return FALSE;
    for (i = index; i < pl->count - 1; i++)
        pl->entries[i] = pl->entries[i + 1];
    pl->count--;
    ZeroMemory(&pl->entries[pl->count], sizeof(PathEntry));
    return TRUE;
}

BOOL PathList_MoveUp(PathList *pl, int index)
{
    PathEntry tmp;
    if (index <= 0 || index >= pl->count)
        return FALSE;
    tmp = pl->entries[index];
    pl->entries[index] = pl->entries[index - 1];
    pl->entries[index - 1] = tmp;
    return TRUE;
}

BOOL PathList_MoveDown(PathList *pl, int index)
{
    PathEntry tmp;
    if (index < 0 || index >= pl->count - 1)
        return FALSE;
    tmp = pl->entries[index];
    pl->entries[index] = pl->entries[index + 1];
    pl->entries[index + 1] = tmp;
    return TRUE;
}

BOOL PathList_Set(PathList *pl, int index, const wchar_t *path)
{
    int len;
    if (index < 0 || index >= pl->count || !path)
        return FALSE;
    len = lstrlenW(path);
    if (len >= PATH_MAX_LEN)
        return FALSE;
    lstrcpyW(pl->entries[index].path, path);
    return TRUE;
}

/* ---- Normalized dedup ---- */

/* Normalize a path for comparison: expand env vars, trim slash, lowercase. */
static void NormalizePath(const wchar_t *src, wchar_t *dst, int dst_size)
{
    wchar_t tmp[PATH_MAX_LEN];

    /* Expand env vars. */
    if (wcschr(src, L'%'))
        ExpandEnvironmentStringsW(src, tmp, PATH_MAX_LEN);
    else
        lstrcpyW(tmp, src);

    /* Remove surrounding quotes. */
    {
        int len = lstrlenW(tmp);
        if (len >= 2 && tmp[0] == L'"' && tmp[len - 1] == L'"') {
            MoveMemory(tmp, tmp + 1, (len - 2) * sizeof(wchar_t));
            tmp[len - 2] = L'\0';
        }
    }

    /* Trim trailing slash. */
    {
        int len = lstrlenW(tmp);
        TrimTrailingSlash(tmp, len);
    }

    /* Lowercase. */
    CharLowerW(tmp);

    lstrcpyW(dst, tmp);
    (void)dst_size;
}

int PathList_MarkDuplicates(PathList *pl)
{
    int i, j, count = 0;
    wchar_t norm_i[PATH_MAX_LEN], norm_j[PATH_MAX_LEN];

    /* Clear existing duplicate flags. */
    for (i = 0; i < pl->count; i++)
        pl->entries[i].status &= ~PST_DUPLICATE;

    for (i = 0; i < pl->count; i++) {
        if (pl->entries[i].status & PST_DUPLICATE)
            continue;
        NormalizePath(pl->entries[i].path, norm_i, PATH_MAX_LEN);
        for (j = i + 1; j < pl->count; j++) {
            if (pl->entries[j].status & PST_DUPLICATE)
                continue;
            NormalizePath(pl->entries[j].path, norm_j, PATH_MAX_LEN);
            if (lstrcmpiW(norm_i, norm_j) == 0) {
                pl->entries[j].status |= PST_DUPLICATE;
                count++;
            }
        }
    }
    return count;
}

int PathList_RemoveDuplicates(PathList *pl)
{
    int removed = 0;
    int i;
    for (i = pl->count - 1; i >= 0; i--) {
        if (pl->entries[i].status & PST_DUPLICATE) {
            PathList_Remove(pl, i);
            removed++;
        }
    }
    return removed;
}

/* ---- Diff ---- */

static void AppendStr(wchar_t **buf, int *len, int *cap, const wchar_t *text)
{
    int needed = lstrlenW(text) + 1;
    while (*len + needed > *cap) {
        *cap *= 2;
        wchar_t *newbuf = (wchar_t *)HeapAlloc(GetProcessHeap(), 0, *cap * sizeof(wchar_t));
        if (!newbuf) return;
        CopyMemory(newbuf, *buf, *len * sizeof(wchar_t));
        HeapFree(GetProcessHeap(), 0, *buf);
        *buf = newbuf;
    }
    lstrcpyW(*buf + *len, text);
    *len += needed - 1;
}

wchar_t *PathList_Diff(const PathList *old_pl, const PathList *new_pl, const wchar_t *label)
{
    int cap = 4096, len = 0;
    wchar_t *buf = (wchar_t *)HeapAlloc(GetProcessHeap(), 0, cap * sizeof(wchar_t));
    wchar_t line[PATH_MAX_LEN + 32];
    int i, j;
    BOOL found;

    if (!buf) return NULL;
    buf[0] = L'\0';

    wsprintfW(line, L"%s:\r\n", label);
    AppendStr(&buf, &len, &cap, line);

    /* Find additions and moves. */
    for (i = 0; i < new_pl->count; i++) {
        found = FALSE;
        for (j = 0; j < old_pl->count; j++) {
            if (lstrcmpW(new_pl->entries[i].path, old_pl->entries[j].path) == 0) {
                found = TRUE;
                if (i != j) {
                    wsprintfW(line, L"  \x2191 %s #%d -> #%d\r\n",
                              new_pl->entries[i].path, j + 1, i + 1);
                    AppendStr(&buf, &len, &cap, line);
                }
                break;
            }
        }
        if (!found) {
            wsprintfW(line, L"  + %s\r\n", new_pl->entries[i].path);
            AppendStr(&buf, &len, &cap, line);
        }
    }

    /* Find removals. */
    for (i = 0; i < old_pl->count; i++) {
        found = FALSE;
        for (j = 0; j < new_pl->count; j++) {
            if (lstrcmpW(old_pl->entries[i].path, new_pl->entries[j].path) == 0) {
                found = TRUE;
                break;
            }
        }
        if (!found) {
            wsprintfW(line, L"  - %s\r\n", old_pl->entries[i].path);
            AppendStr(&buf, &len, &cap, line);
        }
    }

    /* If no changes, say so. */
    if (len == lstrlenW(label) + 3) {
        AppendStr(&buf, &len, &cap, L"  (no changes)\r\n");
    }

    return buf;
}

/* ---- Backup ---- */

static void GetBackupDir(wchar_t *dir, int max_len)
{
    if (!GetEnvironmentVariableW(L"LOCALAPPDATA", dir, max_len)) {
        GetModuleFileNameW(NULL, dir, max_len);
        wchar_t *lastslash = wcsrchr(dir, L'\\');
        if (lastslash) *(lastslash + 1) = L'\0';
    }
    lstrcatW(dir, L"\\WinPathEditor\\backup");
}

wchar_t *Backup_Save(const PathList *user, const PathList *sys)
{
    SYSTEMTIME st;
    wchar_t dirname[MAX_PATH], filename[MAX_PATH], *userJoined, *sysJoined;
    HANDLE hFile;
    DWORD written;
    wchar_t *result;

    GetLocalTime(&st);

    /* Create backup directory in %LOCALAPPDATA%\WinPathEditor\backup\ */
    GetBackupDir(dirname, MAX_PATH);
    /* Ensure parent dirs exist. */
    {
        wchar_t parent[MAX_PATH];
        lstrcpyW(parent, dirname);
        wchar_t *p = wcsrchr(parent, L'\\');
        if (p) { *p = L'\0'; CreateDirectoryW(parent, NULL); }
    }
    CreateDirectoryW(dirname, NULL);

    wsprintfW(filename, L"%s\\path-backup-%04d%02d%02d-%02d%02d%02d.txt",
              dirname, st.wYear, st.wMonth, st.wDay,
              st.wHour, st.wMinute, st.wSecond);

    hFile = CreateFileW(filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return NULL;

    userJoined = PathList_Join(user);
    sysJoined  = PathList_Join(sys);

    {
        wchar_t hdr[64];
        wsprintfW(hdr, L"User PATH (%d entries):\r\n", user->count);
        WriteFile(hFile, hdr, lstrlenW(hdr) * sizeof(wchar_t), &written, NULL);
    }
    if (userJoined) {
        WriteFile(hFile, userJoined, lstrlenW(userJoined) * sizeof(wchar_t), &written, NULL);
        HeapFree(GetProcessHeap(), 0, userJoined);
    }
    {
        wchar_t nl[] = L"\r\n\r\n";
        WriteFile(hFile, nl, lstrlenW(nl) * sizeof(wchar_t), &written, NULL);
    }
    {
        wchar_t hdr[64];
        wsprintfW(hdr, L"System PATH (%d entries):\r\n", sys->count);
        WriteFile(hFile, hdr, lstrlenW(hdr) * sizeof(wchar_t), &written, NULL);
    }
    if (sysJoined) {
        WriteFile(hFile, sysJoined, lstrlenW(sysJoined) * sizeof(wchar_t), &written, NULL);
        HeapFree(GetProcessHeap(), 0, sysJoined);
    }
    {
        wchar_t nl[] = L"\r\n";
        WriteFile(hFile, nl, lstrlenW(nl) * sizeof(wchar_t), &written, NULL);
    }

    CloseHandle(hFile);

    /* Return the filename (caller must free). */
    result = (wchar_t *)HeapAlloc(GetProcessHeap(), 0, (lstrlenW(filename) + 1) * sizeof(wchar_t));
    if (result) lstrcpyW(result, filename);
    return result;
}

wchar_t *Backup_FindLatest(void)
{
    WIN32_FIND_DATAW fd;
    HANDLE hFind;
    wchar_t pattern[MAX_PATH], latest[MAX_PATH], backupDir[MAX_PATH];
    wchar_t *result;

    GetBackupDir(backupDir, MAX_PATH);
    wsprintfW(pattern, L"%s\\path-backup-*.txt", backupDir);

    latest[0] = L'\0';
    hFind = FindFirstFileW(pattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return NULL;

    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            if (lstrcmpW(fd.cFileName, latest) > 0) {
                lstrcpyW(latest, fd.cFileName);
            }
        }
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);

    if (latest[0] == L'\0') return NULL;

    wsprintfW(pattern, L"%s\\%s", backupDir, latest);

    result = (wchar_t *)HeapAlloc(GetProcessHeap(), 0, (lstrlenW(pattern) + 1) * sizeof(wchar_t));
    if (result) lstrcpyW(result, pattern);
    return result;
}

BOOL Backup_Load(const wchar_t *filepath, PathList *user, PathList *sys)
{
    HANDLE hFile;
    DWORD size, read;
    wchar_t *buf, *p, *userStart, *sysStart;

    hFile = CreateFileW(filepath, GENERIC_READ, FILE_SHARE_READ, NULL,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return FALSE;

    size = GetFileSize(hFile, NULL);
    if (size == 0 || size > 65536) { CloseHandle(hFile); return FALSE; }

    buf = (wchar_t *)HeapAlloc(GetProcessHeap(), 0, size + 2);
    if (!buf) { CloseHandle(hFile); return FALSE; }

    ReadFile(hFile, buf, size, &read, NULL);
    CloseHandle(hFile);
    buf[read / 2] = L'\0';

    /* Skip BOM if present. */
    p = buf;
    if (*p == 0xFEFF) p++;

    /* Find "User PATH" line, then the path data after the colon+newline. */
    userStart = wcsstr(p, L"User PATH");
    if (userStart) {
        userStart = wcschr(userStart, L'\n');
        if (userStart) {
            userStart++;
            /* Find end of line (the path data ends at \r\n\r\n). */
            wchar_t *userEnd = wcsstr(userStart, L"\r\n\r\n");
            if (userEnd) {
                *userEnd = L'\0';
                PathList_Parse(user, userStart);
                *userEnd = L'\r';
            }
        }
    }

    /* Find "System PATH" line. */
    sysStart = wcsstr(p, L"System PATH");
    if (sysStart) {
        sysStart = wcschr(sysStart, L'\n');
        if (sysStart) {
            sysStart++;
            /* Path data goes to end of file. */
            /* Trim trailing whitespace. */
            int len = lstrlenW(sysStart);
            while (len > 0 && (sysStart[len-1] == L'\r' || sysStart[len-1] == L'\n'))
                sysStart[--len] = L'\0';
            PathList_Parse(sys, sysStart);
        }
    }

    HeapFree(GetProcessHeap(), 0, buf);
    return TRUE;
}
