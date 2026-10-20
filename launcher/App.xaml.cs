using System.IO;
using System.Windows;
using System.Windows.Threading;

namespace StuntmanLauncher;

public partial class App : Application
{
    // Without these a startup error closes the launcher with no window and
    // no message. Show the error and keep a copy in %TEMP% for bug reports.
    protected override void OnStartup(StartupEventArgs e)
    {
        DispatcherUnhandledException += OnDispatcherError;
        AppDomain.CurrentDomain.UnhandledException += (_, args) => Report(args.ExceptionObject as Exception);
        TaskScheduler.UnobservedTaskException += (_, args) => Report(args.Exception);
        base.OnStartup(e);
    }

    private void OnDispatcherError(object sender, DispatcherUnhandledExceptionEventArgs e)
    {
        Report(e.Exception);
        e.Handled = true;
        Shutdown(1);
    }

    private static void Report(Exception? ex)
    {
        var text = $"{DateTime.Now:yyyy-MM-dd HH:mm:ss}  {Environment.OSVersion}\n{ex}\n\n";
        var log = Path.Combine(Path.GetTempPath(), "StuntmanLauncher-crash.log");
        try { File.AppendAllText(log, text); } catch { }
        MessageBox.Show($"The launcher hit an error and has to close.\n\n{ex?.Message}\n\nDetails were saved to:\n{log}",
            "Stuntman: Ignition", MessageBoxButton.OK, MessageBoxImage.Error);
    }
}
