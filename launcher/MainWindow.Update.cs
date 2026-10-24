using System.IO;
using System.Windows;

namespace StuntmanLauncher;

// Update check for installed games (see Updater): a button under the version
// when GitHub has a newer release. It downloads the new setup, starts it on
// this folder and closes the launcher so the setup can replace it.
public partial class MainWindow
{
    private UpdateInfo? update_;

    // Only for installs made by the installer: a development build next to
    // the repo isn't updated this way.
    private void CheckForUpdate()
    {
        if (!File.Exists(Path.Combine(exeDir_, "install.log"))) return;
        Loaded += async (_, _) =>
        {
            update_ = await Updater.CheckAsync();
            if (update_ == null) return;
            UpdateButton.Content = string.Format(L.UpdateAvailable, update_.Version.ToString(3));
            UpdateButton.Visibility = Visibility.Visible;
        };
    }

    private async void OnUpdate(object sender, RoutedEventArgs e)
    {
        if (update_ == null) return;
        if (!await Confirm(string.Format(L.ConfirmUpdate, update_.Version.ToString(3)))) return;

        UpdateButton.IsEnabled = false;
        PlayButton.IsEnabled = false;
        try
        {
            var progress = new Progress<double>(f => StatusText.Text = string.Format(L.UpdateDownloading, f));
            var setup = await Updater.DownloadAsync(update_, progress);
            Updater.StartSetup(setup, exeDir_);
            Application.Current.Shutdown();
        }
        catch (Exception ex)
        {
            StatusText.Text = string.Format(L.UpdateFailed, ex.Message);
            UpdateButton.IsEnabled = true;
            PlayButton.IsEnabled = true;
        }
    }
}
