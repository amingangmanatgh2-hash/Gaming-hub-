using System;
using System.Runtime.InteropServices;

namespace AMIN_Keyboard_Clipboard
{
    /// <summary>
    /// Low-level Win32 input helpers: SendInput (Unicode text + special keys),
    /// foreground window handling and global hotkeys.
    /// </summary>
    public static class NativeInput
    {
        // Virtual key codes
        public const uint VK_BACK = 0x08;
        public const uint VK_RETURN = 0x0D;
        public const uint VK_SPACE = 0x20;

        private const uint KEYEVENTF_KEYUP = 0x0002;
        private const uint KEYEVENTF_UNICODE = 0x0008;
        private const uint INPUT_KEYBOARD = 1;
        private const int SW_RESTORE = 9;

        [DllImport("user32.dll")] private static extern IntPtr GetForegroundWindow();
        [DllImport("user32.dll")] private static extern bool SetForegroundWindow(IntPtr hWnd);
        [DllImport("user32.dll")] private static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
        [DllImport("user32.dll")] private static extern bool RegisterHotKey(IntPtr hWnd, int id, uint fsModifiers, uint vk);
        [DllImport("user32.dll")] private static extern bool UnregisterHotKey(IntPtr hWnd, int id);
        [DllImport("user32.dll")] private static extern uint SendInput(uint nInputs, INPUT[] pInputs, int cbSize);

        private static IntPtr NativeGetForegroundWindow() { return GetForegroundWindow(); }
        private static bool NativeRegisterHotKey(IntPtr hWnd, int id, uint fsModifiers, uint vk) { return RegisterHotKey(hWnd, id, fsModifiers, vk); }
        private static bool NativeUnregisterHotKey(IntPtr hWnd, int id) { return UnregisterHotKey(hWnd, id); }

        public static IntPtr GetForegroundWindow() { return NativeGetForegroundWindow(); }

        public static void ActivateWindow(IntPtr hWnd)
        {
            if (hWnd != IntPtr.Zero)
            {
                ShowWindow(hWnd, SW_RESTORE);
                SetForegroundWindow(hWnd);
            }
        }

        public static bool RegisterHotKey(IntPtr hWnd, int id, uint modifiers, uint vk)
        {
            return NativeRegisterHotKey(hWnd, id, modifiers, vk);
        }

        public static bool UnregisterHotKey(IntPtr hWnd, int id)
        {
            return NativeUnregisterHotKey(hWnd, id);
        }

        /// <summary>Sends a token: "Enter", "Backspace", "Space", a Unicode char, or a string.</summary>
        public static void SendToken(string token)
        {
            if (string.IsNullOrEmpty(token)) return;
            switch (token)
            {
                case "Enter": SendSpecialKey(VK_RETURN); break;
                case "Backspace": SendSpecialKey(VK_BACK); break;
                case "Space": SendSpecialKey(VK_SPACE); break;
                default: SendText(token); break;
            }
        }

        public static void SendText(string text)
        {
            if (string.IsNullOrEmpty(text)) return;
            for (int i = 0; i < text.Length; i++)
                SendChar(text[i]);
        }

        public static void SendSpecialKey(uint vk)
        {
            SendVk(vk, 0);
            SendVk(vk, KEYEVENTF_KEYUP);
        }

        private static void SendChar(char c)
        {
            // Unicode path: wVk=0, wScan=char, KEYEVENTF_UNICODE.
            // Astral chars (surrogate pairs) are sent code unit by code unit, which
            // Windows reassembles correctly.
            SendKey(0, c, KEYEVENTF_UNICODE);
            SendKey(0, c, KEYEVENTF_UNICODE | KEYEVENTF_KEYUP);
        }

        private static void SendVk(uint vk, uint flags)
        {
            SendKey(vk, 0, flags);
        }

        private static void SendKey(ushort vk, char scan, uint flags)
        {
            INPUT input = new INPUT();
            input.type = INPUT_KEYBOARD;
            input.u.ki.wVk = vk;
            input.u.ki.wScan = scan;
            input.u.ki.dwFlags = flags;
            SendInput(1, new[] { input }, Marshal.SizeOf(typeof(INPUT)));
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct INPUT
    {
        public uint type;
        public INPUT_UNION u;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct INPUT_UNION
    {
        public KEYBDINPUT ki;
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
}
