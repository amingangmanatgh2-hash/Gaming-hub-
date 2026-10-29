using System;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;

namespace AMIN_Keyboard_Clipboard
{
    /// <summary>
    /// Creates a real Startup .lnk (via WScript.Shell COM, no admin rights needed).
    /// Falls back to copying the exe into the Startup folder if COM is unavailable.
    /// </summary>
    public static class StartupShortcut
    {
        public const string ShortcutName = "AMIN Keyboard & Clipboard.lnk";

        public static string ShortcutPath
        {
            get { return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Startup), ShortcutName); }
        }

        public static bool IsEnabled() { return File.Exists(ShortcutPath); }

        public static void Enable(string exePath)
        {
            if (string.IsNullOrEmpty(exePath))
                exePath = Assembly.GetExecutingAssembly().Location;
            Create(exePath);
        }

        public static void Disable()
        {
            try
            {
                if (File.Exists(ShortcutPath)) File.Delete(ShortcutPath);
            }
            catch
            {
                // best effort
            }
        }

        private static void Create(string exePath)
        {
            var startupDir = Environment.GetFolderPath(Environment.SpecialFolder.Startup);
            var shellType = Type.GetTypeFromProgID("WScript.Shell");

            if (shellType == null)
            {
                // Fallback: dropping the exe in the Startup folder also auto-runs it.
                File.Copy(exePath, Path.Combine(startupDir, "AMIN_Keyboard_Clipboard.exe"), true);
                return;
            }

            object shell = null;
            object shortcut = null;
            try
            {
                shell = Activator.CreateInstance(shellType);
                shortcut = shellType.InvokeMember("CreateShortcut",
                    BindingFlags.InvokeMethod | BindingFlags.Instance | BindingFlags.Public,
                    null, shell, new object[] { ShortcutPath });

                SetProp(shortcut, "TargetPath", exePath);
                SetProp(shortcut, "WorkingDirectory", Path.GetDirectoryName(exePath) ?? "");
                SetProp(shortcut, "Description", "AMIN Keyboard & Clipboard — Persian/English keyboard & clipboard manager");
                SetProp(shortcut, "WindowStyle", 1);

                shellType.InvokeMember("Save",
                    BindingFlags.InvokeMethod | BindingFlags.Instance | BindingFlags.Public,
                    null, shortcut, null);
            }
            catch
            {
                File.Copy(exePath, Path.Combine(startupDir, "AMIN_Keyboard_Clipboard.exe"), true);
            }
            finally
            {
                if (shortcut != null) Marshal.ReleaseComObject(shortcut);
                if (shell != null) Marshal.ReleaseComObject(shell);
            }
        }

        private static void SetProp(object target, string name, object value)
        {
            target.GetType().InvokeMember(name,
                BindingFlags.SetProperty | BindingFlags.Instance | BindingFlags.Public,
                null, target, new object[] { value });
        }
    }
}
