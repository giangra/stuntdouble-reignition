using System.Diagnostics;
using System.IO;
using System.Net.Http;
using System.Reflection;
using System.Security.Cryptography;
using System.Text.Json;

namespace StuntmanLauncher;

public sealed record UpdateInfo(Version Version, string AssetName, string AssetUrl, string? Sha256);

// Updates an installed game to the latest GitHub release. Releases carry only
// the launcher (StuntmanSetup-<version>.exe); the game itself is rebuilt on
// this PC. So updating means: download the new setup, start it with
// --update <install folder>, and exit. The new setup fetches the source at its
// own tag and rebuilds from the game files already extracted in the install
// folder (no ISO needed), then replaces this launcher.
public static class Updater
{
    private const string LatestReleaseApi =
        "https://api.github.com/repos/giangra/stuntdouble-reignition/releases/latest";

    public static Version Current =>
        Assembly.GetExecutingAssembly().GetName().Version is { } v ? new Version(v.Major, v.Minor, v.Build) : new(0, 0, 0);

    private static HttpClient NewClient(TimeSpan timeout)
    {
        var http = new HttpClient { Timeout = timeout };
        http.DefaultRequestHeaders.UserAgent.ParseAdd($"StuntmanLauncher/{Current}");
        return http;
    }

    // The latest release if it is newer than this launcher and has a setup
    // exe; null otherwise, also when offline or GitHub doesn't answer.
    public static async Task<UpdateInfo?> CheckAsync(CancellationToken cancel = default)
    {
        try
        {
            using var http = NewClient(TimeSpan.FromSeconds(10));
            http.DefaultRequestHeaders.Accept.ParseAdd("application/vnd.github+json");
            await using var stream = await http.GetStreamAsync(LatestReleaseApi, cancel);
            using var json = await JsonDocument.ParseAsync(stream, cancellationToken: cancel);
            var root = json.RootElement;
            var tag = root.GetProperty("tag_name").GetString() ?? "";
            if (!Version.TryParse(tag.TrimStart('v', 'V'), out var version)) return null;
            version = new Version(version.Major, version.Minor, Math.Max(0, version.Build));
            if (version <= Current) return null;
            foreach (var asset in root.GetProperty("assets").EnumerateArray())
            {
                var name = asset.GetProperty("name").GetString() ?? "";
                if (!name.StartsWith("StuntmanSetup", StringComparison.OrdinalIgnoreCase) ||
                    !name.EndsWith(".exe", StringComparison.OrdinalIgnoreCase))
                    continue;
                var url = asset.GetProperty("browser_download_url").GetString();
                if (url == null) continue;
                // GitHub lists "sha256:<hex>" for assets uploaded since mid-2025
                string? sha = null;
                if (asset.TryGetProperty("digest", out var digest) &&
                    digest.GetString() is string d && d.StartsWith("sha256:", StringComparison.Ordinal))
                    sha = d["sha256:".Length..].ToLowerInvariant();
                return new UpdateInfo(version, name, url, sha);
            }
        }
        catch (Exception e) when (e is HttpRequestException or TaskCanceledException or JsonException
                                      or KeyNotFoundException or InvalidOperationException or IOException)
        {
        }
        return null;
    }

    // Downloads the new setup to %TEMP% and checks it is what the release
    // says: its SHA-256 (when GitHub gives one) and its version.
    public static async Task<string> DownloadAsync(UpdateInfo update, IProgress<double> progress,
        CancellationToken cancel = default)
    {
        var dir = Path.Combine(Path.GetTempPath(), "StuntmanUpdate");
        Directory.CreateDirectory(dir);
        var target = Path.Combine(dir, Path.GetFileName(update.AssetName));

        using var http = NewClient(TimeSpan.FromMinutes(10));
        using var response = await http.GetAsync(update.AssetUrl, HttpCompletionOption.ResponseHeadersRead, cancel);
        response.EnsureSuccessStatusCode();
        long? total = response.Content.Headers.ContentLength;
        await using (var input = await response.Content.ReadAsStreamAsync(cancel))
        await using (var output = File.Create(target))
        {
            var buffer = new byte[1 << 16];
            long done = 0;
            int n;
            while ((n = await input.ReadAsync(buffer, cancel)) > 0)
            {
                await output.WriteAsync(buffer.AsMemory(0, n), cancel);
                done += n;
                if (total > 0) progress.Report((double)done / total.Value);
            }
        }

        if (update.Sha256 != null)
        {
            await using var s = File.OpenRead(target);
            var hash = Convert.ToHexString(await SHA256.HashDataAsync(s, cancel)).ToLowerInvariant();
            if (hash != update.Sha256) throw new InstallException(string.Format(L.UpdateCorrupt, hash));
        }
        var info = FileVersionInfo.GetVersionInfo(target);
        var expected = update.Version.ToString(3);
        if (info.ProductVersion?.StartsWith(expected, StringComparison.Ordinal) != true)
            throw new InstallException(string.Format(L.UpdateCorrupt, info.ProductVersion ?? "?"));
        return target;
    }

    // Starts the new setup on the install folder; the caller then exits so
    // the setup can replace StuntmanLauncher.exe.
    public static void StartSetup(string setup, string installDir)
    {
        var psi = new ProcessStartInfo(setup)
        {
            UseShellExecute = false,
            WorkingDirectory = Path.GetTempPath(),
        };
        psi.ArgumentList.Add("--update");
        psi.ArgumentList.Add(installDir.TrimEnd('\\'));
        Process.Start(psi);
    }
}
