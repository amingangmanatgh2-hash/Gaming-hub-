using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Interop;
using System.Windows.Threading;
using AMIN_Keyboard_Clipboard.Controls;

namespace AMIN_Keyboard_Clipboard
{
    public partial class MainWindow : Window
    {
        private const int WM_HOTKEY = 0x0312;
        private const int WM_KILLFOCUS = 0x0008;

        private const uint MOD_ALT = 0x0001;
        private const uint MOD_CONTROL = 0x0002;
        private const uint VK_K = 0x4B;
        private const uint VK_V = 0x56;

        private const int HotkeyIdKeyboard = 1;
        private const int HotkeyIdClipboard = 2;

        private readonly List<string> _history = new List<string>();
        private KeyboardWindow _keyboardWindow;
        private ClipboardWindow _clipboardWindow;
        private DispatcherTimer _clipTimer;
        private IntPtr _lastExternalWindow = IntPtr.Zero;

        public MainWindow()
        {
            InitializeComponent();
            SourceInitialized += MainWindow_SourceInitialized;
            Loaded += MainWindow_Loaded;
            Deactivated += MainWindow_Deactivated;
            Closing += MainWindow_Closing;
        }

        private void MainWindow_SourceInitialized(object sender, EventArgs e)
        {
            var hwnd = new WindowInteropHelper(this).Handle;
            var src = HwndSource.FromHwnd(hwnd);
            src.AddHook(WndProcHook);

            // Ctrl+Alt+K -> floating keyboard, Ctrl+Alt+V -> clipboard popup
            NativeInput.RegisterHotKey(hwnd, HotkeyIdKeyboard, MOD_CONTROL | MOD_ALT, VK_K);
            NativeInput.RegisterHotKey(hwnd, HotkeyIdClipboard, MOD_CONTROL | MOD_ALT, VK_V);
        }

        private IntPtr WndProcHook(IntPtr hwnd, int msg, IntPtr wParam, IntPtr lParam, ref bool handled)
        {
            if (msg == WM_KILLFOCUS) CaptureForeground();
            return IntPtr.Zero;
        }

        private void CaptureForeground()
        {
            IntPtr fg = NativeInput.GetForegroundWindow();
            IntPtr self = new WindowInteropHelper(this).Handle;
            if (fg != IntPtr.Zero && fg != self)
            {
                _lastExternalWindow = fg;
                TypingTarget.LastExternal = fg;
            }
        }

        private void MainWindow_Deactivated(object sender, EventArgs e)
        {
            CaptureForeground();
        }

        private void MainWindow_Loaded(object sender, RoutedEventArgs e)
        {
            ClipboardStore.Load();
            _history.Clear();
            _history.AddRange(ClipboardStore.GetAll());
            RefreshList();

            _clipTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(500) };
            _clipTimer.Tick += (s, ev) =>
            {
                try
                {
                    if (!Clipboard.ContainsText()) return;
                    string text = Clipboard.GetText();
                    if (string.IsNullOrWhiteSpace(text) || text.Length > 4000) return;
                    if (ClipboardStore.Add(text))
                    {
                        _history.Clear();
                        _history.AddRange(ClipboardStore.GetAll());
                        RefreshList();
                    }
                }
                catch
                {
                    // clipboard can be locked by other apps — ignore
                }
            };
            _clipTimer.Start();

            ClipboardStore.Changed += ClipboardStore_Changed;

            UpdateStartupCheckbox();
            UpdateKeyboardButtonText();
            UpdateClipboardButtonText();
            txtVersion.Text = "Version " + Assembly.GetExecutingAssembly().GetName().Version;
        }

        private void ClipboardStore_Changed(object sender, EventArgs e)
        {
            Dispatcher.BeginInvoke(new Action(() =>
            {
                _history.Clear();
                _history.AddRange(ClipboardStore.GetAll());
                RefreshList();
            }));
        }

        private void RefreshList()
        {
            lock (_history)
            {
                lstHistory.ItemsSource = _history
                    .Where(x => x != null && x.IndexOf(txtSearch.Text, StringComparison.OrdinalIgnoreCase) >= 0)
                    .ToList();
                txtStatus.Text = _history.Count + " items";
            }
        }

        private void TxtSearch_TextChanged(object sender, TextChangedEventArgs e)
        {
            RefreshList();
        }

        private void LstHistory_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            btnCopy.IsEnabled = lstHistory.SelectedItem != null;
        }

        private void BtnCopy_Click(object sender, RoutedEventArgs e)
        {
            if (lstHistory.SelectedItem is string selected)
            {
                try
                {
                    Clipboard.SetText(selected);
                    MessageBox.Show("Copied to clipboard:\n" + Truncate(selected, 120),
                        "AMIN", MessageBoxButton.OK, MessageBoxImage.Information);
                }
                catch (Exception ex)
                {
                    MessageBox.Show(ex.Message, "AMIN", MessageBoxButton.OK, MessageBoxImage.Error);
                }
            }
            else
            {
                MessageBox.Show("Select an item first.", "AMIN", MessageBoxButton.OK, MessageBoxImage.Information);
            }
        }

        private void BtnDelete_Click(object sender, RoutedEventArgs e)
        {
            if (lstHistory.SelectedItem is string selected)
            {
                ClipboardStore.Remove(selected);
            }
        }

        private void BtnClear_Click(object sender, RoutedEventArgs e)
        {
            if (MessageBox.Show("Clear all clipboard history?", "AMIN",
                    MessageBoxButton.YesNo, MessageBoxImage.Question) == MessageBoxResult.Yes)
            {
                ClipboardStore.Clear();
            }
        }

        private void BtnKeyboard_Click(object sender, RoutedEventArgs e)
        {
            ToggleKeyboardWindow();
        }

        private void BtnClipboard_Click(object sender, RoutedEventArgs e)
        {
            ToggleClipboardWindow();
        }

        private void ToggleKeyboardWindow()
        {
            if (_keyboardWindow == null)
            {
                _keyboardWindow = new KeyboardWindow();
                _keyboardWindow.Closed += (s, e) => _keyboardWindow = null;
            }
            if (_keyboardWindow.Visibility == Visibility.Visible)
                _keyboardWindow.Hide();
            else
                _keyboardWindow.Show();
            UpdateKeyboardButtonText();
        }

        private void ToggleClipboardWindow()
        {
            if (_clipboardWindow == null)
            {
                _clipboardWindow = new ClipboardWindow();
                _clipboardWindow.Closed += (s, e) => _clipboardWindow = null;
            }
            if (_clipboardWindow.Visibility == Visibility.Visible)
                _clipboardWindow.Hide();
            else
                _clipboardWindow.Show();
            UpdateClipboardButtonText();
        }

        private void UpdateKeyboardButtonText()
        {
            bool visible = _keyboardWindow != null && _keyboardWindow.Visibility == Visibility.Visible;
            btnKeyboard.Content = visible ? "🗝️ Hide Floating Keyboard" : "🗝️ Show Floating Keyboard";
        }

        private void UpdateClipboardButtonText()
        {
            bool visible = _clipboardWindow != null && _clipboardWindow.Visibility == Visibility.Visible;
            btnClipboard.Content = visible ? "📋 Hide Popup" : "📋 Clipboard Popup";
        }

        private void ChkStartup_Changed(object sender, RoutedEventArgs e)
        {
            string exe = Assembly.GetExecutingAssembly().Location;
            if (chkStartup.IsChecked == true)
                StartupShortcut.Enable(exe);
            else
                StartupShortcut.Disable();
            UpdateStartupCheckbox();
        }

        private void BtnStartup_Click(object sender, RoutedEventArgs e)
        {
            chkStartup.IsChecked = !(chkStartup.IsChecked == true);
        }

        private void UpdateStartupCheckbox()
        {
            chkStartup.IsChecked = StartupShortcut.IsEnabled();
        }

        private void BtnExit_Click(object sender, RoutedEventArgs e)
        {
            Application.Current.Shutdown();
        }

        protected override void WndProc(ref Message m)
        {
            if (m.Msg == WM_HOTKEY)
            {
                int id = m.WParam.ToInt32();
                if (id == HotkeyIdKeyboard) ToggleKeyboardWindow();
                else if (id == HotkeyIdClipboard) ToggleClipboardWindow();
            }
            base.WndProc(ref m);
        }

        private void MainWindow_Closing(object sender, System.ComponentModel.CancelEventArgs e)
        {
            if (_clipTimer != null) _clipTimer.Stop();
            if (_keyboardWindow != null) _keyboardWindow.Close();
            if (_clipboardWindow != null) _clipboardWindow.Close();
            NativeInput.UnregisterHotKey(new WindowInteropHelper(this).Handle, HotkeyIdKeyboard);
            NativeInput.UnregisterHotKey(new WindowInteropHelper(this).Handle, HotkeyIdClipboard);
        }

        private static string Truncate(string s, int max)
        {
            if (s == null) return "";
            return s.Length <= max ? s : s.Substring(0, max) + "…";
        }
    }
}
