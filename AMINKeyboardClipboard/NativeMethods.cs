using System;
using System.Runtime.InteropServices;

namespace AMINKeyboardClipboard
{
    /// <summary>
    /// Thin, honest wrapper around the handful of user32 APIs this app needs.
    /// Every member here exists on Windows 7 and later, so Windows 10 1803 is fine.
    /// </summary>
    internal static class NativeMethods
    {
        // ---- Window messages -------------------------------------------------
        public const int WM_CLIPBOARDUPDATE = 0x031D;
        public const int WM_HOTKEY = 0x0312;

        // ---- Hot key modifiers ----------------------------------------------
        public const uint MOD_ALT = 0x0001;
        public const uint MOD_CONTROL = 0x0002;
        public const uint MOD_SHIFT = 0x0004;
        public const uint MOD_NOREPEAT = 0x4000;
        public const uint MOD_WIN = 0x0008;

        // ---- Virtual keys (only the ones we actually send) -------------------
        public const ushort VK_BACK = 0x08;
        public const ushort VK_TAB = 0x09;
        public const ushort VK_RETURN = 0x0D;
        public const ushort VK_LEFT = 0x25;

        // ---- SendInput -------------------------------------------------------
        public const uint INPUT_KEYBOARD = 1;
        public const uint KEYEVENTF_KEYUP = 0x0002;
        public const uint KEYEVENTF_UNICODE = 0x0004;

        // ---- SetWinEventHook -------------------------------------------------
        public const uint EVENT_SYSTEM_FOREGROUND = 0x0003;
        public const uint WINEVENT_OUTOFCONTEXT = 0x0000;
        public const uint WINEVENT_SKIPOWNPROCESS = 0x0002;

        // ---- Window placement / restore -------------------------------------
        public const int SW_RESTORE = 9;

        [DllImport("user32.dll", SetLastError = true)]
        public static extern bool AddClipboardFormatListener(IntPtr hwnd);

        [DllImport("user32.dll", SetLastError = true)]
        public static extern bool RemoveClipboardFormatListener(IntPtr hwnd);

        [DllImport("user32.dll", SetLastError = true)]
        public static extern bool RegisterHotKey(IntPtr hWnd, int id, uint fsModifiers, uint vk);

        [DllImport("user32.dll", SetLastError = true)]
        public static extern bool UnregisterHotKey(IntPtr hWnd, int id);

        [DllImport("user32.dll")]
        public static extern IntPtr GetForegroundWindow();

        [DllImport("user32.dll")]
        public static extern bool SetForegroundWindow(IntPtr hWnd);

        [DllImport("user32.dll")]
        public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);

        [DllImport("user32.dll")]
        public static extern bool IsWindow(IntPtr hWnd);

        [DllImport("user32.dll")]
        public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint processId);

        [DllImport("user32.dll", SetLastError = true)]
        public static extern uint SendInput(uint nInputs, INPUT[] pInputs, int cbSize);

        [DllImport("user32.dll", SetLastError = true)]
        public static extern IntPtr SetWinEventHook(
            uint eventMin, uint eventMax, IntPtr hmodWinEventProc,
            WinEventDelegate callback, uint idProcess, uint idThread, uint flags);

        [DllImport("user32.dll")]
        public static extern bool UnhookWinEvent(IntPtr hWinEventHook);

        [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        public static extern uint RegisterWindowMessage(string lpString);

        [DllImport("user32.dll", CharSet = CharSet.Unicode)]
        public static extern bool PostMessage(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam);

        // ---- Extended window styles -----------------------------------------
        public const int GWL_EXSTYLE = -20;
        public const int WS_EX_TOOLWINDOW = 0x00000080;
        public const int WS_EX_NOACTIVATE = 0x08000000;

        [DllImport("user32.dll", EntryPoint = "GetWindowLongPtr", SetLastError = true)]
        private static extern IntPtr GetWindowLongPtr64(IntPtr hWnd, int nIndex);

        [DllImport("user32.dll", EntryPoint = "GetWindowLong", SetLastError = true)]
        private static extern int GetWindowLong32(IntPtr hWnd, int nIndex);

        [DllImport("user32.dll", EntryPoint = "SetWindowLongPtr", SetLastError = true)]
        private static extern IntPtr SetWindowLongPtr64(IntPtr hWnd, int nIndex, IntPtr dwNewLong);

        [DllImport("user32.dll", EntryPoint = "SetWindowLong", SetLastError = true)]
        private static extern int SetWindowLong32(IntPtr hWnd, int nIndex, int dwNewLong);

        /// <summary>
        /// Marks a window as a non-activating tool window. This is what keeps an
        /// on-screen keyboard from stealing focus away from the app being typed into.
        /// Uses the bitness-correct P/Invoke on both x86 and x64.
        /// </summary>
        public static void MakeNonActivating(IntPtr hwnd, bool hideFromTaskbar)
        {
            if (hwnd == IntPtr.Zero) return;

            long style = IntPtr.Size == 8
                ? GetWindowLongPtr64(hwnd, GWL_EXSTYLE).ToInt64()
                : GetWindowLong32(hwnd, GWL_EXSTYLE);

            style |= WS_EX_NOACTIVATE;
            if (hideFromTaskbar)
                style |= WS_EX_TOOLWINDOW;

            if (IntPtr.Size == 8)
                SetWindowLongPtr64(hwnd, GWL_EXSTYLE, new IntPtr(style));
            else
                SetWindowLong32(hwnd, GWL_EXSTYLE, unchecked((int)style));
        }

        public static readonly IntPtr HWND_BROADCAST = new IntPtr(0xFFFF);

        public delegate void WinEventDelegate(
            IntPtr hWinEventHook, uint eventType, IntPtr hwnd,
            int idObject, int idChild, uint idEventThread, uint msEventTime);

        /// <summary>Message id used to wake up an already running instance.</summary>
        public static readonly uint ShowMainWindowMessage = RegisterWindowMessage("AMIN_KB_SHOW_MAIN_WINDOW");

        public static void BroadcastShowMainWindow()
        {
            if (ShowMainWindowMessage != 0)
                PostMessage(HWND_BROADCAST, ShowMainWindowMessage, IntPtr.Zero, IntPtr.Zero);
        }

        // ---- SendInput structures -------------------------------------------
        // Full, correctly sized union. Using a truncated union changes sizeof(INPUT)
        // and makes SendInput fail with ERROR_INVALID_PARAMETER.
        [StructLayout(LayoutKind.Sequential)]
        public struct INPUT
        {
            public uint type;
            public InputUnion U;
        }

        [StructLayout(LayoutKind.Explicit)]
        public struct InputUnion
        {
            [FieldOffset(0)]
            public MOUSEINPUT mi;

            [FieldOffset(0)]
            public KEYBDINPUT ki;

            [FieldOffset(0)]
            public HARDWAREINPUT hi;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct MOUSEINPUT
        {
            public int dx;
            public int dy;
            public uint mouseData;
            public uint dwFlags;
            public uint time;
            public IntPtr dwExtraInfo;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct KEYBDINPUT
        {
            public ushort wVk;
            public ushort wScan;
            public uint dwFlags;
            public uint time;
            public IntPtr dwExtraInfo;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct HARDWAREINPUT
        {
            public uint uMsg;
            public ushort wParamL;
            public ushort wParamH;
        }

        // ---- Helpers ---------------------------------------------------------

        /// <summary>Builds one Unicode character key-down + key-up pair.</summary>
        public static INPUT[] UnicodeChar(char c)
        {
            return new[]
            {
                MakeKey(0, c, KEYEVENTF_UNICODE),
                MakeKey(0, c, KEYEVENTF_UNICODE | KEYEVENTF_KEYUP)
            };
        }

        /// <summary>Builds one virtual-key down + up pair.</summary>
        public static INPUT[] VirtualKey(ushort vk)
        {
            return new[]
            {
                MakeKey(vk, 0, 0),
                MakeKey(vk, 0, KEYEVENTF_KEYUP)
            };
        }

        public static INPUT MakeKey(ushort vk, ushort scan, uint flags)
        {
            return new INPUT
            {
                type = INPUT_KEYBOARD,
                U = new InputUnion
                {
                    ki = new KEYBDINPUT
                    {
                        wVk = vk,
                        wScan = scan,
                        dwFlags = flags,
                        time = 0,
                        dwExtraInfo = IntPtr.Zero
                    }
                }
            };
        }

        public static int InputSize
        {
            get { return Marshal.SizeOf(typeof(INPUT)); }
        }
    }
}
