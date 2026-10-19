using System.Globalization;

namespace StuntmanLauncher;

// Launcher interface text in English and Italian, picked from the Windows
// display language.
public static class L
{
    private static readonly bool It = CultureInfo.CurrentUICulture.TwoLetterISOLanguageName == "it";

    private static string T(string en, string it) => It ? it : en;

    public static string Tagline => T("RECOMPILED FOR PC", "RICOMPILATO PER PC");
    public static string Play => T("PLAY", "GIOCA");
    public static string Running => T("RUNNING…", "IN CORSO…");
    public static string Save => T("Save", "Salva");
    public static string Defaults => T("Defaults", "Predefiniti");

    public static string TabVideo => T("VIDEO", "VIDEO");
    public static string TabGame => T("GAME", "GIOCO");
    public static string TabAdvanced => T("ADVANCED", "AVANZATE");
    public static string TabAchievements => T("ACHIEVEMENTS", "OBIETTIVI");
    public static string TabSaves => T("SAVES", "SALVATAGGI");

    public static string CurrentSave => T("CURRENT SAVE", "SALVATAGGIO ATTUALE");
    public static string NoSaves => T("No saves yet: they appear after you play.",
        "Nessun salvataggio: compare dopo aver giocato.");
    public static string Replays => T("SAVED REPLAYS", "REPLAY SALVATI");
    public static string BackupsTitle => T("BACKUPS", "BACKUP");
    public static string NoBackups => T("No backups yet.", "Nessun backup.");
    public static string StartOver => T("START OVER", "RICOMINCIA DA ZERO");
    public static string BackupNow => T("Back up now", "Fai un backup");
    public static string OpenFolder => T("Open folder", "Apri cartella");
    public static string Restore => T("Restore", "Ripristina");
    public static string Delete => T("Delete", "Elimina");
    public static string DeleteSaves => T("Delete all saves", "Cancella i salvataggi");
    public static string ResetAchievements => T("Reset achievements", "Azzera gli obiettivi");
    public static string StartOverHint => T(
        "A backup is made automatically before anything is deleted or restored.",
        "Prima di cancellare o ripristinare viene sempre fatto un backup automatico.");
    public static string Files => T("{0} files", "{0} file");
    public static string LastSaved => T("saved {0}", "salvato il {0}");

    public static string SectionName(string folder) => folder switch
    {
        "CareerData" => T("Career", "Carriera"),
        "replays" => T("Replays", "Replay"),
        "ghosts" => T("Ghosts", "Fantasmi"),
        "Arenas" => T("Arenas", "Arene"),
        "ConstructorChallenge" => T("Constructor challenges", "Sfide di costruzione"),
        "Horns" => T("Horns", "Clacson"),
        "Vehicles" => T("Vehicles", "Veicoli"),
        "Other" => T("Other", "Altro"),
        _ => folder,
    };

    public static string Yes => T("Yes", "Sì");
    public static string No => T("Cancel", "Annulla");
    public static string ConfirmDeleteSaves => T(
        "Delete all saves and start the career from scratch? A backup is made first.",
        "Cancellare tutti i salvataggi e ricominciare la carriera da zero? Prima viene fatto un backup.");
    public static string ConfirmResetAchievements => T(
        "Mark every achievement as locked again? A backup is made first.",
        "Riportare tutti gli obiettivi a bloccati? Prima viene fatto un backup.");
    public static string ConfirmRestore => T(
        "Replace the current saves and achievements with this backup from {0}? The current state is backed up first.",
        "Sostituire salvataggi e obiettivi attuali con questo backup del {0}? Prima viene fatto un backup dello stato attuale.");
    public static string ConfirmDeleteReplay => T("Delete the replay \"{0}\"?", "Eliminare il replay \"{0}\"?");
    public static string ConfirmDeleteBackup => T("Delete the backup from {0}?", "Eliminare il backup del {0}?");
    public static string BackupDone => T("Backup saved.", "Backup salvato.");
    public static string SavesDeleted => T("Saves deleted. The game starts a new career.",
        "Salvataggi cancellati. Il gioco inizierà una nuova carriera.");
    public static string AchievementsReset => T("Achievements reset.", "Obiettivi azzerati.");
    public static string Restored => T("Backup restored.", "Backup ripristinato.");
    public static string NothingToBackUp => T("Nothing to back up yet.", "Non c'è ancora niente da salvare.");

    public static string AchievementsUnlocked => T("{0} of {1} unlocked", "{0} di {1} sbloccati");
    public static string Locked => T("Locked", "Bloccato");
    public static string NoCatalog => T(
        "Start the game once to load the achievement list.",
        "Avvia il gioco una volta per caricare l'elenco degli obiettivi.");

    public static string On => T("On", "Attivo");
    public static string Off => T("Off", "Disattivo");

    public static string DisplayMode => T("Display mode", "Modalità schermo");
    public static string Fullscreen => T("Fullscreen", "Schermo intero");
    public static string Windowed => T("Windowed", "Finestra");
    public static string DisplayModeHint => T(
        "Fullscreen covers the whole monitor. Windowed opens a window of the chosen resolution.",
        "Schermo intero copre tutto il monitor. Finestra apre una finestra della risoluzione scelta.");

    public static string Resolution => T("Resolution", "Risoluzione");
    public static string Native => T("native", "nativa");
    public static string ResolutionHint => T(
        "The size the game renders at. Ultrawide and 16:10 sizes are listed when your monitor supports them.",
        "La risoluzione a cui il gioco disegna. Le risoluzioni ultrawide e 16:10 compaiono se il monitor le supporta.");

    public static string WideScreen => T("Non-16:9 screens", "Schermi non 16:9");
    public static string Bars => T("16:9 with black bars", "16:9 con bande nere");
    public static string Stretch => T("Stretch to fill", "Stirato a tutto schermo");
    public static string WideScreenHint => T(
        "The game is made for 16:9. Black bars keep its proportions; stretching fills the screen but widens everything. " +
        "A true 21:9 view (wider field of view, no stretching) needs a game patch that is in the works.",
        "Il gioco è fatto per il 16:9. Le bande nere ne mantengono le proporzioni; stirato riempie lo schermo ma allarga tutto. " +
        "Il vero 21:9 (visuale più ampia, senza deformazioni) richiede una patch al gioco, in lavorazione.");
    public static string WideScreenOnly169 => T("Only for non-16:9 resolutions", "Solo per risoluzioni non 16:9");

    public static string Supersampling => T("Supersampling", "Supersampling");
    public static string SupersamplingHint => T(
        "Renders at 2x or 3x the resolution and scales down: smoother edges, much heavier on the GPU.",
        "Disegna a 2x o 3x la risoluzione e poi riduce: bordi più puliti, ma molto più pesante per la GPU.");

    public static string VSync => T("VSync", "VSync");
    public static string VSyncHint => T(
        "Syncs frames to the monitor to avoid tearing.",
        "Sincronizza i fotogrammi con il monitor per evitare lo strappo dell'immagine.");

    public static string TextureFiltering => T("Texture filtering", "Filtro texture");
    public static string GameDefault => T("Game default", "Predefinito del gioco");
    public static string TextureFilteringHint => T(
        "Anisotropic filtering keeps roads and ground sharp into the distance.",
        "Il filtro anisotropico mantiene nitide strade e terreno in lontananza.");

    public static string Upscaler => T("Output filter", "Filtro di uscita");
    public static string Bilinear => T("Bilinear (original)", "Bilineare (originale)");
    public static string UpscalerHint => T(
        "How the image is scaled to the screen. CAS sharpens it, FSR upscales it with AMD FidelityFX.",
        "Come l'immagine viene scalata sullo schermo. CAS la rende più nitida, FSR la ingrandisce con AMD FidelityFX.");

    public static string FrameRate => T("Frame rate", "Frame rate");
    public static string FrameRate30 => T("30 FPS (original)", "30 FPS (originale)");
    public static string FrameRate60 => T("60 FPS (experimental)", "60 FPS (sperimentale)");
    public static string FrameRateHint => T(
        "60 FPS is experimental: if the game runs too fast or physics feel wrong, go back to 30.",
        "I 60 FPS sono sperimentali: se il gioco va troppo veloce o la fisica sembra strana, torna a 30.");

    public static string Language => T("Game language", "Lingua del gioco");
    public static string LanguageHint => T(
        "Menus, subtitles and speech. The game reads it at startup.",
        "Menu, sottotitoli e parlato. Il gioco la legge all'avvio.");

    public static string Log => T("Log file", "File di log");
    public static string LogHint => T(
        "Writes stuntman.log next to the game. Useful when reporting a problem.",
        "Scrive stuntman.log accanto al gioco. Utile per segnalare un problema.");

    public static string Saved => T("Settings saved.", "Impostazioni salvate.");
    public static string DefaultsRestored => T("Defaults restored. Save or press Play to keep them.",
        "Predefiniti ripristinati. Salva o premi Gioca per mantenerli.");
    public static string ExeMissing => T("stuntman.exe not found: put the launcher next to it.",
        "stuntman.exe non trovato: metti il launcher nella stessa cartella.");
    public static string DataMissing => T("Game files not found: the game folder (game\\default.xex) is missing.",
        "File del gioco non trovati: manca la cartella game (game\\default.xex).");
    public static string Crashed => T("The game closed with error {0}. Turn on the log file in Advanced to see why.",
        "Il gioco si è chiuso con errore {0}. Attiva il file di log in Avanzate per capire perché.");
    public static string StartFailed => T("Could not start the game: {0}", "Impossibile avviare il gioco: {0}");
}
