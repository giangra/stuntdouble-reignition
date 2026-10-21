using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Windows;
using System.Windows.Controls;

namespace StuntmanLauncher;

// Installs the game (MainWindow.Install.cs), starts it, and manages its saves
// (MainWindow.Saves.cs). Video, controls and achievements are in the game's
// own PC menu (start screen, or F1 in game).
public partial class MainWindow : Window
{
    private const string GameExe = "stuntman.exe";

    private readonly string exeDir_ = AppContext.BaseDirectory;
    private readonly ConfigFile config_;

    public MainWindow()
    {
        InitializeComponent();
        config_ = new ConfigFile(Path.Combine(exeDir_, "stuntman.toml"));

        var version = Assembly.GetExecutingAssembly().GetName().Version;
        VersionText.Text = $"Launcher {version?.ToString(3)} · ReXGlue";
        if (!File.Exists(Path.Combine(exeDir_, GameExe)))
        {
            EnterInstallMode();
        }
        else
        {
            CheckInstall();
            AddUninstall();
            ShowSaves();
        }
        Loaded += (_, _) => PlayButton.Focus();
    }

    // Only for installs made by the installer (it leaves install.log).
    private void AddUninstall()
    {
        if (!File.Exists(Path.Combine(exeDir_, "install.log"))) return;
        var uninstall = new Button
        {
            Content = L.Uninstall,
            Style = (Style)Application.Current.Resources["SmallDangerButton"],
            HorizontalAlignment = HorizontalAlignment.Right,
            VerticalAlignment = VerticalAlignment.Center,
            Margin = new Thickness(0, 0, 40, 0),
        };
        uninstall.Click += async (_, _) => await UninstallAsync();
        Grid.SetRow(uninstall, 1);
        RightPanel.Children.Add(uninstall);
    }

    private async Task UninstallAsync()
    {
        var dir = exeDir_.TrimEnd('\\');
        if (!await Confirm(string.Format(L.ConfirmUninstall, dir))) return;
        Shortcuts.Remove();
        // The folder holds this exe, so a detached shell deletes it once the
        // launcher has exited. rmdir /s removes junctions without following them.
        Process.Start(new ProcessStartInfo("cmd.exe",
            $"/c timeout /t 2 /nobreak >nul & rmdir /s /q \"{dir}\"")
        {
            CreateNoWindow = true,
            UseShellExecute = false,
            WorkingDirectory = Path.GetTempPath(),
        });
        Application.Current.Shutdown();
    }

    // The game looks for its data in a "game" folder next to the exe or in
    // the working directory. Start it from the nearest folder that has one,
    // so a development build in out\build\... finds the repo's game folder.
    private string? FindDataRoot()
    {
        for (var dir = new DirectoryInfo(exeDir_); dir != null; dir = dir.Parent)
        {
            if (File.Exists(Path.Combine(dir.FullName, "game", "default.xex"))) return dir.FullName;
        }
        return null;
    }

    private bool CheckInstall()
    {
        if (!File.Exists(Path.Combine(exeDir_, GameExe)))
        {
            StatusText.Text = L.ExeMissing;
            PlayButton.IsEnabled = false;
            return false;
        }
        if (FindDataRoot() == null)
        {
            StatusText.Text = L.DataMissing;
            PlayButton.IsEnabled = false;
            return false;
        }
        return true;
    }

    private async void OnPlay(object sender, RoutedEventArgs e)
    {
        if (installMode_)
        {
            await InstallAsync();
            return;
        }
        if (!CheckInstall()) return;

        Process process;
        try
        {
            process = Process.Start(new ProcessStartInfo
            {
                FileName = Path.Combine(exeDir_, GameExe),
                WorkingDirectory = FindDataRoot()!,
                UseShellExecute = false,
            })!;
        }
        catch (Exception ex)
        {
            StatusText.Text = string.Format(L.StartFailed, ex.Message);
            return;
        }

        // Step aside while the game runs, come back when it exits.
        PlayButton.IsEnabled = false;
        PlayButton.Content = L.Running;
        StatusText.Text = "";
        Hide();
        await process.WaitForExitAsync();
        Show();
        Activate();
        PlayButton.Content = L.Play;
        PlayButton.IsEnabled = true;
        ShowSaves();
        if (process.ExitCode != 0)
            StatusText.Text = string.Format(L.Crashed, $"0x{process.ExitCode:X8}");
    }

    private void OnMinimize(object sender, RoutedEventArgs e) => WindowState = WindowState.Minimized;

    private void OnClose(object sender, RoutedEventArgs e) => Close();
}
