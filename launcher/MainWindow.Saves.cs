using System.Diagnostics;
using System.IO;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;

namespace StuntmanLauncher;

// The Saves tab: current save, replays, backups, start over.
public partial class MainWindow
{
    private string UserFolder => GamePaths.UserFolder(config_);

    private Brush Res(string key) => (Brush)Application.Current.Resources[key];

    private TextBlock Heading(string text) => new()
    {
        Text = text,
        Margin = new Thickness(0, 18, 0, 8),
        FontFamily = (FontFamily)Application.Current.Resources["UiFont"],
        FontSize = 13,
        FontWeight = FontWeights.Bold,
        Foreground = Res("AccentBrush"),
    };

    private TextBlock Label(string text, bool dim = false, double size = 15) => new()
    {
        Text = text,
        VerticalAlignment = VerticalAlignment.Center,
        TextTrimming = TextTrimming.CharacterEllipsis,
        FontFamily = (FontFamily)Application.Current.Resources["UiFont"],
        FontSize = size,
        Foreground = Res(dim ? "DimTextBrush" : "TextBrush"),
    };

    private Button SmallButton(string text, Func<Task> action, bool danger = false)
    {
        var b = new Button
        {
            Content = text,
            Style = (Style)Application.Current.Resources[danger ? "SmallDangerButton" : "SmallButton"],
        };
        b.Click += async (_, _) => await Run(action);
        return b;
    }

    private Button BigButton(string text, Func<Task> action, bool danger = false)
    {
        var b = new Button
        {
            Content = text,
            Margin = new Thickness(0, 0, 8, 0),
            Style = (Style)Application.Current.Resources[danger ? "DangerButton" : "FlatButton"],
        };
        b.Click += async (_, _) => await Run(action);
        return b;
    }

    // A row: label (and dim detail) on the left, controls on the right.
    private Border Row(string label, string detail, params UIElement[] right)
    {
        var grid = new Grid();
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
        grid.Children.Add(Label(label));
        var d = Label(detail, dim: true, size: 13);
        d.Margin = new Thickness(12, 0, 12, 0);
        Grid.SetColumn(d, 1);
        grid.Children.Add(d);
        var buttons = new StackPanel { Orientation = Orientation.Horizontal };
        foreach (var r in right) buttons.Children.Add(r);
        Grid.SetColumn(buttons, 2);
        grid.Children.Add(buttons);
        return new Border
        {
            Child = grid,
            Background = Res("RowActiveBrush"),
            Padding = new Thickness(14, 9, 10, 9),
            Margin = new Thickness(0, 0, 0, 4),
            MinHeight = 44,
        };
    }

    private static string Size(long bytes) =>
        bytes < 1024 * 1024 ? $"{Math.Max(1, bytes / 1024)} KB" : $"{bytes / (1024.0 * 1024):0.0} MB";

    private void ShowSaves()
    {
        var user = UserFolder;
        SavesContent.Children.Clear();

        // Current save
        SavesContent.Children.Add(Heading(L.CurrentSave));
        var sections = SaveManager.Sections(user).Where(s => s.Files > 0).ToList();
        if (sections.Count == 0)
        {
            SavesContent.Children.Add(Label(L.NoSaves, dim: true, size: 14));
        }
        else
        {
            // Career first, then the rest by name
            foreach (var s in sections.OrderBy(s => s.Folder != "CareerData").ThenBy(s => L.SectionName(s.Folder)))
            {
                string detail = s.Folder == "CareerData" && s.LastWrite != null
                    ? string.Format(L.LastSaved, s.LastWrite.Value.ToString("g"))
                    : $"{string.Format(L.Files, s.Files)} · {Size(s.Bytes)}";
                SavesContent.Children.Add(Row(L.SectionName(s.Folder), detail));
            }
        }

        // Replays
        var replays = SaveManager.Replays(user);
        if (replays.Count > 0)
        {
            SavesContent.Children.Add(Heading(L.Replays));
            foreach (var r in replays)
            {
                SavesContent.Children.Add(Row(r.Name, $"{r.LastWrite:g} · {Size(r.Bytes)}",
                    SmallButton(L.Delete, async () =>
                    {
                        if (!await Confirm(string.Format(L.ConfirmDeleteReplay, r.Name))) return;
                        File.Delete(r.Path);
                    }, danger: true)));
            }
        }

        // Backups
        SavesContent.Children.Add(Heading(L.BackupsTitle));
        var backups = SaveManager.Backups(user);
        if (backups.Count == 0)
            SavesContent.Children.Add(Label(L.NoBackups, dim: true, size: 14));
        foreach (var b in backups)
        {
            var when = b.Created.ToString("g");
            SavesContent.Children.Add(Row(when, Size(b.Bytes),
                SmallButton(L.Restore, async () =>
                {
                    if (!await Confirm(string.Format(L.ConfirmRestore, when))) return;
                    SaveManager.Backup(user, " (auto)");
                    SaveManager.Restore(user, b.Path);
                    StatusText.Text = L.Restored;
                }),
                SmallButton(L.Delete, async () =>
                {
                    if (!await Confirm(string.Format(L.ConfirmDeleteBackup, when))) return;
                    File.Delete(b.Path);
                }, danger: true)));
        }
        var backupRow = new StackPanel { Orientation = Orientation.Horizontal, Margin = new Thickness(0, 6, 0, 0) };
        backupRow.Children.Add(BigButton(L.BackupNow, () =>
        {
            StatusText.Text = SaveManager.Backup(user) != null ? L.BackupDone : L.NothingToBackUp;
            return Task.CompletedTask;
        }));
        backupRow.Children.Add(BigButton(L.OpenFolder, () =>
        {
            Directory.CreateDirectory(user);
            Process.Start(new ProcessStartInfo { FileName = user, UseShellExecute = true });
            return Task.CompletedTask;
        }));
        SavesContent.Children.Add(backupRow);

        // Start over
        SavesContent.Children.Add(Heading(L.StartOver));
        var hint = Label(L.StartOverHint, dim: true, size: 13);
        hint.TextWrapping = TextWrapping.Wrap;
        hint.Margin = new Thickness(0, 0, 0, 10);
        SavesContent.Children.Add(hint);
        var dangerRow = new StackPanel { Orientation = Orientation.Horizontal };
        dangerRow.Children.Add(BigButton(L.DeleteSaves, async () =>
        {
            if (!await Confirm(L.ConfirmDeleteSaves)) return;
            SaveManager.Backup(user, " (auto)");
            SaveManager.DeleteSaves(user);
            StatusText.Text = L.SavesDeleted;
        }, danger: true));
        dangerRow.Children.Add(BigButton(L.ResetAchievements, async () =>
        {
            if (!await Confirm(L.ConfirmResetAchievements)) return;
            SaveManager.Backup(user, " (auto)");
            SaveManager.DeleteAchievements(user);
            StatusText.Text = L.AchievementsReset;
        }, danger: true));
        SavesContent.Children.Add(dangerRow);
    }

    // Runs a save action, reports errors, and redraws the tab.
    private async Task Run(Func<Task> action)
    {
        try
        {
            await action();
        }
        catch (Exception e)
        {
            StatusText.Text = e.Message;
        }
        ShowSaves();
    }

    private Task<bool> Confirm(string text)
    {
        var tcs = new TaskCompletionSource<bool>();
        ConfirmText.Text = text;
        ConfirmOverlay.Visibility = Visibility.Visible;
        void Done(bool result)
        {
            ConfirmOverlay.Visibility = Visibility.Collapsed;
            ConfirmYes.Click -= OnYes;
            ConfirmNo.Click -= OnNo;
            tcs.TrySetResult(result);
        }
        void OnYes(object s, RoutedEventArgs e) => Done(true);
        void OnNo(object s, RoutedEventArgs e) => Done(false);
        ConfirmYes.Click += OnYes;
        ConfirmNo.Click += OnNo;
        ConfirmNo.Focus();
        return tcs.Task;
    }
}
