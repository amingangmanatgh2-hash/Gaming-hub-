using System;
using System.Runtime.InteropServices;

namespace AMIN_Keyboard_Clipboard
{
    /// <summary>
    /// Keeps track of which application the user was last working in, so the
    /// virtual keyboard can type into that app instead of the AMIN window itself.
    /// </summary>
    public static class TypingTarget
    {
        /// <summary>Handle of the last external window that had focus, or IntPtr.Zero.</summary>
        public static IntPtr LastExternal { get; set; }

        public static void Type(string token)
        {
            IntPtr target = LastExternal != IntPtr.Zero ? LastExternal : NativeInput.GetForegroundWindow();
            if (target != IntPtr.Zero)
                NativeInput.ActivateWindow(target);
            NativeInput.SendToken(token);
        }
    }
}
