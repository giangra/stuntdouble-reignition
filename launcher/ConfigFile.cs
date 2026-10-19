using System.Globalization;
using System.IO;
using System.Text.RegularExpressions;

namespace StuntmanLauncher;

// Edits stuntman.toml, the cvar file the game loads at startup. Keys are flat
// `name = value` lines. Edits keep every other line (comments, keys the
// launcher doesn't know, tables) as they were.
public sealed partial class ConfigFile
{
    private readonly string path_;
    private readonly List<string> lines_ = new();

    [GeneratedRegex(@"^\s*([A-Za-z0-9_]+)\s*=\s*(.*?)\s*$")]
    private static partial Regex KeyLine();

    [GeneratedRegex(@"^\s*\[")]
    private static partial Regex TableLine();

    public ConfigFile(string path)
    {
        path_ = path;
        if (File.Exists(path))
            lines_.AddRange(File.ReadAllLines(path));
    }

    public string Path => path_;

    // Index of the key's line in the top-level section, or -1.
    private int Find(string key)
    {
        for (int i = 0; i < lines_.Count; i++)
        {
            if (TableLine().IsMatch(lines_[i])) break;
            var m = KeyLine().Match(lines_[i]);
            if (m.Success && m.Groups[1].Value == key) return i;
        }
        return -1;
    }

    public string? GetRaw(string key)
    {
        int i = Find(key);
        if (i < 0) return null;
        var value = KeyLine().Match(lines_[i]).Groups[2].Value;
        if (value.StartsWith('"'))
        {
            int end = value.LastIndexOf('"');
            if (end <= 0) return value.Trim('"');
            return value[1..end].Replace("\\\"", "\"").Replace("\\\\", "\\");
        }
        int hash = value.IndexOf('#');
        if (hash >= 0) value = value[..hash].Trim();
        return value;
    }

    public int? GetInt(string key) =>
        int.TryParse(GetRaw(key), NumberStyles.Integer, CultureInfo.InvariantCulture, out var v) ? v : null;

    public bool? GetBool(string key) => GetRaw(key) switch
    {
        "true" => true,
        "false" => false,
        _ => null,
    };

    public double? GetDouble(string key) =>
        double.TryParse(GetRaw(key), NumberStyles.Float, CultureInfo.InvariantCulture, out var v) ? v : null;

    public void Set(string key, string tomlValue)
    {
        string line = $"{key} = {tomlValue}";
        int i = Find(key);
        if (i >= 0)
        {
            lines_[i] = line;
            return;
        }
        // New keys go before the first table so they stay top-level.
        int insert = lines_.FindIndex(l => TableLine().IsMatch(l));
        if (insert < 0) lines_.Add(line);
        else lines_.Insert(insert, line);
    }

    public void Set(string key, int value) => Set(key, value.ToString(CultureInfo.InvariantCulture));
    public void Set(string key, bool value) => Set(key, value ? "true" : "false");
    public void Set(string key, double value) => Set(key, value.ToString("0.0###", CultureInfo.InvariantCulture));
    public void SetString(string key, string value) => Set(key, $"\"{value.Replace("\\", "\\\\").Replace("\"", "\\\"")}\"");

    public void Remove(string key)
    {
        int i = Find(key);
        if (i >= 0) lines_.RemoveAt(i);
    }

    public void Save()
    {
        if (lines_.Count == 0 || !lines_[0].StartsWith('#'))
            lines_.Insert(0, "# Stuntman: Ignition settings (written by the launcher and the game)");
        File.WriteAllLines(path_, lines_);
    }
}
