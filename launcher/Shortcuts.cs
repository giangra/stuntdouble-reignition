using System.IO;

namespace StuntmanLauncher;

// Desktop and Start menu shortcuts to the installed launcher.
public static class Shortcuts
{
    public const string Name = "Stuntman Ignition";

    private static IEnumerable<string> Paths() => new[]
    {
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory), Name + ".lnk"),
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Programs), Name + ".lnk"),
    };

    public static void Create(string target, string workingDir)
    {
        var type = Type.GetTypeFromProgID("WScript.Shell") ?? throw new InvalidOperationException("WScript.Shell");
        dynamic shell = Activator.CreateInstance(type)!;
        foreach (var path in Paths())
        {
            dynamic link = shell.CreateShortcut(path);
            link.TargetPath = target;
            link.WorkingDirectory = workingDir;
            link.IconLocation = target + ",0";
            link.Description = "Stuntman: Ignition";
            link.Save();
        }
    }

    public static void Remove()
    {
        foreach (var path in Paths())
        {
            if (File.Exists(path)) File.Delete(path);
        }
    }
}
