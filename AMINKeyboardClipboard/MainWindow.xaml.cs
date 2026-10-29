using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Interop;

namespace AMINKeyboardClipboard
{
    public partial class MainWindow : Window
    {
        private const int HotKeyId = 7001;
        private const uint Vk_V = 0x56;

        private ClipboardHistoryService clipboardService;
        private FloatingKeyboardWindow keyboardWindow;
        private HwndSource hwndSource;
        private IntPtr hwnd = IntPtr.Zero;
        private bool suppressStartupEvents;

        public MainWindow()
        {
            InitializeComponent();

            Loaded += MainWindow_Loaded;
            Closed += MainWindow_Closed;
        }

        private void MainWindow_Loaded(object sender, RoutedEventArgs e)
        {
            hwnd = new WindowInteropHelper(this).Handle;
            hwndSource = HwndSource.FromHwnd(hwnd);
            if (hwndSource != null)
                hwndSource.AddHook(WndProc);

            clipboardService = new ClipboardHistoryService();
            clipboardService.HistoryChanged += ClipboardService_HistoryChanged;

            bool listening = clipboardService.Start(hwnd);
            ListenerStatusText.Text = listening
                ? "فعال — فقط متن کپی‌شده ثبت می‌شود."
                : "غیرفعال — ویندوز اجازه ثبت شنونده کلیپ‌بورد را نداد. تاریخچه دستی کار می‌کند.";

            DataPathText.Text = Settings.DataFolder;

            suppressStartupEvents = true;
            StartupCheckBox.IsChecked = Settings.IsStartupEnabled();
            suppressStartupEvents = false;

            RegisterClipboardShortcut();
            RefreshClipboardList();

            if (!Settings.FirstRunCompleted())
            {
                var wizard = new FirstRunWindow { Owner = this };
                wizard.ShowDialog();
                Settings.SetFirstRunCompleted();
            }
        }

        // ------------------------------------------------------------------
        // Global hot key
        // ------------------------------------------------------------------

        private void RegisterClipboardShortcut()
        {
            // Win+V is owned by Windows 10 (clipboard history) on most systems, so
            // this registration normally fails — that is expected, not an error.
            bool winV = NativeMethods.RegisterHotKey(
                hwnd,
                HotKeyId,
                NativeMethods.MOD_WIN | NativeMethods.MOD_NOREPEAT,
                Vk_V);

            if (winV)
            {
                ShortcutStatusText.Text = "فعال: Win + V";
                return;
            }

            bool fallback = NativeMethods.RegisterHotKey(
                hwnd,
                HotKeyId,
                NativeMethods.MOD_CONTROL | NativeMethods.MOD_ALT | NativeMethods.MOD_NOREPEAT,
                Vk_V);

            ShortcutStatusText.Text = fallback
                ? "فعال: Ctrl + Alt + V  (کلید Win + V توسط ویندوز یا برنامه دیگری در اختیار است)"
                : "میانبر سراسری در دسترس نیست. برنامه از نوار وظیفه باز می‌شود؛ این نسخه آیکن سینی ندارد.";
        }

        private IntPtr WndProc(IntPtr hWnd, int msg, IntPtr wParam, IntPtr lParam, ref bool handled)
        {
            if (clipboardService == null)
                return IntPtr.Zero;

            if (msg == NativeMethods.WM_CLIPBOARDUPDATE)
            {
                clipboardService.TryReadClipboardText();
            }
            else if (msg == NativeMethods.WM_HOTKEY && wParam.ToInt32() == HotKeyId)
            {
                ToggleWindowFromHotKey();
                handled = true;
            }
            else if (NativeMethods.ShowMainWindowMessage != 0 &&
                     msg == (int)NativeMethods.ShowMainWindowMessage)
            {
                ShowAndActivate();
                handled = true;
            }

            return IntPtr.Zero;
        }

        private void ToggleWindowFromHotKey()
        {
            if (IsVisible && WindowState != WindowState.Minimized && IsActive)
                Hide();
            else
                ShowAndActivate();
        }

        private void ShowAndActivate()
        {
            Show();
            if (WindowState == WindowState.Minimized)
                WindowState = WindowState.Normal;

            Activate();
            Topmost = true;
            Topmost = false;
            Focus();
        }

        // ------------------------------------------------------------------
        // History list
        // ------------------------------------------------------------------

        private void ClipboardService_HistoryChanged()
        {
            // Clipboard events arrive on the UI thread already, but stay defensive.
            if (!Dispatcher.CheckAccess())
            {
                Dispatcher.BeginInvoke(new Action(RefreshClipboardList));
                return;
            }

            RefreshClipboardList();
        }

        private void RefreshClipboardList()
        {
            if (clipboardService == null) return;

            string previousSelection = SelectedFullText();
            string filter = (SearchBox.Text ?? string.Empty).Trim();

            List<string> source = clipboardService.History.ToList();

            if (filter.Length > 0)
            {
                source = source
                    .Where(x => x.IndexOf(filter, StringComparison.OrdinalIgnoreCase) >= 0)
                    .ToList();
            }

            List<ClipboardEntry> rows = source.Select(ClipboardEntry.From).ToList();
            ClipboardList.ItemsSource = rows;

            if (previousSelection != null)
            {
                ClipboardEntry match = rows.FirstOrDefault(
                    r => string.Equals(r.FullText, previousSelection, StringComparison.Ordinal));

                if (match != null)
                    ClipboardList.SelectedItem = match;
            }

            CountText.Text = clipboardService.Count == 0
                ? "خالی"
                : string.Format("{0} مورد از {1}", rows.Count, clipboardService.Count);

            EmptyStateText.Text = filter.Length > 0
                ? "هیچ موردی با این جستجو پیدا نشد."
                : "هنوز متنی کپی نشده است. با Ctrl+C هر متنی را کپی کنید.";

            EmptyStateText.Visibility = rows.Count == 0 ? Visibility.Visible : Visibility.Collapsed;
        }

        private string SelectedFullText()
        {
            var entry = ClipboardList.SelectedItem as ClipboardEntry;
            return entry == null ? null : entry.FullText;
        }

        private void CopySelectedItem()
        {
            string item = SelectedFullText();
            if (string.IsNullOrEmpty(item))
            {
                MessageBox.Show("ابتدا یک مورد را از تاریخچه انتخاب کنید.",
                    "چیزی انتخاب نشده", MessageBoxButton.OK, MessageBoxImage.Information);
                return;
            }

            if (clipboardService.CopyToClipboard(item))
            {
                ReturnToTargetWindow();
            }
            else
            {
                MessageBox.Show("کپی به کلیپ‌بورد ممکن نشد؛ ممکن است برنامه دیگری کلیپ‌بورد را در اختیار داشته باشد.",
                    "خطای کلیپ‌بورد", MessageBoxButton.OK, MessageBoxImage.Warning);
            }
        }

        private void ReturnToTargetWindow()
        {
            IntPtr target = keyboardWindow != null ? keyboardWindow.LastExternalWindow : IntPtr.Zero;
            if (target == IntPtr.Zero || !NativeMethods.IsWindow(target)) return;

            NativeMethods.SetForegroundWindow(target);
        }

        private void ClipboardList_MouseDoubleClick(object sender, MouseButtonEventArgs e)
        {
            // Ignore double clicks on empty space below the last row.
            if (ClipboardList.SelectedItem == null) return;
            CopySelectedItem();
        }

        private void ClipboardList_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.Key == Key.Enter)
            {
                CopySelectedItem();
                e.Handled = true;
            }
            else if (e.Key == Key.Delete)
            {
                DeleteSelectedItem();
                e.Handled = true;
            }
        }

        private void SearchBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            RefreshClipboardList();
        }

        private void CopySelected_Click(object sender, RoutedEventArgs e)
        {
            CopySelectedItem();
        }

        private void DeleteSelected_Click(object sender, RoutedEventArgs e)
        {
            DeleteSelectedItem();
        }

        private void DeleteSelectedItem()
        {
            string item = SelectedFullText();
            if (string.IsNullOrEmpty(item)) return;

            clipboardService.Remove(item);
            RefreshClipboardList();
        }

        private void ClearHistory_Click(object sender, RoutedEventArgs e)
        {
            if (clipboardService.Count == 0) return;

            MessageBoxResult answer = MessageBox.Show(
                "همه تاریخچه متنی حذف شود؟ این کار قابل بازگشت نیست.",
                "تأیید حذف",
                MessageBoxButton.YesNo,
                MessageBoxImage.Warning,
                MessageBoxResult.No);

            if (answer != MessageBoxResult.Yes) return;

            clipboardService.Clear();
            RefreshClipboardList();
        }

        // ------------------------------------------------------------------
        // Keyboard window
        // ------------------------------------------------------------------

        private void EnsureKeyboardWindow()
        {
            if (keyboardWindow != null) return;

            keyboardWindow = new FloatingKeyboardWindow();
            keyboardWindow.LanguageChanged += KeyboardWindow_LanguageChanged;
            keyboardWindow.Closed += (s, a) =>
            {
                keyboardWindow = null;
                LanguageStatusText.Text = "کیبورد شناور بسته شده است.";
            };
        }

        private void KeyboardWindow_LanguageChanged(bool persian)
        {
            LanguageStatusText.Text = persian
                ? "زبان فعلی کیبورد: فارسی"
                : "زبان فعلی کیبورد: انگلیسی";
        }

        private void ShowKeyboard_Click(object sender, RoutedEventArgs e)
        {
            EnsureKeyboardWindow();
            keyboardWindow.Show();
            keyboardWindow.Activate();
        }

        private void HideKeyboard_Click(object sender, RoutedEventArgs e)
        {
            if (keyboardWindow != null)
                keyboardWindow.Hide();
        }

        private void OpenClipboard_Click(object sender, RoutedEventArgs e)
        {
            ShowAndActivate();
            Tabs.SelectedIndex = 1;
            SearchBox.Focus();
        }

        // ------------------------------------------------------------------
        // Settings
        // ------------------------------------------------------------------

        private void StartupCheckBox_Changed(object sender, RoutedEventArgs e)
        {
            if (suppressStartupEvents || !IsLoaded) return;

            bool enable = StartupCheckBox.IsChecked == true;

            try
            {
                Settings.SetStartupEnabled(enable);
            }
            catch (Exception ex)
            {
                MessageBox.Show("تنظیم اجرای خودکار انجام نشد:\n" + ex.Message,
                    "خطا", MessageBoxButton.OK, MessageBoxImage.Warning);

                suppressStartupEvents = true;
                StartupCheckBox.IsChecked = !enable;
                suppressStartupEvents = false;
            }
        }

        private void OpenDataFolder_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                Process.Start("explorer.exe", "\"" + Settings.DataFolder + "\"");
            }
            catch (Exception ex)
            {
                MessageBox.Show("باز کردن پوشه ممکن نشد:\n" + ex.Message,
                    "خطا", MessageBoxButton.OK, MessageBoxImage.Warning);
            }
        }

        private void ResetLocalData_Click(object sender, RoutedEventArgs e)
        {
            MessageBoxResult answer = MessageBox.Show(
                "همه داده‌های محلی (تاریخچه کلیپ‌بورد، محل کیبورد و تنظیم اجرای خودکار) حذف شود؟",
                "تأیید حذف داده‌ها",
                MessageBoxButton.YesNo,
                MessageBoxImage.Warning,
                MessageBoxResult.No);

            if (answer != MessageBoxResult.Yes) return;

            try
            {
                clipboardService.Clear();

                string[] files = System.IO.Directory.GetFiles(Settings.DataFolder);
                foreach (string file in files)
                    System.IO.File.Delete(file);

                Settings.SetStartupEnabled(false);

                suppressStartupEvents = true;
                StartupCheckBox.IsChecked = false;
                suppressStartupEvents = false;

                RefreshClipboardList();

                MessageBox.Show("داده‌های محلی حذف شد.", "انجام شد",
                    MessageBoxButton.OK, MessageBoxImage.Information);
            }
            catch (Exception ex)
            {
                MessageBox.Show("حذف داده‌ها کامل نشد:\n" + ex.Message,
                    "خطا", MessageBoxButton.OK, MessageBoxImage.Warning);
            }
        }

        // ------------------------------------------------------------------
        // Shutdown
        // ------------------------------------------------------------------

        private void Exit_Click(object sender, RoutedEventArgs e)
        {
            Close();
        }

        private void MainWindow_Closed(object sender, EventArgs e)
        {
            if (hwnd != IntPtr.Zero)
                NativeMethods.UnregisterHotKey(hwnd, HotKeyId);

            if (clipboardService != null)
            {
                clipboardService.HistoryChanged -= ClipboardService_HistoryChanged;
                clipboardService.Stop(hwnd);
            }

            if (hwndSource != null)
            {
                hwndSource.RemoveHook(WndProc);
                hwndSource = null;
            }

            if (keyboardWindow != null)
            {
                keyboardWindow.Close();
                keyboardWindow = null;
            }
        }
    }
}
