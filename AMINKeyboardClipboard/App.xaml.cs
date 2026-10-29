using System;
using System.Threading;
using System.Windows;
using System.Windows.Threading;

namespace AMINKeyboardClipboard
{
    /// <summary>
    /// Application entry point. Keeps a single instance alive because the app owns
    /// a global hot key; launching the .exe again just brings the existing window back.
    /// </summary>
    public partial class App : Application
    {
        private const string MutexName = @"Local\AMINKeyboardClipboard.SingleInstance";

        private Mutex singleInstanceMutex;

        protected override void OnStartup(StartupEventArgs e)
        {
            bool createdNew;
            singleInstanceMutex = new Mutex(true, MutexName, out createdNew);

            if (!createdNew)
            {
                // Another instance is already running: ask it to show itself, then exit.
                NativeMethods.BroadcastShowMainWindow();
                singleInstanceMutex.Dispose();
                singleInstanceMutex = null;
                Shutdown();
                return;
            }

            DispatcherUnhandledException += OnDispatcherUnhandledException;

            base.OnStartup(e);

            var window = new MainWindow();
            this.MainWindow = window;
            window.Show();
        }

        private void OnDispatcherUnhandledException(object sender, DispatcherUnhandledExceptionEventArgs e)
        {
            // Never let an unexpected UI-thread exception kill the app silently.
            MessageBox.Show(
                "خطای پیش‌بینی‌نشده‌ای رخ داد:\n\n" + e.Exception.Message +
                "\n\nبرنامه به کار خود ادامه می‌دهد.",
                "AMIN Keyboard & Clipboard",
                MessageBoxButton.OK,
                MessageBoxImage.Warning);

            e.Handled = true;
        }

        protected override void OnExit(ExitEventArgs e)
        {
            if (singleInstanceMutex != null)
            {
                try
                {
                    singleInstanceMutex.ReleaseMutex();
                }
                catch (ApplicationException)
                {
                    // Mutex was not owned by this thread; nothing to release.
                }

                singleInstanceMutex.Dispose();
                singleInstanceMutex = null;
            }

            base.OnExit(e);
        }
    }
}
