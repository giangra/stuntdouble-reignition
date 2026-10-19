using System.IO;
using System.IO.Compression;
using System.Text.RegularExpressions;

namespace StuntmanLauncher;

public sealed record SaveSection(string Folder, int Files, long Bytes, DateTime? LastWrite);
public sealed record ReplayFile(string Path, string Name, long Bytes, DateTime LastWrite);
public sealed record BackupFile(string Path, DateTime Created, long Bytes);

// The game's saves as the runtime lays them out in the user folder:
//   <user>\<profile XUID>\545107F4\00000001\Save Data\<section>\...   content
//   <user>\<profile XUID>\545107F4\Headers\00000001\Save Data.header
// A backup is a zip of every profile's 545107F4 folder plus the achievement
// unlock file, so restoring one puts back exactly that state.
public static partial class SaveManager
{
    private const string Title = Achievements.TitleId;

    [GeneratedRegex("^[0-9A-Fa-f]{16}$")]
    private static partial Regex ProfileName();

    public static string BackupFolder(string user) => Path.Combine(user, "backups");

    private static string UnlockFile(string user) => Path.Combine(user, "achievements", $"{Title}.toml");

    // <user>\<XUID>\545107F4 for each profile that has played the game.
    public static List<string> TitleFolders(string user)
    {
        if (!Directory.Exists(user)) return new();
        return Directory.GetDirectories(user)
            .Where(d => ProfileName().IsMatch(Path.GetFileName(d)))
            .Select(d => Path.Combine(d, Title))
            .Where(Directory.Exists)
            .ToList();
    }

    private static IEnumerable<string> SaveDataFolders(string user) =>
        TitleFolders(user).Select(t => Path.Combine(t, "00000001", "Save Data")).Where(Directory.Exists);

    public static bool HasSaves(string user) => SaveDataFolders(user).Any();

    // One entry per top-level folder of the save (CareerData, replays, ...).
    public static List<SaveSection> Sections(string user)
    {
        var sections = new Dictionary<string, SaveSection>();
        foreach (var data in SaveDataFolders(user))
        {
            foreach (var dir in Directory.GetDirectories(data))
            {
                var files = new DirectoryInfo(dir).GetFiles("*", SearchOption.AllDirectories);
                var name = Path.GetFileName(dir);
                var last = files.Length == 0 ? (DateTime?)null : files.Max(f => f.LastWriteTime);
                sections.TryGetValue(name, out var prev);
                sections[name] = new SaveSection(
                    name,
                    (prev?.Files ?? 0) + files.Length,
                    (prev?.Bytes ?? 0) + files.Sum(f => f.Length),
                    Max(prev?.LastWrite, last));
            }
        }
        return sections.Values.ToList();
    }

    private static DateTime? Max(DateTime? a, DateTime? b) => a == null ? b : b == null ? a : (a > b ? a : b);

    public static List<ReplayFile> Replays(string user) =>
        SaveDataFolders(user)
            .Select(d => Path.Combine(d, "replays"))
            .Where(Directory.Exists)
            .SelectMany(d => new DirectoryInfo(d).GetFiles("*.rep"))
            .OrderByDescending(f => f.LastWriteTime)
            .Select(f => new ReplayFile(f.FullName, Path.GetFileNameWithoutExtension(f.Name), f.Length, f.LastWriteTime))
            .ToList();

    public static List<BackupFile> Backups(string user)
    {
        var dir = BackupFolder(user);
        if (!Directory.Exists(dir)) return new();
        return new DirectoryInfo(dir).GetFiles("*.zip")
            .OrderByDescending(f => f.CreationTime)
            .Select(f => new BackupFile(f.FullName, f.CreationTime, f.Length))
            .ToList();
    }

    // Zips the saves and the achievement unlocks. Returns the zip's path, or
    // null when there is nothing to back up.
    public static string? Backup(string user, string label = "")
    {
        var titles = TitleFolders(user);
        var unlocks = UnlockFile(user);
        if (titles.Count == 0 && !File.Exists(unlocks)) return null;

        Directory.CreateDirectory(BackupFolder(user));
        var name = $"Stuntman {DateTime.Now:yyyy-MM-dd HH.mm.ss}{label}.zip";
        var zipPath = Path.Combine(BackupFolder(user), name);
        using var zip = ZipFile.Open(zipPath, ZipArchiveMode.Create);
        foreach (var title in titles)
        {
            foreach (var file in Directory.GetFiles(title, "*", SearchOption.AllDirectories))
                zip.CreateEntryFromFile(file, Path.GetRelativePath(user, file).Replace('\\', '/'));
        }
        if (File.Exists(unlocks))
            zip.CreateEntryFromFile(unlocks, Path.GetRelativePath(user, unlocks).Replace('\\', '/'));
        return zipPath;
    }

    // Removes every profile's saves for the game. Achievements are kept.
    public static void DeleteSaves(string user)
    {
        foreach (var title in TitleFolders(user))
            Directory.Delete(title, recursive: true);
    }

    public static void DeleteAchievements(string user)
    {
        var unlocks = UnlockFile(user);
        if (File.Exists(unlocks)) File.Delete(unlocks);
    }

    // Replaces the current saves and unlocks with the backup's.
    public static void Restore(string user, string zipPath)
    {
        DeleteSaves(user);
        DeleteAchievements(user);
        using var zip = ZipFile.OpenRead(zipPath);
        var root = Path.GetFullPath(user) + Path.DirectorySeparatorChar;
        foreach (var entry in zip.Entries)
        {
            if (string.IsNullOrEmpty(entry.Name)) continue;  // folder entry
            var target = Path.GetFullPath(Path.Combine(user, entry.FullName));
            if (!target.StartsWith(root, StringComparison.OrdinalIgnoreCase)) continue;  // stay inside
            Directory.CreateDirectory(Path.GetDirectoryName(target)!);
            entry.ExtractToFile(target, overwrite: true);
        }
    }
}
