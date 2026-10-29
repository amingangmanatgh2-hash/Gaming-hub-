/* -------------------------------------------------------------------------
 * AMIN Keyboard & Clipboard - shared declarations
 *
 * Pure Win32 / C, no .NET, no CRT-specific extras beyond the Windows headers.
 * Builds as a single self-contained .exe that runs on Windows 7 -> Windows 11
 * with no runtime to install.
 *
 * Deliberately written in C89-friendly style (declarations at the top of a
 * block, no compound literals) so the same sources compile with both MSVC
 * (used by CI) and clang/mingw (used for the offline cross-compile check).
 * ------------------------------------------------------------------------- */

#ifndef AMIN_APP_H
#define AMIN_APP_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601 /* Windows 7 baseline; everything used here is Vista+ */
#endif

#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>
#include <windowsx.h>  /* GET_X_LPARAM / GET_Y_LPARAM */
#include <shlwapi.h>   /* wnsprintfW */
#include <shellapi.h>

/* Not present in all SDK/header combinations (it is a Windows Vista+ edit
 * message, delivered through the normal message table). */
#ifndef EM_SETCUEBANNER
#define EM_SETCUEBANNER 0x1501
#endif

#define APP_TITLE          L"AMIN Keyboard & Clipboard"
#define APP_SHORT          L"AMIN Keyboard"
#define APP_VERSION_STR    L"1.1.0"
#define APP_VERSION_MAJOR  1
#define APP_VERSION_MINOR  1
#define APP_VERSION_PATCH  0

#define APP_CLASS_MAIN     L"AMINMainWindowClass"
#define APP_CLASS_KEYBOARD L"AMINKeyboardWindowClass"
#define APP_MUTEX_NAME     L"Local\\AMINKeyboardClipboard.SingleInstance"

/* Private messages */
#define APP_WM_SHOW        (WM_APP + 1)  /* second instance asks us to show  */
#define APP_WM_TRAY        (WM_APP + 2)  /* tray icon callback              */

#define HOTKEY_ID          7001
#define TRAY_ICON_ID       1

#define HISTORY_MAX        100
#define HISTORY_MAX_CHARS  20000

/* ---------------- settings.c ---------------- */

void  Settings_DataFolder(wchar_t *out, int cch);
void  Settings_FileInDataFolder(const wchar_t *name, wchar_t *out, int cch);

BOOL  Settings_FirstRunDone(void);
void  Settings_SetFirstRunDone(void);

BOOL  Settings_StartupEnabled(void);
BOOL  Settings_SetStartupEnabled(BOOL enable);

void  Settings_SaveKeyboardPos(int x, int y);
BOOL  Settings_LoadKeyboardPos(int *x, int *y);

/* ---------------- clipboard.c --------------- */

void  Clip_Load(void);
void  Clip_Save(void);

int   Clip_Count(void);
const wchar_t *Clip_Get(int index);

BOOL  Clip_Add(const wchar_t *text);      /* TRUE when the history changed */
int   Clip_RemoveAt(int index);
void  Clip_Clear(void);

/* Reads CF_UNICODETEXT from the clipboard. Returns a heap string or NULL. */
wchar_t *Clip_ReadClipboard(HWND owner);
void  Clip_FreeText(wchar_t *text);

/* Puts text on the clipboard and remembers it so it is not re-added. */
BOOL  Clip_PutClipboard(HWND owner, const wchar_t *text);

/* Remembers text that is already on the clipboard (de-duplication). */
void  Clip_NoteText(const wchar_t *text);
BOOL  Clip_IsSameAsLast(const wchar_t *text);

/* ---------------- keyboard.c ---------------- */

BOOL Keyboard_RegisterClass(HINSTANCE instance);
void Keyboard_Show(HINSTANCE instance);
void Keyboard_Hide(void);
void Keyboard_Toggle(HINSTANCE instance);
BOOL Keyboard_IsVisible(void);
void Keyboard_Destroy(void);

/* Last non-owned foreground window, the typing target. */
HWND Keyboard_TargetWindow(void);

/* ---------------- main.c -------------------- */

void Main_ShowWindow(void);          /* restore + foreground, safe to call late */
void Main_RefreshHistory(void);      /* re-read the model into the list box      */
HWND Main_GetHandle(void);
void Main_UpdateLanguageLabel(BOOL persian);

#endif /* AMIN_APP_H */
