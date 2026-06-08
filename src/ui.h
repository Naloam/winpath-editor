#ifndef UI_H
#define UI_H

#include <windows.h>
#include "path.h"

/* Forward declarations for the two global PathLists. */
extern PathList g_userPath;
extern PathList g_sysPath;

/* Application-wide state. */
typedef struct {
    HWND hMain;
    HWND hListUser;
    HWND hListSys;
    HWND hSearch;
    HWND hStatus;
    HWND hBtnAddUser, hBtnDelUser, hBtnUpUser, hBtnDownUser, hBtnRefreshU;
    HWND hBtnAddSys,  hBtnDelSys,  hBtnUpSys,  hBtnDownSys,  hBtnRefreshS;
    HWND hBtnSave, hBtnExport, hBtnQuit, hBtnDedup, hBtnRestore;
    HINSTANCE hInst;
    BOOL dirty;            /* TRUE if unsaved changes exist */
    wchar_t filter[256];   /* current search filter */
    int savedPos[4];       /* saved window position: x, y, w, h */
} AppState;

extern AppState g_app;

/* Create all child controls. Called from WM_CREATE. */
void UI_CreateControls(HWND hwnd, HINSTANCE hInst);

/* Resize controls to fit the window. Called from WM_SIZE. */
void UI_Resize(void);

/* Refresh both ListViews from their PathLists. */
void UI_RefreshLists(void);

/* Update the status bar text. */
void UI_UpdateStatus(void);

/* Initialize common controls (InitCommonControlsEx). */
void UI_InitCommonControls(void);

#endif /* UI_H */
