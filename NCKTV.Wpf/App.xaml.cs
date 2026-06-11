using System;
using System.Windows;

namespace NCKTV.Wpf
{
    public partial class App : Application
    {
        protected override void OnStartup(StartupEventArgs e)
        {
            AppDomain.CurrentDomain.UnhandledException += (s, ev) =>
            {
                MessageBox.Show($"Fatal Unhandled Exception:\n{ev.ExceptionObject}", "Fatal Error", MessageBoxButton.OK, MessageBoxImage.Error);
            };
            base.OnStartup(e);
        }
    }
}
