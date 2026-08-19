using System.Runtime.InteropServices;
using Microsoft.UI;
using Microsoft.UI.Windowing;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Media.Imaging;
using Windows.Graphics;
using WinRT.Interop;

namespace FidelityReference;

public sealed partial class MainWindow : Window
{
    // Matches themed_component_harness.cpp's kFluentLogicalWidth/Height —
    // both sides must render the exact same fixed canvas for
    // compare_windows_cpp.py's pixel diff to be meaningful with no
    // size-reconciliation step (see that script's doc comment).
    private const double LogicalWidth = 480.0;
    private const double LogicalHeight = 360.0;

    // {builder}_{state} case ids to export this pass — mirrors
    // themed_component_harness.cpp's builderStates() map exactly (same
    // content/labels). Extend both sides together as coverage grows, same
    // incremental-per-builder discipline android_fidelity_reference's
    // ComponentCatalog.kt comment describes.
    private static readonly string[] Builders =
    {
        "button", "switch", "slider", "textField", "card", "listTile", "divider",
        "appBar", "navigationBar", "dialog", "popupMenuButton", "dropdownButton",
        "primaryActionButton", "tabBar", "chip", "segmentedButton", "bottomSheet",
        "badge", "iconButton", "stepper", "ratingIndicator", "actionSheet", "searchField",
        "datePicker", "timePicker", "expansionTile", "toggleButtons", "banner",
        "navigationRail", "dataTable",
    };
    private static readonly Dictionary<string, string[]> States = new()
    {
        ["button"] = new[] { "primary", "secondary", "tertiary", "danger", "disabled" },
        ["switch"] = new[] { "on", "off", "disabled" },
        ["slider"] = new[] { "value", "disabled" },
        ["textField"] = new[] { "empty", "filled", "disabled" },
        ["card"] = new[] { "elevated", "filled", "outlined" },
        // "with_icon" intentionally omitted — see ComponentCatalog.Render()'s
        // matching comment.
        ["listTile"] = new[] { "one_line", "two_line" },
        ["divider"] = new[] { "default", "indented" },
        ["appBar"] = new[] { "default", "center_title" },
        ["navigationBar"] = new[] { "three_items" },
        ["dialog"] = new[] { "one_action", "two_actions", "three_actions" },
        ["popupMenuButton"] = new[] { "closed", "open" },
        ["dropdownButton"] = new[] { "closed", "open" },
        ["primaryActionButton"] = new[] { "icon", "label" },
        ["tabBar"] = new[] { "two_tabs" },
        ["chip"] = new[] { "unselected", "selected" },
        ["segmentedButton"] = new[] { "three_segments" },
        ["bottomSheet"] = new[] { "partial" },
        ["badge"] = new[] { "dot", "number" },
        ["iconButton"] = new[] { "plain", "filled", "selected" },
        ["stepper"] = new[] { "default", "disabled" },
        ["ratingIndicator"] = new[] { "three_of_five" },
        ["actionSheet"] = new[] { "open" },
        ["searchField"] = new[] { "empty", "filled" },
        ["datePicker"] = new[] { "compact" },
        ["timePicker"] = new[] { "compact" },
        ["expansionTile"] = new[] { "collapsed", "expanded" },
        ["toggleButtons"] = new[] { "multi" },
        ["banner"] = new[] { "default" },
        ["navigationRail"] = new[] { "compact", "extended" },
        ["dataTable"] = new[] { "default" },
    };

    private readonly string[] _themes;

    [DllImport("user32.dll")]
    private static extern uint GetDpiForWindow(IntPtr hwnd);

    public MainWindow(string[] themes)
    {
        InitializeComponent();
        _themes = themes;

        Activated += OnActivatedOnce;
    }

    private async void OnActivatedOnce(object sender, WindowActivatedEventArgs args)
    {
        // Only run the export loop once — Activated can fire more than once
        // (e.g. focus changes) and this must happen exactly once per process.
        Activated -= OnActivatedOnce;

        ResizeToLogicalSize();

        await RunExportLoopAsync();

        Application.Current.Exit();
        Environment.Exit(0);
    }

    private void ResizeToLogicalSize()
    {
        var hwnd = WindowNative.GetWindowHandle(this);
        var dpi = GetDpiForWindow(hwnd);
        var scale = dpi / 96.0;

        var windowId = Win32Interop.GetWindowIdFromWindow(hwnd);
        var appWindow = AppWindow.GetFromWindowId(windowId);
        appWindow.ResizeClient(new SizeInt32(
            (int)Math.Round(LogicalWidth * scale),
            (int)Math.Round(LogicalHeight * scale)));
    }

    private async Task RunExportLoopAsync()
    {
        var outputRoot = Path.Combine(AppContext.BaseDirectory, "fidelity_output");

        foreach (var theme in _themes)
        {
            // Application.Current.RequestedTheme (which ComponentCatalog.cs's
            // direct Application.Current.Resources[key] lookups depend on) is
            // fixed once per process in App.xaml.cs's constructor — see its
            // comment. RootBorder.RequestedTheme still needs setting here too
            // for XAML-declared ThemeResource bindings/built-in control
            // styles, even though in practice it now always agrees with the
            // app-level theme (each process only ever handles one theme).
            RootBorder.RequestedTheme = theme.EndsWith("_dark", StringComparison.Ordinal)
                ? ElementTheme.Dark
                : ElementTheme.Light;

            var themeDir = Path.Combine(outputRoot, theme);
            Directory.CreateDirectory(themeDir);

            foreach (var builder in Builders)
            {
                if (!States.TryGetValue(builder, out var states)) continue;

                foreach (var state in states)
                {
                    var caseId = $"{builder}_{state}";
                    var content = ComponentCatalog.Render(caseId);
                    if (content == null)
                    {
                        Console.WriteLine($"Unknown case: {caseId}");
                        continue;
                    }

                    CenterGrid.Children.Clear();
                    CenterGrid.Children.Add(content);

                    // Layout/composition needs at least one full frame to
                    // settle after swapping content before a
                    // RenderTargetBitmap capture reflects it — a fixed
                    // delay here matches this codebase's established
                    // pattern for UI-settling waits (e.g.
                    // android_fidelity_reference/export_references.sh's
                    // splash-screen-clearing sleep).
                    RootBorder.UpdateLayout();
                    await Task.Delay(100);

                    var outPath = Path.Combine(themeDir, $"{caseId}.png");
                    await ScreenshotExporter.CaptureToPngAsync(RootBorder, outPath);
                    Console.WriteLine($"Captured {theme}/{caseId}.png");
                }
            }
        }
    }
}
