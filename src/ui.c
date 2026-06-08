#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include "ui.h"
#include "resource.h"
#include "path.h"
#include "registry.h"

/* Global state. */
PathList g_userPath;
PathList g_sysPath;
AppState g_app;

/* Undo snapshot. */
static PathList g_undoUser;
static PathList g_undoSys;
static BOOL g_hasUndo = FALSE;

/* Drag-and-drop state. */
static BOOL  g_dragging = FALSE;
static HWND  g_dragList = NULL;
static int   g_dragItem = -1;
static int   g_dragRealIdx = -1;

static void SaveUndoSnapshot(void)
{
    CopyMemory(&g_undoUser, &g_userPath, sizeof(PathList));
    CopyMemory(&g_undoSys,  &g_sysPath,  sizeof(PathList));
    g_hasUndo = TRUE;
}

static void RestoreUndoSnapshot(void)
{
    if (!g_hasUndo) return;
    CopyMemory(&g_userPath, &g_undoUser, sizeof(PathList));
    CopyMemory(&g_sysPath,  &g_undoSys,  sizeof(PathList));
    g_hasUndo = FALSE;
    g_app.dirty = TRUE;
    UI_RefreshLists();
}

/* ---- ListView helpers ---- */

static void ListView_InsertPathEntry(HWND hList, int idx, const PathEntry *pe, int realIdx)
{
    LVITEMW lvi = {0};
    wchar_t buf[PATH_MAX_LEN + 16];
    const wchar_t *icon;

    lvi.mask     = LVIF_TEXT | LVIF_PARAM;
    lvi.iItem    = idx;
    lvi.lParam   = (LPARAM)realIdx;

    /* Determine status icon. Priority: duplicate > missing > not_dir > envvar > network > exists. */
    if (pe->status & PST_DUPLICATE)
        icon = L"[D]";      /* duplicate */
    else if (pe->status & PST_MISSING)
        icon = L"[X]";      /* missing */
    else if (pe->status & PST_NOT_DIR)
        icon = L"[!]";      /* not a directory */
    else if (pe->status & PST_EXISTS) {
        if (pe->status & PST_HAS_ENVVAR)
            icon = L"[%]";  /* contains env var */
        else if (pe->status & PST_NETWORK)
            icon = L"[~]";  /* network */
        else
            icon = L"[OK]"; /* exists */
    } else {
        icon = L"[?]";      /* unknown */
    }

    wsprintfW(buf, L"%s  %s", icon, pe->path);
    lvi.pszText  = buf;
    ListView_InsertItem(hList, &lvi);
}

static void RefreshOneList(HWND hList, const PathList *pl, const wchar_t *filter)
{
    int i;
    ListView_DeleteAllItems(hList);
    for (i = 0; i < pl->count; i++) {
        if (filter && filter[0]) {
            if (!StrStrIW(pl->entries[i].path, filter))
                continue;
        }
        ListView_InsertPathEntry(hList, ListView_GetItemCount(hList), &pl->entries[i], i);
    }
}

void UI_RefreshLists(void)
{
    RefreshOneList(g_app.hListUser, &g_userPath, g_app.filter);
    RefreshOneList(g_app.hListSys,  &g_sysPath,  g_app.filter);
    UI_UpdateStatus();
}

void UI_UpdateStatus(void)
{
    int i, userBad = 0, sysBad = 0, userDup = 0, sysDup = 0;
    wchar_t buf[256];

    for (i = 0; i < g_userPath.count; i++) {
        if (g_userPath.entries[i].status & PST_MISSING) userBad++;
        if (g_userPath.entries[i].status & PST_DUPLICATE) userDup++;
    }
    for (i = 0; i < g_sysPath.count; i++) {
        if (g_sysPath.entries[i].status & PST_MISSING) sysBad++;
        if (g_sysPath.entries[i].status & PST_DUPLICATE) sysDup++;
    }

    wsprintfW(buf, L"User: %d (%d missing, %d dup)  |  System: %d (%d missing, %d dup)%s",
              g_userPath.count, userBad, userDup,
              g_sysPath.count, sysBad, sysDup,
              g_app.dirty ? L"  * unsaved" : L"");
    SetWindowTextW(g_app.hStatus, buf);
}

/* ---- Button command handlers ---- */

static void DoAdd(PathList *pl)
{
    wchar_t buf[MAX_PATH] = {0};
    BROWSEINFOW bi = {0};
    PIDLIST_ABSOLUTE pidl;

    bi.hwndOwner  = g_app.hMain;
    bi.pszDisplayName = buf;
    bi.ulFlags    = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    bi.lpszTitle  = L"Select folder to add to PATH";

    pidl = SHBrowseForFolderW(&bi);
    if (pidl) {
        SHGetPathFromIDListW(pidl, buf);
        CoTaskMemFree(pidl);
        if (buf[0]) {
            SaveUndoSnapshot();
            PathList_Add(pl, buf);
            PathList_Validate(pl);
            g_app.dirty = TRUE;
            UI_RefreshLists();
        }
    }
}

static void DoDelete(PathList *pl, HWND hList)
{
    int sel = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
    if (sel < 0) return;

    /* Get the real index from lParam. */
    LVITEMW lvi = {0};
    lvi.mask  = LVIF_PARAM;
    lvi.iItem = sel;
    ListView_GetItem(hList, &lvi);
    int realIdx = (int)lvi.lParam;

    SaveUndoSnapshot();
    PathList_Remove(pl, realIdx);
    g_app.dirty = TRUE;
    UI_RefreshLists();
}

static void DoMoveUp(PathList *pl, HWND hList)
{
    int sel = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
    if (sel <= 0) return;

    LVITEMW lvi = {0};
    lvi.mask  = LVIF_PARAM;
    lvi.iItem = sel;
    ListView_GetItem(hList, &lvi);
    int realIdx = (int)lvi.lParam;

    SaveUndoSnapshot();
    PathList_MoveUp(pl, realIdx);
    g_app.dirty = TRUE;
    UI_RefreshLists();
    /* Re-select the moved item. */
    ListView_SetItemState(hList, sel - 1, LVIS_SELECTED | LVIS_FOCUSED, 0xFFFF);
}

static void DoMoveDown(PathList *pl, HWND hList)
{
    int sel = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
    if (sel < 0) return;

    LVITEMW lvi = {0};
    lvi.mask  = LVIF_PARAM;
    lvi.iItem = sel;
    ListView_GetItem(hList, &lvi);
    int realIdx = (int)lvi.lParam;

    SaveUndoSnapshot();
    PathList_MoveDown(pl, realIdx);
    g_app.dirty = TRUE;
    UI_RefreshLists();
    ListView_SetItemState(hList, sel + 1, LVIS_SELECTED | LVIS_FOCUSED, 0xFFFF);
}

static void DoRefresh(void)
{
    wchar_t *raw = NULL;
    DWORD len = 0;

    if (Reg_ReadUserPath(HKEY_CURRENT_USER, &raw, &len)) {
        PathList_Parse(&g_userPath, raw);
        HeapFree(GetProcessHeap(), 0, raw);
    }
    PathList_Validate(&g_userPath);

    raw = NULL;
    if (Reg_ReadSystemPath(&raw, &len)) {
        PathList_Parse(&g_sysPath, raw);
        HeapFree(GetProcessHeap(), 0, raw);
    }
    PathList_Validate(&g_sysPath);

    g_app.dirty = FALSE;
    g_hasUndo = FALSE;
    UI_RefreshLists();
}

/* Forward declaration for edit subclass. */
static LRESULT CALLBACK EditSubclassProc(HWND hEdit, UINT msg, WPARAM wp, LPARAM lp,
                                          UINT_PTR uidRef, DWORD_PTR dwRef);

static void DoEdit(PathList *pl, HWND hList, int iItem)
{
    if (iItem < 0) return;

    LVITEMW lvi = {0};
    lvi.mask  = LVIF_PARAM;
    lvi.iItem = iItem;
    ListView_GetItem(hList, &lvi);
    int realIdx = (int)lvi.lParam;

    /* Simple input via a custom edit-in-place: create an edit control over the item. */
    RECT rc;
    ListView_GetSubItemRect(hList, iItem, 0, LVIR_LABEL, &rc);

    HWND hEdit = CreateWindowExW(0, L"EDIT", pl->entries[realIdx].path,
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
        hList, NULL, g_app.hInst, NULL);
    SendMessageW(hEdit, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    SetFocus(hEdit);
    SendMessageW(hEdit, EM_SETSEL, 0, -1);

    /* Subclass to handle Enter/Escape. Store real index for the edit. */
    SetWindowLongPtrW(hEdit, GWLP_USERDATA, (LONG_PTR)realIdx);
    SetWindowSubclass(hEdit, EditSubclassProc, 0, 0);
}

/* Snapshot of PATH at load time, for diff comparison. */
static PathList g_snapshotUser;
static PathList g_snapshotSys;
static BOOL g_hasSnapshot = FALSE;

/* Raw registry PATH strings at load time, for external change detection. */
static wchar_t *g_rawUserAtLoad = NULL;
static wchar_t *g_rawSysAtLoad  = NULL;

static void SaveSnapshot(void)
{
    DWORD len;
    CopyMemory(&g_snapshotUser, &g_userPath, sizeof(PathList));
    CopyMemory(&g_snapshotSys,  &g_sysPath,  sizeof(PathList));
    g_hasSnapshot = TRUE;

    /* Save raw registry values for external change detection. */
    if (g_rawUserAtLoad) { HeapFree(GetProcessHeap(), 0, g_rawUserAtLoad); g_rawUserAtLoad = NULL; }
    if (g_rawSysAtLoad)  { HeapFree(GetProcessHeap(), 0, g_rawSysAtLoad);  g_rawSysAtLoad  = NULL; }
    Reg_ReadUserPath(HKEY_CURRENT_USER, &g_rawUserAtLoad, &len);
    Reg_ReadSystemPath(&g_rawSysAtLoad, &len);
}

/* Check if PATH has been modified externally since we loaded it.
   Returns: 0 = no change, 1 = user changed, 2 = sys changed, 3 = both changed. */
static int CheckExternalChanges(void)
{
    wchar_t *curUser = NULL, *curSys = NULL;
    DWORD len;
    int result = 0;

    if (g_rawUserAtLoad) {
        if (Reg_ReadUserPath(HKEY_CURRENT_USER, &curUser, &len)) {
            if (lstrcmpW(curUser, g_rawUserAtLoad) != 0)
                result |= 1;
            HeapFree(GetProcessHeap(), 0, curUser);
        }
    }
    if (g_rawSysAtLoad) {
        if (Reg_ReadSystemPath(&curSys, &len)) {
            if (lstrcmpW(curSys, g_rawSysAtLoad) != 0)
                result |= 2;
            HeapFree(GetProcessHeap(), 0, curSys);
        }
    }
    return result;
}

static void DoSave(void)
{
    wchar_t *joined, *diffUser, *diffSys, *diffMsg, *backupFile;
    int rc;
    int diffLen;

    /* Check for external changes. */
    rc = CheckExternalChanges();
    if (rc) {
        wchar_t msg[256];
        wsprintfW(msg, L"PATH has been modified externally since WinPath Editor was opened.\n"
                       L"Changes detected in: %s%s%s\n\n"
                       L"Click OK to overwrite external changes, or Cancel to reload.",
                  (rc & 1) ? L"User PATH" : L"",
                  (rc & 3) == 3 ? L" and " : L"",
                  (rc & 2) ? L"System PATH" : L"");
        rc = MessageBoxW(g_app.hMain, msg, L"External Changes Detected",
                         MB_OKCANCEL | MB_ICONWARNING);
        if (rc == IDCANCEL) {
            DoRefresh();
            return;
        }
    }

    /* Build diff preview. */
    diffUser = g_hasSnapshot ? PathList_Diff(&g_snapshotUser, &g_userPath, L"User PATH") : NULL;
    diffSys  = g_hasSnapshot ? PathList_Diff(&g_snapshotSys,  &g_sysPath,  L"System PATH") : NULL;

    /* Combine diffs into one message. */
    diffLen = (diffUser ? lstrlenW(diffUser) : 0) + (diffSys ? lstrlenW(diffSys) : 0) + 64;
    diffMsg = (wchar_t *)HeapAlloc(GetProcessHeap(), 0, diffLen * sizeof(wchar_t));
    if (diffMsg) {
        diffMsg[0] = L'\0';
        if (diffUser) lstrcatW(diffMsg, diffUser);
        if (diffSys) {
            if (diffUser) lstrcatW(diffMsg, L"\r\n");
            lstrcatW(diffMsg, diffSys);
        }
        lstrcatW(diffMsg, L"\r\nProceed with save?");
    }

    /* Show diff and confirm. */
    rc = MessageBoxW(g_app.hMain,
        diffMsg ? diffMsg : L"Save PATH changes?",
        L"WinPath Editor — Confirm Save",
        MB_OKCANCEL | MB_ICONINFORMATION);

    if (diffUser) HeapFree(GetProcessHeap(), 0, diffUser);
    if (diffSys)  HeapFree(GetProcessHeap(), 0, diffSys);
    if (diffMsg)  HeapFree(GetProcessHeap(), 0, diffMsg);

    if (rc != IDOK) return;

    /* Auto-backup before saving. */
    backupFile = Backup_Save(&g_userPath, &g_sysPath);
    if (backupFile) {
        /* Show brief notification. */
        wchar_t msg[MAX_PATH + 64];
        wsprintfW(msg, L"Backup saved to:\n%s", backupFile);
        /* Non-blocking: just update status bar. */
        SetWindowTextW(g_app.hStatus, msg);
        HeapFree(GetProcessHeap(), 0, backupFile);
    }

    /* Save user PATH. */
    joined = PathList_Join(&g_userPath);
    if (joined) {
        Reg_WriteUserPath(joined, (lstrlenW(joined) + 1) * sizeof(wchar_t));
        HeapFree(GetProcessHeap(), 0, joined);
    }

    /* Save system PATH (may fail without admin). */
    joined = PathList_Join(&g_sysPath);
    if (joined) {
        if (!Reg_WriteSystemPath(joined, (lstrlenW(joined) + 1) * sizeof(wchar_t))) {
            rc = MessageBoxW(g_app.hMain,
                L"Failed to write system PATH.\nRetry as Administrator?",
                L"WinPath Editor", MB_YESNO | MB_ICONWARNING);
            if (rc == IDYES) {
                /* Re-launch self as admin. */
                wchar_t exe[MAX_PATH];
                GetModuleFileNameW(NULL, exe, MAX_PATH);
                ShellExecuteW(NULL, L"runas", exe, NULL, NULL, SW_SHOWNORMAL);
                PostQuitMessage(0);
            }
        }
        HeapFree(GetProcessHeap(), 0, joined);
    }

    Reg_BroadcastEnvChange();
    g_app.dirty = FALSE;
    /* Update snapshot to current state. */
    SaveSnapshot();
    UI_UpdateStatus();
    MessageBoxW(g_app.hMain, L"PATH saved successfully.\nNew terminals will pick up changes.",
                L"WinPath Editor", MB_OK | MB_ICONINFORMATION);
}

static void DoRestore(void)
{
    wchar_t *latest;
    PathList tmpUser, tmpSys;
    int rc;

    latest = Backup_FindLatest();
    if (!latest) {
        MessageBoxW(g_app.hMain, L"No backups found.\nBackups are created automatically when you save.",
                    L"Restore Backup", MB_OK | MB_ICONINFORMATION);
        return;
    }

    PathList_Init(&tmpUser, FALSE);
    PathList_Init(&tmpSys, TRUE);

    if (!Backup_Load(latest, &tmpUser, &tmpSys)) {
        MessageBoxW(g_app.hMain, L"Failed to read backup file.", L"Restore Backup", MB_OK | MB_ICONERROR);
        HeapFree(GetProcessHeap(), 0, latest);
        return;
    }

    /* Show what will be restored. */
    {
        wchar_t msg[512];
        wsprintfW(msg, L"Restore from:\n%s\n\nUser PATH: %d entries\nSystem PATH: %d entries\n\nThis will replace your current edits.",
                  latest, tmpUser.count, tmpSys.count);
        rc = MessageBoxW(g_app.hMain, msg, L"Restore Backup", MB_OKCANCEL | MB_ICONQUESTION);
    }

    if (rc == IDOK) {
        CopyMemory(&g_userPath, &tmpUser, sizeof(PathList));
        CopyMemory(&g_sysPath,  &tmpSys,  sizeof(PathList));
        PathList_Validate(&g_userPath);
        PathList_Validate(&g_sysPath);
        g_app.dirty = TRUE;
        UI_RefreshLists();
    }

    HeapFree(GetProcessHeap(), 0, latest);
}

static void DoExport(void)
{
    OPENFILENAMEW ofn = {0};
    wchar_t file[MAX_PATH] = L"path_export.txt";
    wchar_t *joined;
    HANDLE hFile;
    DWORD written;

    ofn.lStructSize  = sizeof(ofn);
    ofn.hwndOwner    = g_app.hMain;
    ofn.lpstrFilter  = L"Text Files (*.txt)\0*.txt\0All Files\0*.*\0";
    ofn.lpstrFile    = file;
    ofn.nMaxFile     = MAX_PATH;
    ofn.Flags        = OFN_OVERWRITEPROMPT;
    ofn.lpstrDefExt  = L"txt";

    if (!GetSaveFileNameW(&ofn))
        return;

    hFile = CreateFileW(file, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return;

    /* Write user PATH. */
    joined = PathList_Join(&g_userPath);
    if (joined) {
        wchar_t hdr[] = L"User PATH:\r\n";
        WriteFile(hFile, hdr, lstrlenW(hdr) * sizeof(wchar_t), &written, NULL);
        WriteFile(hFile, joined, lstrlenW(joined) * sizeof(wchar_t), &written, NULL);
        wchar_t nl[] = L"\r\n\r\n";
        WriteFile(hFile, nl, lstrlenW(nl) * sizeof(wchar_t), &written, NULL);
        HeapFree(GetProcessHeap(), 0, joined);
    }

    /* Write system PATH. */
    joined = PathList_Join(&g_sysPath);
    if (joined) {
        wchar_t hdr[] = L"System PATH:\r\n";
        WriteFile(hFile, hdr, lstrlenW(hdr) * sizeof(wchar_t), &written, NULL);
        WriteFile(hFile, joined, lstrlenW(joined) * sizeof(wchar_t), &written, NULL);
        wchar_t nl[] = L"\r\n";
        WriteFile(hFile, nl, lstrlenW(nl) * sizeof(wchar_t), &written, NULL);
        HeapFree(GetProcessHeap(), 0, joined);
    }

    CloseHandle(hFile);
    MessageBoxW(g_app.hMain, L"PATH exported successfully.", L"Export", MB_OK | MB_ICONINFORMATION);
}

static void DoDedup(void)
{
    int r1 = PathList_RemoveDuplicates(&g_userPath);
    int r2 = PathList_RemoveDuplicates(&g_sysPath);
    if (r1 + r2 > 0) {
        g_app.dirty = TRUE;
        UI_RefreshLists();
        wchar_t buf[128];
        wsprintfW(buf, L"Removed %d duplicate(s).", r1 + r2);
        MessageBoxW(g_app.hMain, buf, L"Dedup", MB_OK | MB_ICONINFORMATION);
    } else {
        MessageBoxW(g_app.hMain, L"No duplicates found.", L"Dedup", MB_OK);
    }
}

/* ---- Edit-in-place subclass (Enter/Escape handling) ---- */

static LRESULT CALLBACK EditSubclassProc(HWND hEdit, UINT msg, WPARAM wp, LPARAM lp,
                                          UINT_PTR uidRef, DWORD_PTR dwRef)
{
    (void)uidRef; (void)dwRef;
    if (msg == WM_KEYDOWN) {
        if (wp == VK_RETURN) {
            wchar_t buf[PATH_MAX_LEN];
            GetWindowTextW(hEdit, buf, PATH_MAX_LEN);
            int realIdx = (int)GetWindowLongPtrW(hEdit, GWLP_USERDATA);
            /* Determine which PathList this belongs to by checking parent list. */
            HWND hList = GetParent(hEdit);
            PathList *pl = (hList == g_app.hListUser) ? &g_userPath : &g_sysPath;
            SaveUndoSnapshot();
            PathList_Set(pl, realIdx, buf);
            PathList_Validate(pl);
            g_app.dirty = TRUE;
            DestroyWindow(hEdit);
            UI_RefreshLists();
            return 0;
        }
        if (wp == VK_ESCAPE) {
            DestroyWindow(hEdit);
            return 0;
        }
    }
    if (msg == WM_KILLFOCUS) {
        DestroyWindow(hEdit);
        return 0;
    }
    return DefSubclassProc(hEdit, msg, wp, lp);
}

/* ---- Layout constants ---- */

#define MARGIN      8
#define BTN_H       24
#define BTN_W       70
#define SEARCH_H    24
#define STATUS_H    20
#define LABEL_H     18
#define LIST_MIN_H  100

void UI_InitCommonControls(void)
{
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(icex);
    icex.dwICC  = ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES;
    InitCommonControlsEx(&icex);
}

void UI_CreateControls(HWND hwnd, HINSTANCE hInst)
{
    HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

    g_app.hInst = hInst;

    /* Search box. */
    g_app.hSearch = CreateWindowExW(0, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        0, 0, 0, 0, hwnd, (HMENU)(UINT_PTR)IDC_SEARCH, hInst, NULL);
    SendMessageW(g_app.hSearch, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessageW(g_app.hSearch, EM_SETCUEBANNER, TRUE, (LPARAM)L"Search paths...");

    /* ---- User PATH section ---- */
    g_app.hListUser = CreateWindowExW(0, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        0, 0, 0, 0, hwnd, (HMENU)(UINT_PTR)IDC_LIST_USER, hInst, NULL);
    ListView_SetExtendedListViewStyle(g_app.hListUser,
        LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);

    /* Add column. */
    LVCOLUMNW lvc = {0};
    lvc.mask    = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    lvc.pszText = L"User PATH";
    lvc.cx      = 600;
    lvc.iSubItem = 0;
    ListView_InsertColumn(g_app.hListUser, 0, &lvc);

    /* User buttons. */
    g_app.hBtnAddUser     = CreateWindowW(L"BUTTON", L"+ Add",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0,0,0,0, hwnd, (HMENU)(UINT_PTR)IDC_BTN_ADD_USER, hInst, NULL);
    g_app.hBtnDelUser     = CreateWindowW(L"BUTTON", L"- Del",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0,0,0,0, hwnd, (HMENU)(UINT_PTR)IDC_BTN_DEL_USER, hInst, NULL);
    g_app.hBtnUpUser      = CreateWindowW(L"BUTTON", L"\x2191 Up",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0,0,0,0, hwnd, (HMENU)(UINT_PTR)IDC_BTN_UP_USER, hInst, NULL);
    g_app.hBtnDownUser    = CreateWindowW(L"BUTTON", L"\x2193 Down",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0,0,0,0, hwnd, (HMENU)(UINT_PTR)IDC_BTN_DOWN_USER, hInst, NULL);
    g_app.hBtnRefreshU    = CreateWindowW(L"BUTTON", L"Refresh",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0,0,0,0, hwnd, (HMENU)(UINT_PTR)IDC_BTN_REFRESH_U, hInst, NULL);

    /* ---- System PATH section ---- */
    g_app.hListSys = CreateWindowExW(0, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        0, 0, 0, 0, hwnd, (HMENU)(UINT_PTR)IDC_LIST_SYSTEM, hInst, NULL);
    ListView_SetExtendedListViewStyle(g_app.hListSys,
        LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);

    lvc.pszText = L"System PATH (admin)";
    ListView_InsertColumn(g_app.hListSys, 0, &lvc);

    g_app.hBtnAddSys  = CreateWindowW(L"BUTTON", L"+ Add",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0,0,0,0, hwnd, (HMENU)(UINT_PTR)IDC_BTN_ADD_SYS, hInst, NULL);
    g_app.hBtnDelSys  = CreateWindowW(L"BUTTON", L"- Del",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0,0,0,0, hwnd, (HMENU)(UINT_PTR)IDC_BTN_DEL_SYS, hInst, NULL);
    g_app.hBtnUpSys   = CreateWindowW(L"BUTTON", L"\x2191 Up",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0,0,0,0, hwnd, (HMENU)(UINT_PTR)IDC_BTN_UP_SYS, hInst, NULL);
    g_app.hBtnDownSys = CreateWindowW(L"BUTTON", L"\x2193 Down",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0,0,0,0, hwnd, (HMENU)(UINT_PTR)IDC_BTN_DOWN_SYS, hInst, NULL);
    g_app.hBtnRefreshS = CreateWindowW(L"BUTTON", L"Refresh",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0,0,0,0, hwnd, (HMENU)(UINT_PTR)IDC_BTN_REFRESH_S, hInst, NULL);

    /* ---- Bottom buttons ---- */
    g_app.hBtnSave   = CreateWindowW(L"BUTTON", L"Save (Ctrl+S)",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0,0,0,0, hwnd, (HMENU)(UINT_PTR)IDC_BTN_SAVE, hInst, NULL);
    g_app.hBtnExport = CreateWindowW(L"BUTTON", L"Export",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0,0,0,0, hwnd, (HMENU)(UINT_PTR)IDC_BTN_EXPORT, hInst, NULL);
    g_app.hBtnDedup  = CreateWindowW(L"BUTTON", L"Dedup",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0,0,0,0, hwnd, (HMENU)(UINT_PTR)IDC_BTN_DEDUP, hInst, NULL);
    g_app.hBtnRestore = CreateWindowW(L"BUTTON", L"Restore Backup",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0,0,0,0, hwnd, (HMENU)(UINT_PTR)IDC_BTN_RESTORE, hInst, NULL);
    g_app.hBtnQuit   = CreateWindowW(L"BUTTON", L"Quit",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0,0,0,0, hwnd, (HMENU)(UINT_PTR)IDC_BTN_QUIT, hInst, NULL);

    /* Status bar. */
    g_app.hStatus = CreateWindowExW(0, STATUSCLASSNAMEW, L"",
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0, hwnd, (HMENU)(UINT_PTR)IDC_STATUSBAR, hInst, NULL);

    /* Set font on all buttons. */
    {
        HWND btns[] = {
            g_app.hBtnAddUser, g_app.hBtnDelUser, g_app.hBtnUpUser, g_app.hBtnDownUser, g_app.hBtnRefreshU,
            g_app.hBtnAddSys, g_app.hBtnDelSys, g_app.hBtnUpSys, g_app.hBtnDownSys, g_app.hBtnRefreshS,
            g_app.hBtnSave, g_app.hBtnExport, g_app.hBtnDedup, g_app.hBtnRestore, g_app.hBtnQuit
        };
        int i;
        for (i = 0; i < (int)(sizeof(btns)/sizeof(btns[0])); i++)
            SendMessageW(btns[i], WM_SETFONT, (WPARAM)hFont, TRUE);
    }
}

void UI_Resize(void)
{
    RECT rc;
    int w, h, listW, y, btnY;
    int sectionH;

    GetClientRect(g_app.hMain, &rc);
    w = rc.right - rc.left;
    h = rc.bottom - rc.top;

    if (w < 300 || h < 200) return;

    listW = w - 2 * MARGIN;
    sectionH = (h - 3 * MARGIN - SEARCH_H - STATUS_H - 2 * BTN_H - 4 * MARGIN) / 2;
    if (sectionH < LIST_MIN_H) sectionH = LIST_MIN_H;

    y = MARGIN;

    /* Search box. */
    MoveWindow(g_app.hSearch, MARGIN, y, listW, SEARCH_H, TRUE);
    y += SEARCH_H + MARGIN;

    /* User PATH section. */
    MoveWindow(g_app.hListUser, MARGIN, y, listW, sectionH - BTN_H - 4, TRUE);
    btnY = y + sectionH - BTN_H - 2;
    {
        int bx = MARGIN;
        MoveWindow(g_app.hBtnAddUser,     bx, btnY, BTN_W, BTN_H, TRUE); bx += BTN_W + 4;
        MoveWindow(g_app.hBtnDelUser,     bx, btnY, BTN_W, BTN_H, TRUE); bx += BTN_W + 4;
        MoveWindow(g_app.hBtnUpUser,      bx, btnY, BTN_W, BTN_H, TRUE); bx += BTN_W + 4;
        MoveWindow(g_app.hBtnDownUser,    bx, btnY, BTN_W, BTN_H, TRUE); bx += BTN_W + 4;
        MoveWindow(g_app.hBtnRefreshU,    bx, btnY, BTN_W, BTN_H, TRUE);
    }
    y += sectionH + MARGIN;

    /* System PATH section. */
    MoveWindow(g_app.hListSys, MARGIN, y, listW, sectionH - BTN_H - 4, TRUE);
    btnY = y + sectionH - BTN_H - 2;
    {
        int bx = MARGIN;
        MoveWindow(g_app.hBtnAddSys,  bx, btnY, BTN_W, BTN_H, TRUE); bx += BTN_W + 4;
        MoveWindow(g_app.hBtnDelSys,  bx, btnY, BTN_W, BTN_H, TRUE); bx += BTN_W + 4;
        MoveWindow(g_app.hBtnUpSys,   bx, btnY, BTN_W, BTN_H, TRUE); bx += BTN_W + 4;
        MoveWindow(g_app.hBtnDownSys, bx, btnY, BTN_W, BTN_H, TRUE); bx += BTN_W + 4;
        MoveWindow(g_app.hBtnRefreshS, bx, btnY, BTN_W, BTN_H, TRUE);
    }
    y += sectionH + MARGIN;

    /* Bottom buttons. */
    btnY = y;
    {
        int bx = MARGIN;
        MoveWindow(g_app.hBtnSave,    bx, btnY, BTN_W + 20, BTN_H, TRUE); bx += BTN_W + 24;
        MoveWindow(g_app.hBtnExport,  bx, btnY, BTN_W, BTN_H, TRUE);       bx += BTN_W + 4;
        MoveWindow(g_app.hBtnDedup,   bx, btnY, BTN_W, BTN_H, TRUE);       bx += BTN_W + 4;
        MoveWindow(g_app.hBtnRestore, bx, btnY, BTN_W + 30, BTN_H, TRUE);  bx += BTN_W + 34;
        MoveWindow(g_app.hBtnQuit,    w - MARGIN - BTN_W, btnY, BTN_W, BTN_H, TRUE);
    }

    /* Resize status bar (auto-sized by the system, but force it). */
    SendMessageW(g_app.hStatus, WM_SIZE, 0, 0);

    /* Update column widths. */
    ListView_SetColumnWidth(g_app.hListUser, 0, listW - 4);
    ListView_SetColumnWidth(g_app.hListSys,  0, listW - 4);
}

/* ---- Main window procedure ---- */

static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE:
        UI_CreateControls(hwnd, g_app.hInst);
        DoRefresh();
        SaveSnapshot();
        return 0;

    case WM_SIZE:
        UI_Resize();
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_BTN_ADD_USER:   DoAdd(&g_userPath); break;
        case IDC_BTN_DEL_USER:   DoDelete(&g_userPath, g_app.hListUser); break;
        case IDC_BTN_UP_USER:    DoMoveUp(&g_userPath, g_app.hListUser); break;
        case IDC_BTN_DOWN_USER:  DoMoveDown(&g_userPath, g_app.hListUser); break;
        case IDC_BTN_REFRESH_U:  PathList_Validate(&g_userPath); UI_RefreshLists(); break;

        case IDC_BTN_ADD_SYS:    DoAdd(&g_sysPath); break;
        case IDC_BTN_DEL_SYS:    DoDelete(&g_sysPath, g_app.hListSys); break;
        case IDC_BTN_UP_SYS:     DoMoveUp(&g_sysPath, g_app.hListSys); break;
        case IDC_BTN_DOWN_SYS:   DoMoveDown(&g_sysPath, g_app.hListSys); break;
        case IDC_BTN_REFRESH_S:  PathList_Validate(&g_sysPath); UI_RefreshLists(); break;

        case IDC_BTN_SAVE:       DoSave(); break;
        case IDC_BTN_EXPORT:     DoExport(); break;
        case IDC_BTN_QUIT:       PostMessageW(hwnd, WM_CLOSE, 0, 0); break;
        case IDC_BTN_DEDUP:      DoDedup(); break;
        case IDC_BTN_RESTORE:    DoRestore(); break;

        case IDC_SEARCH:
            if (HIWORD(wp) == EN_CHANGE) {
                GetWindowTextW(g_app.hSearch, g_app.filter, 256);
                UI_RefreshLists();
            }
            break;
        }
        return 0;

    case WM_NOTIFY:
    {
        NMHDR *nm = (NMHDR *)lp;
        if (nm->code == NM_DBLCLK) {
            NMLISTVIEW *nmlv = (NMLISTVIEW *)lp;
            PathList *pl;
            HWND hList;
            if (nm->idFrom == IDC_LIST_USER) {
                pl = &g_userPath; hList = g_app.hListUser;
            } else if (nm->idFrom == IDC_LIST_SYSTEM) {
                pl = &g_sysPath; hList = g_app.hListSys;
            } else break;
            DoEdit(pl, hList, nmlv->iItem);
        }
        if (nm->code == LVN_BEGINDRAG) {
            NMLISTVIEW *nmlv = (NMLISTVIEW *)lp;
            LVITEMW lvi = {0};
            lvi.mask  = LVIF_PARAM;
            lvi.iItem = nmlv->iItem;
            ListView_GetItem(nm->hwndFrom, &lvi);
            g_dragging   = TRUE;
            g_dragList   = nm->hwndFrom;
            g_dragItem   = nmlv->iItem;
            g_dragRealIdx = (int)lvi.lParam;
            SetCapture(hwnd);
        }
        return 0;
    }

    case WM_MOUSEMOVE:
        if (g_dragging) {
            POINT pt;
            int hitItem;
            pt.x = (short)LOWORD(lp);
            pt.y = (short)HIWORD(lp);
            MapWindowPoints(hwnd, g_dragList, &pt, 1);
            hitItem = ListView_HitTest(g_dragList, &(LVHITTESTINFO){.pt = pt});
            if (hitItem >= 0 && hitItem != g_dragItem) {
                /* Visual feedback: highlight the drop target. */
                ListView_SetItemState(g_dragList, hitItem, LVIS_DROPHILITED, LVIS_DROPHILITED);
                ListView_SetItemState(g_dragList, g_dragItem, 0, LVIS_DROPHILITED);
            }
        }
        return 0;

    case WM_LBUTTONUP:
        if (g_dragging) {
            POINT pt;
            int hitItem;
            PathList *pl;

            ReleaseCapture();

            pt.x = (short)LOWORD(lp);
            pt.y = (short)HIWORD(lp);
            MapWindowPoints(hwnd, g_dragList, &pt, 1);
            hitItem = ListView_HitTest(g_dragList, &(LVHITTESTINFO){.pt = pt});

            pl = (g_dragList == g_app.hListUser) ? &g_userPath : &g_sysPath;

            if (hitItem >= 0 && hitItem != g_dragItem) {
                LVITEMW lvi = {0};
                lvi.mask  = LVIF_PARAM;
                lvi.iItem = hitItem;
                ListView_GetItem(g_dragList, &lvi);
                int targetRealIdx = (int)lvi.lParam;

                SaveUndoSnapshot();

                /* Move the entry: remove from old position, insert at new. */
                PathEntry tmp = pl->entries[g_dragRealIdx];
                /* Remove from old position. */
                int i;
                for (i = g_dragRealIdx; i < pl->count - 1; i++)
                    pl->entries[i] = pl->entries[i + 1];
                pl->count--;
                /* Insert at new position. */
                for (i = pl->count; i > targetRealIdx; i--)
                    pl->entries[i] = pl->entries[i - 1];
                pl->entries[targetRealIdx] = tmp;
                pl->count++;

                g_app.dirty = TRUE;
                UI_RefreshLists();
            }

            /* Clear drop highlight. */
            ListView_SetItemState(g_dragList, -1, 0, LVIS_DROPHILITED);

            g_dragging = FALSE;
            g_dragList = NULL;
            g_dragItem = -1;
            g_dragRealIdx = -1;
        }
        return 0;

    case WM_KEYDOWN:
        if (GetKeyState(VK_CONTROL) & 0x8000) {
            if (wp == L'S' || wp == L's') { DoSave(); return 0; }
            if (wp == L'Z' || wp == L'z') { RestoreUndoSnapshot(); return 0; }
            if (wp == L'F' || wp == L'f') { SetFocus(g_app.hSearch); return 0; }
        }
        return 0;

    case WM_CLOSE:
        if (g_app.dirty) {
            int rc = MessageBoxW(hwnd,
                L"You have unsaved changes. Save before quitting?",
                L"WinPath Editor", MB_YESNOCANCEL | MB_ICONQUESTION);
            if (rc == IDCANCEL) return 0;
            if (rc == IDYES) DoSave();
        }
        /* Save window position to registry. */
        {
            RECT rc;
            HKEY hKey;
            int vals[4];
            GetWindowRect(hwnd, &rc);
            vals[0] = rc.left; vals[1] = rc.top;
            vals[2] = rc.right - rc.left; vals[3] = rc.bottom - rc.top;
            if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\WinPathEditor", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
                RegSetValueExW(hKey, L"WindowPos", 0, REG_BINARY, (BYTE*)vals, sizeof(vals));
                RegCloseKey(hKey);
            }
        }
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

/* ---- WinMain ---- */

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrev, LPWSTR lpCmd, int nShow)
{
    WNDCLASSEXW wc = {0};
    MSG msg;
    HWND hwnd;
    HANDLE hMutex;

    (void)hPrev; (void)lpCmd;

    /* Single instance lock. */
    hMutex = CreateMutexW(NULL, TRUE, L"Global\\WinPathEditorSingleInstance");
    (void)hMutex;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        /* Find existing window and bring it to front. */
        HWND existing = FindWindowW(L"WinPathEditor", NULL);
        if (existing) {
            ShowWindow(existing, SW_RESTORE);
            SetForegroundWindow(existing);
        }
        return 0;
    }

    g_app.hInst = hInstance;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    UI_InitCommonControls();

    PathList_Init(&g_userPath, FALSE);
    PathList_Init(&g_sysPath, TRUE);

    /* Load saved window position from registry. */
    g_app.savedPos[0] = g_app.savedPos[1] = g_app.savedPos[2] = g_app.savedPos[3] = 0;
    {
        HKEY hKey;
        DWORD type, size;
        int vals[4];
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\WinPathEditor", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            size = sizeof(vals);
            if (RegQueryValueExW(hKey, L"WindowPos", NULL, &type, (BYTE*)vals, &size) == ERROR_SUCCESS && size == sizeof(vals)) {
                g_app.savedPos[0] = vals[0]; g_app.savedPos[1] = vals[1];
                g_app.savedPos[2] = vals[2]; g_app.savedPos[3] = vals[3];
            }
            RegCloseKey(hKey);
        }
    }

    /* Register window class. */
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = MainWndProc;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APPICON));
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"WinPathEditor";
    wc.hIconSm       = wc.hIcon;
    RegisterClassExW(&wc);

    /* Create main window. */
    hwnd = CreateWindowExW(0, L"WinPathEditor", L"WinPath Editor",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 720, 640,
        NULL, NULL, hInstance, NULL);
    g_app.hMain = hwnd;

    /* Restore saved window position if available. */
    if (g_app.savedPos[2] > 0 && g_app.savedPos[3] > 0) {
        SetWindowPos(hwnd, NULL, g_app.savedPos[0], g_app.savedPos[1],
                     g_app.savedPos[2], g_app.savedPos[3], SWP_NOZORDER);
    }

    ShowWindow(hwnd, nShow);
    UpdateWindow(hwnd);

    /* Message loop with accelerator-style handling. */
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        /* Let edit controls handle keyboard first. */
        if (msg.message == WM_KEYDOWN && (GetKeyState(VK_CONTROL) & 0x8000)) {
            if (msg.wParam == L'S' || msg.wParam == L's' ||
                msg.wParam == L'Z' || msg.wParam == L'z' ||
                msg.wParam == L'F' || msg.wParam == L'f') {
                SendMessageW(g_app.hMain, msg.message, msg.wParam, msg.lParam);
                continue;
            }
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}
