using System;
using System.Windows;
using System.Windows.Threading;

namespace AMIN_Keyboard_Clipboard
{
    public partial class App : Application
    {
        protected override void OnStartup(StartupEventArgs e)
        {
            base.OnStartup(e);

            // Exit when the main dashboard is closed (floating windows close with it).
            Current.ShutdownMode = ShutdownMode.OnMainWindowClose;
            Current.DispatcherUnhandledException += App_DispatcherUnhandledException;
        }

        private void App_DispatcherUnhandledException(object sender, DispatcherUnhandledExceptionEventArgs e)
        {
            MessageBox.Show(
                "Application error:\n\n" + e.Exception.Message,
                "AMIN Error", MessageBoxButton.OK, MessageBoxImage.Error);
            e.Handled = true;
        }
    }
}
