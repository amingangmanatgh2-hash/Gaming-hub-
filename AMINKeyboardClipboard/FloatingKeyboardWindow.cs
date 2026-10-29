using System;
using System.Diagnostics;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Interop;
using System.Windows.Media;

namespace AMINKeyboardClipboard
{
    /// <summary>
    /// Always-on-top on-screen keyboard that types into the last external window.
    ///
    /// Two things make it usable rather than frustrating:
    ///  - the window is created with WS_EX_NOACTIVATE, so clicking a key does not
    ///    move focus away from the application being typed into;
    ///  - characters are injected as Unicode scan codes through SendInput, which
    ///    works for Persian letters regardless of the active keyboard layout.
    /// </summary>
    public class FloatingKeyboardWindow : Window
    {
        private const int KeyWidth = 40;
        private const int KeyHeight = 40;
        private const int KeyGap = 3;

        private static readonly SolidColorBrush KeyBrush =
            new SolidColorBrush(Color.FromRgb(38, 41, 58));

        private static readonly SolidColorBrush KeyHoverBrush =
            new SolidColorBrush(Color.FromRgb(52, 56, 79));

        private static readonly SolidColorBrush WindowBrush =
            new SolidColorBrush(Color.FromRgb(18, 19, 29));

        private static readonly SolidColorBrush MutedBrush =
            new SolidColorBrush(Color.FromRgb(185, 184, 200));

        private static readonly string[][] PersianRows =
        {
            new[] { "۱", "۲", "۳", "۴", "۵", "۶", "۷", "۸", "۹", "۰", "-", "=" },
            new[] { "ض", "ص", "ث", "ق", "ف", "غ", "ع", "ه", "خ", "ح", "ج", "چ" },
            new[] { "ش", "س", "ی", "ب", "ل", "ا", "ت", "ن", "م", "ک", "گ" },
            new[] { "ظ", "ط", "ز", "ر", "ذ", "د", "پ", "و", "؟", "،", "." }
        };

        // Base (lower case) layout. Upper case is derived, never mutated in place.
        private static readonly string[][] EnglishRows =
        {
            new[] { "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "=" },
            new[] { "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "[", "]" },
            new[] { "a", "s", "d", "f", "g", "h", "j", "k", "l", ";", "'" },
            new[] { "z", "x", "c", "v", "b", "n", "m", ",", ".", "/" }
        };

        private static ControlTemplate cachedKeyTemplate;

        private readonly StackPanel rowsPanel;
        private readonly TextBlock languageLabel;
        private readonly Border headerBorder;

        private bool dragging;
        private Point dragScreenStart;
        private Point dragWindowStart;

        private bool persian = true;
        private bool shift;

        private IntPtr hook;
        private NativeMethods.WinEventDelegate foregroundCallback;

        /// <summary>The last window outside this process that had the foreground.</summary>
        public IntPtr LastExternalWindow { get; private set; }

        public bool IsPersian
        {
            get { return persian; }
        }

        /// <summary>Raised when the user switches between the Persian and English layouts.</summary>
        public event Action<bool> LanguageChanged;

        public FloatingKeyboardWindow()
        {
            Title = "AMIN Keyboard";
            Width = 720;
            Height = 344;
            MinWidth = 420;
            MinHeight = 300;
            Topmost = true;
            ShowInTaskbar = false;
            WindowStyle = WindowStyle.SingleBorderWindow;
            ResizeMode = ResizeMode.NoResize; // a WS_EX_NOACTIVATE window cannot use the resize grip
            Background = WindowBrush;
            Foreground = Brushes.White;
            FontFamily = new FontFamily("Segoe UI, Tahoma");
            WindowStartupLocation = WindowStartupLocation.Manual;
            SnapsToDevicePixels = true;

            var root = new DockPanel { Margin = new Thickness(10) };

            // ---- Header (also the drag handle: NOACTIVATE windows cannot be
            //      moved through the caption by the system) ----
            headerBorder = new Border
            {
                Background = Brushes.Transparent,
                Padding = new Thickness(0, 0, 0, 2),
                Cursor = System.Windows.Input.Cursors.SizeAll
            };

            var header = new DockPanel { LastChildFill = false };
            headerBorder.Child = header;
            headerBorder.MouseLeftButtonDown += OnHeaderMouseDown;
            headerBorder.MouseMove += OnHeaderMouseMove;
            headerBorder.MouseLeftButtonUp += OnHeaderMouseUp;

            DockPanel.SetDock(headerBorder, Dock.Top);

            var title = new TextBlock
            {
                Text = "کیبورد مجازی AMIN",
                FontSize = 16,
                FontWeight = FontWeights.SemiBold,
                VerticalAlignment = VerticalAlignment.Center
            };

            languageLabel = new TextBlock
            {
                Text = "فارسی",
                Foreground = MutedBrush,
                Margin = new Thickness(12, 0, 12, 0),
                VerticalAlignment = VerticalAlignment.Center
            };

            var languageButton = MakeChromeButton("فا / EN", 74);
            languageButton.Click += (s, e) =>
            {
                persian = !persian;
                shift = false;
                BuildKeyboard();
                RaiseLanguageChanged();
            };

            var closeButton = MakeChromeButton("مخفی کردن", 96);
            closeButton.Click += (s, e) => Hide();

            var rightGroup = new StackPanel { Orientation = Orientation.Horizontal };
            rightGroup.Children.Add(languageButton);
            rightGroup.Children.Add(closeButton);

            DockPanel.SetDock(rightGroup, Dock.Right);
            header.Children.Add(rightGroup);
            header.Children.Add(languageLabel);
            header.Children.Add(title);

            root.Children.Add(headerBorder);

            // ---- Keys ----
            rowsPanel = new StackPanel
            {
                Margin = new Thickness(0, 12, 0, 0)
            };

            root.Children.Add(rowsPanel);
            Content = root;

            Loaded += OnLoaded;
            Closing += OnClosing;
        }

        private void OnLoaded(object sender, RoutedEventArgs e)
        {
            // WPF re-applies its own window styles during Show(), so the extended
            // style is set again here to be certain the keyboard never takes focus.
            NativeMethods.MakeNonActivating(new WindowInteropHelper(this).Handle, true);

            BuildKeyboard();
            RestorePosition();
            RaiseLanguageChanged();
        }

        protected override void OnSourceInitialized(EventArgs e)
        {
            base.OnSourceInitialized(e);

            IntPtr hwnd = new WindowInteropHelper(this).Handle;

            // Do not steal focus on click, and stay out of Alt+Tab / taskbar.
            NativeMethods.MakeNonActivating(hwnd, true);

            // Track which external window should receive the typed characters.
            foregroundCallback = OnForegroundChanged;
            hook = NativeMethods.SetWinEventHook(
                NativeMethods.EVENT_SYSTEM_FOREGROUND,
                NativeMethods.EVENT_SYSTEM_FOREGROUND,
                IntPtr.Zero,
                foregroundCallback,
                0,
                0,
                NativeMethods.WINEVENT_OUTOFCONTEXT | NativeMethods.WINEVENT_SKIPOWNPROCESS);

            IntPtr foreground = NativeMethods.GetForegroundWindow();
            RememberIfExternal(foreground);
        }

        // ------------------------------------------------------------------
        // Layout
        // ------------------------------------------------------------------

        private void BuildKeyboard()
        {
            rowsPanel.Children.Clear();

            string[][] rows = persian ? PersianRows : EnglishRows;
            FlowDirection direction = persian ? FlowDirection.RightToLeft : FlowDirection.LeftToRight;

            int rowIndex = 0;

            if (!persian && shift)
            {
                // Derive the upper-case layout instead of mutating the static arrays.
                string[][] shifted = new string[rows.Length][];
                for (int r = 0; r < rows.Length; r++)
                {
                    shifted[r] = new string[rows[r].Length];
                    for (int c = 0; c < rows[r].Length; c++)
                        shifted[r][c] = rows[r][c].ToUpperInvariant();
                }

                rows = shifted;
            }

            foreach (string[] row in rows)
            {
                var rowPanel = new WrapPanel
                {
                    FlowDirection = direction,
                    HorizontalAlignment = HorizontalAlignment.Center,
                    Margin = new Thickness(0, rowIndex == 0 ? 0 : KeyGap, 0, 0)
                };

                foreach (string key in row)
                {
                    string captured = key;
                    Button button = MakeKeyButton(captured, KeyWidth);
                    button.Click += (s, e) => SendText(captured);
                    rowPanel.Children.Add(button);
                }

                rowsPanel.Children.Add(rowPanel);
                rowIndex++;
            }

            // Bottom action row: language-neutral keys, always LTR so the arrow-ish
            // keys read consistently next to the text keys.
            var actions = new WrapPanel
            {
                FlowDirection = FlowDirection.LeftToRight,
                HorizontalAlignment = HorizontalAlignment.Center,
                Margin = new Thickness(0, KeyGap + 4, 0, 0)
            };

            var shiftButton = MakeKeyButton("Shift", 62);
            shiftButton.IsEnabled = !persian;
            shiftButton.ToolTip = persian
                ? "در حالت فارسی Shift کاربردی ندارد."
                : "تغییر حروف بزرگ و کوچک";
            shiftButton.Click += (s, e) =>
            {
                shift = !shift;
                BuildKeyboard();
            };
            actions.Children.Add(shiftButton);

            var tabButton = MakeKeyButton("Tab", 56);
            tabButton.Click += (s, e) => SendVirtualKey(NativeMethods.VK_TAB);
            actions.Children.Add(tabButton);

            var spaceButton = MakeKeyButton("Space", 240);
            spaceButton.Click += (s, e) => SendText(" ");
            actions.Children.Add(spaceButton);

            var backspaceButton = MakeKeyButton("⌫", 56);
            backspaceButton.ToolTip = "پاک کردن یک کاراکتر";
            backspaceButton.Click += (s, e) => SendVirtualKey(NativeMethods.VK_BACK);
            actions.Children.Add(backspaceButton);

            var enterButton = MakeKeyButton("Enter", 66);
            enterButton.Click += (s, e) => SendVirtualKey(NativeMethods.VK_RETURN);
            actions.Children.Add(enterButton);

            var leftButton = MakeKeyButton("←", 44);
            leftButton.Click += (s, e) => SendVirtualKey(NativeMethods.VK_LEFT);
            actions.Children.Add(leftButton);

            rowsPanel.Children.Add(actions);

            languageLabel.Text = persian ? "فارسی" : (shift ? "انگلیسی — حروف بزرگ" : "انگلیسی");
        }

        private static Button MakeKeyButton(string text, double width)
        {
            var button = new Button
            {
                Content = text,
                Width = width,
                Height = KeyHeight,
                Margin = new Thickness(KeyGap),
                Background = KeyBrush,
                Foreground = Brushes.White,
                BorderThickness = new Thickness(0),
                FontSize = 15,
                Cursor = System.Windows.Input.Cursors.Hand,
                Template = KeyTemplate(),
                SnapsToDevicePixels = true,
                Focusable = false
            };

            return button;
        }

        private static Button MakeChromeButton(string text, double width)
        {
            var button = new Button
            {
                Content = text,
                Width = width,
                Height = 30,
                Margin = new Thickness(4, 0, 0, 0),
                Background = new SolidColorBrush(Color.FromRgb(52, 56, 79)),
                Foreground = Brushes.White,
                BorderThickness = new Thickness(0),
                FontSize = 12,
                Cursor = System.Windows.Input.Cursors.Hand,
                Template = KeyTemplate(),
                Focusable = false
            };

            return button;
        }

        private static ControlTemplate KeyTemplate()
        {
            if (cachedKeyTemplate != null) return cachedKeyTemplate;

            var border = new FrameworkElementFactory(typeof(Border)) { Name = "Chrome" };
            border.SetValue(Border.CornerRadiusProperty, new CornerRadius(8));
            border.SetValue(Border.BackgroundProperty,
                new TemplateBindingExtension(Control.BackgroundProperty));
            border.SetValue(UIElement.SnapsToDevicePixelsProperty, true);

            var presenter = new FrameworkElementFactory(typeof(ContentPresenter));
            presenter.SetValue(FrameworkElement.HorizontalAlignmentProperty, HorizontalAlignment.Center);
            presenter.SetValue(FrameworkElement.VerticalAlignmentProperty, VerticalAlignment.Center);
            border.AppendChild(presenter);

            var template = new ControlTemplate(typeof(Button)) { VisualTree = border };

            var hover = new Trigger { Property = UIElement.IsMouseOverProperty, Value = true };
            hover.Setters.Add(new Setter(Border.BackgroundProperty, KeyHoverBrush, "Chrome"));
            template.Triggers.Add(hover);

            var pressed = new Trigger { Property = ButtonBase.IsPressedProperty, Value = true };
            pressed.Setters.Add(new Setter(UIElement.OpacityProperty, 0.7, "Chrome"));
            template.Triggers.Add(pressed);

            var disabled = new Trigger { Property = UIElement.IsEnabledProperty, Value = false };
            disabled.Setters.Add(new Setter(UIElement.OpacityProperty, 0.35, "Chrome"));
            template.Triggers.Add(disabled);

            cachedKeyTemplate = template;
            return template;
        }

        private void RaiseLanguageChanged()
        {
            Action<bool> handler = LanguageChanged;
            if (handler != null) handler(persian);
        }

        // ------------------------------------------------------------------
        // Target window tracking
        // ------------------------------------------------------------------

        private void OnForegroundChanged(
            IntPtr hWinEventHook, uint eventType, IntPtr hwnd,
            int idObject, int idChild, uint idEventThread, uint msEventTime)
        {
            if (idObject != 0) return; // only the window itself, not child objects
            RememberIfExternal(hwnd);
        }

        private void RememberIfExternal(IntPtr hwnd)
        {
            if (hwnd == IntPtr.Zero) return;

            uint pid;
            NativeMethods.GetWindowThreadProcessId(hwnd, out pid);

            if (pid == 0 || pid == (uint)Process.GetCurrentProcess().Id)
                return;

            LastExternalWindow = hwnd;
        }

        /// <summary>
        /// Makes sure the recorded target is the foreground window before typing.
        /// Returns false (and explains why) when no usable target exists.
        /// </summary>
        private bool EnsureTarget()
        {
            if (LastExternalWindow != IntPtr.Zero && NativeMethods.IsWindow(LastExternalWindow))
            {
                if (NativeMethods.GetForegroundWindow() != LastExternalWindow)
                    NativeMethods.SetForegroundWindow(LastExternalWindow);

                return true;
            }

            MessageBox.Show(
                "اول یک برنامه مقصد (مثلاً Notepad یا Word) را باز کنید، داخل محل تایپ کلیک کنید، بعد از کیبورد استفاده کنید.",
                "پنجره مقصد پیدا نشد",
                MessageBoxButton.OK,
                MessageBoxImage.Information);

            return false;
        }

        // ------------------------------------------------------------------
        // Input injection
        // ------------------------------------------------------------------

        private void SendText(string text)
        {
            if (string.IsNullOrEmpty(text)) return;
            if (!EnsureTarget()) return;

            // One SendInput call per character keeps the array small and lets a
            // single failure abort cleanly instead of half-typing a long string.
            foreach (char c in text)
            {
                NativeMethods.INPUT[] inputs = NativeMethods.UnicodeChar(c);
                uint sent = NativeMethods.SendInput(
                    (uint)inputs.Length,
                    inputs,
                    NativeMethods.InputSize);

                if (sent != inputs.Length)
                {
                    // ERROR_ACCESS_DENIED (5) means the target runs elevated.
                    int error = System.Runtime.InteropServices.Marshal.GetLastWin32Error();
                    string hint = error == 5
                        ? "پنجره مقصد با دسترسی مدیر اجرا شده است. برای تایپ در آن، خود AMIN را هم «Run as administrator» اجرا کنید."
                        : "کد خطای ویندوز: " + error;

                    MessageBox.Show(
                        "ارسال کاراکتر به پنجره مقصد ممکن نشد.\n\n" + hint,
                        "ارسال ورودی ناموفق",
                        MessageBoxButton.OK,
                        MessageBoxImage.Warning);

                    return;
                }
            }
        }

        private void SendVirtualKey(ushort key)
        {
            if (!EnsureTarget()) return;

            NativeMethods.INPUT[] inputs = NativeMethods.VirtualKey(key);
            NativeMethods.SendInput((uint)inputs.Length, inputs, NativeMethods.InputSize);
        }

        // ------------------------------------------------------------------
        // Dragging (manual, because the window never activates)
        // ------------------------------------------------------------------

        private void OnHeaderMouseDown(object sender, System.Windows.Input.MouseButtonEventArgs e)
        {
            dragging = true;
            dragScreenStart = PointToScreen(e.GetPosition(this));
            dragWindowStart = new Point(Left, Top);
            headerBorder.CaptureMouse();
            e.Handled = true;
        }

        private void OnHeaderMouseMove(object sender, System.Windows.Input.MouseEventArgs e)
        {
            if (!dragging) return;

            Point current = PointToScreen(e.GetPosition(this));
            double scaleX = 1, scaleY = 1;

            PresentationSource source = PresentationSource.FromVisual(this);
            if (source != null && source.CompositionTarget != null)
            {
                scaleX = source.CompositionTarget.TransformFromDevice.M11;
                scaleY = source.CompositionTarget.TransformFromDevice.M22;
            }

            Left = dragWindowStart.X + (current.X - dragScreenStart.X) * scaleX;
            Top = dragWindowStart.Y + (current.Y - dragScreenStart.Y) * scaleY;
        }

        private void OnHeaderMouseUp(object sender, System.Windows.Input.MouseButtonEventArgs e)
        {
            if (!dragging) return;

            dragging = false;
            headerBorder.ReleaseMouseCapture();
            e.Handled = true;
        }

        // ------------------------------------------------------------------
        // Position persistence
        // ------------------------------------------------------------------

        private void RestorePosition()
        {
            double left, top, width, height;

            if (Settings.TryLoadKeyboardBounds(out left, out top, out width, out height))
            {
                // Reject coordinates that no longer land on a visible desktop
                // (e.g. a monitor was unplugged).
                if (left > SystemParameters.VirtualScreenLeft - 50 &&
                    top > SystemParameters.VirtualScreenTop - 50 &&
                    left < SystemParameters.VirtualScreenLeft + SystemParameters.VirtualScreenWidth - 100 &&
                    top < SystemParameters.VirtualScreenTop + SystemParameters.VirtualScreenHeight - 100)
                {
                    WindowStartupLocation = WindowStartupLocation.Manual;
                    Left = left;
                    Top = top;
                    Width = Math.Max(MinWidth, width);
                    Height = Math.Max(MinHeight, height);
                    return;
                }
            }

            // Default: bottom centre of the primary work area.
            WindowStartupLocation = WindowStartupLocation.Manual;
            Left = SystemParameters.WorkArea.Left +
                   Math.Max(0, (SystemParameters.WorkArea.Width - Width) / 2);
            Top = SystemParameters.WorkArea.Bottom - Height - 12;
        }

        private void OnClosing(object sender, System.ComponentModel.CancelEventArgs e)
        {
            if (WindowState == WindowState.Normal)
                Settings.SaveKeyboardBounds(Left, Top, Width, Height);

            if (hook != IntPtr.Zero)
            {
                NativeMethods.UnhookWinEvent(hook);
                hook = IntPtr.Zero;
            }

            foregroundCallback = null;
        }
    }
}
