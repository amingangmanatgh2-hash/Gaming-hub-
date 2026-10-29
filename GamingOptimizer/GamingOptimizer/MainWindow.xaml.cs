using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using GamingHubOptimizer.Core;

namespace GamingHubOptimizer
{
    public partial class MainWindow : Window
    {
        readonly Dictionary<CheckBox, Tweak> _map = new Dictionary<CheckBox, Tweak>();
        SystemSnapshot _snap;

        public MainWindow()
        {
            InitializeComponent();
            BuildTweaks();
            Scan();
            Log("خوش آمدید! اول داشبورد را ببینید، بعد از تب «پروفایل‌ها» یک پروفایل اعمال کنید.");
        }

        void Log(string m) { TxtLog.AppendText("[" + DateTime.Now.ToString("HH:mm:ss") + "] " + m + Environment.NewLine); TxtLog.ScrollToEnd(); }

        void BuildTweaks()
        {
            foreach (var t in TweakService.All()) {
                var cb = new CheckBox { Content = t.TitleFa + "  —  " + t.DescFa, IsChecked = t.DefaultChecked, Tag = t.Id };
                _map[cb] = t;
                if (t.Category == "FPS") PanelFps.Children.Add(cb);
                else if (t.Category == "Quality") PanelQuality.Children.Add(cb);
                else PanelDisplay.Children.Add(cb);
            }
        }

        void Scan()
        {
            try {
                _snap = SystemInfoService.Scan();
                TxtSystem.Text = "ویندوز: " + _snap.Os + "\nپردازنده: " + _snap.Cpu + "  (" + _snap.Cores + " رشته)\nکارت گرافیک: " + _snap.Gpu + "\nرم: " + _snap.RamGb + " GB\nدیسپلی: " + _snap.Resolution + " @ " + _snap.RefreshRate + "Hz";
                TxtScore.Text = "امتیاز گیمینگ سیستم: " + _snap.Score + " / 100";
                TxtVerdict.Text = _snap.VerdictFa;
                TxtDisplay.Text = SystemInfoService.DisplayReport(_snap);
                TxtClean.Text = "حجم قابل پاک‌سازی (Temp + Shader Cache): حدود " + CleanerService.TotalMb() + " MB";
            } catch (Exception ex) { Log("خطای اسکن: " + ex.Message); }
        }

        void BtnScan_Click(object s, RoutedEventArgs e) { Scan(); Log("اسکن دوباره انجام شد."); }

        void BtnClean_Click(object s, RoutedEventArgs e)
        {
            BtnClean.IsEnabled = false;
            string r = CleanerService.Clean(Log);
            Log(r); TxtClean.Text = r; BtnClean.IsEnabled = true;
        }

        void SelectProfile(string name)
        {
            foreach (var kv in _map) {
                var t = kv.Value; bool on = false;
                if (name == "fps") on = t.Category == "FPS";
                else if (name == "quality") on = t.Category == "Quality" || t.Category == "Display" || t.Id == "gamemode";
                else on = t.DefaultChecked; // balanced
                // FPS and Quality visual tweaks conflict — profile decides:
                if (name == "fps" && t.Id == "visualquality") on = false;
                if (name == "quality" && t.Id == "visualfps") on = false;
                kv.Key.IsChecked = on;
            }
            Log("پروفایل انتخاب شد: " + name + " — حالا «اعمال» را بزنید.");
        }
        void BtnProfileFps_Click(object s, RoutedEventArgs e) { SelectProfile("fps"); }
        void BtnProfileBalanced_Click(object s, RoutedEventArgs e) { SelectProfile("balanced"); }
        void BtnProfileQuality_Click(object s, RoutedEventArgs e) { SelectProfile("quality"); }

        void BtnApply_Click(object s, RoutedEventArgs e)
        {
            var chosen = _map.Where(kv => kv.Key.IsChecked == true).Select(kv => kv.Value).ToList();
            if (chosen.Count == 0) { Log("هیچ تنظیمی انتخاب نشده!"); return; }
            if (MessageBox.Show("قبل از اعمال، از تنظیمات فعلی بکاپ گرفته می‌شود و هر وقت خواستید از دکمه‌ی «ری‌استور» برمی‌گردد.\nادامه می‌دهید؟", "Gaming Hub Optimizer", MessageBoxButton.YesNo, MessageBoxImage.Question) != MessageBoxResult.Yes) return;
            Log(TweakService.Apply(chosen, Log));
        }
        void BtnRestore_Click(object s, RoutedEventArgs e) { Log(TweakService.RestoreLast(Log)); }
        void BtnRestorePoint_Click(object s, RoutedEventArgs e) { TweakService.CreateRestorePoint(Log); }

        void BtnOpenDisplay_Click(object s, RoutedEventArgs e) { try { Process.Start("ms-settings:display"); } catch { } }
        void BtnOpenGraphics_Click(object s, RoutedEventArgs e) { try { Process.Start("ms-settings:display-advancedgraphics"); } catch { } }
    }
}
