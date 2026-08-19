using Microsoft.UI.Xaml;

namespace FidelityReference;

/// <summary>
/// App entry point. Parses `--themes=fluent_light,fluent_dark` from the
/// command line (mirrors ios_fidelity_reference's `--themes=` /
/// `--present-case=` launch-argument convention), drives MainWindow's
/// batch export loop, then exits the process — matching
/// RealCapture.swift's/ComponentCatalog.kt's "one process run, then
/// exit(0)" pattern export_references.ps1 waits on.
/// </summary>
public partial class App : Application
{
    private Window? _window;

    public App()
    {
        // Application.RequestedTheme can only be set once, before any
        // window/content tree exists — setting it later (e.g. per-theme
        // inside the export loop) crashes the app (STATUS_FATAL_APP_EXIT).
        // ComponentCatalog.cs resolves ~23 colors via
        // Application.Current.Resources[key] directly in C#, which depends
        // on this property, not on MainWindow.RootBorder's per-subtree
        // ElementTheme override — so a single process run can only
        // correctly export ONE app-level theme. Fixed by requiring the
        // caller pass a single theme via --themes= (export_references.ps1
        // now runs the exe twice, once per theme) and setting the theme
        // here based on it, before InitializeComponent().
        var themes = ParseThemesArg();
        RequestedTheme = themes.Any(t => t.EndsWith("_dark", StringComparison.Ordinal))
            ? ApplicationTheme.Dark
            : ApplicationTheme.Light;

        InitializeComponent();
    }

    protected override void OnLaunched(LaunchActivatedEventArgs args)
    {
        var themes = ParseThemesArg();

        _window = new MainWindow(themes);
        _window.Activate();
    }

    private static string[] ParseThemesArg()
    {
        var raw = Environment.GetCommandLineArgs();
        foreach (var arg in raw)
        {
            if (arg.StartsWith("--themes=", StringComparison.Ordinal))
            {
                return arg["--themes=".Length..].Split(',', StringSplitOptions.RemoveEmptyEntries);
            }
        }
        return new[] { "fluent_light", "fluent_dark" };
    }
}
