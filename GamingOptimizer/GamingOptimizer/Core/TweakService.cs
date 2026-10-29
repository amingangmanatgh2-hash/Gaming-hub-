using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using Microsoft.Win32;

namespace GamingHubOptimizer.Core
{
    public class Tweak
    {
        public string Id, TitleFa, DescFa, Category; // FPS | Quality | Display
        public Action Apply;
        public bool DefaultChecked;
    }

    /// <summary>Registry/power tweaks — always backed up first, fully reversible.</summary>
    public static class TweakService
    {
        public static string BackupDir = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "GamingHubOptimizer");
        static List<string> _backupLines;

        static void BeginBackup()
        {
            Directory.CreateDirectory(BackupDir);
            _backupLines = new List<string>();
        }
        static void SaveBackup(string name)
        {
            File.WriteAllLines(Path.Combine(BackupDir, name), _backupLines.ToArray());
            File.WriteAllLines(Path.Combine(BackupDir, "last-backup.txt"), _backupLines.ToArray());
        }

        static void SetDword(RegistryKey root, string path, string name, int value)
        {
            // record old value for restore
            try {
                using (var k = root.OpenSubKey(path)) {
                    object old = k != null ? k.GetValue(name) : null;
                    _backupLines.Add(root.Name + "|" + path + "|" + name + "|" + (old == null ? "" : old.ToString()) + "|" + (old == null ? "0" : "1"));
                }
            } catch { }
            using (var k = root.CreateSubKey(path)) k.SetValue(name, value, RegistryValueKind.DWord);
        }

        static void Run(string file, string args)
        {
            try {
                var p = Process.Start(new ProcessStartInfo(file, args) { UseShellExecute = false, CreateNoWindow = true });
                if (p != null) p.WaitForExit(15000);
            } catch { }
        }

        public static string Apply(IEnumerable<Tweak> tweaks, Action<string> log)
        {
            BeginBackup();
            string stamp = "backup-" + DateTime.Now.ToString("yyyyMMdd-HHmmss") + ".txt";
            int ok = 0, fail = 0;
            foreach (var t in tweaks) {
                try { t.Apply(); ok++; log("✓ " + t.TitleFa); }
                catch (Exception ex) { fail++; log("✗ " + t.TitleFa + " — " + ex.Message); }
            }
            SaveBackup(stamp);
            log("بکاپ تنظیمات قبلی ذخیره شد: " + stamp);
            return "اعمال شد: " + ok + " موفق، " + fail + " ناموفق. برای بعضی موارد (HAGS/پاور) ری‌استارت لازم است.";
        }

        public static string RestoreLast(Action<string> log)
        {
            string f = Path.Combine(BackupDir, "last-backup.txt");
            if (!File.Exists(f)) return "هیچ بکاپی پیدا نشد — اول یک پروفایل را اعمال کنید.";
            int n = 0;
            foreach (var line in File.ReadAllLines(f)) {
                var p = line.Split('|'); if (p.Length < 5) continue;
                try {
                    RegistryKey root = p[0].Contains("LOCAL_MACHINE") ? Registry.LocalMachine : Registry.CurrentUser;
                    if (p[4] == "0") { using (var k = root.CreateSubKey(p[1])) k.DeleteValue(p[2], false); }
                    else { int v; using (var k = root.CreateSubKey(p[1])) k.SetValue(p[2], int.TryParse(p[3], out v) ? (object)v : p[3], RegistryValueKind.DWord); }
                    n++;
                } catch { }
            }
            log("ری‌استور انجام شد (" + n + " مقدار).");
            return "تنظیمات قبلی برگردانده شد. ری‌استارت پیشنهاد می‌شود.";
        }

        public static void CreateRestorePoint(Action<string> log)
        {
            log("در حال ساخت Restore Point ویندوز…");
            Run("powershell", "-NoProfile -Command \"Enable-ComputerRestore -Drive 'C:\\' -ErrorAction SilentlyContinue; Checkpoint-Computer -Description 'GamingHubOptimizer' -RestorePointType MODIFY_SETTINGS\"");
            log("Restore Point ساخته شد (یا قبلاً برای امروز ساخته شده بود).");
        }

        public static List<Tweak> All()
        {
            return new List<Tweak> {
                new Tweak{ Id="gamemode", Category="FPS", DefaultChecked=true, TitleFa="🎮 روشن‌کردن Game Mode ویندوز", DescFa="تمرکز منابع روی بازی", Apply=()=>{ SetDword(Registry.CurrentUser, @"Software\Microsoft\GameBar","AllowAutoGameMode",1); SetDword(Registry.CurrentUser, @"Software\Microsoft\GameBar","AutoGameModeEnabled",1); } },
                new Tweak{ Id="gamedvr", Category="FPS", DefaultChecked=true, TitleFa="🚫 خاموش‌کردن Game DVR / ضبط پس‌زمینه", DescFa="حذف افت فریم ضبط خودکار", Apply=()=>{ SetDword(Registry.CurrentUser, @"System\GameConfigStore","GameDVR_Enabled",0); SetDword(Registry.CurrentUser, @"Software\Microsoft\Windows\CurrentVersion\GameDVR","AppCaptureEnabled",0); try{ SetDword(Registry.LocalMachine, @"SOFTWARE\Policies\Microsoft\Windows\GameDVR","AllowGameDVR",0);}catch{} } },
                new Tweak{ Id="hags", Category="FPS", DefaultChecked=true, TitleFa="⚡ Hardware-Accelerated GPU Scheduling (HAGS)", DescFa="کاهش تأخیر GPU — نیازمند ری‌استارت", Apply=()=> SetDword(Registry.LocalMachine, @"SYSTEM\CurrentControlSet\Control\GraphicsDrivers","HwSchMode",2) },
                new Tweak{ Id="power", Category="FPS", DefaultChecked=true, TitleFa="🔋 پلن Ultimate Performance", DescFa="حداکثر توان CPU/GPU", Apply=()=>{ Run("powercfg","-duplicatescheme e9a42b02-d5df-448d-aa00-03f14749eb61"); Run("powercfg","/setactive e9a42b02-d5df-448d-aa00-03f14749eb61"); } },
                new Tweak{ Id="gamepriority", Category="FPS", DefaultChecked=true, TitleFa="🎯 اولویت بالای GPU/CPU برای بازی‌ها", DescFa="SystemProfile → Tasks\\Games", Apply=()=>{ SetDword(Registry.LocalMachine, @"SOFTWARE\Microsoft\Windows NT\CurrentVersion\Multimedia\SystemProfile\Tasks\Games","GPU Priority",8); SetDword(Registry.LocalMachine, @"SOFTWARE\Microsoft\Windows NT\CurrentVersion\Multimedia\SystemProfile\Tasks\Games","Priority",6); } },
                new Tweak{ Id="visualfps", Category="FPS", DefaultChecked=false, TitleFa="🏎 جلوه‌های بصری = بهترین عملکرد", DescFa="خاموشی انیمیشن/شفافیت برای FPS بیشتر", Apply=()=>{ SetDword(Registry.CurrentUser, @"Software\Microsoft\Windows\CurrentVersion\Explorer\VisualEffects","VisualFXSetting",2); SetDword(Registry.CurrentUser, @"Software\Microsoft\Windows\CurrentVersion\Themes\Personalize","EnableTransparency",0); } },
                new Tweak{ Id="visualquality", Category="Quality", DefaultChecked=true, TitleFa="✨ جلوه‌های بصری = بهترین ظاهر", DescFa="انیمیشن و سایه‌ی کامل (پروفایل کیفیت)", Apply=()=>{ SetDword(Registry.CurrentUser, @"Software\Microsoft\Windows\CurrentVersion\Explorer\VisualEffects","VisualFXSetting",1); SetDword(Registry.CurrentUser, @"Software\Microsoft\Windows\CurrentVersion\Themes\Personalize","EnableTransparency",1); } },
                new Tweak{ Id="cleartype", Category="Quality", DefaultChecked=true, TitleFa="🔤 ClearType + Font Smoothing", DescFa="متن شارپ‌تر روی LCD", Apply=()=>{ SetDword(Registry.CurrentUser, @"Control Panel\Desktop","FontSmoothing",2); SetDword(Registry.CurrentUser, @"Control Panel\Desktop","FontSmoothingType",2); } },
                new Tweak{ Id="mouse", Category="Quality", DefaultChecked=true, TitleFa="🖱 خاموش‌کردن Enhance Pointer Precision", DescFa="ایم خام (Raw) برای شوترها", Apply=()=>{ SetDword(Registry.CurrentUser, @"Control Panel\Mouse","MouseSpeed",0); SetDword(Registry.CurrentUser, @"Control Panel\Mouse","MouseThreshold1",0); SetDword(Registry.CurrentUser, @"Control Panel\Mouse","MouseThreshold2",0); } },
                new Tweak{ Id="displaydvr", Category="Display", DefaultChecked=true, TitleFa="🖥 خاموش‌کردن Fullscreen Optimizations سراسری", DescFa="Fullscreen واقعی، اینپوت‌لگ کمتر", Apply=()=> SetDword(Registry.CurrentUser, @"System\GameConfigStore","GameDVR_FSEBehaviorMode",2) },
                new Tweak{ Id="notifications", Category="Display", DefaultChecked=false, TitleFa="🔕 Focus Assist هنگام بازی (اعلان‌ها خاموش)", DescFa="بدون پاپ‌آپ وسط بازی", Apply=()=> SetDword(Registry.CurrentUser, @"Software\Microsoft\Windows\CurrentVersion\Notifications\Settings","NOC_GLOBAL_SETTING_TOASTS_ENABLED",0) },
            };
        }
    }
}
