#ifndef PATH_H
#define PATH_H

#include <windows.h>

#define PATH_MAX_ENTRIES 256
#define PATH_MAX_LEN     2048

/* Path status flags (can be combined). */
#define PST_EXISTS      0x0001   /* directory exists */
#define PST_MISSING     0x0002   /* does not exist */
#define PST_NOT_DIR     0x0004   /* exists but is not a directory */
#define PST_HAS_ENVVAR  0x0008   /* contains %VAR% references */
#define PST_NETWORK     0x0010   /* UNC or network path */
#define PST_DUPLICATE   0x0020   /* duplicate of another entry */

typedef struct {
    wchar_t path[PATH_MAX_LEN];       /* original text (preserved on save) */
    wchar_t expanded[PATH_MAX_LEN];   /* expanded form (for validation) */
    DWORD   status;                    /* PST_* flags */
} PathEntry;

typedef struct {
    PathEntry entries[PATH_MAX_ENTRIES];
    int       count;
    BOOL      is_system;   /* TRUE = system PATH, FALSE = user PATH */
} PathList;

/* Initialize a PathList. */
void PathList_Init(PathList *pl, BOOL is_system);

/* Parse a semicolon-delimited PATH string into a PathList. */
void PathList_Parse(PathList *pl, const wchar_t *raw);

/* Re-check existence, expand env vars, detect status flags. */
void PathList_Validate(PathList *pl);

/* Serialize PathList back to semicolon-delimited string (uses original paths). Caller must free. */
wchar_t *PathList_Join(const PathList *pl);

/* Add a path at the end. Returns TRUE on success. */
BOOL PathList_Add(PathList *pl, const wchar_t *path);

/* Remove entry at index. */
BOOL PathList_Remove(PathList *pl, int index);

/* Swap entries. */
BOOL PathList_MoveUp(PathList *pl, int index);
BOOL PathList_MoveDown(PathList *pl, int index);

/* Replace entry at index with new text. */
BOOL PathList_Set(PathList *pl, int index, const wchar_t *path);

/* Mark duplicate entries (normalized comparison). Returns count of duplicates found. */
int PathList_MarkDuplicates(PathList *pl);

/* Remove entries marked as duplicate. Returns count removed. */
int PathList_RemoveDuplicates(PathList *pl);

/* Compute diff between old and new PathLists. Returns a heap-allocated string. Caller must free. */
wchar_t *PathList_Diff(const PathList *old_pl, const PathList *new_pl, const wchar_t *label);

/* Backup current PATH lists to a timestamped file in backup/ dir. Returns filename or NULL. */
wchar_t *Backup_Save(const PathList *user, const PathList *sys);

/* Find the most recent backup file. Returns filename or NULL (caller must free). */
wchar_t *Backup_FindLatest(void);

/* Load PathLists from a backup file. Returns TRUE on success. */
BOOL Backup_Load(const wchar_t *filepath, PathList *user, PathList *sys);

#endif /* PATH_H */
