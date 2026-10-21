using System.IO;

namespace StuntmanLauncher;

// Where the game keeps its user data (Documents\stuntman by default).
public static class GamePaths
{
    public const string TitleId = "545107F4";

    public static string UserFolder(ConfigFile cfg)
    {
        var root = cfg.GetRaw("user_data_root");
        if (!string.IsNullOrEmpty(root)) return root;
        return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments), "stuntman");
    }
}
