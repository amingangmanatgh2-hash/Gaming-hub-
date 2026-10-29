/* -------------------------------------------------------------------------
 * main.c - main window, tray icon, global hot key and clipboard listener.
 *
 * Layered so that a missing feature never takes the whole app down:
 *   - the hot key falls back from Win+V to Ctrl+Alt+V and reports which one won;
 *   - the clipboard listener reports its state in the Settings page;
 *   - closing the window hides it to the tray so the hot key keeps working.
 * ------------------------------------------------------------------------- */

#include "app.h"

#define IDC_TAB_KEYBOARD   100
#define IDC_TAB_HISTORY    101
#define IDC_TAB_SETTINGS   102

#define IDC_SHOW_KEYBOARD  110
#define IDC_HIDE_KEYBOARD  111
#define IDC_KB_STATUS      112
#define IDC_KB_HELP        113

#define IDC_SEARCH         120
#define IDC_LIST           121
#define IDC_COPY           122
#define IDC_DELETE         123
#define IDC_CLEAR          124
#define IDC_COUNT          125

#define IDC_STARTUP        130
#define IDC_DATA_PATH      131
#define IDC_OPEN_FOLDER    132
#define IDC_RESET_DATA     133
#define IDC_HOTKEY_STATUS  134
#define IDC_LISTENER       135
#define IDC_PRIVACY        136
#define IDC_VERSION        137
#define IDC_EXIT           138

#define IDM_TRAY_SHOW      200
#define IDM_TRAY_KEYBOARD  201
#define IDM_TRAY_EXIT      202

#define PREVIEW_CHARS      220

static HINSTANCE g_inst;
static HWND      g_hwnd;
static HFONT     g_font;
static HICON     g_icon;
static HBRUSH    g_windowBrush;
static BOOL      g_trayAdded;
static BOOL      g_exiting;
static BOOL      g_listening;
static UINT      g_showMessage;
static int       g_page;
static int       g_hotkeyMods;
static UINT      g_hotkeyVk;

/* Controls; created once and shown/hidden per page. */
static HWND hTabKeyboard;
static HWND hTabHistory;
static HWND hTabSettings;

static HWND hShowKeyboard;
static HWND hHideKeyboard;
static HWND hKbStatus;
static HWND hKbHelp;

static HWND hSearch;
static HWND hList;
static HWND hCopy;
static HWND hDelete;
static HWND hClear;
static HWND hCount;

static HWND hStartup;
static HWND hDataPath;
static HWND hOpenFolder;
static HWND hResetData;
static HWND hHotkeyStatus;
static HWND hListener;
static HWND hPrivacy;
static HWND hVersion;
static HWND hExit;

/* Number of rows currently visible in the list box (after filtering). */
static int g_rowCount;

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

static HWND MakeChild(const wchar_t *cls, const wchar_t *text, DWORD style,
                      int id, DWORD exStyle)
{
    HWND h = CreateWindowExW(exStyle, cls, text,
                             WS_CHILD | style,
                             0, 0, 10, 10,
                             g_hwnd, (HMENU)(INT_PTR)id, g_inst, NULL);

    if (h != NULL)
        SendMessageW(h, WM_SETFONT, (WPARAM)g_font, TRUE);

    return h;
}

static void SetText(HWND control, const wchar_t *text)
{
    if (control != NULL) SetWindowTextW(control, text);
}

static void ShowControl(HWND control, BOOL visible)
{
    if (control != NULL)
        ShowWindow(control, visible ? SW_SHOW : SW_HIDE);
}

static void DataFolderText(wchar_t *out, int cch)
{
    Settings_DataFolder(out, cch);
}

/* ------------------------------------------------------------------ */
/* Layout                                                             */
/* ------------------------------------------------------------------ */

static void Layout(int cx, int cy)
{
    const int margin = 14;
    const int tabH = 34;
    const int rowH = 34;
    const int gap = 8;
    int top;
    int width;
    BOOL kbPage = (g_page == 0);
    BOOL histPage = (g_page == 1);
    BOOL setPage = (g_page == 2);

    width = cx - margin * 2;

    /* Tab strip */
    MoveWindow(hTabKeyboard, margin, margin, 150, tabH, TRUE);
    MoveWindow(hTabHistory, margin + 150 + 6, margin, 170, tabH, TRUE);
    MoveWindow(hTabSettings, margin + 150 + 170 + 12, margin, 180, tabH, TRUE);

    top = margin + tabH + 16;

    /* ---- page 0: keyboard ---- */
    ShowControl(hShowKeyboard, kbPage);
    ShowControl(hHideKeyboard, kbPage);
    ShowControl(hKbStatus, kbPage);
    ShowControl(hKbHelp, kbPage);

    if (kbPage)
    {
        MoveWindow(hShowKeyboard, margin, top, 190, rowH, TRUE);
        MoveWindow(hHideKeyboard, margin + 190 + gap, top, 160, rowH, TRUE);
        MoveWindow(hKbStatus, margin, top + rowH + gap, width, rowH, TRUE);
        MoveWindow(hKbHelp, margin, top + rowH * 2 + gap * 2,
                   width, cy - (top + rowH * 2 + gap * 2) - margin, TRUE);
    }

    /* ---- page 1: history ---- */
    ShowControl(hSearch, histPage);
    ShowControl(hList, histPage);
    ShowControl(hCopy, histPage);
    ShowControl(hDelete, histPage);
    ShowControl(hClear, histPage);
    ShowControl(hCount, histPage);

    if (histPage)
    {
        MoveWindow(hSearch, margin, top, width - 120 - gap, rowH, TRUE);
        MoveWindow(hClear, margin + width - 120, top, 120, rowH, TRUE);

        MoveWindow(hCount, margin, top + rowH + 4, width, 22, TRUE);

        MoveWindow(hList, margin, top + rowH + 30,
                   width, cy - (top + rowH + 30) - rowH - gap - margin, TRUE);

        MoveWindow(hCopy, margin, cy - margin - rowH, 190, rowH, TRUE);
        MoveWindow(hDelete, margin + 190 + gap, cy - margin - rowH, 180, rowH, TRUE);
    }

    /* ---- page 2: settings ---- */
    ShowControl(hStartup, setPage);
    ShowControl(hDataPath, setPage);
    ShowControl(hOpenFolder, setPage);
    ShowControl(hResetData, setPage);
    ShowControl(hHotkeyStatus, setPage);
    ShowControl(hListener, setPage);
    ShowControl(hPrivacy, setPage);
    ShowControl(hVersion, setPage);
    ShowControl(hExit, setPage);

    if (setPage)
    {
        int y = top;

        MoveWindow(hStartup, margin, y, width, 26, TRUE);
        y += 26 + 10;

        MoveWindow(hHotkeyStatus, margin, y, width, 22, TRUE);
        y += 22 + 6;

        MoveWindow(hListener, margin, y, width, 22, TRUE);
        y += 22 + 12;

        MoveWindow(hDataPath, margin, y, width, 22, TRUE);
        y += 22 + 6;

        MoveWindow(hOpenFolder, margin, y, 150, rowH, TRUE);
        MoveWindow(hResetData, margin + 150 + gap, y, 210, rowH, TRUE);
        MoveWindow(hExit, margin + 150 + 210 + gap * 2, y, 140, rowH, TRUE);
        y += rowH + 14;

        MoveWindow(hPrivacy, margin, y, width, cy - y - 30 - margin, TRUE);
        MoveWindow(hVersion, margin, cy - margin - 20, width, 20, TRUE);
    }
}

/* ------------------------------------------------------------------ */
/* History list                                                       */
/* ------------------------------------------------------------------ */

static void MakePreview(const wchar_t *text, wchar_t *out, int cch)
{
    int i = 0;
    int written = 0;

    if (cch <= 0) return;
    out[0] = L'\0';

    while (text[i] != L'\0' && written < cch - 2 && written < PREVIEW_CHARS)
    {
        wchar_t c = text[i];

        if (c == L'\r' || c == L'\n' || c == L'\t')
        {
            if (written > 0 && out[written - 1] != L' ')
                out[written++] = L' ';
        }
        else
        {
            out[written++] = c;
        }

        i++;
    }

    if (text[i] != L'\0' && written <= cch - 4)
    {
        out[written++] = L'.';
        out[written++] = L'.';
        out[written++] = L'.';
    }

    out[written] = L'\0';
}

void Main_RefreshHistory(void)
{
    wchar_t filter[256];
    wchar_t preview[PREVIEW_CHARS + 8];
    wchar_t counter[128];
    int i;
    int total;

    if (g_hwnd == NULL || hList == NULL) return;

    GetWindowTextW(hSearch, filter, 256);

    SendMessageW(hList, WM_SETREDRAW, FALSE, 0);
    SendMessageW(hList, LB_RESETCONTENT, 0, 0);

    g_rowCount = 0;
    total = Clip_Count();

    for (i = 0; i < total; i++)
    {
        const wchar_t *item = Clip_Get(i);
        int row;

        if (item == NULL) continue;

        if (filter[0] != L'\0')
        {
            if (StrStrIW(item, filter) == NULL) continue;
        }

        MakePreview(item, preview, PREVIEW_CHARS + 8);

        row = (int)SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)preview);
        if (row >= 0)
        {
            SendMessageW(hList, LB_SETITEMDATA, (WPARAM)row, (LPARAM)i);
            g_rowCount++;
        }
    }

    SendMessageW(hList, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(hList, NULL, TRUE);

    if (total == 0)
        wnsprintfW(counter, 128, L"تاریخچه خالی است.");
    else if (filter[0] != L'\0')
        wnsprintfW(counter, 128, L"%d مورد از %d مورد (فیلتر فعال)", g_rowCount, total);
    else
        wnsprintfW(counter, 128, L"%d مورد از حداکثر %d مورد", total, HISTORY_MAX);

    SetText(hCount, counter);
}

static int SelectedHistoryIndex(void)
{
    int sel;
    LRESULT data;

    sel = (int)SendMessageW(hList, LB_GETCURSEL, 0, 0);
    if (sel < 0) return -1;

    data = SendMessageW(hList, LB_GETITEMDATA, (WPARAM)sel, 0);
    if (data == LB_ERR) return -1;

    return (int)data;
}

static void CopySelectedToClipboard(void)
{
    int index = SelectedHistoryIndex();

    if (index < 0)
    {
        MessageBoxW(g_hwnd, L"اول یک مورد را از فهرست انتخاب کنید.",
                    APP_TITLE, MB_OK | MB_ICONINFORMATION);
        return;
    }

    if (!Clip_PutClipboard(g_hwnd, Clip_Get(index)))
    {
        MessageBoxW(g_hwnd,
                    L"کپی به کلیپ‌بورد ممکن نشد.\n"
                    L"ممکن است برنامه دیگری کلیپ‌بورد را در اختیار داشته باشد.",
                    APP_TITLE, MB_OK | MB_ICONWARNING);
    }
}

static void DeleteSelected(void)
{
    int index = SelectedHistoryIndex();

    if (index < 0) return;

    Clip_RemoveAt(index);
    Main_RefreshHistory();
}

/* ------------------------------------------------------------------ */
/* Clipboard monitoring                                               */
/* ------------------------------------------------------------------ */

static void OnClipboardUpdate(void)
{
    wchar_t *text;

    text = Clip_ReadClipboard(g_hwnd);
    if (text == NULL) return;

    if (!Clip_IsSameAsLast(text))
    {
        Clip_NoteText(text);

        if (Clip_Add(text))
            Main_RefreshHistory();
    }

    Clip_FreeText(text);
}

/* ------------------------------------------------------------------ */
/* Tray                                                               */
/* ------------------------------------------------------------------ */

static void TrayAdd(void)
{
    NOTIFYICONDATAW nid;

    if (g_trayAdded) return;

    ZeroMemory(&nid, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_hwnd;
    nid.uID = TRAY_ICON_ID;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = APP_WM_TRAY;
    nid.hIcon = g_icon;
    lstrcpynW(nid.szTip, L"AMIN Keyboard & Clipboard", 64);

    if (Shell_NotifyIconW(NIM_ADD, &nid))
        g_trayAdded = TRUE;
}

static void TrayRemove(void)
{
    NOTIFYICONDATAW nid;

    if (!g_trayAdded) return;

    ZeroMemory(&nid, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_hwnd;
    nid.uID = TRAY_ICON_ID;

    Shell_NotifyIconW(NIM_DELETE, &nid);
    g_trayAdded = FALSE;
}

static void ShowTrayMenu(void)
{
    HMENU menu;
    POINT pt;
    int cmd;

    menu = CreatePopupMenu();
    if (menu == NULL) return;

    AppendMenuW(menu, MF_STRING, IDM_TRAY_SHOW, L"نمایش پنجره اصلی");
    AppendMenuW(menu, MF_STRING, IDM_TRAY_KEYBOARD, L"نمایش / مخفی کردن کیبورد شناور");
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, IDM_TRAY_EXIT, L"خروج");

    SetForegroundWindow(g_hwnd);

    GetCursorPos(&pt);
    cmd = (int)TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                              pt.x, pt.y, 0, g_hwnd, NULL);

    DestroyMenu(menu);

    if (cmd == IDM_TRAY_SHOW)
        Main_ShowWindow();
    else if (cmd == IDM_TRAY_KEYBOARD)
        Keyboard_Toggle(g_inst);
    else if (cmd == IDM_TRAY_EXIT)
    {
        g_exiting = TRUE;
        DestroyWindow(g_hwnd);
    }
}

/* ------------------------------------------------------------------ */
/* Settings page refresh                                              */
/* ------------------------------------------------------------------ */

static void RefreshSettings(void)
{
    wchar_t path[MAX_PATH * 2];
    wchar_t status[512];

    SendMessageW(hStartup, BM_SETCHECK,
                 Settings_StartupEnabled() ? BST_CHECKED : BST_UNCHECKED, 0);

    DataFolderText(path, MAX_PATH * 2);
    SetText(hDataPath, path);

    if (g_hotkeyVk == 'V' && g_hotkeyMods == (MOD_WIN | MOD_NOREPEAT))
        wnsprintfW(status, 512, L"میانبر سراسری: Win + V");
    else if (g_hotkeyVk == 'V' && g_hotkeyMods == (MOD_CONTROL | MOD_ALT | MOD_NOREPEAT))
        wnsprintfW(status, 512,
                   L"میانبر سراسری: Ctrl + Alt + V  (کلید Win + V توسط ویندوز یا برنامه دیگری گرفته شده است)");
    else
        wnsprintfW(status, 512,
                   L"میانبر سراسری در دسترس نیست. برنامه را از آیکن کنار ساعت یا این پنجره باز کنید.");

    SetText(hHotkeyStatus, status);

    SetText(hListener, g_listening
        ? L"شنود کلیپ‌بورد: فعال — فقط متن کپی‌شده ثبت می‌شود."
        : L"شنود کلیپ‌بورد: غیرفعال — ویندوز اجازه ثبت شنونده را نداد.");
}

/* ------------------------------------------------------------------ */
/* Commands                                                           */
/* ------------------------------------------------------------------ */

static void SwitchPage(int page)
{
    RECT rc;

    g_page = page;
    GetClientRect(g_hwnd, &rc);
    Layout(rc.right, rc.bottom);
    InvalidateRect(g_hwnd, NULL, TRUE);
}

static void ResetLocalData(void)
{
    wchar_t folder[MAX_PATH * 2];
    wchar_t pattern[MAX_PATH * 2];
    WIN32_FIND_DATAW data;
    HANDLE find;

    DataFolderText(folder, MAX_PATH * 2);
    wnsprintfW(pattern, MAX_PATH * 2, L"%s\\*.*", folder);

    if (MessageBoxW(g_hwnd,
                    L"همه داده‌های محلی حذف شود؟\n\n"
                    L"• تاریخچه کلیپ‌بورد\n"
                    L"• تنظیم اجرای خودکار در ویندوز\n"
                    L"• محل ذخیره‌شده کیبورد\n\n"
                    L"این کار قابل بازگشت نیست.",
                    L"تأیید حذف داده‌ها",
                    MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES)
    {
        return;
    }

    Clip_Clear();

    find = FindFirstFileW(pattern, &data);
    if (find != INVALID_HANDLE_VALUE)
    {
        do
        {
            wchar_t full[MAX_PATH * 2];

            if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;

            wnsprintfW(full, MAX_PATH * 2, L"%s\\%s", folder, data.cFileName);
            DeleteFileW(full);
        }
        while (FindNextFileW(find, &data));

        FindClose(find);
    }

    Settings_SetStartupEnabled(FALSE);

    Main_RefreshHistory();
    RefreshSettings();

    MessageBoxW(g_hwnd, L"داده‌های محلی حذف شد.", APP_TITLE,
                MB_OK | MB_ICONINFORMATION);
}

static void OpenDataFolder(void)
{
    wchar_t folder[MAX_PATH * 2];

    DataFolderText(folder, MAX_PATH * 2);
    ShellExecuteW(g_hwnd, L"open", folder, NULL, NULL, SW_SHOWNORMAL);
}

static void RegisterHotKeyOrFallback(void)
{
    g_hotkeyMods = MOD_WIN | MOD_NOREPEAT;
    g_hotkeyVk = 'V';

    if (RegisterHotKey(g_hwnd, HOTKEY_ID,
                       MOD_WIN | MOD_NOREPEAT, (UINT)'V'))
    {
        return;
    }

    g_hotkeyMods = MOD_CONTROL | MOD_ALT | MOD_NOREPEAT;

    if (!RegisterHotKey(g_hwnd, HOTKEY_ID,
                        MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, (UINT)'V'))
    {
        g_hotkeyMods = 0;
        g_hotkeyVk = 0;
    }
}

void Main_UpdateLanguageLabel(BOOL persian)
{
    if (hKbStatus == NULL) return;

    SetText(hKbStatus, persian
        ? L"زبان کیبورد: فارسی — برای تایپ، اول روی پنجره مقصد کلیک کنید."
        : L"زبان کیبورد: انگلیسی — برای تایپ، اول روی پنجره مقصد کلیک کنید.");
}

void Main_ShowWindow(void)
{
    if (g_hwnd == NULL) return;

    ShowWindow(g_hwnd, SW_RESTORE);
    SetForegroundWindow(g_hwnd);
}

HWND Main_GetHandle(void)
{
    return g_hwnd;
}

/* ------------------------------------------------------------------ */
/* Window procedure                                                   */
/* ------------------------------------------------------------------ */

static void OnCommand(int id, int code)
{
    switch (id)
    {
    case IDC_TAB_KEYBOARD: SwitchPage(0); break;
    case IDC_TAB_HISTORY:  SwitchPage(1); break;
    case IDC_TAB_SETTINGS: SwitchPage(2); RefreshSettings(); break;

    case IDC_SHOW_KEYBOARD: Keyboard_Show(g_inst); break;
    case IDC_HIDE_KEYBOARD: Keyboard_Hide(); break;

    case IDC_SEARCH:
        if (code == EN_CHANGE) Main_RefreshHistory();
        break;

    case IDC_CLEAR:
        if (Clip_Count() == 0) break;
        if (MessageBoxW(g_hwnd, L"همه تاریخچه متنی حذف شود؟",
                        L"تأیید حذف",
                        MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) == IDYES)
        {
            Clip_Clear();
            Main_RefreshHistory();
        }
        break;

    case IDC_COPY:   CopySelectedToClipboard(); break;
    case IDC_DELETE: DeleteSelected(); break;

    case IDC_LIST:
        if (code == LBN_DBLCLK) CopySelectedToClipboard();
        break;

    case IDC_STARTUP:
    {
        BOOL want = (SendMessageW(hStartup, BM_GETCHECK, 0, 0) == BST_CHECKED);

        if (!Settings_SetStartupEnabled(want))
        {
            MessageBoxW(g_hwnd,
                        L"تنظیم اجرای خودکار انجام نشد.\n"
                        L"دسترسی به تنظیمات کاربر (HKCU) ممکن نیست.",
                        APP_TITLE, MB_OK | MB_ICONWARNING);

            SendMessageW(hStartup, BM_SETCHECK,
                         want ? BST_UNCHECKED : BST_CHECKED, 0);
        }
        break;
    }

    case IDC_OPEN_FOLDER: OpenDataFolder(); break;
    case IDC_RESET_DATA:  ResetLocalData(); break;

    case IDC_EXIT:
        g_exiting = TRUE;
        DestroyWindow(g_hwnd);
        break;

    default:
        break;
    }
}

static LRESULT CALLBACK MainProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        g_hwnd = hwnd;
        return 0;

    case WM_SIZE:
        Layout(LOWORD(lParam), HIWORD(lParam));
        return 0;

    case WM_CTLCOLORSTATIC:
        /* Keep labels visually part of the window background. */
        SetBkColor((HDC)wParam, RGB(250, 250, 252));
        SetTextColor((HDC)wParam, RGB(30, 30, 36));
        if (g_windowBrush != NULL) return (LRESULT)g_windowBrush;
        break;

    case WM_CTLCOLORBTN:
        if (g_windowBrush != NULL) return (LRESULT)g_windowBrush;
        break;

    case WM_GETMINMAXINFO:
    {
        MINMAXINFO *info = (MINMAXINFO *)lParam;
        info->ptMinTrackSize.x = 640;
        info->ptMinTrackSize.y = 460;
        return 0;
    }

    case WM_COMMAND:
        OnCommand(LOWORD(wParam), HIWORD(wParam));
        return 0;

    case WM_CLIPBOARDUPDATE:
        OnClipboardUpdate();
        return 0;

    case WM_HOTKEY:
        if ((int)wParam == HOTKEY_ID)
        {
            /* Bring the app forward; the clipboard page is where you act on it. */
            Main_ShowWindow();
            SwitchPage(1);
            SetFocus(hSearch);
        }
        return 0;

    case APP_WM_TRAY:
        if (LOWORD(lParam) == WM_RBUTTONUP || LOWORD(lParam) == WM_CONTEXTMENU)
            ShowTrayMenu();
        else if (LOWORD(lParam) == WM_LBUTTONDBLCLK)
            Main_ShowWindow();
        return 0;

    case WM_CLOSE:
        /* Hide to the tray so the global hot key keeps working. If the tray
         * icon could not be created, hiding would make the app unreachable,
         * so in that case exit properly instead. */
        if (!g_exiting && g_trayAdded)
        {
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        }

        g_exiting = TRUE;
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        if (g_hotkeyMods != 0)
            UnregisterHotKey(hwnd, HOTKEY_ID);

        if (g_listening)
            RemoveClipboardFormatListener(hwnd);

        TrayRemove();
        Keyboard_Destroy();
        PostQuitMessage(0);
        return 0;

    case WM_ENDSESSION:
        /* Logging off / shutting down: persist and leave cleanly. */
        Clip_Save();
        return 0;

    default:
        if (msg == g_showMessage && g_showMessage != 0)
        {
            Main_ShowWindow();
            return 0;
        }
        break;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

/* ------------------------------------------------------------------ */
/* Creation helpers                                                   */
/* ------------------------------------------------------------------ */

static void CreateControls(void)
{
    hTabKeyboard = MakeChild(L"BUTTON", L"کیبورد مجازی", BS_PUSHBUTTON, IDC_TAB_KEYBOARD, 0);
    hTabHistory  = MakeChild(L"BUTTON", L"تاریخچه کلیپ‌بورد", BS_PUSHBUTTON, IDC_TAB_HISTORY, 0);
    hTabSettings = MakeChild(L"BUTTON", L"تنظیمات و حریم خصوصی", BS_PUSHBUTTON, IDC_TAB_SETTINGS, 0);

    /* Page 0 */
    hShowKeyboard = MakeChild(L"BUTTON", L"نمایش کیبورد شناور", BS_PUSHBUTTON, IDC_SHOW_KEYBOARD, 0);
    hHideKeyboard = MakeChild(L"BUTTON", L"مخفی کردن کیبورد", BS_PUSHBUTTON, IDC_HIDE_KEYBOARD, 0);
    hKbStatus = MakeChild(L"STATIC", L"زبان کیبورد: فارسی", SS_LEFT, IDC_KB_STATUS, 0);
    hKbHelp = MakeChild(L"STATIC",
        L"۱. اول برنامه مقصد (Notepad، Word، مرورگر) را باز کنید و داخل محل تایپ کلیک کنید.\r\n"
        L"۲. «نمایش کیبورد شناور» را بزنید.\r\n"
        L"۳. روی کلیدها کلیک کنید؛ کاراکتر به همان پنجره ارسال می‌شود.\r\n"
        L"۴. دکمه «فا / EN» زبان را عوض می‌کند؛ در حالت انگلیسی دکمه Shift حروف بزرگ/کوچک را تغییر می‌دهد.\r\n"
        L"۵. برای جابه‌جایی کیبورد، نوار عنوان آن را با ماوس بکشید (محلش ذخیره می‌شود).\r\n"
        L"۶. این پنجره را ببندید → برنامه به سینی (کنار ساعت) می‌رود و میانبر سراسری فعال می‌ماند.\r\n\r\n"
        L"نکته: ویندوز اجازه نمی‌دهد برنامه‌ای با دسترسی معمولی در پنجره‌ای که با «Run as administrator» اجرا شده تایپ کند."
        L" برای این حالت خود AMIN را هم با دسترسی مدیر اجرا کنید.",
        SS_LEFT, IDC_KB_HELP, 0);

    /* Page 1 */
    hSearch = MakeChild(L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, IDC_SEARCH, WS_EX_CLIENTEDGE);
    hList = MakeChild(L"LISTBOX", L"", WS_VSCROLL | WS_TABSTOP | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
                      IDC_LIST, WS_EX_CLIENTEDGE);
    hCopy = MakeChild(L"BUTTON", L"کپی مورد انتخاب‌شده", BS_PUSHBUTTON, IDC_COPY, 0);
    hDelete = MakeChild(L"BUTTON", L"حذف مورد انتخاب‌شده", BS_PUSHBUTTON, IDC_DELETE, 0);
    hClear = MakeChild(L"BUTTON", L"پاک کردن همه", BS_PUSHBUTTON, IDC_CLEAR, 0);
    hCount = MakeChild(L"STATIC", L"", SS_LEFT, IDC_COUNT, 0);

    /* Page 2 */
    hStartup = MakeChild(L"BUTTON", L"اجرای برنامه هنگام ورود به ویندوز",
                         BS_AUTOCHECKBOX | WS_TABSTOP, IDC_STARTUP, 0);
    hDataPath = MakeChild(L"STATIC", L"", SS_LEFT | SS_PATHELLIPSIS, IDC_DATA_PATH, 0);
    hOpenFolder = MakeChild(L"BUTTON", L"باز کردن پوشه داده‌ها", BS_PUSHBUTTON, IDC_OPEN_FOLDER, 0);
    hResetData = MakeChild(L"BUTTON", L"حذف کامل داده‌های محلی", BS_PUSHBUTTON, IDC_RESET_DATA, 0);
    hHotkeyStatus = MakeChild(L"STATIC", L"", SS_LEFT, IDC_HOTKEY_STATUS, 0);
    hListener = MakeChild(L"STATIC", L"", SS_LEFT, IDC_LISTENER, 0);
    hPrivacy = MakeChild(L"STATIC",
        L"حریم خصوصی:\r\n"
        L"• فقط متن کپی‌شده ذخیره می‌شود؛ تصویر، فایل و هر قالب دیگری نادیده گرفته می‌شود.\r\n"
        L"• برنامه هیچ اتصال شبکه‌ای ندارد: نه سرور، نه آمار، نه به‌روزرسانی خودکار.\r\n"
        L"• هیچ گذرواژه‌ای خوانده، تشخیص داده یا ذخیره نمی‌شود.\r\n"
        L"• حداکثر ۱۰۰ مورد و هر مورد حداکثر ۲۰٬۰۰۰ کاراکتر نگه داشته می‌شود.\r\n"
        L"• تاریخچه رمزنگاری نمی‌شود؛ روی رایانه مشترک از «حذف کامل داده‌های محلی» استفاده کنید.\r\n"
        L"• هیچ بخشی از این برنامه به .NET Framework نیاز ندارد.",
        SS_LEFT, IDC_PRIVACY, 0);
    hExit = MakeChild(L"BUTTON", L"خروج از برنامه", BS_PUSHBUTTON, IDC_EXIT, 0);
    hVersion = MakeChild(L"STATIC", L"AMIN Keyboard & Clipboard — نسخه " APP_VERSION_STR
                                     L" — بدون نیاز به .NET",
                         SS_LEFT, IDC_VERSION, 0);

    /* Search hint */
    SendMessageW(hSearch, EM_SETCUEBANNER, TRUE, (LPARAM)L"جستجو در تاریخچه…");
}

static void CreateFonts(void)
{
    NONCLIENTMETRICSW metrics;

    ZeroMemory(&metrics, sizeof(metrics));
    metrics.cbSize = sizeof(metrics);

    if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0))
    {
        g_font = CreateFontIndirectW(&metrics.lfMessageFont);
    }
    else
    {
        g_font = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Tahoma");
    }
}

/* ------------------------------------------------------------------ */
/* Entry point                                                        */
/* ------------------------------------------------------------------ */

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR commandLine, int show)
{
    WNDCLASSEXW wc;
    MSG msg;
    HANDLE mutex;
    wchar_t firstRunText[1024];

    (void)previous;
    (void)commandLine;
    (void)show;

    g_inst = instance;
    g_showMessage = RegisterWindowMessageW(L"AMIN_KB_SHOW_MAIN_WINDOW");

    mutex = CreateMutexW(NULL, TRUE, APP_MUTEX_NAME);
    if (mutex != NULL && GetLastError() == ERROR_ALREADY_EXISTS)
    {
        /* Another copy is running: ask it to show itself, then leave. */
        PostMessageW(HWND_BROADCAST, g_showMessage, 0, 0);
        CloseHandle(mutex);
        return 0;
    }

    /* No common controls dependency: stock Win32 controls only. */
    CreateFonts();

    g_icon = LoadIconW(instance, MAKEINTRESOURCEW(1));
    if (g_icon == NULL)
        g_icon = LoadIconW(NULL, IDI_APPLICATION);

    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = MainProc;
    wc.hInstance = instance;
    wc.hIcon = g_icon;
    wc.hIconSm = g_icon;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    g_windowBrush = CreateSolidBrush(RGB(250, 250, 252));
    wc.hbrBackground = g_windowBrush;
    wc.lpszClassName = APP_CLASS_MAIN;

    if (!RegisterClassExW(&wc))
    {
        MessageBoxW(NULL, L"ثبت پنجره اصلی ممکن نشد.", APP_TITLE,
                    MB_OK | MB_ICONERROR);
        return 1;
    }

    if (!Keyboard_RegisterClass(instance))
    {
        MessageBoxW(NULL, L"ثبت پنجره کیبورد ممکن نشد.", APP_TITLE,
                    MB_OK | MB_ICONERROR);
        return 1;
    }

    g_hwnd = CreateWindowExW(0, APP_CLASS_MAIN, APP_TITLE,
                             WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT, 900, 620,
                             NULL, NULL, instance, NULL);

    if (g_hwnd == NULL)
    {
        MessageBoxW(NULL, L"ساخت پنجره اصلی ممکن نشد.", APP_TITLE,
                    MB_OK | MB_ICONERROR);
        return 1;
    }

    CreateControls();

    {
        RECT rc;
        GetClientRect(g_hwnd, &rc);
        g_page = 0;
        Layout(rc.right, rc.bottom);
    }

    /* Data + clipboard */
    Clip_Load();
    g_listening = AddClipboardFormatListener(g_hwnd);
    RegisterHotKeyOrFallback();

    Main_RefreshHistory();
    RefreshSettings();
    Main_UpdateLanguageLabel(TRUE);

    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);

    TrayAdd();

    if (!Settings_FirstRunDone())
    {
        wnsprintfW(firstRunText, 1024,
            L"به AMIN Keyboard & Clipboard خوش آمدید.\n\n"
            L"• کیبورد فارسی/انگلیسی شناور با ارسال یونیکد.\n"
            L"• تاریخچه متنی کلیپ‌بورد (حداکثر %d مورد).\n"
            L"• همه داده‌ها فقط روی همین رایانه می‌مانند.\n"
            L"• هیچ اتصال اینترنتی وجود ندارد.\n"
            L"• این برنامه به .NET Framework یا هیچ رانتایم دیگری نیاز ندارد.\n\n"
            L"میانبر سراسری: %s\n\n"
            L"بستن پنجره برنامه را به سینی می‌برد؛ برای خروج کامل از منوی آیکن سینی «خروج» را بزنید.",
            HISTORY_MAX,
            (g_hotkeyMods == (MOD_WIN | MOD_NOREPEAT)) ? L"Win + V" :
            (g_hotkeyMods != 0) ? L"Ctrl + Alt + V" : L"در دسترس نیست");

        if (g_hotkeyMods == 0)
        {
            /* Append a short explanation without complicating the format above. */
            lstrcatW(firstRunText, L"\n\nنکته: ثبت میانبر سراسری ممکن نشد؛ برنامه را از آیکن سینی باز کنید.");
        }

        MessageBoxW(g_hwnd, firstRunText, L"خوش آمدید",
                    MB_OK | MB_ICONINFORMATION);

        Settings_SetFirstRunDone();
    }

    while (GetMessageW(&msg, NULL, 0, 0) > 0)
    {
        /* The clipboard page should behave normally for Tab / arrow keys. */
        if (g_page == 1 && (msg.hwnd == hSearch || msg.hwnd == hList))
        {
            if (IsDialogMessageW(g_hwnd, &msg))
                continue;
        }

        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (mutex != NULL)
    {
        ReleaseMutex(mutex);
        CloseHandle(mutex);
    }

    return (int)msg.wParam;
}
