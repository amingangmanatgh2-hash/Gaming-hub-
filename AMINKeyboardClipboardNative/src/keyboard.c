/* -------------------------------------------------------------------------
 * keyboard.c - always-on-top on-screen keyboard for Persian + English.
 *
 * Everything is drawn by hand (no child controls). That is a deliberate
 * choice: the window is created with WS_EX_NOACTIVATE so clicking a key can
 * never steal focus from the application being typed into, and child controls
 * inside such a window are a reliable source of focus bugs.
 *
 * Characters are injected as Unicode scan codes through SendInput, so Persian
 * letters arrive correctly no matter which keyboard layout is active.
 * ------------------------------------------------------------------------- */

#include "app.h"

#define KEY_W        44
#define KEY_H        46
#define KEY_GAP      4
#define PAD          10
#define TITLE_H      34
#define WINDOW_W     (12 * (KEY_W + KEY_GAP) + KEY_GAP + PAD * 2)
#define WINDOW_H     (TITLE_H + 5 * (KEY_H + KEY_GAP) + KEY_GAP + PAD)

#define ACTION_TEXT   0
#define ACTION_VK     1
#define ACTION_LANG   2
#define ACTION_SHIFT  3
#define ACTION_HIDE   4

typedef struct
{
    RECT rc;
    wchar_t label[8];
    int action;
    wchar_t text[8];
    UINT vk;
} Key;

#define MAX_KEYS 80

static const wchar_t *kPersianRows[4] =
{
    L"۱۲۳۴۵۶۷۸۹۰-=",
    L"ضصثقفغعهخحجچ",
    L"شسیبلاتنمکگ",
    L"ظطزرذدپو؟،."
};

static const wchar_t *kEnglishRows[4] =
{
    L"1234567890-=",
    L"qwertyuiop[]",
    L"asdfghjkl;'",
    L"zxcvbnm,./"
};

static HINSTANCE     g_inst;
static HWND          g_hwnd;
static HWND          g_target;
static HWINEVENTHOOK g_hook;
static HFONT         g_fontKey;
static HFONT         g_fontTitle;
static HBRUSH        g_bgBrush;
static BOOL          g_persian = TRUE;
static BOOL          g_shift = FALSE;

static Key  g_keys[MAX_KEYS];
static int  g_keyCount;
static int  g_hotKey = -1;
static int  g_pressedKey = -1;

static BOOL g_dragging = FALSE;
static POINT g_dragStartCursor;
static POINT g_dragStartWindow;
static BOOL g_trackingLeave = FALSE;

/* ------------------------------------------------------------------ */
/* Foreground tracking                                                */
/* ------------------------------------------------------------------ */

static void RememberIfExternal(HWND hwnd)
{
    DWORD pid = 0;

    if (hwnd == NULL || !IsWindow(hwnd)) return;

    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0) return;
    if (pid == GetCurrentProcessId()) return;

    g_target = hwnd;
}

static void CALLBACK ForegroundProc(HWINEVENTHOOK hook, DWORD event, HWND hwnd,
                                    LONG idObject, LONG idChild,
                                    DWORD threadId, DWORD time)
{
    (void)hook;
    (void)event;
    (void)threadId;
    (void)time;

    if (idObject != OBJID_WINDOW) return;

    RememberIfExternal(hwnd);
}

HWND Keyboard_TargetWindow(void)
{
    return g_target;
}

/* ------------------------------------------------------------------ */
/* Input injection                                                    */
/* ------------------------------------------------------------------ */

static BOOL EnsureTarget(void)
{
    if (g_target != NULL && IsWindow(g_target))
    {
        if (GetForegroundWindow() != g_target)
            SetForegroundWindow(g_target);

        return TRUE;
    }

    MessageBoxW(NULL,
        L"اول یک برنامه مقصد (مثلاً Notepad یا Word) را باز کنید،\n"
        L"داخل محل تایپ کلیک کنید، بعد از کیبورد استفاده کنید.",
        L"پنجره مقصد پیدا نشد",
        MB_OK | MB_ICONINFORMATION | MB_TOPMOST);

    return FALSE;
}

static void SendUnicodeChar(wchar_t c)
{
    INPUT in[2];

    ZeroMemory(in, sizeof(in));

    in[0].type = INPUT_KEYBOARD;
    in[0].ki.wScan = (WORD)c;
    in[0].ki.dwFlags = KEYEVENTF_UNICODE;

    in[1].type = INPUT_KEYBOARD;
    in[1].ki.wScan = (WORD)c;
    in[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;

    if (SendInput(2, in, sizeof(INPUT)) != 2)
    {
        DWORD err = GetLastError();
        wchar_t msg[512];

        if (err == ERROR_ACCESS_DENIED)
        {
            wnsprintfW(msg, 512,
                L"ارسال کاراکتر ممکن نشد.\n\n"
                L"پنجره مقصد با دسترسی مدیر (Administrator) اجرا شده است.\n"
                L"ویندوز اجازه نمی‌دهد برنامه معمولی در آن تایپ کند.\n"
                L"برای این حالت، خود AMIN را هم «Run as administrator» اجرا کنید.");
        }
        else
        {
            wnsprintfW(msg, 512,
                L"ارسال کاراکتر ممکن نشد.\n\nکد خطای ویندوز: %lu", err);
        }

        MessageBoxW(g_hwnd, msg, L"ارسال ورودی ناموفق",
                    MB_OK | MB_ICONWARNING | MB_TOPMOST);
    }
}

static void SendText(const wchar_t *text)
{
    if (text == NULL || text[0] == L'\0') return;
    if (!EnsureTarget()) return;

    while (*text)
    {
        SendUnicodeChar(*text);
        text++;
    }
}

static void SendVirtualKey(UINT vk)
{
    INPUT in[2];

    if (!EnsureTarget()) return;

    ZeroMemory(in, sizeof(in));

    in[0].type = INPUT_KEYBOARD;
    in[0].ki.wVk = (WORD)vk;

    in[1].type = INPUT_KEYBOARD;
    in[1].ki.wVk = (WORD)vk;
    in[1].ki.dwFlags = KEYEVENTF_KEYUP;

    SendInput(2, in, sizeof(INPUT));
}

/* ------------------------------------------------------------------ */
/* Layout                                                             */
/* ------------------------------------------------------------------ */

static void AddTextKey(int row, int col, int rowCount, wchar_t c)
{
    Key *k;
    int rowWidth;

    if (g_keyCount >= MAX_KEYS) return;

    k = &g_keys[g_keyCount];
    ZeroMemory(k, sizeof(*k));

    rowWidth = rowCount * (KEY_W + KEY_GAP) - KEY_GAP;
    k->rc.left = (WINDOW_W - PAD - rowWidth) / 2 + col * (KEY_W + KEY_GAP);
    k->rc.top = PAD + TITLE_H + row * (KEY_H + KEY_GAP);
    k->rc.right = k->rc.left + KEY_W;
    k->rc.bottom = k->rc.top + KEY_H;

    k->label[0] = c;
    k->label[1] = L'\0';
    k->text[0] = c;
    k->text[1] = L'\0';
    k->action = ACTION_TEXT;

    g_keyCount++;
}

static void AddActionKey(int x, int width, const wchar_t *label, int action)
{
    Key *k;

    if (g_keyCount >= MAX_KEYS) return;

    k = &g_keys[g_keyCount];
    ZeroMemory(k, sizeof(*k));

    k->rc.left = x;
    k->rc.top = PAD + TITLE_H + 4 * (KEY_H + KEY_GAP);
    k->rc.right = x + width;
    k->rc.bottom = k->rc.top + KEY_H;

    lstrcpynW(k->label, label, 8);
    k->action = action;

    g_keyCount++;
}

static void BuildLayout(void)
{
    int row;
    int col;
    int len;
    int x;
    const wchar_t *rows[4];

    g_keyCount = 0;
    g_hotKey = -1;
    g_pressedKey = -1;

    for (row = 0; row < 4; row++)
        rows[row] = g_persian ? kPersianRows[row] : kEnglishRows[row];

    for (row = 0; row < 4; row++)
    {
        len = lstrlenW(rows[row]);

        for (col = 0; col < len; col++)
        {
            wchar_t c = rows[row][col];

            if (!g_persian && g_shift)
            {
                if (c >= L'a' && c <= L'z') c = (wchar_t)(c - L'a' + L'A');
            }

            AddTextKey(row, col, len, c);
        }
    }

    /* Bottom row of actions. Widths are chosen so the row fits inside the
     * client area (WINDOW_W - 2 * PAD) with room to spare. */
    x = PAD;

    AddActionKey(x, 66, g_persian ? L"فا / EN" : L"EN / فا", ACTION_LANG);
    x += 66 + KEY_GAP;

    AddActionKey(x, 58, L"Shift", ACTION_SHIFT);
    x += 58 + KEY_GAP;

    AddActionKey(x, 54, L"Tab", ACTION_VK);
    g_keys[g_keyCount - 1].vk = VK_TAB;
    x += 54 + KEY_GAP;

    AddActionKey(x, 150, L"Space", ACTION_TEXT);
    g_keys[g_keyCount - 1].text[0] = L' ';
    g_keys[g_keyCount - 1].text[1] = L'\0';
    x += 150 + KEY_GAP;

    AddActionKey(x, 50, L"⌫", ACTION_VK);
    g_keys[g_keyCount - 1].vk = VK_BACK;
    x += 50 + KEY_GAP;

    AddActionKey(x, 60, L"Enter", ACTION_VK);
    g_keys[g_keyCount - 1].vk = VK_RETURN;
    x += 60 + KEY_GAP;

    AddActionKey(x, 42, L"←", ACTION_VK);
    g_keys[g_keyCount - 1].vk = VK_LEFT;
    x += 42 + KEY_GAP;

    AddActionKey(x, 60, L"مخفی", ACTION_HIDE);
}

/* ------------------------------------------------------------------ */
/* Painting                                                           */
/* ------------------------------------------------------------------ */

static void FillRoundRect(HDC dc, const RECT *rc, COLORREF fill)
{
    HBRUSH brush;
    HPEN pen;

    brush = CreateSolidBrush(fill);
    pen = CreatePen(PS_SOLID, 1, fill);

    SelectObject(dc, brush);
    SelectObject(dc, pen);

    RoundRect(dc, rc->left, rc->top, rc->right, rc->bottom, 10, 10);

    DeleteObject(brush);
    DeleteObject(pen);
}

static void DrawKey(HDC dc, const Key *key, int index)
{
    COLORREF fill;
    RECT labelRect;

    if (key->action == ACTION_SHIFT && g_persian)
        fill = RGB(28, 30, 43); /* disabled look in the Persian layout */
    else if (index == g_pressedKey)
        fill = RGB(113, 87, 232);
    else if (index == g_hotKey)
        fill = RGB(52, 56, 79);
    else if (key->action == ACTION_LANG)
        fill = RGB(56, 139, 255);
    else if (key->action == ACTION_HIDE)
        fill = RGB(120, 62, 76);
    else
        fill = RGB(38, 41, 58);

    FillRoundRect(dc, &key->rc, fill);

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, key->action == ACTION_SHIFT && g_persian
                     ? RGB(110, 110, 125)
                     : RGB(246, 244, 255));

    SelectObject(dc, g_fontKey);

    labelRect = key->rc;
    DrawTextW(dc, key->label, -1, &labelRect,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

static void PaintWindow(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC dc;
    HDC mem;
    HBITMAP bitmap;
    HBITMAP oldBitmap;
    RECT client;
    RECT titleRect;
    int i;

    dc = BeginPaint(hwnd, &ps);
    GetClientRect(hwnd, &client);

    /* Double buffering keeps the hover highlight from flickering. */
    mem = CreateCompatibleDC(dc);
    bitmap = CreateCompatibleBitmap(dc, client.right, client.bottom);
    oldBitmap = (HBITMAP)SelectObject(mem, bitmap);

    FillRect(mem, &client, g_bgBrush);

    titleRect = client;
    titleRect.bottom = TITLE_H;

    SelectObject(mem, g_fontTitle);
    SetBkMode(mem, TRANSPARENT);
    SetTextColor(mem, RGB(246, 244, 255));

    {
        RECT textRect = titleRect;
        textRect.left += PAD;
        DrawTextW(mem, L"کیبورد مجازی AMIN", -1, &textRect,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    {
        RECT hintRect = titleRect;
        wchar_t hint[128];

        hintRect.right -= PAD;
        wnsprintfW(hint, 128, L"%s — برای جابه‌جایی، همین نوار را بکشید",
                   g_persian ? L"فارسی" : L"English");

        SetTextColor(mem, RGB(185, 184, 200));
        DrawTextW(mem, hint, -1, &hintRect,
                  DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    for (i = 0; i < g_keyCount; i++)
        DrawKey(mem, &g_keys[i], i);

    BitBlt(dc, 0, 0, client.right, client.bottom, mem, 0, 0, SRCCOPY);

    SelectObject(mem, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(mem);

    EndPaint(hwnd, &ps);
}

/* ------------------------------------------------------------------ */
/* Interaction                                                        */
/* ------------------------------------------------------------------ */

static int HitTest(POINT pt)
{
    int i;

    for (i = 0; i < g_keyCount; i++)
    {
        if (PtInRect(&g_keys[i].rc, pt))
            return i;
    }

    return -1;
}

static void ActivateKey(const Key *key)
{
    switch (key->action)
    {
    case ACTION_TEXT:
        SendText(key->text);
        break;

    case ACTION_VK:
        SendVirtualKey(key->vk);
        break;

    case ACTION_LANG:
        g_persian = !g_persian;
        g_shift = FALSE;
        BuildLayout();
        InvalidateRect(g_hwnd, NULL, FALSE);
        Main_UpdateLanguageLabel(g_persian);
        break;

    case ACTION_SHIFT:
        if (!g_persian)
        {
            g_shift = !g_shift;
            BuildLayout();
            InvalidateRect(g_hwnd, NULL, FALSE);
        }
        break;

    case ACTION_HIDE:
        ShowWindow(g_hwnd, SW_HIDE);
        break;

    default:
        break;
    }
}

static void SaveWindowPos(void)
{
    RECT rc;

    if (g_hwnd == NULL) return;
    if (!GetWindowRect(g_hwnd, &rc)) return;

    Settings_SaveKeyboardPos(rc.left, rc.top);
}

static LRESULT CALLBACK KeyboardProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        g_hwnd = hwnd;
        return 0;

    case WM_PAINT:
        PaintWindow(hwnd);
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_MOUSEACTIVATE:
        /* Never become the active window. */
        return MA_NOACTIVATE;

    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT)
        {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hwnd, &pt);

            if (pt.y < TITLE_H || HitTest(pt) >= 0)
            {
                SetCursor(LoadCursorW(NULL, IDC_HAND));
                return TRUE;
            }
        }
        break;

    case WM_LBUTTONDOWN:
    {
        POINT pt;
        int hit;

        pt.x = GET_X_LPARAM(lParam);
        pt.y = GET_Y_LPARAM(lParam);

        if (pt.y < TITLE_H)
        {
            g_dragging = TRUE;
            GetCursorPos(&g_dragStartCursor);

            {
                RECT rc;
                GetWindowRect(hwnd, &rc);
                g_dragStartWindow.x = rc.left;
                g_dragStartWindow.y = rc.top;
            }

            SetCapture(hwnd);
            return 0;
        }

        hit = HitTest(pt);
        if (hit >= 0)
        {
            if (g_keys[hit].action == ACTION_SHIFT && g_persian)
            {
                /* Shift is intentionally inert in the Persian layout. */
                return 0;
            }

            g_pressedKey = hit;
            SetCapture(hwnd);
            InvalidateRect(hwnd, &g_keys[hit].rc, FALSE);
        }

        return 0;
    }

    case WM_MOUSEMOVE:
    {
        POINT pt;
        int hit;

        pt.x = GET_X_LPARAM(lParam);
        pt.y = GET_Y_LPARAM(lParam);

        if (g_dragging)
        {
            POINT cursor;

            GetCursorPos(&cursor);
            SetWindowPos(hwnd, NULL,
                         g_dragStartWindow.x + (cursor.x - g_dragStartCursor.x),
                         g_dragStartWindow.y + (cursor.y - g_dragStartCursor.y),
                         0, 0,
                         SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }

        hit = (pt.y < TITLE_H) ? -1 : HitTest(pt);
        if (hit != g_hotKey)
        {
            g_hotKey = hit;
            InvalidateRect(hwnd, NULL, FALSE);
        }

        if (!g_trackingLeave)
        {
            TRACKMOUSEEVENT tme;

            tme.cbSize = sizeof(tme);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd;
            tme.dwHoverTime = 0;

            if (TrackMouseEvent(&tme)) g_trackingLeave = TRUE;
        }

        return 0;
    }

    case WM_MOUSELEAVE:
        g_trackingLeave = FALSE;
        g_hotKey = -1;
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_LBUTTONUP:
    {
        POINT pt;
        int hit;

        pt.x = GET_X_LPARAM(lParam);
        pt.y = GET_Y_LPARAM(lParam);

        if (g_dragging)
        {
            g_dragging = FALSE;
            ReleaseCapture();
            SaveWindowPos();
            return 0;
        }

        hit = HitTest(pt);

        if (g_pressedKey >= 0)
        {
            int pressed = g_pressedKey;

            g_pressedKey = -1;
            ReleaseCapture();
            InvalidateRect(hwnd, &g_keys[pressed].rc, FALSE);

            if (hit == pressed)
                ActivateKey(&g_keys[pressed]);
        }

        return 0;
    }

    case WM_CAPTURECHANGED:
        g_dragging = FALSE;
        if (g_pressedKey >= 0)
        {
            g_pressedKey = -1;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;

    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        return 0;

    case WM_DESTROY:
        if (g_hook != NULL)
        {
            UnhookWinEvent(g_hook);
            g_hook = NULL;
        }

        if (g_fontKey != NULL) { DeleteObject(g_fontKey); g_fontKey = NULL; }
        if (g_fontTitle != NULL) { DeleteObject(g_fontTitle); g_fontTitle = NULL; }
        if (g_bgBrush != NULL) { DeleteObject(g_bgBrush); g_bgBrush = NULL; }

        g_hwnd = NULL;
        return 0;

    default:
        break;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

/* ------------------------------------------------------------------ */
/* Window management                                                  */
/* ------------------------------------------------------------------ */

BOOL Keyboard_RegisterClass(HINSTANCE instance)
{
    WNDCLASSEXW wc;

    g_inst = instance;

    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = KeyboardProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = APP_CLASS_KEYBOARD;

    return (RegisterClassExW(&wc) != 0);
}

BOOL Keyboard_IsVisible(void)
{
    return (g_hwnd != NULL && IsWindowVisible(g_hwnd));
}

void Keyboard_Hide(void)
{
    if (g_hwnd != NULL) ShowWindow(g_hwnd, SW_HIDE);
}

void Keyboard_Destroy(void)
{
    if (g_hwnd != NULL) DestroyWindow(g_hwnd);
}

void Keyboard_Show(HINSTANCE instance)
{
    int x = 0;
    int y = 0;

    (void)instance;

    if (g_hwnd == NULL)
    {
        DWORD exStyle = WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE;
        DWORD style = WS_POPUP | WS_BORDER;
        RECT frame;

        frame.left = frame.top = 0;
        frame.right = WINDOW_W;
        frame.bottom = WINDOW_H;
        AdjustWindowRectEx(&frame, style, FALSE, exStyle);

        g_hwnd = CreateWindowExW(
            exStyle,
            APP_CLASS_KEYBOARD,
            APP_SHORT,
            style,
            0, 0,
            frame.right - frame.left,
            frame.bottom - frame.top,
            NULL, NULL, g_inst, NULL);

        if (g_hwnd == NULL)
        {
            MessageBoxW(NULL, L"ساخت پنجره کیبورد ممکن نشد.", APP_TITLE,
                        MB_OK | MB_ICONERROR);
            return;
        }

        g_fontKey = CreateFontW(-19, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                                CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Tahoma");

        g_fontTitle = CreateFontW(-15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Tahoma");

        g_bgBrush = CreateSolidBrush(RGB(18, 19, 29));

        BuildLayout();

        /* Watch foreground changes so we know where to type. */
        g_hook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
                                 NULL, ForegroundProc, 0, 0,
                                 WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

        RememberIfExternal(GetForegroundWindow());

        /* Restore the previous position when it is still on a monitor;
         * otherwise sit at the bottom centre of the work area. */
        {
            BOOL placed = FALSE;

            if (Settings_LoadKeyboardPos(&x, &y))
            {
                RECT probe;

                probe.left = x;
                probe.top = y;
                probe.right = x + WINDOW_W;
                probe.bottom = y + WINDOW_H;

                if (MonitorFromRect(&probe, MONITOR_DEFAULTTONULL) != NULL)
                    placed = TRUE;
            }

            if (!placed)
            {
                RECT work;

                SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
                x = work.left + (work.right - work.left - WINDOW_W) / 2;
                y = work.bottom - WINDOW_H - 12;

                if (y < work.top) y = work.top;
            }
        }

        SetWindowPos(g_hwnd, HWND_TOPMOST, x, y, 0, 0,
                     SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOACTIVATE);
    }
    else
    {
        ShowWindow(g_hwnd, SW_SHOWNOACTIVATE);
        SetWindowPos(g_hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }

    InvalidateRect(g_hwnd, NULL, FALSE);
    Main_UpdateLanguageLabel(g_persian);
}

void Keyboard_Toggle(HINSTANCE instance)
{
    if (Keyboard_IsVisible())
        Keyboard_Hide();
    else
        Keyboard_Show(instance);
}
