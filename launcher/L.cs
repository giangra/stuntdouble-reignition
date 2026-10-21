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
    public static string InGameMenu => T(
        "Video, controls and achievements are in the game: on its start screen, or press F1 (Back + Start on a controller) while playing.",
        "Video, comandi e obiettivi sono nel gioco: nella schermata iniziale, oppure premi F1 (Back + Start sul controller) mentre giochi.");

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

    // ---- Installer ----
    public static string Install => T("INSTALL", "INSTALLA");
    public static string Installing => T("INSTALLING…", "INSTALLAZIONE…");
    public static string InstallTitle => T("INSTALLATION", "INSTALLAZIONE");
    public static string InstallIntro => T(
        "The game is built on your PC from your own copy of the disc. Nothing of the game is downloaded.",
        "Il gioco viene compilato sul tuo PC a partire dalla tua copia del disco. Del gioco non viene scaricato niente.");
    public static string IsoLabel => T("Disc image (ISO)", "Immagine del disco (ISO)");
    public static string IsoPick => T("Choose the Stuntman: Ignition ISO…", "Scegli la ISO di Stuntman: Ignition…");
    public static string FolderLabel => T("Install folder", "Cartella di installazione");
    public static string Browse => T("Browse…", "Sfoglia…");
    public static string KeepBuild => T("Keep build files", "Mantieni file di compilazione");
    public static string InstallNeeds => T(
        "Keeping the build files takes about 3 GB more and makes updates faster. Needs about 12 GB free. If missing, the free Microsoft Visual Studio Build Tools (about 6 GB, asks for administrator rights) and a portable Git are installed. The first install takes 30–60 minutes.",
        "Mantenere i file di compilazione occupa circa 3 GB in più e rende gli aggiornamenti più rapidi. Servono circa 12 GB liberi. Se mancano, vengono installati i Visual Studio Build Tools di Microsoft, gratuiti (circa 6 GB, chiedono i permessi di amministratore), e una copia portatile di Git. La prima installazione richiede 30–60 minuti.");
    public static string IsoOk => T("Stuntman: Ignition disc found.", "Disco di Stuntman: Ignition riconosciuto.");
    public static string IsoWrong => T("This is not a Stuntman: Ignition Xbox 360 disc image ({0}).",
        "Questa non è un'immagine del disco Xbox 360 di Stuntman: Ignition ({0}).");
    public static string NoSpace => T("Not enough space on {0}: about {1} GB are needed.",
        "Spazio insufficiente su {0}: servono circa {1} GB.");
    public static string StepCheckIso => T("Check the disc image", "Verifica dell'immagine del disco");
    public static string StepTools => T("Microsoft build tools", "Strumenti di compilazione Microsoft");
    public static string StepGit => T("Git", "Git");
    public static string StepSource => T("Download the port's source", "Download dei sorgenti del port");
    public static string StepExtract => T("Extract the disc", "Estrazione del disco");
    public static string StepBuild => T("Generate and compile the game", "Generazione e compilazione del gioco");
    public static string StepFinish => T("Shortcut and cleanup", "Collegamento e pulizia");
    public static string DownloadingBuildTools => T("Downloading the Build Tools installer…", "Download del programma di installazione dei Build Tools…");
    public static string InstallingBuildTools => T("Installing the Build Tools (see the Visual Studio Installer window)…",
        "Installazione dei Build Tools (vedi la finestra di Visual Studio Installer)…");
    public static string BuildToolsFailed => T("The Visual Studio Build Tools could not be installed.",
        "Non è stato possibile installare i Visual Studio Build Tools.");
    public static string BuildPreparing => T("Preparing…", "Preparazione…");
    public static string BuildSdk => T("Downloading and building the ReXGlue SDK…", "Download e compilazione dell'SDK ReXGlue…");
    public static string BuildCodegen => T("Translating the game's code to C++…", "Traduzione del codice del gioco in C++…");
    public static string BuildCompile => T("Compiling…", "Compilazione…");
    public static string BuildCopy => T("Copying the game…", "Copia del gioco…");
    public static string BuildFailed => T("The build did not produce the game.", "La compilazione non ha prodotto il gioco.");
    public static string StepFailed => T("{0} failed (code {1}).", "{0} non riuscito (codice {1}).");
    public static string InstallDone => T("Installed. A \"Stuntman Ignition\" shortcut is on your desktop.",
        "Installato. Sul desktop trovi il collegamento \"Stuntman Ignition\".");
    public static string InstallFailed => T("Installation failed: {0} Press Install to retry; finished steps are kept. Log: {1}",
        "Installazione non riuscita: {0} Premi Installa per riprovare: i passi già fatti vengono mantenuti. Log: {1}");
    public static string ShowLog => T("Show log", "Mostra log");
    public static string ConfirmCancelInstall => T("Stop the installation? You can resume it later.",
        "Interrompere l'installazione? Potrai riprenderla più tardi.");

    public static string Uninstall => T("Uninstall", "Disinstalla");
    public static string ConfirmUninstall => T(
        "Remove the game from {0} and its shortcuts? Saves and achievements in Documents\\stuntman are kept.",
        "Rimuovere il gioco da {0} e i suoi collegamenti? Salvataggi e obiettivi in Documenti\\stuntman restano.");

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

    public static string ExeMissing => T("stuntman.exe not found: put the launcher next to it.",
        "stuntman.exe non trovato: metti il launcher nella stessa cartella.");
    public static string DataMissing => T("Game files not found: the game folder (game\\default.xex) is missing.",
        "File del gioco non trovati: manca la cartella game (game\\default.xex).");
    public static string Crashed => T("The game closed with error {0}. Turn on the log file in Advanced to see why.",
        "Il gioco si è chiuso con errore {0}. Attiva il file di log in Avanzate per capire perché.");
    public static string StartFailed => T("Could not start the game: {0}", "Impossibile avviare il gioco: {0}");
}
