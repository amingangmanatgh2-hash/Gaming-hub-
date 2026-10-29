using System;
using System.Windows;
using System.Windows.Input;

namespace AMIN_Keyboard_Clipboard
{
    /// <summary>
    /// Floating, always-on-top virtual keyboard. Does not steal keyboard focus
    /// (Focusable=False), so keys type into whatever application the user works in.
    /// </summary>
    public partial class KeyboardWindow : Window
    {
        private IntPtr _capturedTarget = IntPtr.Zero;

        public KeyboardWindow()
        {
            InitializeComponent();
        }

        protected override void OnShow(EventArgs e)
        {
            base.OnShow(e);
            // Remember the app the user was in so typed characters go there.
            _capturedTarget = NativeInput.GetForegroundWindow();
            TypingTarget.LastExternal = _capturedTarget;
        }

        protected override void OnShown(EventArgs e)
        {
            base.OnShown(e);
            // Restore focus to the target app so typing works immediately.
            if (_capturedTarget != IntPtr.Zero)
                NativeInput.ActivateWindow(_capturedTarget);
        }

        private void Card_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
        {
            try { DragMove(); }
            catch { /* ignore drag errors on non-active windows */ }
        }

        private void BtnClose_Click(object sender, RoutedEventArgs e)
        {
            Hide();
        }
    }
}
