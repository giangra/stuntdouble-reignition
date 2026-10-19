using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Windows;
using System.Windows.Controls;

namespace StuntmanLauncher;

public partial class MainWindow : Window
{
    private const string GameExe = "stuntman.exe";

    private readonly string exeDir_ = AppContext.BaseDirectory;
    private readonly (int W, int H) desktop_ = Display.Desktop();
    private ConfigFile config_;
    private Settings settings_;

    private OptionRow displayMode_ = null!;
    private OptionRow resolution_ = null!;
    private OptionRow aspect_ = null!;
    private OptionRow supersampling_ = null!;
    private OptionRow vsync_ = null!;
    private OptionRow aniso_ = null!;
    private OptionRow upscaler_ = null!;
    private OptionRow language_ = null!;
    private OptionRow fps_ = null!;
    private OptionRow log_ = null!;

    public MainWindow()
    {
        InitializeComponent();
        config_ = new ConfigFile(Path.Combine(exeDir_, "stuntman.toml"));
        settings_ = Settings.Load(config_, desktop_);
        BuildRows();
        ShowSettings(settings_);

        var version = Assembly.GetExecutingAssembly().GetName().Version;
        VersionText.Text = $"Launcher {version?.ToString(3)} · ReXGlue";
        CheckInstall();
        Loaded += (_, _) => PlayButton.Focus();
    }

    // ---- Rows -------------------------------------------------------------

    private OptionRow AddRow(Panel panel, string label, string hint)
    {
        var row = new OptionRow(label, hint);
        row.HintShown += text => HintText.Text = text;
        row.Changed += _ => ReadSettings();
        panel.Children.Add(row);
        return row;
    }

    private static (string, object)[] OnOff() => new (string, object)[] { (L.On, true), (L.Off, false) };

    private void BuildRows()
    {
        displayMode_ = AddRow(VideoPanel, L.DisplayMode, L.DisplayModeHint);
        displayMode_.SetOptions(new (string, object)[] { (L.Fullscreen, true), (L.Windowed, false) }, true);

        resolution_ = AddRow(VideoPanel, L.Resolution, L.ResolutionHint);
        resolution_.SetOptions(Display.Resolutions(desktop_).Select(r =>
        {
            string label = $"{r.W} × {r.H}  ·  {Display.AspectName(r.W, r.H)}";
            if (r == desktop_) label += $"  ({L.Native})";
            return (label, (object)r);
        }), desktop_);

        aspect_ = AddRow(VideoPanel, L.WideScreen, L.WideScreenHint);
        aspect_.SetOptions(new (string, object)[] { (L.Bars, AspectMode.Bars), (L.Stretch, AspectMode.Stretch) },
            AspectMode.Bars);

        supersampling_ = AddRow(VideoPanel, L.Supersampling, L.SupersamplingHint);
        supersampling_.SetOptions(new (string, object)[] { (L.Off, 1), ("2x", 2), ("3x", 3) }, 1);

        vsync_ = AddRow(VideoPanel, L.VSync, L.VSyncHint);
        vsync_.SetOptions(OnOff(), true);

        aniso_ = AddRow(VideoPanel, L.TextureFiltering, L.TextureFilteringHint);
        aniso_.SetOptions(new (string, object)[]
        {
            (L.GameDefault, -1), (L.Off, 0), ("2x", 2), ("4x", 3), ("8x", 4), ("16x", 5),
        }, 3);

        upscaler_ = AddRow(VideoPanel, L.Upscaler, L.UpscalerHint);
        upscaler_.SetOptions(new (string, object)[]
        {
            (L.Bilinear, "bilinear"), ("AMD FidelityFX CAS", "cas"), ("AMD FidelityFX FSR", "fsr"),
        }, "bilinear");

        language_ = AddRow(GamePanel, L.Language, L.LanguageHint);
        language_.SetOptions(Languages.All.Select(l => (l.Name, (object)l.Id)), Languages.English);

        fps_ = AddRow(GamePanel, L.FrameRate, L.FrameRateHint);
        fps_.SetOptions(new (string, object)[] { (L.FrameRate30, 30), (L.FrameRate60, 60) }, 30);

        log_ = AddRow(AdvancedPanel, L.Log, L.LogHint);
        log_.SetOptions(OnOff(), false);
    }

    private void ShowSettings(Settings s)
    {
        displayMode_.Select(s.Fullscreen);
        resolution_.Select((s.Width, s.Height));
        aspect_.Select(s.Aspect);
        supersampling_.Select(s.RenderScale);
        vsync_.Select(s.VSync);
        aniso_.Select(s.Anisotropic);
        upscaler_.Select(s.Upscaler);
        language_.Select(s.Language);
        fps_.Select(s.TargetFps);
        log_.Select(s.WriteLog);
        UpdateAspectRow();
    }

    private void ReadSettings()
    {
        settings_.Fullscreen = (bool)displayMode_.Value;
        (settings_.Width, settings_.Height) = ((int, int))resolution_.Value;
        settings_.Aspect = (AspectMode)aspect_.Value;
        settings_.RenderScale = (int)supersampling_.Value;
        settings_.VSync = (bool)vsync_.Value;
        settings_.Anisotropic = (int)aniso_.Value;
        settings_.Upscaler = (string)upscaler_.Value;
        settings_.Language = (int)language_.Value;
        settings_.TargetFps = (int)fps_.Value;
        settings_.WriteLog = (bool)log_.Value;
        UpdateAspectRow();
    }

    private void UpdateAspectRow()
    {
        var (w, h) = ((int, int))resolution_.Value;
        aspect_.IsEnabled = !Display.Is16x9(w, h);
        aspect_.ToolTip = aspect_.IsEnabled ? null : L.WideScreenOnly169;
    }

    // ---- Actions ----------------------------------------------------------

    private bool SaveSettings()
    {
        try
        {
            settings_.Apply(config_);
            config_.Save();
            return true;
        }
        catch (Exception e)
        {
            StatusText.Text = e.Message;
            return false;
        }
    }

    private void OnSave(object sender, RoutedEventArgs e)
    {
        if (SaveSettings()) StatusText.Text = L.Saved;
    }

    private void OnDefaults(object sender, RoutedEventArgs e)
    {
        settings_ = new Settings { Width = desktop_.W, Height = desktop_.H, Language = Languages.FromWindows() };
        ShowSettings(settings_);
        StatusText.Text = L.DefaultsRestored;
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
        if (!CheckInstall() || !SaveSettings()) return;

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
        if (TabAchievements.IsChecked == true) ShowAchievements();
        if (TabSaves.IsChecked == true) ShowSaves();
        if (process.ExitCode != 0)
            StatusText.Text = string.Format(L.Crashed, $"0x{process.ExitCode:X8}");
    }

    private void OnTab(object sender, RoutedEventArgs e)
    {
        if (VideoPanel == null) return;  // fires during InitializeComponent
        VideoPanel.Visibility = TabVideo.IsChecked == true ? Visibility.Visible : Visibility.Collapsed;
        GamePanel.Visibility = TabGame.IsChecked == true ? Visibility.Visible : Visibility.Collapsed;
        AdvancedPanel.Visibility = TabAdvanced.IsChecked == true ? Visibility.Visible : Visibility.Collapsed;
        bool achievements = TabAchievements.IsChecked == true;
        bool saves = TabSaves.IsChecked == true;
        AchievementsPanel.Visibility = achievements ? Visibility.Visible : Visibility.Collapsed;
        SavesPanel.Visibility = saves ? Visibility.Visible : Visibility.Collapsed;
        Footer.Visibility = achievements || saves ? Visibility.Collapsed : Visibility.Visible;
        if (achievements) ShowAchievements();
        if (saves) ShowSaves();
        HintText.Text = "";
    }

    // ---- Achievements -----------------------------------------------------

    private void ShowAchievements()
    {
        AchList.Children.Clear();
        AchSummary.Text = "";
        List<Achievement>? list;
        try
        {
            list = Achievements.Load(Achievements.UserFolder(config_));
        }
        catch (Exception e)
        {
            list = null;
            AchSummary.Text = e.Message;
        }
        if (list == null)
        {
            if (string.IsNullOrEmpty(AchSummary.Text)) AchSummary.Text = L.NoCatalog;
            AchScore.Text = "";
            SetProgress(0, 1);
            return;
        }

        int unlocked = list.Count(a => a.UnlockedAt != null);
        int score = list.Where(a => a.UnlockedAt != null).Sum(a => a.Gamerscore);
        int total = list.Sum(a => a.Gamerscore);
        AchSummary.Text = string.Format(L.AchievementsUnlocked, unlocked, list.Count);
        AchScore.Text = $"{score} / {total} G";
        SetProgress(unlocked, list.Count);

        foreach (var a in list) AchList.Children.Add(AchievementItem(a));
    }

    private void SetProgress(int done, int total)
    {
        AchDone.Width = new GridLength(done, GridUnitType.Star);
        AchLeft.Width = new GridLength(Math.Max(0, total - done), GridUnitType.Star);
    }

    private static FrameworkElement AchievementItem(Achievement a)
    {
        bool done = a.UnlockedAt != null;
        var res = Application.Current.Resources;
        var font = (System.Windows.Media.FontFamily)res["UiFont"];

        var grid = new Grid { Margin = new Thickness(0, 0, 0, 6) };
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(64) });
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });

        var icon = new System.Windows.Controls.Image { Width = 56, Height = 56, Margin = new Thickness(4) };
        if (a.IconPath != null)
        {
            var bmp = new System.Windows.Media.Imaging.BitmapImage();
            bmp.BeginInit();
            bmp.UriSource = new Uri(a.IconPath);
            bmp.CacheOption = System.Windows.Media.Imaging.BitmapCacheOption.OnLoad;
            bmp.EndInit();
            // Locked ones in greyscale, as on the console
            icon.Source = done ? bmp : new System.Windows.Media.Imaging.FormatConvertedBitmap(
                bmp, System.Windows.Media.PixelFormats.Gray8, null, 0);
            if (!done) icon.Opacity = 0.45;
        }
        grid.Children.Add(icon);

        var text = new StackPanel { VerticalAlignment = VerticalAlignment.Center, Margin = new Thickness(14, 0, 12, 0) };
        text.Children.Add(new TextBlock
        {
            Text = a.Label,
            FontFamily = font,
            FontSize = 16,
            FontWeight = FontWeights.SemiBold,
            Foreground = (System.Windows.Media.Brush)res[done ? "TextBrush" : "DimTextBrush"],
            TextTrimming = TextTrimming.CharacterEllipsis,
        });
        text.Children.Add(new TextBlock
        {
            Text = a.Description,
            FontFamily = font,
            FontSize = 13,
            Margin = new Thickness(0, 3, 0, 0),
            Foreground = (System.Windows.Media.Brush)res["DimTextBrush"],
            TextWrapping = TextWrapping.Wrap,
        });
        Grid.SetColumn(text, 1);
        grid.Children.Add(text);

        var right = new StackPanel { VerticalAlignment = VerticalAlignment.Center, MinWidth = 120 };
        right.Children.Add(new TextBlock
        {
            Text = $"{a.Gamerscore} G",
            FontFamily = font,
            FontSize = 16,
            FontWeight = FontWeights.Bold,
            HorizontalAlignment = HorizontalAlignment.Right,
            Foreground = (System.Windows.Media.Brush)res[done ? "AccentBrush" : "DimTextBrush"],
        });
        right.Children.Add(new TextBlock
        {
            Text = done ? a.UnlockedAt!.Value.ToString("g") : L.Locked,
            FontFamily = font,
            FontSize = 12,
            Margin = new Thickness(0, 3, 0, 0),
            HorizontalAlignment = HorizontalAlignment.Right,
            Foreground = (System.Windows.Media.Brush)res["DimTextBrush"],
        });
        Grid.SetColumn(right, 2);
        grid.Children.Add(right);

        return new Border
        {
            Child = grid,
            Padding = new Thickness(6),
            Margin = new Thickness(0, 0, 0, 6),
            Background = (System.Windows.Media.Brush)res[done ? "RowActiveBrush" : "WindowBrush"],
            BorderThickness = new Thickness(3, 0, 0, 0),
            BorderBrush = done ? (System.Windows.Media.Brush)res["AccentBrush"] : System.Windows.Media.Brushes.Transparent,
        };
    }

    private void OnMinimize(object sender, RoutedEventArgs e) => WindowState = WindowState.Minimized;

    private void OnClose(object sender, RoutedEventArgs e) => Close();
}
