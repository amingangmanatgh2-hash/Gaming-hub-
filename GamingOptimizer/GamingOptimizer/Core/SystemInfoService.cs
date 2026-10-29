using System;
using System.Management;
using System.Text;

namespace GamingHubOptimizer.Core
{
    public class SystemSnapshot
    {
        public string Os, Cpu, Gpu, Resolution;
        public int Cores, RefreshRate;
        public double RamGb;
        public int Score;
        public string VerdictFa;
    }

    public static class SystemInfoService
    {
        static string Query(string cls, string prop)
        {
            try {
                using (var s = new ManagementObjectSearcher("SELECT " + prop + " FROM " + cls))
                foreach (ManagementObject o in s.Get()) { var v = o[prop]; if (v != null) return v.ToString(); }
            } catch { }
            return "—";
        }
        static long QueryLong(string cls, string prop)
        {
            try {
                using (var s = new ManagementObjectSearcher("SELECT " + prop + " FROM " + cls))
                foreach (ManagementObject o in s.Get()) { var v = o[prop]; if (v != null) return Convert.ToInt64(v); }
            } catch { }
            return 0;
        }

        public static SystemSnapshot Scan()
        {
            var snap = new SystemSnapshot();
            snap.Os = Query("Win32_OperatingSystem", "Caption");
            snap.Cpu = Query("Win32_Processor", "Name").Trim();
            snap.Cores = (int)QueryLong("Win32_Processor", "NumberOfLogicalProcessors");
            snap.RamGb = Math.Round(QueryLong("Win32_ComputerSystem", "TotalPhysicalMemory") / 1073741824.0, 1);
            snap.Gpu = Query("Win32_VideoController", "Name");
            long w = QueryLong("Win32_VideoController", "CurrentHorizontalResolution");
            long h = QueryLong("Win32_VideoController", "CurrentVerticalResolution");
            snap.RefreshRate = (int)QueryLong("Win32_VideoController", "CurrentRefreshRate");
            snap.Resolution = (w > 0 ? w + " × " + h : System.Windows.SystemParameters.PrimaryScreenWidth + " × " + System.Windows.SystemParameters.PrimaryScreenHeight);

            int score = 30;
            if (snap.Cores >= 12) score += 25; else if (snap.Cores >= 8) score += 20; else if (snap.Cores >= 4) score += 12;
            if (snap.RamGb >= 32) score += 25; else if (snap.RamGb >= 16) score += 20; else if (snap.RamGb >= 8) score += 10;
            if (snap.RefreshRate >= 144) score += 20; else if (snap.RefreshRate >= 75) score += 12; else if (snap.RefreshRate >= 60) score += 6;
            snap.Score = Math.Min(99, score);
            snap.VerdictFa = snap.Score >= 80 ? "عالی — آماده‌ی بازی رقابتی 🏆" : snap.Score >= 60 ? "خوب — با پروفایل FPS بهتر هم می‌شود ⚡" : snap.Score >= 40 ? "متوسط — پروفایل FPS + پاک‌سازی پیشنهاد می‌شود 🔧" : "ضعیف — پروفایل Max FPS را اعمال کنید 🚀";
            return snap;
        }

        public static string DisplayReport(SystemSnapshot s)
        {
            var sb = new StringBuilder();
            sb.AppendLine("رزولوشن فعلی: " + s.Resolution);
            sb.AppendLine("نرخ نوسازی: " + (s.RefreshRate > 0 ? s.RefreshRate + " Hz" : "نامشخص"));
            sb.AppendLine(s.RefreshRate > 0 && s.RefreshRate < 75
                ? "⚠ مانیتورت روی کمتر از ۷۵ هرتز است! از تنظیمات دیسپلی ویندوز بالاترین Hz را انتخاب کن."
                : "✓ نرخ نوسازی مناسب است.");
            sb.AppendLine("GPU: " + s.Gpu);
            sb.AppendLine("پیشنهاد: G-Sync/FreeSync را از پنل کارت گرافیک روشن کن، داخل بازی V-Sync را خاموش و FPS را روی Hz مانیتور قفل کن.");
            return sb.ToString();
        }
    }
}
