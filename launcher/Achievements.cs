using System.IO;
using System.Text.Json;
using System.Text.RegularExpressions;

namespace StuntmanLauncher;

public sealed record Achievement(
    int Id,
    string Label,
    string Description,
    int Gamerscore,
    string? IconPath,
    DateTime? UnlockedAt);

// Reads what the game leaves in its user folder (Documents\stuntman by
// default): the catalog it exports at startup (text and icons) and the
// runtime's unlock file (achievement id -> FILETIME).
public static partial class Achievements
{
    public const string TitleId = "545107F4";

    [GeneratedRegex(@"\[unlocked\.(\d+)\]\s*filetime\s*=\s*(\d+)")]
    private static partial Regex UnlockEntry();

    public static string UserFolder(ConfigFile cfg)
    {
        var root = cfg.GetRaw("user_data_root");
        if (!string.IsNullOrEmpty(root)) return root;
        return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments), "stuntman");
    }

    // Null when the game hasn't written the catalog yet (never started).
    public static List<Achievement>? Load(string userFolder)
    {
        var dir = Path.Combine(userFolder, "achievements");
        var catalogPath = Path.Combine(dir, $"{TitleId}.catalog.json");
        if (!File.Exists(catalogPath)) return null;

        var unlocks = new Dictionary<int, DateTime>();
        var unlockPath = Path.Combine(dir, $"{TitleId}.toml");
        if (File.Exists(unlockPath))
        {
            foreach (Match m in UnlockEntry().Matches(File.ReadAllText(unlockPath)))
            {
                if (int.TryParse(m.Groups[1].Value, out int id) && long.TryParse(m.Groups[2].Value, out long ft))
                    unlocks[id] = DateTime.FromFileTimeUtc(ft).ToLocalTime();
            }
        }

        using var doc = JsonDocument.Parse(File.ReadAllText(catalogPath));
        var list = new List<Achievement>();
        foreach (var e in doc.RootElement.GetProperty("achievements").EnumerateArray())
        {
            int id = e.GetProperty("id").GetInt32();
            int image = e.GetProperty("image_id").GetInt32();
            var icon = Path.Combine(dir, "icons", TitleId, $"{image}.png");
            bool unlocked = unlocks.TryGetValue(id, out var when);
            string desc = e.GetProperty("description").GetString() ?? "";
            if (!unlocked)
            {
                var locked = e.GetProperty("unachieved_description").GetString();
                if (!string.IsNullOrEmpty(locked)) desc = locked;
            }
            list.Add(new Achievement(
                id,
                e.GetProperty("label").GetString() ?? "",
                desc,
                e.GetProperty("gamerscore").GetInt32(),
                File.Exists(icon) ? icon : null,
                unlocked ? when : null));
        }

        // Newest unlocks first, then the locked ones in game order.
        return list
            .OrderBy(a => a.UnlockedAt == null)
            .ThenByDescending(a => a.UnlockedAt)
            .ThenBy(a => a.Id)
            .ToList();
    }
}
