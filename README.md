# WinPath Editor

**[English](#english) | [中文](#中文)**

---

# English

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

---

# 中文

Windows 专用的 PATH 环境变量编辑器。纯 C + Win32 API，体积 **不到 100KB**。

## 为什么做这个

Windows 自带的 PATH 编辑器是一个不能调整大小的文本框，没有搜索、没有验证、没有拖拽、没有撤销。编辑 PATH 既容易出错又让人紧张——一不小心就可能搞坏开发环境。

WinPath Editor 解决了这个问题。它小、专一、而且**安全**：自动备份、保存前 diff 预览、外部变更检测。

## 功能

| 类别 | 功能 |
|------|------|
| **显示** | 用户 PATH 和系统 PATH 双列表视图 |
| **状态** | `[OK]` 存在、`[X]` 不存在、`[!]` 不是目录、`[%]` 含环境变量、`[~]` 网络路径、`[D]` 重复 |
| **编辑** | 添加（文件夹选择器）、删除、上移/下移、拖拽排序、双击内联编辑 |
| **搜索** | 实时过滤，Ctrl+F 聚焦搜索框 |
| **安全** | 每次保存前自动备份 |
| **安全** | 保存前 diff 预览——确认前能看到改了什么 |
| **安全** | 外部变更检测——其他程序改了 PATH 会警告 |
| **安全** | 一键从备份恢复 |
| **安全** | 单实例锁——防止两个窗口互相覆盖 |
| **验证** | 展开 `%VAR%` 后检测路径是否存在 |
| **验证** | 规范化去重（大小写不敏感、尾斜杠、引号） |
| **其他** | 导出为文本文件、窗口位置记忆、Ctrl+Z 撤销 |

## 下载

从 [Releases](../../releases) 下载 `winpath.exe`。

每个 Release 都附带 SHA256。

## 从源码构建

需要 **MSVC**（开发者命令提示符）或 **MinGW-w64**（gcc 在 PATH 中）。

```bat
build.bat          # 自动检测编译器
build.bat msvc     # 强制使用 MSVC（推荐，完整 ASLR 支持）
build.bat mingw    # 强制使用 MinGW
```

输出：`build\winpath.exe`

### ASLR 说明

构建设置了 `DYNAMIC_BASE`、`NX_COMPAT` 和 `HIGH_ENTROPY_VA`。推荐使用 MSVC 构建以获得完整的重定位/ASLR 支持。旧版 MinGW 的 ld 不会为可执行文件生成 `.reloc`。

## 使用方法

1. 运行 `winpath.exe`
2. 编辑用户 PATH 和/或系统 PATH
3. 点击 **Save (Ctrl+S)**——会显示 diff 预览
4. 确认保存——自动创建备份
5. 新打开的终端会立即使用更新后的 PATH

### 权限说明

- **用户 PATH**：无需管理员权限即可编辑
- **系统 PATH**：需要提权（应用会提示以管理员身份重新启动）

### 备份位置

```
%LOCALAPPDATA%\WinPathEditor\backup\path-backup-YYYYMMDD-HHMMSS.txt
```

## 快捷键

| 按键 | 操作 |
|------|------|
| Ctrl+S | 保存（带 diff 预览） |
| Ctrl+Z | 撤销 |
| Ctrl+F | 聚焦搜索框 |
| Delete | 删除选中项 |
| Insert | 添加文件夹 |
| 双击 | 内联编辑 |

## 注册表键位

| PATH 类型 | 注册表键 |
|-----------|----------|
| 用户 | `HKCU\Environment` → `Path` |
| 系统 | `HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Environment` → `Path` |

## 局限性

- 这是一个**早期 beta 版本**，请谨慎使用。
- 保存前务必检查 diff 预览。
- 应用会自动备份，但首次使用前建议手动备份 PATH。

## 许可证

Apache 2.0
