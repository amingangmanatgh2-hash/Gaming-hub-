/* -------------------------------------------------------------------------
 * settings.c - per-user settings, no registry parsing beyond HKCU\Run.
 * ------------------------------------------------------------------------- */

#include "app.h"

#define RUN_KEY_PATH L"Software\\Microsoft\\Windows\\CurrentVersion\\Run"
#define RUN_VALUE_NAME L"AMIN Keyboard Clipboard"

void Settings_DataFolder(wchar_t *out, int cch)
{
    wchar_t base[MAX_PATH * 2];
    DWORD n;

    if (out == NULL || cch <= 0) return;
    out[0] = L'\0';

    base[0] = L'\0';
    n = GetEnvironmentVariableW(L"LOCALAPPDATA", base, (DWORD)(sizeof(base) / sizeof(base[0])));

    if (n == 0 || n >= sizeof(base) / sizeof(base[0]))
    {
        /* Very old or unusual profiles: fall back to the user profile folder. */
        base[0] = L'\0';
        n = GetEnvironmentVariableW(L"USERPROFILE", base, (DWORD)(sizeof(base) / sizeof(base[0])));
        if (n == 0) return;
    }

    wnsprintfW(out, cch, L"%s\\AMINKeyboardClipboard", base);
    CreateDirectoryW(out, NULL); /* already exists -> harmless */
}

void Settings_FileInDataFolder(const wchar_t *name, wchar_t *out, int cch)
{
    wchar_t folder[MAX_PATH * 2];

    Settings_DataFolder(folder, MAX_PATH * 2);
    wnsprintfW(out, cch, L"%s\\%s", folder, name);
}

/* ------------------------------------------------------------------ */
/* First run                                                          */
/* ------------------------------------------------------------------ */

BOOL Settings_FirstRunDone(void)
{
    wchar_t path[MAX_PATH * 2];
    HANDLE h;

    Settings_FileInDataFolder(L"first-run.done", path, MAX_PATH * 2);

    h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return FALSE;

    CloseHandle(h);
    return TRUE;
}

void Settings_SetFirstRunDone(void)
{
    wchar_t path[MAX_PATH * 2];
    HANDLE h;
    DWORD written;

    Settings_FileInDataFolder(L"first-run.done", path, MAX_PATH * 2);

    h = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                    CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;

    WriteFile(h, "done", 4, &written, NULL);
    CloseHandle(h);
}

/* ------------------------------------------------------------------ */
/* Autostart (HKCU only, never needs administrator rights)            */
/* ------------------------------------------------------------------ */

static void CurrentExePath(wchar_t *out, int cch)
{
    DWORD n;

    n = GetModuleFileNameW(NULL, out, (DWORD)cch);
    if (n == 0 || n >= (DWORD)cch) out[0] = L'\0';
}

BOOL Settings_StartupEnabled(void)
{
    HKEY key;
    wchar_t value[MAX_PATH * 2];
    wchar_t exe[MAX_PATH * 2];
    DWORD type;
    DWORD size;
    BOOL same;

    if (RegOpenKeyExW(HKEY_CURRENT_USER, RUN_KEY_PATH, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
        return FALSE;

    size = sizeof(value);
    type = 0;
    value[0] = L'\0';

    if (RegQueryValueExW(key, RUN_VALUE_NAME, NULL, &type, (LPBYTE)value, &size) != ERROR_SUCCESS ||
        type != REG_SZ)
    {
        RegCloseKey(key);
        return FALSE;
    }

    RegCloseKey(key);

    /* Strip surrounding quotes before comparing. */
    {
        wchar_t *p = value;
        size_t len = wcslen(value);

        if (len >= 2 && p[0] == L'"' && p[len - 1] == L'"')
        {
            p[len - 1] = L'\0';
            p++;
        }

        CurrentExePath(exe, MAX_PATH * 2);
        same = (lstrcmpiW(p, exe) == 0);
    }

    return same;
}

BOOL Settings_SetStartupEnabled(BOOL enable)
{
    HKEY key;
    LONG rc;

    rc = RegCreateKeyExW(HKEY_CURRENT_USER, RUN_KEY_PATH, 0, NULL, 0,
                         KEY_SET_VALUE, NULL, &key, NULL);
    if (rc != ERROR_SUCCESS) return FALSE;

    if (enable)
    {
        wchar_t exe[MAX_PATH * 2];
        wchar_t quoted[MAX_PATH * 2 + 4];

        CurrentExePath(exe, MAX_PATH * 2);
        wnsprintfW(quoted, MAX_PATH * 2 + 4, L"\"%s\"", exe);

        rc = RegSetValueExW(key, RUN_VALUE_NAME, 0, REG_SZ,
                            (const BYTE *)quoted,
                            (DWORD)((wcslen(quoted) + 1) * sizeof(wchar_t)));
    }
    else
    {
        rc = RegDeleteValueW(key, RUN_VALUE_NAME);
        if (rc == ERROR_FILE_NOT_FOUND) rc = ERROR_SUCCESS;
    }

    RegCloseKey(key);
    return (rc == ERROR_SUCCESS);
}

/* ------------------------------------------------------------------ */
/* Floating keyboard position (stored in settings.ini next to the data) */
/* ------------------------------------------------------------------ */

void Settings_SaveKeyboardPos(int x, int y)
{
    wchar_t path[MAX_PATH * 2];
    wchar_t buf[32];

    Settings_FileInDataFolder(L"settings.ini", path, MAX_PATH * 2);

    wnsprintfW(buf, 32, L"%d", x);
    WritePrivateProfileStringW(L"keyboard", L"x", buf, path);

    wnsprintfW(buf, 32, L"%d", y);
    WritePrivateProfileStringW(L"keyboard", L"y", buf, path);
}

BOOL Settings_LoadKeyboardPos(int *x, int *y)
{
    wchar_t path[MAX_PATH * 2];
    int px;
    int py;

    if (x == NULL || y == NULL) return FALSE;

    Settings_FileInDataFolder(L"settings.ini", path, MAX_PATH * 2);

    px = GetPrivateProfileIntW(L"keyboard", L"x", -32768, path);
    py = GetPrivateProfileIntW(L"keyboard", L"y", -32768, path);

    if (px == -32768 || py == -32768) return FALSE;

    *x = px;
    *y = py;
    return TRUE;
}
