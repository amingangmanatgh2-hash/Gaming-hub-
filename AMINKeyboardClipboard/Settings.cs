using System;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using Microsoft.Win32;

namespace AMINKeyboardClipboard
{
    /// <summary>
    /// Local, per-user settings: first-run flag, startup entry, window position.
    /// Nothing is stored anywhere except the current user's own profile.
    /// </summary>
    internal static class Settings
    {
        private const string StartupValueName = "AMIN Keyboard Clipboard";
        private const string RunKeyPath = @"Software\Microsoft\Windows\CurrentVersion\Run";

        public static string DataFolder
        {
            get
            {
                string path = Path.Combine(
                    Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                    "AMINKeyboardClipboard");

                Directory.CreateDirectory(path);
                return path;
            }
        }

        private static string FirstRunFile
        {
            get { return Path.Combine(DataFolder, "first-run.done"); }
        }

        private static string KeyboardBoundsFile
        {
            get { return Path.Combine(DataFolder, "keyboard-bounds.txt"); }
        }

        private static string ExePath
        {
            get { return Assembly.GetExecutingAssembly().Location; }
        }

        // ---- First run -------------------------------------------------------

        public static bool FirstRunCompleted()
        {
            try
            {
                return File.Exists(FirstRunFile);
            }
            catch
            {
                return true;
            }
        }

        public static void SetFirstRunCompleted()
        {
            try
            {
                File.WriteAllText(FirstRunFile, "done");
            }
            catch
            {
                // Not being able to persist the flag only means the intro shows again.
            }
        }

        // ---- Startup entry ---------------------------------------------------

        /// <summary>
        /// The startup entry counts as enabled only when it points at *this* exe,
        /// so a stale path from an older install never shows a misleading checked box.
        /// </summary>
        public static bool IsStartupEnabled()
        {
            try
            {
                using (RegistryKey key = Registry.CurrentUser.OpenSubKey(RunKeyPath, false))
                {
                    if (key == null) return false;

                    string value = key.GetValue(StartupValueName) as string;
                    if (string.IsNullOrWhiteSpace(value)) return false;

                    return string.Equals(
                        value.Replace("\"", string.Empty).Trim(),
                        ExePath,
                        StringComparison.OrdinalIgnoreCase);
                }
            }
            catch
            {
                return false;
            }
        }

        public static void SetStartupEnabled(bool enabled)
        {
            using (RegistryKey key = Registry.CurrentUser.OpenSubKey(RunKeyPath, true))
            {
                if (key == null)
                    throw new InvalidOperationException("دسترسی به تنظیمات کاربر ممکن نیست.");

                if (enabled)
                {
                    string quoted = "\"" + ExePath + "\"";
                    key.SetValue(StartupValueName, quoted, RegistryValueKind.String);
                }
                else
                {
                    key.DeleteValue(StartupValueName, false);
                }
            }
        }

        // ---- Floating keyboard position -------------------------------------
        // Stored as four comma separated numbers so the keyboard reopens where
        // the user left it, even after a multi-monitor change.

        public static bool TryLoadKeyboardBounds(out double left, out double top, out double width, out double height)
        {
            left = top = width = height = 0;

            try
            {
                if (!File.Exists(KeyboardBoundsFile)) return false;

                string[] parts = File.ReadAllText(KeyboardBoundsFile)
                    .Split(new[] { ',' }, StringSplitOptions.RemoveEmptyEntries);

                if (parts.Length != 4) return false;

                double l, t, w, h;
                if (!double.TryParse(parts[0], out l)) return false;
                if (!double.TryParse(parts[1], out t)) return false;
                if (!double.TryParse(parts[2], out w)) return false;
                if (!double.TryParse(parts[3], out h)) return false;

                if (w < 200 || h < 150) return false;

                left = l; top = t; width = w; height = h;
                return true;
            }
            catch
            {
                return false;
            }
        }

        public static void SaveKeyboardBounds(double left, double top, double width, double height)
        {
            try
            {
                File.WriteAllText(
                    KeyboardBoundsFile,
                    string.Format(
                        System.Globalization.CultureInfo.InvariantCulture,
                        "{0},{1},{2},{3}",
                        left, top, width, height));
            }
            catch
            {
                // Position persistence is a convenience only.
            }
        }
    }
}
