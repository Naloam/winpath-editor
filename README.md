# WinPath Editor

[中文版 README](README_CN.md)

A dedicated PATH environment variable editor for Windows. Pure C + Win32 API, under **100KB**.

## Why

Windows' built-in PATH editor is a non-resizable text box with no search, no validation, no drag-and-drop, and no undo. Editing PATH is error-prone and stressful — one wrong move can break your development environment.

WinPath Editor fixes that. It's small, focused, and **safe**: automatic backups, diff preview before saving, and external change detection.

## Features

| Category | Feature |
|----------|---------|
| **Display** | Dual view for User PATH and System PATH |
| **Status** | `[OK]` exists, `[X]` missing, `[!]` not a directory, `[%]` env var, `[~]` network, `[D]` duplicate |
| **Edit** | Add (folder picker), delete, move up/down, drag-and-drop, inline edit (double-click) |
| **Search** | Real-time filter, Ctrl+F to focus |
| **Safety** | Auto-backup before every save |
| **Safety** | Diff preview — see exactly what changed before confirming |
| **Safety** | External change detection — warns if another program modified PATH |
| **Safety** | One-click restore from backup |
| **Safety** | Single instance lock — prevents conflicting edits |
| **Validation** | Expands `%VAR%` before checking existence |
| **Validation** | Normalized dedup (case-insensitive, trailing slash, quotes) |
| **Other** | Export to text file, window position memory, Ctrl+Z undo |

## Download

Download `winpath.exe` from [Releases](../../releases).

SHA256 is listed in each release.

## Build from Source

Requires **MSVC** (Developer Command Prompt) or **MinGW-w64** (gcc in PATH).

```bat
build.bat          # auto-detects compiler
build.bat msvc     # force MSVC (recommended, full ASLR support)
build.bat mingw    # force MinGW
```

Output: `build\winpath.exe`

### ASLR Note

The build sets `DYNAMIC_BASE`, `NX_COMPAT`, and `HIGH_ENTROPY_VA`. MSVC build is recommended for full relocation/ASLR support. Older MinGW ld versions don't generate `.reloc` for executables.

## Usage

1. Run `winpath.exe`
2. Edit User PATH and/or System PATH
3. Click **Save (Ctrl+S)** — a diff preview will show what changed
4. Confirm to save — a backup is created automatically
5. New terminals pick up the changes immediately

### Permissions

- **User PATH**: editable without admin rights
- **System PATH**: requires elevation (the app will prompt to re-launch as admin)

### Backup Location

```
%LOCALAPPDATA%\WinPathEditor\backup\path-backup-YYYYMMDD-HHMMSS.txt
```

## Keyboard Shortcuts

| Key | Action |
|-----|--------|
| Ctrl+S | Save (with diff preview) |
| Ctrl+Z | Undo |
| Ctrl+F | Focus search box |
| Delete | Remove selected entry |
| Insert | Add folder |
| Double-click | Inline edit |

## Registry Keys

| PATH Type | Registry Key |
|-----------|-------------|
| User | `HKCU\Environment` → `Path` |
| System | `HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Environment` → `Path` |

## Limitations

- This is an **early beta**. Use with care.
- Always review the diff preview before saving.
- The app creates automatic backups, but you should also back up your PATH manually before first use.

## License

Apache 2.0
