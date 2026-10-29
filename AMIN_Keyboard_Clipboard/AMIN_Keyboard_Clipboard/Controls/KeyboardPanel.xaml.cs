using System;
using System.Collections.Generic;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;

namespace AMIN_Keyboard_Clipboard.Controls
{
    /// <summary>
    /// Persian (ISIRI 9147) + English (QWERTY) on-screen keyboard.
    /// Clicking a key sends the character to the application the user was last in.
    /// </summary>
    public partial class KeyboardPanel : UserControl
    {
        private static readonly string[] PersianNumbers = { "۱", "۲", "۳", "۴", "۵", "۶", "۷", "۸", "۹", "۰" };
        private static readonly string[] PersianRow1 = { "ض", "ص", "ث", "ق", "ف", "غ", "ع", "ه", "خ", "ح", "چ" };
        private static readonly string[] PersianRow2 = { "ش", "س", "ی", "ب", "ل", "ا", "ت", "ن", "م", "ک", "گ" };
        private static readonly string[] PersianRow3 = { "ظ", "ط", "ز", "ر", "ذ", "د", "پ", "و", "ژ", "،", "؛", "؟" };

        private static readonly string[] EnglishNumbers = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "0" };
        private static readonly string[] EnglishRow1 = { "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P" };
        private static readonly string[] EnglishRow2 = { "A", "S", "D", "F", "G", "H", "J", "K", "L", ";" };
        private static readonly string[] EnglishRow3 = { "Z", "X", "C", "V", "B", "N", "M", ",", ".", "/" };

        private bool _persianMode = true;
        private bool _shiftHeld;
        private Button _btnShift;
        private Button _btnMode;

        public KeyboardPanel()
        {
            InitializeComponent();
            BuildLayout();
        }

        private void BuildLayout()
        {
            root.Children.Clear();

            // Header: language toggle + hint
            var header = new StackPanel { Orientation = Orientation.Horizontal, Margin = new Thickness(0, 0, 0, 6) };
            _btnMode = MakeKey(_persianMode ? "FA" : "EN", "FA", 56, 28, 12);
            header.Children.Add(_btnMode);
            var hint = new TextBlock
            {
                Text = _persianMode ? "Persian (فارسی) — standard ISIRI layout" : "English — QWERTY",
                FontSize = 11,
                Foreground = (SolidColorBrush)Application.Current.Resources["TextMuted"],
                VerticalAlignment = VerticalAlignment.Center,
                Margin = new Thickness(8, 0, 0, 0)
            };
            header.Children.Add(hint);
            root.Children.Add(header);

            // Key rows
            string[][] rows = _persianMode
                ? new[] { PersianNumbers, PersianRow1, PersianRow2, PersianRow3 }
                : new[] { EnglishNumbers, EnglishRow1, EnglishRow2, EnglishRow3 };
            foreach (var row in rows)
                root.Children.Add(MakeRow(row, row.Length));

            // Bottom row: ZWNJ | Shift | Space (spans 2) | Backspace | Enter
            var bottom = new UniformGrid { Columns = 6, Margin = new Thickness(0, 4, 0, 0) };

            bottom.Children.Add(MakeKey("ZWNJ", "‌", 64, 44, 11));

            _btnShift = MakeKey("⇧", "Shift", 56, 44, 16);
            bottom.Children.Add(_btnShift);

            var space = MakeKey("Space", "Space", double.NaN, 44, 16);
            UniformGrid.SetColumnSpan(space, 2);
            bottom.Children.Add(space);

            bottom.Children.Add(MakeKey("⌫", "Backspace", 56, 44, 18));
            bottom.Children.Add(MakeKey("↵", "Enter", 56, 44, 18));

            root.Children.Add(bottom);
        }

        private Button MakeKey(string content, string tag, double width, double height, double fontSize)
        {
            var b = new Button
            {
                Content = content,
                Tag = tag,
                Style = (Style)FindResource("KeyStyle"),
                Height = height,
                FontSize = fontSize,
                Margin = new Thickness(2)
            };
            if (width > 0) b.Width = width;
            b.Click += Key_Click;
            return b;
        }

        private UniformGrid MakeRow(IList<string> keys, int columns)
        {
            var g = new UniformGrid { Columns = columns };
            foreach (var key in keys)
            {
                g.Children.Add(MakeKey(key, key, double.NaN, 44, 16));
            }
            return g;
        }

        private void Key_Click(object sender, RoutedEventArgs e)
        {
            var btn = sender as Button;
            if (btn == null) return;
            string tag = btn.Tag as string;
            if (tag == null) return;

            if (tag == "Shift")
            {
                _shiftHeld = !_shiftHeld;
                _btnShift.Content = _shiftHeld ? "⇪" : "⇧";
                _btnShift.FontWeight = _shiftHeld ? FontWeights.Bold : FontWeights.Normal;
                return;
            }

            if (tag == "FA")
            {
                _persianMode = !_persianMode;
                BuildLayout();
                return;
            }

            string text = tag;
            if (!_persianMode && _shiftHeld && text.Length == 1 && char.IsLetter(text[0]))
                text = text.ToUpperInvariant();

            TypingTarget.Type(text);
        }
    }
}
