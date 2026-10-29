using System;
using System.IO;
using System.Runtime.InteropServices;

namespace GamingHubOptimizer.Core
{
    public static class CleanerService
    {
        [DllImport("shell32.dll")] static extern int SHEmptyRecycleBin(IntPtr h, string p, uint f);

        public static string[] Targets()
        {
            string local = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
            return new[] {
                Path.GetTempPath(),
                Path.Combine(local, "Temp"),
                Path.Combine(local, "D3DSCache"),
                Path.Combine(local, @"NVIDIA\DXCache"),
                Path.Combine(local, @"AMD\DxCache"),
            };
        }
        static long SizeOf(string dir)
        {
            long s = 0;
            try { foreach (var f in Directory.GetFiles(dir, "*", SearchOption.AllDirectories)) { try { s += new FileInfo(f).Length; } catch { } } } catch { }
            return s;
        }
        public static double TotalMb()
        {
            long s = 0; foreach (var t in Targets()) s += SizeOf(t);
            return Math.Round(s / 1048576.0, 1);
        }
        public static string Clean(Action<string> log)
        {
            long freed = 0;
            foreach (var t in Targets()) {
                if (!Directory.Exists(t)) continue;
                foreach (var f in Directory.GetFiles(t, "*", SearchOption.AllDirectories)) {
                    try { long len = new FileInfo(f).Length; File.Delete(f); freed += len; } catch { }
                }
                log("پاک شد: " + t);
            }
            try { SHEmptyRecycleBin(IntPtr.Zero, null, 1 | 2); log("سطل زباله خالی شد."); } catch { }
            return "پاک‌سازی تمام شد — آزاد شد: " + Math.Round(freed / 1048576.0, 1) + " MB";
        }
    }
}
