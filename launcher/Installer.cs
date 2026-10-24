using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Net.Http;
using System.Reflection;
using System.Security.Cryptography;
using System.Security.Cryptography.X509Certificates;
using System.Text.RegularExpressions;

namespace StuntmanLauncher;

public enum InstallStep { CheckIso, Tools, Git, Source, Extract, Build, Finish }

public sealed record InstallProgress(InstallStep Step, double? Fraction, string? Detail);

// Installs the game from the user's own disc image: checks the ISO, gets the
// build tools if missing (Visual Studio Build Tools, a portable Git), fetches
// this project's source at the matching tag, extracts the ISO, then runs
// scripts\build_release.ps1, which generates and compiles the game and puts
// the result in the install folder. Nothing of the game is downloaded.
// Without an ISO it updates an existing install (see Updater): the game files
// extracted the first time are reused and only the source and build are new.
public sealed partial class Installer
{
    public const string TitleId = "545107F4";
    public const string RepoUrl = "https://github.com/giangra/stuntdouble-reignition.git";
    private const string BuildToolsUrl = "https://aka.ms/vs/18/stable/vs_buildtools.exe";
    private const string MinGitUrl =
        "https://github.com/git-for-windows/git/releases/download/v2.56.0.windows.2/MinGit-2.56.0.2-64-bit.zip";
    private const string MinGitSha256 = "da35e72aa21c005a5a0d298cfbae110bc1609a815730ea0dde84b01a1b3cd3be";

    // What the build needs from Visual Studio
    private static readonly string[] VsComponents =
    {
        "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
        "Microsoft.VisualStudio.Component.VC.Llvm.Clang",
        "Microsoft.VisualStudio.Component.VC.CMake.Project",
    };

    public const long RequiredFreeBytes = 12L << 30;

    private readonly string? iso_;
    private readonly string dir_;
    private readonly bool keepBuild_;
    private readonly IProgress<InstallProgress> progress_;
    private readonly StreamWriter log_;
    private readonly Dictionary<string, string> env_ = new();

    public Installer(string? iso, string installDir, bool keepBuild, IProgress<InstallProgress> progress)
    {
        iso_ = iso;
        dir_ = Path.GetFullPath(installDir);
        keepBuild_ = keepBuild;
        progress_ = progress;
        Directory.CreateDirectory(dir_);
        log_ = new StreamWriter(LogPath, append: true) { AutoFlush = true };
        // Some SDK submodules have deep paths
        env_["GIT_CONFIG_COUNT"] = "1";
        env_["GIT_CONFIG_KEY_0"] = "core.longpaths";
        env_["GIT_CONFIG_VALUE_0"] = "true";
    }

    public string LogPath => Path.Combine(dir_, "install.log");
    private string BuildDir => Path.Combine(dir_, "build");
    private string GameDir => Path.Combine(dir_, "game");
    private string ToolsDir => Path.Combine(dir_, "tools");
    public string LauncherPath => Path.Combine(dir_, "StuntmanLauncher.exe");
    public string GamePath => Path.Combine(dir_, "stuntman.exe");
    public bool IsUpdate => iso_ == null;

    public event Action<string>? LogLine;

    private void Log(string line)
    {
        log_.WriteLine(line);
        LogLine?.Invoke(line);
    }

    private void Report(InstallStep step, double? fraction = null, string? detail = null) =>
        progress_.Report(new InstallProgress(step, fraction, detail));

    // The checks the UI can run before starting: is this the game's disc?
    public static string? CheckIso(string path)
    {
        try
        {
            using var x = new XisoReader(path);
            var xex = x.Find("default.xex");
            if (xex == null) return "default.xex";
            var id = XisoReader.XexTitleId(x.ReadStart(xex, 64 * 1024));
            return id == Convert.ToUInt32(TitleId, 16) ? null : $"{id:X8}";
        }
        catch (Exception e)
        {
            return e.Message;
        }
    }

    public async Task RunAsync(CancellationToken cancel)
    {
        Log(IsUpdate
            ? $"=== Update {DateTime.Now:u} of {dir_} to {Updater.Current}"
            : $"=== Install {DateTime.Now:u} to {dir_}, ISO {iso_}");
        try
        {
            await Task.Run(() => CheckSpace(), cancel);
            await EnsureBuildToolsAsync(cancel);
            await EnsureGitAsync(cancel);
            await FetchSourceAsync(cancel);
            await Task.Run(() => ExtractIso(cancel), cancel);
            await BuildAsync(cancel);
            await Task.Run(() => Finish(), cancel);
            Log("=== Done");
        }
        catch (Exception e)
        {
            Log($"=== Failed: {e}");
            throw;
        }
        finally
        {
            log_.Dispose();
        }
    }

    // ---- Steps ------------------------------------------------------------

    private void CheckSpace()
    {
        Report(InstallStep.CheckIso);
        if (IsUpdate)
        {
            if (!File.Exists(Path.Combine(GameDir, "default.xex")))
                throw new InstallException(L.UpdateNoGameFiles);
            // The build replaces stuntman.exe
            if (Process.GetProcessesByName("stuntman").Length > 0) throw new InstallException(L.UpdateCloseGame);
        }
        else
        {
            var error = CheckIso(iso_!);
            if (error != null) throw new InstallException(string.Format(L.IsoWrong, error));
        }

        var drive = new DriveInfo(Path.GetPathRoot(dir_)!);
        long needed = RequiredFreeBytes - (Directory.Exists(GameDir) ? 6L << 30 : 0);
        if (drive.AvailableFreeSpace < needed)
            throw new InstallException(string.Format(L.NoSpace, drive.Name, needed >> 30));
    }

    private static string VsWhere =>
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86),
            "Microsoft Visual Studio", "Installer", "vswhere.exe");

    private async Task<string?> FindVisualStudioAsync()
    {
        if (!File.Exists(VsWhere)) return null;
        var args = new List<string> { "-latest", "-prerelease", "-products", "*", "-requires" };
        args.AddRange(VsComponents);
        args.AddRange(new[] { "-property", "installationPath" });
        var (code, output) = await CaptureAsync(VsWhere, args);
        var path = output.Split('\n').Select(l => l.Trim()).FirstOrDefault(l => l.Length > 0);
        return code == 0 && path != null && Directory.Exists(path) ? path : null;
    }

    private async Task EnsureBuildToolsAsync(CancellationToken cancel)
    {
        Report(InstallStep.Tools);
        var vs = await FindVisualStudioAsync();
        if (vs != null)
        {
            Log($"Visual Studio found: {vs}");
            return;
        }

        Report(InstallStep.Tools, null, L.DownloadingBuildTools);
        var bootstrapper = Path.Combine(ToolsDir, "vs_buildtools.exe");
        await DownloadAsync(BuildToolsUrl, bootstrapper, InstallStep.Tools, cancel);
        CheckMicrosoftSignature(bootstrapper);

        // The Visual Studio installer asks for administrator rights itself
        // and shows its own progress window (--passive).
        Report(InstallStep.Tools, null, L.InstallingBuildTools);
        var args = new List<string> { "--passive", "--wait", "--norestart", "--nocache",
            "--add", "Microsoft.VisualStudio.Workload.VCTools", "--includeRecommended" };
        foreach (var c in VsComponents.Append("Microsoft.VisualStudio.Component.VC.Llvm.ClangToolset"))
        {
            args.Add("--add");
            args.Add(c);
        }
        Log($"> {bootstrapper} {string.Join(' ', args)}");
        var psi = new ProcessStartInfo(bootstrapper) { UseShellExecute = true, Verb = "runas" };
        foreach (var a in args) psi.ArgumentList.Add(a);
        using var p = Process.Start(psi) ?? throw new InstallException(L.BuildToolsFailed);
        await p.WaitForExitAsync(cancel);
        Log($"Build Tools installer exit code {p.ExitCode}");
        // 3010: done, reboot recommended
        if (p.ExitCode != 0 && p.ExitCode != 3010)
            throw new InstallException($"{L.BuildToolsFailed} ({p.ExitCode})");
        if (await FindVisualStudioAsync() == null)
            throw new InstallException(L.BuildToolsFailed);
    }

    private static void CheckMicrosoftSignature(string file)
    {
#pragma warning disable SYSLIB0057
        var cert = X509Certificate.CreateFromSignedFile(file);
#pragma warning restore SYSLIB0057
        if (!cert.Subject.Contains("O=Microsoft Corporation"))
            throw new InstallException($"Unexpected signer for {Path.GetFileName(file)}: {cert.Subject}");
    }

    private string? git_;

    private async Task EnsureGitAsync(CancellationToken cancel)
    {
        Report(InstallStep.Git);
        var portable = Path.Combine(ToolsDir, "git", "cmd", "git.exe");
        if (File.Exists(portable))
        {
            git_ = portable;
        }
        else if (await IsOnPathAsync("git"))
        {
            git_ = "git";
        }
        else
        {
            var zip = Path.Combine(ToolsDir, "mingit.zip");
            await DownloadAsync(MinGitUrl, zip, InstallStep.Git, cancel);
            using (var s = File.OpenRead(zip))
            {
                var hash = Convert.ToHexString(await SHA256.HashDataAsync(s, cancel)).ToLowerInvariant();
                if (hash != MinGitSha256) throw new InstallException($"Git download is corrupt ({hash})");
            }
            ZipFile.ExtractToDirectory(zip, Path.Combine(ToolsDir, "git"), overwriteFiles: true);
            File.Delete(zip);
            git_ = portable;
        }
        if (git_ != "git")
            env_["PATH"] = Path.GetDirectoryName(git_) + ";" + Environment.GetEnvironmentVariable("PATH");
        Log($"Git: {git_}");
    }

    // The source at the tag of this launcher's version. For testing,
    // STUNTMAN_SOURCE and STUNTMAN_SOURCE_REF pick another repo or branch.
    private async Task FetchSourceAsync(CancellationToken cancel)
    {
        Report(InstallStep.Source);
        if (Directory.Exists(Path.Combine(BuildDir, ".git")))
        {
            if (!IsUpdate)
            {
                Log("Source already present, reusing it");
                return;
            }
            // Kept build files are of the old version: start from a clean
            // checkout of the new one.
            Log("Removing the previous version's build files");
            await Task.Run(RemoveBuildDir, cancel);
        }
        var version = Assembly.GetExecutingAssembly().GetName().Version!;
        var source = Environment.GetEnvironmentVariable("STUNTMAN_SOURCE") ?? RepoUrl;
        var reference = Environment.GetEnvironmentVariable("STUNTMAN_SOURCE_REF") ?? $"v{version.ToString(3)}";
        await RunAsync(git_!, new[] { "clone", "--depth", "1", "--branch", reference, source, BuildDir },
            null, InstallStep.Source, cancel);
    }

    private void ExtractIso(CancellationToken cancel)
    {
        if (IsUpdate)
            Report(InstallStep.Extract, 1);
        else
            ExtractFiles(cancel);
        LinkGameFolder();
    }

    private void ExtractFiles(CancellationToken cancel)
    {
        using var x = new XisoReader(iso_!);
        long total = x.TotalFileBytes;
        var progress = new Progress<long>(done => Report(InstallStep.Extract, (double)done / total));
        Report(InstallStep.Extract, 0);
        x.ExtractAll(GameDir, new ThrottledProgress(progress), cancel);
        Log($"Extracted {x.Entries.Count} entries to {GameDir}");
    }

    private void LinkGameFolder()
    {
        // The build reads the game from build\game
        var link = Path.Combine(BuildDir, "game");
        if (!Directory.Exists(link))
        {
            var p = Process.Start(new ProcessStartInfo("cmd.exe", $"/c mklink /J \"{link}\" \"{GameDir}\"")
            {
                CreateNoWindow = true,
                UseShellExecute = false,
            })!;
            p.WaitForExit();
            if (!Directory.Exists(link)) throw new InstallException("Could not link the game folder");
        }
    }

    [GeneratedRegex(@"^\[(\d+)/(\d+)\]")]
    private static partial Regex NinjaProgress();

    [GeneratedRegex(@"^##STEP (\d+) (\S+)")]
    private static partial Regex ScriptStep();

    private async Task BuildAsync(CancellationToken cancel)
    {
        Report(InstallStep.Build, null, L.BuildPreparing);
        var script = Path.Combine(BuildDir, "scripts", "build_release.ps1");
        var shell = await IsOnPathAsync("pwsh") ? "pwsh" : "powershell.exe";
        string detail = L.BuildPreparing;
        await RunAsync(shell,
            new[] { "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", script, "-InstallDir", dir_ },
            line =>
            {
                var step = ScriptStep().Match(line);
                if (step.Success)
                {
                    detail = step.Groups[2].Value switch
                    {
                        "sdk" => L.BuildSdk,
                        "configure" => L.BuildCodegen,
                        "build" => L.BuildCompile,
                        "install" => L.BuildCopy,
                        _ => L.BuildPreparing,
                    };
                    Report(InstallStep.Build, null, detail);
                    return;
                }
                var m = NinjaProgress().Match(line);
                if (m.Success && double.TryParse(m.Groups[2].Value, out var total) && total > 0)
                    Report(InstallStep.Build, double.Parse(m.Groups[1].Value) / total, detail);
            },
            InstallStep.Build, cancel);
        if (!File.Exists(Path.Combine(dir_, "stuntman.exe")))
            throw new InstallException(L.BuildFailed);
    }

    private void Finish()
    {
        Report(InstallStep.Finish);
        var self = Environment.ProcessPath!;
        if (!string.Equals(Path.GetFullPath(self), LauncherPath, StringComparison.OrdinalIgnoreCase))
            CopyLauncher(self);

        Shortcuts.Create(GamePath, LauncherPath, dir_);

        if (!keepBuild_)
        {
            RemoveBuildDir();
            DeleteTree(ToolsDir);
            Log("Build files removed");
        }
    }

    private void RemoveBuildDir()
    {
        // Unlink the game folder before deleting, so the delete can't reach
        // through the junction into the extracted files.
        var link = Path.Combine(BuildDir, "game");
        if (Directory.Exists(link)) Directory.Delete(link);
        DeleteTree(BuildDir);
    }

    // When updating, the old launcher that started this one may take a moment
    // to exit and release its exe.
    private void CopyLauncher(string self)
    {
        for (int attempt = 0; ; ++attempt)
        {
            try
            {
                File.Copy(self, LauncherPath, overwrite: true);
                return;
            }
            catch (IOException) when (attempt < 20)
            {
                Thread.Sleep(500);
            }
        }
    }

    private static void DeleteTree(string path)
    {
        if (!Directory.Exists(path)) return;
        foreach (var f in Directory.EnumerateFiles(path, "*", SearchOption.AllDirectories))
            File.SetAttributes(f, FileAttributes.Normal);  // git's read-only objects
        Directory.Delete(path, recursive: true);
    }

    // ---- Process and download helpers -------------------------------------

    private async Task<bool> IsOnPathAsync(string exe)
    {
        try
        {
            var (code, _) = await CaptureAsync("where.exe", new[] { exe });
            return code == 0;
        }
        catch
        {
            return false;
        }
    }

    private async Task<(int, string)> CaptureAsync(string exe, IEnumerable<string> args)
    {
        var psi = new ProcessStartInfo(exe)
        {
            UseShellExecute = false,
            RedirectStandardOutput = true,
            CreateNoWindow = true,
        };
        foreach (var a in args) psi.ArgumentList.Add(a);
        using var p = Process.Start(psi)!;
        var output = await p.StandardOutput.ReadToEndAsync();
        await p.WaitForExitAsync();
        return (p.ExitCode, output);
    }

    private async Task RunAsync(string exe, IEnumerable<string> args, Action<string>? onLine, InstallStep step,
        CancellationToken cancel)
    {
        var psi = new ProcessStartInfo(exe)
        {
            UseShellExecute = false,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            CreateNoWindow = true,
            WorkingDirectory = dir_,
        };
        foreach (var a in args) psi.ArgumentList.Add(a);
        foreach (var (k, v) in env_) psi.Environment[k] = v;
        Log($"> {exe} {string.Join(' ', psi.ArgumentList)}");

        using var p = new Process { StartInfo = psi };
        void Handle(string? line)
        {
            if (line == null) return;
            Log(line);
            onLine?.Invoke(line);
        }
        p.OutputDataReceived += (_, e) => Handle(e.Data);
        p.ErrorDataReceived += (_, e) => Handle(e.Data);
        p.Start();
        p.BeginOutputReadLine();
        p.BeginErrorReadLine();
        using (cancel.Register(() => { try { p.Kill(entireProcessTree: true); } catch { } }))
            await p.WaitForExitAsync();
        cancel.ThrowIfCancellationRequested();
        if (p.ExitCode != 0)
            throw new InstallException(string.Format(L.StepFailed, Path.GetFileName(exe), p.ExitCode));
    }

    private async Task DownloadAsync(string url, string target, InstallStep step, CancellationToken cancel)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(target)!);
        Log($"Download {url}");
        using var http = new HttpClient();
        http.DefaultRequestHeaders.UserAgent.ParseAdd("StuntmanLauncher");
        using var response = await http.GetAsync(url, HttpCompletionOption.ResponseHeadersRead, cancel);
        response.EnsureSuccessStatusCode();
        long? total = response.Content.Headers.ContentLength;
        await using var input = await response.Content.ReadAsStreamAsync(cancel);
        await using var output = File.Create(target);
        var buffer = new byte[1 << 16];
        long done = 0;
        int n;
        while ((n = await input.ReadAsync(buffer, cancel)) > 0)
        {
            await output.WriteAsync(buffer.AsMemory(0, n), cancel);
            done += n;
            if (total > 0) Report(step, (double)done / total.Value);
        }
    }

    // Progress<T> posts every report to the UI thread; extraction reports per
    // megabyte, so pass on only every ~0.2%.
    private sealed class ThrottledProgress(IProgress<long> inner) : IProgress<long>
    {
        private long last_ = -1;
        public void Report(long value)
        {
            if (last_ >= 0 && value - last_ < 12 << 20) return;
            last_ = value;
            inner.Report(value);
        }
    }
}

public sealed class InstallException(string message) : Exception(message);
