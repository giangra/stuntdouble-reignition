using System.IO;

namespace StuntmanLauncher;

// Shortcuts made by the installer: the game on the desktop and in the Start
// menu, the launcher (saves, uninstall) in the Start menu.
public static class Shortcuts
{
    public const string Name = "Stuntman Ignition";
    private const string LauncherName = "Stuntman Ignition Launcher";

    private static string Desktop(string name) =>
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory), name + ".lnk");

    private static string StartMenu(string name) =>
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Programs), name + ".lnk");

    public static void Create(string game, string launcher, string workingDir)
    {
        var type = Type.GetTypeFromProgID("WScript.Shell") ?? throw new InvalidOperationException("WScript.Shell");
        dynamic shell = Activator.CreateInstance(type)!;
        void Make(string path, string target, string description)
        {
            dynamic link = shell.CreateShortcut(path);
            link.TargetPath = target;
            link.WorkingDirectory = workingDir;
            link.IconLocation = target + ",0";
            link.Description = description;
            link.Save();
        }
        Make(Desktop(Name), game, "Stuntman: Ignition");
        Make(StartMenu(Name), game, "Stuntman: Ignition");
        Make(StartMenu(LauncherName), launcher, "Stuntman: Ignition launcher (saves, uninstall)");
    }

    public static void Remove()
    {
        foreach (var path in new[] { Desktop(Name), StartMenu(Name), StartMenu(LauncherName) })
        {
            if (File.Exists(path)) File.Delete(path);
        }
    }
}
