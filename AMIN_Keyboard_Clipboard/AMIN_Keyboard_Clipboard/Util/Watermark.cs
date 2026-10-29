using System.Windows;
using System.Windows.Controls;
using System.Windows.Documents;
using System.Windows.Media;

namespace AMIN_Keyboard_Clipboard
{
    /// <summary>
    /// Simple watermark (placeholder) support for TextBox — WPF .NET Framework 4.8
    /// has no built-in PlaceholderText.
    /// Usage: &lt;TextBox app:Watermark.Watermark="Search..."/&gt;
    /// </summary>
    public static class Watermark
    {
        public static readonly DependencyProperty WatermarkProperty =
            DependencyProperty.RegisterAttached("Watermark", typeof(string), typeof(Watermark),
                new PropertyMetadata(null, OnWatermarkChanged));

        public static string GetWatermark(DependencyObject element)
        {
            return (string)element.GetValue(WatermarkProperty);
        }

        public static void SetWatermark(DependencyObject element, string value)
        {
            element.SetValue(WatermarkProperty, value);
        }

        private static void OnWatermarkChanged(DependencyObject d, DependencyPropertyChangedEventArgs e)
        {
            var tb = d as TextBox;
            if (tb == null || e.NewValue == null) return;
            if (GetAdorner(tb) != null) return;

            var adorner = new WatermarkAdorner(tb, e.NewValue as string);
            AdornerLayer.GetAdornerLayer(tb).Add(adorner);

            tb.TextChanged += (s, ev) => adorner.Update();
            tb.GotFocus += (s, ev) => adorner.Update();
            tb.LostFocus += (s, ev) => adorner.Update();
        }

        private static WatermarkAdorner GetAdorner(TextBox tb)
        {
            var adorners = AdornerLayer.GetAdornerLayer(tb).GetAdorners(tb);
            foreach (var a in adorners)
            {
                var w = a as WatermarkAdorner;
                if (w != null) return w;
            }
            return null;
        }
    }

    public class WatermarkAdorner : Adorner
    {
        private readonly TextBlock _text;

        public WatermarkAdorner(TextBox textBox, string text) : base(textBox)
        {
            _text = new TextBlock
            {
                Text = text,
                Foreground = (SolidColorBrush)Application.Current.Resources["TextMuted"],
                FontSize = textBox.FontSize,
                FontFamily = textBox.FontFamily,
                IsHitTestVisible = false,
                IsEnabled = false,
                Margin = new Thickness(4, 0, 4, 0),
                HorizontalAlignment = HorizontalAlignment.Left,
                VerticalAlignment = VerticalAlignment.Center
            };
            Child = _text;
        }

        protected override Size ArrangeOverride(Size finalSize) { return finalSize; }
        protected override Size MeasureOverride(Size constraint) { return constraint; }

        public void Update()
        {
            string t = ((TextBox)AdornedElement).Text;
            _text.Visibility = string.IsNullOrEmpty(t) ? Visibility.Visible : Visibility.Collapsed;
        }
    }
}
