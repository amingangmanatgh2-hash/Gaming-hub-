/* -------------------------------------------------------------------------
 * clipboard.c - text-only clipboard history.
 *
 *  - Only CF_UNICODETEXT is ever read. Images, files and custom formats are
 *    ignored, so nothing binary is captured.
 *  - Newest first, de-duplicated, capped at HISTORY_MAX entries and
 *    HISTORY_MAX_CHARS characters per entry.
 *  - Stored in a small binary file in the current user's LocalAppData folder.
 *    It is written to a .tmp file first and then swapped in, so an interrupted
 *    write cannot truncate the history.
 * ------------------------------------------------------------------------- */

#include "app.h"

#define HISTORY_MAGIC 0x424E4941u /* 'AINB' */
#define HISTORY_VER   1u

static wchar_t *g_items[HISTORY_MAX];
static int g_count;

static wchar_t *g_lastText; /* last text we saw on the clipboard */

/* Clips a string to HISTORY_MAX_CHARS. Returns a new allocation. */
static wchar_t *DupClipped(const wchar_t *text)
{
    size_t len;
    wchar_t *copy;

    if (text == NULL) return NULL;

    len = wcslen(text);
    if (len > HISTORY_MAX_CHARS) len = HISTORY_MAX_CHARS;

    copy = (wchar_t *)malloc((len + 1) * sizeof(wchar_t));
    if (copy == NULL) return NULL;

    memcpy(copy, text, len * sizeof(wchar_t));
    copy[len] = L'\0';
    return copy;
}

int Clip_Count(void)
{
    return g_count;
}

const wchar_t *Clip_Get(int index)
{
    if (index < 0 || index >= g_count) return NULL;
    return g_items[index];
}

/* ------------------------------------------------------------------ */
/* Persistence                                                        */
/* ------------------------------------------------------------------ */

static void HistoryFilePath(wchar_t *out, int cch)
{
    Settings_FileInDataFolder(L"clipboard-history.dat", out, cch);
}

void Clip_Load(void)
{
    wchar_t path[MAX_PATH * 2];
    HANDLE h;
    DWORD magic;
    DWORD version;
    DWORD count;
    DWORD read;
    DWORD i;

    HistoryFilePath(path, MAX_PATH * 2);

    h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;

    if (!ReadFile(h, &magic, sizeof(magic), &read, NULL) || read != sizeof(magic) ||
        magic != HISTORY_MAGIC)
    {
        CloseHandle(h);
        return;
    }

    if (!ReadFile(h, &version, sizeof(version), &read, NULL) || read != sizeof(version) ||
        version != HISTORY_VER)
    {
        CloseHandle(h);
        return;
    }

    if (!ReadFile(h, &count, sizeof(count), &read, NULL) || read != sizeof(count))
    {
        CloseHandle(h);
        return;
    }

    if (count > HISTORY_MAX) count = HISTORY_MAX;

    for (i = 0; i < count; i++)
    {
        DWORD chars;
        wchar_t *item;

        if (!ReadFile(h, &chars, sizeof(chars), &read, NULL) || read != sizeof(chars))
            break;

        if (chars == 0 || chars > HISTORY_MAX_CHARS)
            break;

        item = (wchar_t *)malloc((chars + 1) * sizeof(wchar_t));
        if (item == NULL)
            break;

        if (!ReadFile(h, item, chars * sizeof(wchar_t), &read, NULL) ||
            read != chars * sizeof(wchar_t))
        {
            free(item);
            break;
        }

        item[chars] = L'\0';
        g_items[g_count++] = item;
    }

    CloseHandle(h);
}

void Clip_Save(void)
{
    wchar_t path[MAX_PATH * 2];
    wchar_t temp[MAX_PATH * 2 + 8];
    HANDLE h;
    DWORD written;
    DWORD magic;
    DWORD version;
    DWORD count;
    DWORD i;

    HistoryFilePath(path, MAX_PATH * 2);
    wnsprintfW(temp, MAX_PATH * 2 + 8, L"%s.tmp", path);

    h = CreateFileW(temp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;

    magic = HISTORY_MAGIC;
    version = HISTORY_VER;
    count = (DWORD)g_count;

    WriteFile(h, &magic, sizeof(magic), &written, NULL);
    WriteFile(h, &version, sizeof(version), &written, NULL);
    WriteFile(h, &count, sizeof(count), &written, NULL);

    for (i = 0; i < (DWORD)g_count; i++)
    {
        DWORD chars = (DWORD)wcslen(g_items[i]);

        WriteFile(h, &chars, sizeof(chars), &written, NULL);
        WriteFile(h, g_items[i], chars * sizeof(wchar_t), &written, NULL);
    }

    CloseHandle(h);

    MoveFileExW(temp, path, MOVEFILE_REPLACE_EXISTING);
}

/* ------------------------------------------------------------------ */
/* Model operations                                                   */
/* ------------------------------------------------------------------ */

static void FreeAll(void)
{
    int i;

    for (i = 0; i < g_count; i++)
    {
        free(g_items[i]);
        g_items[i] = NULL;
    }

    g_count = 0;
}

BOOL Clip_Add(const wchar_t *text)
{
    wchar_t *copy;
    wchar_t *existing;
    int i;

    if (text == NULL) return FALSE;

    /* Reject whitespace-only entries. */
    {
        const wchar_t *p = text;
        BOOL hasContent = FALSE;

        while (*p)
        {
            if (*p != L' ' && *p != L'\t' && *p != L'\r' && *p != L'\n')
            {
                hasContent = TRUE;
                break;
            }
            p++;
        }

        if (!hasContent) return FALSE;
    }

    copy = DupClipped(text);
    if (copy == NULL) return FALSE;

    /* Duplicate removal: an exact match moves to the top instead of repeating. */
    existing = NULL;
    for (i = 0; i < g_count; i++)
    {
        if (wcscmp(g_items[i], copy) == 0)
        {
            existing = g_items[i];
            break;
        }
    }

    if (existing != NULL)
    {
        int j;

        free(existing);
        for (j = i; j < g_count - 1; j++)
            g_items[j] = g_items[j + 1];

        g_count--;
    }

    /* Insert at the front. */
    if (g_count == HISTORY_MAX)
    {
        free(g_items[HISTORY_MAX - 1]);
        g_count--;
    }
    else if (g_count > HISTORY_MAX)
    {
        while (g_count >= HISTORY_MAX)
        {
            free(g_items[g_count - 1]);
            g_count--;
        }
    }

    {
        int k;
        for (k = g_count; k > 0; k--)
            g_items[k] = g_items[k - 1];
    }

    g_items[0] = copy;
    g_count++;

    Clip_Save();
    return TRUE;
}

int Clip_RemoveAt(int index)
{
    int i;

    if (index < 0 || index >= g_count) return -1;

    free(g_items[index]);
    for (i = index; i < g_count - 1; i++)
        g_items[i] = g_items[i + 1];

    g_items[g_count - 1] = NULL;
    g_count--;

    Clip_Save();
    return 0;
}

void Clip_Clear(void)
{
    if (g_count == 0) return;

    FreeAll();
    Clip_Save();
}

/* ------------------------------------------------------------------ */
/* Clipboard interop                                                  */
/* ------------------------------------------------------------------ */

wchar_t *Clip_ReadClipboard(HWND owner)
{
    int attempt;
    wchar_t *result = NULL;

    for (attempt = 0; attempt < 5 && result == NULL; attempt++)
    {
        HANDLE handle;
        const wchar_t *data;

        if (!IsClipboardFormatAvailable(CF_UNICODETEXT))
            return NULL;

        if (!OpenClipboard(owner))
        {
            Sleep(30);
            continue;
        }

        handle = GetClipboardData(CF_UNICODETEXT);
        if (handle != NULL)
        {
            data = (const wchar_t *)GlobalLock(handle);
            if (data != NULL)
            {
                result = DupClipped(data);
                GlobalUnlock(handle);
            }
        }

        CloseClipboard();

        if (result == NULL) Sleep(30);
    }

    return result;
}

void Clip_FreeText(wchar_t *text)
{
    if (text != NULL) free(text);
}

BOOL Clip_PutClipboard(HWND owner, const wchar_t *text)
{
    int attempt;
    size_t bytes;
    HGLOBAL mem;
    void *dest;
    BOOL ok;

    if (text == NULL) return FALSE;

    ok = FALSE;
    bytes = (wcslen(text) + 1) * sizeof(wchar_t);

    for (attempt = 0; attempt < 5 && !ok; attempt++)
    {
        mem = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (mem == NULL) return FALSE;

        dest = GlobalLock(mem);
        if (dest == NULL)
        {
            GlobalFree(mem);
            return FALSE;
        }

        memcpy(dest, text, bytes);
        GlobalUnlock(mem);

        if (OpenClipboard(owner))
        {
            EmptyClipboard();
            if (SetClipboardData(CF_UNICODETEXT, mem) != NULL)
            {
                ok = TRUE;
                mem = NULL; /* ownership passed to the clipboard */
            }
            CloseClipboard();
        }

        if (mem != NULL) GlobalFree(mem);
        if (!ok) Sleep(30);
    }

    if (ok) Clip_NoteText(text);
    return ok;
}

void Clip_NoteText(const wchar_t *text)
{
    if (g_lastText != NULL)
    {
        free(g_lastText);
        g_lastText = NULL;
    }

    if (text != NULL)
        g_lastText = DupClipped(text);
}

BOOL Clip_IsSameAsLast(const wchar_t *text)
{
    if (g_lastText == NULL || text == NULL) return FALSE;
    return (wcscmp(g_lastText, text) == 0);
}
