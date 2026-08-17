import UIKit

/// Renders each component case into a PNG and writes it to the app
/// Documents directory under theme subdirectories.
enum ScreenshotExporter {
    /// Logical reference size — the iPhone 16 Pro / 17 Pro's actual size
    /// (1206x2622 physical / 3x, confirmed via a real simctl screenshot),
    /// identical across both simulators (Pro-size devices share the same
    /// point dimensions across generations). Not 393x852 — that's the
    /// *base* iPhone 16's size, the wrong constant for the Pro models this
    /// exports actually run on.
    static let referenceSize = CGSize(width: 402, height: 874)

    /// - Parameter themes: Only cases in these themes are exported — each
    ///   simulator run owns a disjoint pair (classic themes render on a
    ///   pre-Liquid-Glass OS, glass themes on iOS 26+; see AppDelegate's
    ///   `--themes=` launch argument). Defaults to every theme for a plain
    ///   manual run with no launch argument.
    static func exportAll(themes: [ComponentCase.Theme] = ComponentCase.Theme.allCases) {
        let outputRoot = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask).first!
            .appendingPathComponent("fidelity_output")

        try? FileManager.default.createDirectory(at: outputRoot, withIntermediateDirectories: true)

        for theme in themes {
            let themeDir = outputRoot.appendingPathComponent(theme.rawValue)
            try? FileManager.default.createDirectory(at: themeDir, withIntermediateDirectories: true)
        }

        let cases = ComponentCase.allCases().filter { themes.contains($0.theme) }
        print("Exporting \(cases.count) reference screenshots for themes: \(themes.map(\.rawValue).joined(separator: ", "))...")

        for componentCase in cases {
            let vc = ReferenceViewControllers.viewController(for: componentCase)
            let image = render(vc.view, theme: componentCase.theme)
            let url = outputRoot
                .appendingPathComponent(componentCase.theme.rawValue)
                .appendingPathComponent(componentCase.fileName + ".png")
            if let data = image.pngData() {
                try? data.write(to: url)
            }
        }

        print("Done. Output: \(outputRoot.path)")
    }

    private static func render(_ view: UIView, theme: ComponentCase.Theme) -> UIImage {
        let isDark = (theme == .cupertinoDark || theme == .liquidGlassDark)
        view.backgroundColor = isDark ? .systemBackground : .systemBackground
        view.overrideUserInterfaceStyle = isDark ? .dark : .light

        let size = referenceSize
        view.frame = CGRect(origin: .zero, size: size)

        // UIVisualEffectView-backed blur/vibrancy (used throughout the
        // Liquid Glass reference cases, and by system chrome like
        // UIAlertController's own background material) does not composite
        // correctly when snapshotted via drawHierarchy on a view that was
        // never part of a real UIWindow — a well-known UIKit limitation.
        // Host the view in an actual (offscreen, but real) window before
        // laying out and snapshotting.
        let window = UIWindow(frame: CGRect(origin: .zero, size: size))
        window.backgroundColor = .clear
        window.addSubview(view)
        window.isHidden = false
        window.makeKeyAndVisible()

        view.setNeedsLayout()
        view.layoutIfNeeded()

        let format = UIGraphicsImageRendererFormat()
        format.scale = 3.0
        format.opaque = false
        let renderer = UIGraphicsImageRenderer(size: size, format: format)
        let image = renderer.image { ctx in
            view.drawHierarchy(in: CGRect(origin: .zero, size: size), afterScreenUpdates: true)
        }

        view.removeFromSuperview()
        window.isHidden = true
        return image
    }
}
