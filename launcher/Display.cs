using System.Runtime.InteropServices;

namespace StuntmanLauncher;

public static class Display
{
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    private struct DEVMODE
    {
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string dmDeviceName;
        public short dmSpecVersion, dmDriverVersion, dmSize, dmDriverExtra;
        public int dmFields, dmPositionX, dmPositionY, dmDisplayOrientation, dmDisplayFixedOutput;
        public short dmColor, dmDuplex, dmYResolution, dmTTOption, dmCollate;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string dmFormName;
        public short dmLogPixels;
        public int dmBitsPerPel, dmPelsWidth, dmPelsHeight, dmDisplayFlags, dmDisplayFrequency;
        public int dmICMMethod, dmICMIntent, dmMediaType, dmDitherType, dmReserved1, dmReserved2;
        public int dmPanningWidth, dmPanningHeight;
    }

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern bool EnumDisplaySettings(string? deviceName, int modeNum, ref DEVMODE devMode);

    private const int ENUM_CURRENT_SETTINGS = -1;

    // The primary display's resolution in physical pixels (ignores DPI scaling).
    public static (int W, int H) Desktop()
    {
        var mode = new DEVMODE { dmSize = (short)Marshal.SizeOf<DEVMODE>() };
        if (EnumDisplaySettings(null, ENUM_CURRENT_SETTINGS, ref mode) && mode.dmPelsWidth > 0)
            return (mode.dmPelsWidth, mode.dmPelsHeight);
        return (1920, 1080);
    }

    public static bool Is16x9(int w, int h) => Math.Abs(w * 9 - h * 16) <= 16;

    // The largest 16:9 size that fits in w x h, rounded to even pixels.
    public static (int W, int H) Fit16x9(int w, int h)
    {
        if (Is16x9(w, h)) return (w, h);
        if (w * 9 > h * 16) return ((h * 16 / 9) & ~1, h);  // wider than 16:9
        return (w, (w * 9 / 16) & ~1);                       // taller (16:10, 4:3)
    }

    public static string AspectName(int w, int h)
    {
        double r = (double)w / h;
        if (Is16x9(w, h)) return "16:9";
        if (Math.Abs(r - 16.0 / 10) < 0.02) return "16:10";
        if (Math.Abs(r - 4.0 / 3) < 0.02) return "4:3";
        if (r > 3.3) return "32:9";
        if (r > 2.2) return "21:9";
        return $"{r:0.00}:1";
    }

    private static readonly (int W, int H)[] Presets =
    {
        (1280, 720), (1600, 900), (1920, 1080), (2560, 1440), (3840, 2160),
        (2560, 1080), (3440, 1440), (3840, 1600), (5120, 2160),
        (3840, 1080), (5120, 1440),
        (1920, 1200), (2560, 1600),
    };

    // Presets that fit on the desktop, plus the desktop size itself.
    public static List<(int W, int H)> Resolutions((int W, int H) desktop)
    {
        var list = Presets.Where(p => p.W <= desktop.W && p.H <= desktop.H).ToList();
        if (!list.Contains(desktop)) list.Add(desktop);
        return list.OrderBy(p => p.W * p.H).ThenBy(p => p.W).ToList();
    }
}
