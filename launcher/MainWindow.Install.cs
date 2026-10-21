using System.Diagnostics;
using System.IO;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;

namespace StuntmanLauncher;

// Install mode: the launcher runs where the game isn't installed (e.g. the
// downloaded release exe). The right panel becomes the installer and Play
// becomes Install.
public partial class MainWindow
{
    private bool installMode_;
    private bool installing_;
    private bool installed_;
    private string? isoPath_;
    private string installDir_ = DefaultInstallDir();
    private OptionRow keepBuild_ = null!;
    private CancellationTokenSource? installCancel_;
    private Installer? installer_;
    private readonly Dictionary<InstallStep, StepRow> stepRows_ = new();

    private static string DefaultInstallDir() =>
        Path.Combine(Path.GetPathRoot(Environment.SystemDirectory) ?? @"C:\", "Games", "StuntmanIgnition");

    private void EnterInstallMode()
    {
        installMode_ = true;
        SavesTitle.Visibility = Visibility.Collapsed;
        SavesPanel.Visibility = Visibility.Collapsed;
        InstallPanel.Visibility = Visibility.Visible;
        PlayButton.Content = L.Install;
        PlayButton.IsEnabled = false;
        StatusText.Text = "";
        InstallDirText.Text = installDir_;

        keepBuild_ = new OptionRow(L.KeepBuild, "");
        keepBuild_.SetOptions(new (string, object)[] { ("No", false), (L.Yes, true) }, false);
        KeepBuildHost.Children.Add(keepBuild_);

        Closing += (_, e) =>
        {
            if (!installing_) return;
            e.Cancel = true;
            _ = ConfirmCancelInstall();
        };

        // StuntmanSetup.exe --iso <file> [--dir <folder>] [--keep-build] [--auto]
        var args = Environment.GetCommandLineArgs();
        string? Arg(string name)
        {
            int i = Array.IndexOf(args, name);
            return i >= 0 && i + 1 < args.Length ? args[i + 1] : null;
        }
        if (Arg("--dir") is string dir)
        {
            installDir_ = Path.GetFullPath(dir);
            InstallDirText.Text = installDir_;
        }
        if (args.Contains("--keep-build")) keepBuild_.Select(true);
        if (Arg("--iso") is string iso) SetIso(Path.GetFullPath(iso));
        if (args.Contains("--auto") && PlayButton.IsEnabled) Loaded += async (_, _) => await InstallAsync();
    }

    private async Task ConfirmCancelInstall()
    {
        if (await Confirm(L.ConfirmCancelInstall))
        {
            installCancel_?.Cancel();
            installing_ = false;
            Close();
        }
    }

    private void OnPickIso(object sender, RoutedEventArgs e)
    {
        var dialog = new Microsoft.Win32.OpenFileDialog
        {
            Filter = "Xbox 360 ISO (*.iso)|*.iso|*.*|*.*",
            Title = L.IsoPick,
        };
        if (dialog.ShowDialog(this) != true) return;
        SetIso(dialog.FileName);
    }

    private void SetIso(string path)
    {
        isoPath_ = path;
        IsoPath.Text = isoPath_;
        IsoPath.Foreground = Res("TextBrush");
        var error = Installer.CheckIso(isoPath_);
        IsoStatus.Text = error == null ? L.IsoOk : string.Format(L.IsoWrong, error);
        IsoStatus.Foreground = error == null ? Res("AccentBrush") : Res("DangerBrush");
        PlayButton.IsEnabled = error == null;
    }

    private void OnPickDir(object sender, RoutedEventArgs e)
    {
        var dialog = new Microsoft.Win32.OpenFolderDialog { Title = L.FolderLabel };
        if (dialog.ShowDialog(this) != true) return;
        // A dedicated folder inside the chosen one, unless it already is one
        var chosen = dialog.FolderName;
        installDir_ = Path.GetFileName(chosen).Contains("Stuntman", StringComparison.OrdinalIgnoreCase)
            ? chosen
            : Path.Combine(chosen, "StuntmanIgnition");
        InstallDirText.Text = installDir_;
    }

    private void OnShowLog(object sender, RoutedEventArgs e)
    {
        bool show = InstallLog.Visibility != Visibility.Visible;
        InstallLog.Visibility = show ? Visibility.Visible : Visibility.Collapsed;
        InstallSteps.Visibility = show ? Visibility.Collapsed : Visibility.Visible;
        InstallLog.ScrollToEnd();
    }

    private async Task InstallAsync()
    {
        if (installed_)
        {
            Process.Start(new ProcessStartInfo(installer_!.GamePath)
            {
                WorkingDirectory = installDir_,
                UseShellExecute = false,
            });
            Close();
            return;
        }
        if (isoPath_ == null) return;

        installing_ = true;
        PlayButton.IsEnabled = false;
        PlayButton.Content = L.Installing;
        StatusText.Text = "";
        InstallForm.Visibility = Visibility.Collapsed;
        InstallProgressPanel.Visibility = Visibility.Visible;
        BuildStepRows();
        InstallLog.Clear();

        var progress = new Progress<InstallProgress>(OnInstallProgress);
        installCancel_ = new CancellationTokenSource();
        try
        {
            installer_ = new Installer(isoPath_, installDir_, (bool)keepBuild_.Value, progress);
            installer_.LogLine += line => Dispatcher.BeginInvoke(() => AppendLog(line));
            await installer_.RunAsync(installCancel_.Token);
            foreach (var row in stepRows_.Values) row.SetDone();
            installed_ = true;
            PlayButton.Content = L.Play;
            StatusText.Text = L.InstallDone;
        }
        catch (OperationCanceledException)
        {
            return;
        }
        catch (Exception ex)
        {
            current_?.SetFailed();
            PlayButton.Content = L.Install;
            var message = ex is InstallException ? ex.Message : ex.GetType().Name + ": " + ex.Message;
            StatusText.Text = string.Format(L.InstallFailed, message.TrimEnd('.') + ".",
                installer_?.LogPath ?? "");
            InstallForm.Visibility = Visibility.Visible;
            InstallProgressPanel.Visibility = Visibility.Collapsed;
        }
        finally
        {
            installing_ = false;
            PlayButton.IsEnabled = true;
        }
    }

    private const int MaxLogChars = 200_000;

    private void AppendLog(string line)
    {
        InstallLog.AppendText(line + "\n");
        if (InstallLog.Text.Length > MaxLogChars)
            InstallLog.Text = InstallLog.Text[^(MaxLogChars / 2)..];
        if (InstallLog.Visibility == Visibility.Visible) InstallLog.ScrollToEnd();
    }

    private StepRow? current_;

    private void OnInstallProgress(InstallProgress p)
    {
        foreach (var (step, row) in stepRows_)
        {
            if (step < p.Step) row.SetDone();
        }
        current_ = stepRows_[p.Step];
        current_.SetActive(p.Fraction, p.Detail);
    }

    private void BuildStepRows()
    {
        InstallSteps.Children.Clear();
        stepRows_.Clear();
        var names = new Dictionary<InstallStep, string>
        {
            [InstallStep.CheckIso] = L.StepCheckIso,
            [InstallStep.Tools] = L.StepTools,
            [InstallStep.Git] = L.StepGit,
            [InstallStep.Source] = L.StepSource,
            [InstallStep.Extract] = L.StepExtract,
            [InstallStep.Build] = L.StepBuild,
            [InstallStep.Finish] = L.StepFinish,
        };
        foreach (var (step, name) in names)
        {
            var row = new StepRow(name);
            stepRows_[step] = row;
            InstallSteps.Children.Add(row.Root);
        }
    }

    // One step of the install: state glyph, name, detail and a thin bar.
    private sealed class StepRow
    {
        private static Brush Res(string key) => (Brush)Application.Current.Resources[key];
        private static FontFamily Font(string key) => (FontFamily)Application.Current.Resources[key];

        private readonly TextBlock glyph_;
        private readonly TextBlock name_;
        private readonly TextBlock detail_;
        private readonly Grid bar_;
        private readonly ColumnDefinition done_ = new() { Width = new GridLength(0, GridUnitType.Star) };
        private readonly ColumnDefinition left_ = new() { Width = new GridLength(1, GridUnitType.Star) };

        public Border Root { get; }

        public StepRow(string name)
        {
            glyph_ = new TextBlock
            {
                Text = "\uE91F",  // small circle
                FontFamily = Font("IconFont"),
                FontSize = 12,
                Width = 26,
                VerticalAlignment = VerticalAlignment.Center,
                Foreground = Res("LineBrush"),
            };
            name_ = new TextBlock
            {
                Text = name,
                FontFamily = Font("UiFont"),
                FontSize = 15,
                Foreground = Res("DimTextBrush"),
            };
            detail_ = new TextBlock
            {
                FontFamily = Font("UiFont"),
                FontSize = 12,
                Foreground = Res("DimTextBrush"),
                TextTrimming = TextTrimming.CharacterEllipsis,
                Visibility = Visibility.Collapsed,
                Margin = new Thickness(0, 2, 0, 0),
            };
            bar_ = new Grid
            {
                Height = 3,
                Margin = new Thickness(0, 6, 0, 0),
                Background = Res("LineBrush"),
                Visibility = Visibility.Collapsed,
            };
            bar_.ColumnDefinitions.Add(done_);
            bar_.ColumnDefinitions.Add(left_);
            bar_.Children.Add(new Border { Background = Res("AccentBrush") });

            var text = new StackPanel();
            text.Children.Add(name_);
            text.Children.Add(detail_);
            text.Children.Add(bar_);
            var row = new DockPanel();
            DockPanel.SetDock(glyph_, Dock.Left);
            row.Children.Add(glyph_);
            row.Children.Add(text);
            Root = new Border { Child = row, Padding = new Thickness(10, 7, 10, 7), Margin = new Thickness(0, 0, 0, 3) };
        }

        public void SetActive(double? fraction, string? detail)
        {
            glyph_.Text = "\uE768";  // play
            glyph_.Foreground = Res("AccentBrush");
            name_.Foreground = Res("TextBrush");
            Root.Background = Res("RowActiveBrush");
            if (detail != null)
            {
                detail_.Text = detail;
                detail_.Visibility = Visibility.Visible;
            }
            if (fraction is double f)
            {
                bar_.Visibility = Visibility.Visible;
                done_.Width = new GridLength(Math.Clamp(f, 0, 1), GridUnitType.Star);
                left_.Width = new GridLength(1 - Math.Clamp(f, 0, 1), GridUnitType.Star);
            }
        }

        public void SetDone()
        {
            glyph_.Text = "\uE73E";  // check
            glyph_.Foreground = Res("AccentBrush");
            name_.Foreground = Res("TextBrush");
            Root.Background = Brushes.Transparent;
            detail_.Visibility = Visibility.Collapsed;
            bar_.Visibility = Visibility.Collapsed;
        }

        public void SetFailed()
        {
            glyph_.Text = "\uE711";  // cross
            glyph_.Foreground = Res("DangerBrush");
        }
    }
}
