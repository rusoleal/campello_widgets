import UIKit

@main
class AppDelegate: UIResponder, UIApplicationDelegate {
    var window: UIWindow?

    func application(
        _ application: UIApplication,
        didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey: Any]?
    ) -> Bool {
        window = UIWindow(frame: UIScreen.main.bounds)
        window?.rootViewController = UIViewController()
        window?.makeKeyAndVisible()

        if let presentCase = AppDelegate.requestedPresentCase() {
            // Real-capture mode: build and present/add the actual system
            // control, then idle — no exportAll(), no exit(0). The host
            // script (export_references.sh) screenshots this process via
            // `simctl io screenshot` from the outside, then terminates it.
            RealCapture.present(presentCase, in: window!.rootViewController!, window: window!)
            return true
        }

        DispatchQueue.main.async {
            ScreenshotExporter.exportAll(themes: AppDelegate.requestedThemes())
            // Exit cleanly once exports are done.
            exit(0)
        }

        return true
    }

    /// Reads `--present-case=<theme>:<builder>_<state>` (e.g.
    /// `--present-case=cupertino_light:dialog_three_actions`) and resolves
    /// it to the matching `ComponentCase`. Returns nil for a normal batch
    /// export run with no such argument.
    private static func requestedPresentCase() -> ComponentCase? {
        for arg in ProcessInfo.processInfo.arguments {
            guard arg.hasPrefix("--present-case=") else { continue }
            let value = String(arg.dropFirst("--present-case=".count))
            let parts = value.split(separator: ":", maxSplits: 1)
            guard parts.count == 2, let theme = ComponentCase.Theme(rawValue: String(parts[0])) else {
                return nil
            }
            let fileName = String(parts[1])
            return ComponentCase.allCases().first { $0.theme == theme && $0.fileName == fileName }
        }
        return nil
    }

    /// Reads `--themes=cupertino_light,cupertino_dark` from the launch
    /// arguments (set by export_references.sh, one simulator run per OS
    /// version owning a disjoint theme pair) and returns the matching
    /// `ComponentCase.Theme`s. Falls back to every theme for a plain manual
    /// run with no launch argument.
    private static func requestedThemes() -> [ComponentCase.Theme] {
        for arg in ProcessInfo.processInfo.arguments {
            guard arg.hasPrefix("--themes=") else { continue }
            let rawValues = arg.dropFirst("--themes=".count).split(separator: ",")
            let themes = rawValues.compactMap { ComponentCase.Theme(rawValue: String($0)) }
            if !themes.isEmpty { return themes }
        }
        return ComponentCase.Theme.allCases
    }
}
