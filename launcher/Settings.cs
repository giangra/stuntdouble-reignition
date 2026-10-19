namespace StuntmanLauncher;

public enum AspectMode
{
    // Keep the game's 16:9 picture and fill the rest with black bars.
    Bars,
    // Render at the display's full size; the 16:9 picture is stretched.
    Stretch,
}

// The launcher's view of stuntman.toml. Load reads the cvars back into these
// fields, Apply writes them out. Defaults match the game's own.
public sealed class Settings
{
    public bool Fullscreen = true;
    public int Width;
    public int Height;
    public AspectMode Aspect = AspectMode.Bars;
    public int RenderScale = 1;
    public bool VSync = true;
    public int Anisotropic = 3;
    public string Upscaler = "bilinear";
    public int Language = Languages.English;
    public int TargetFps = 30;
    public bool WriteLog = false;

    public static Settings Load(ConfigFile cfg, (int W, int H) desktop)
    {
        var s = new Settings
        {
            Fullscreen = cfg.GetBool("fullscreen") ?? true,
            Width = cfg.GetInt("window_width") ?? cfg.GetInt("video_mode_width") ?? desktop.W,
            Height = cfg.GetInt("window_height") ?? cfg.GetInt("video_mode_height") ?? desktop.H,
            RenderScale = Math.Clamp(cfg.GetInt("resolution_scale") ?? 1, 1, 3),
            VSync = cfg.GetBool("vsync") ?? true,
            Anisotropic = Math.Clamp(cfg.GetInt("anisotropic_override") ?? 3, -1, 5),
            Upscaler = cfg.GetRaw("present_effect") ?? "bilinear",
            Language = Languages.IsSupported(cfg.GetInt("user_language") ?? 0)
                ? cfg.GetInt("user_language")!.Value
                : Languages.FromWindows(),
            TargetFps = cfg.GetInt("target_fps") == 60 ? 60 : 30,
            WriteLog = !string.IsNullOrEmpty(cfg.GetRaw("log_file")),
        };
        if (s.Width <= 0 || s.Height <= 0) (s.Width, s.Height) = desktop;

        // The guest video mode differs from the window only in "bars" mode.
        int? vw = cfg.GetInt("video_mode_width");
        int? vh = cfg.GetInt("video_mode_height");
        if (vw.HasValue && vh.HasValue && !Display.Is16x9(s.Width, s.Height))
            s.Aspect = (vw == s.Width && vh == s.Height) ? AspectMode.Stretch : AspectMode.Bars;
        return s;
    }

    public void Apply(ConfigFile cfg)
    {
        // The game sizes its back buffer from the video mode, so it renders
        // natively at that size. In "bars" mode it gets the largest 16:9 area
        // that fits; the presenter letterboxes it into the window.
        var (vw, vh) = Aspect == AspectMode.Bars ? Display.Fit16x9(Width, Height) : (Width, Height);

        cfg.Remove("resolution");  // preset cvar; would compete with the explicit sizes
        cfg.Set("fullscreen", Fullscreen);
        cfg.Set("window_width", Width);
        cfg.Set("window_height", Height);
        cfg.Set("video_mode_width", vw);
        cfg.Set("video_mode_height", vh);
        cfg.Set("present_letterbox", true);
        cfg.Set("resolution_scale", RenderScale);
        cfg.Set("vsync", VSync);
        cfg.Set("anisotropic_override", Anisotropic);
        cfg.SetString("present_effect", Upscaler);
        cfg.Set("user_language", Language);
        cfg.Set("target_fps", TargetFps);

        if (WriteLog)
        {
            if (string.IsNullOrEmpty(cfg.GetRaw("log_file"))) cfg.SetString("log_file", "stuntman.log");
        }
        else
        {
            cfg.Remove("log_file");
        }
    }
}

// Xbox 360 language IDs (XLanguage). The disc has text and speech for these.
public static class Languages
{
    public const int English = 1;
    public const int German = 3;
    public const int French = 4;
    public const int Spanish = 5;
    public const int Italian = 6;

    public static readonly (int Id, string Name)[] All =
    {
        (English, "English"),
        (French, "Français"),
        (German, "Deutsch"),
        (Spanish, "Español"),
        (Italian, "Italiano"),
    };

    public static bool IsSupported(int id) => All.Any(l => l.Id == id);

    public static int FromWindows() => System.Globalization.CultureInfo.CurrentUICulture.TwoLetterISOLanguageName switch
    {
        "fr" => French,
        "de" => German,
        "es" => Spanish,
        "it" => Italian,
        _ => English,
    };
}
