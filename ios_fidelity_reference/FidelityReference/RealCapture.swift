import UIKit

/// Builds and presents/adds a *real*, live system control for the four
/// builders whose reference mockups exist only because their genuine
/// translucent/glass chrome doesn't initialize when `.view` is extracted
/// and snapshotted standalone (UIAlertController's presentation-controller
/// dependency; UITabBar/UISearchTextField's SDK defaults with no public
/// override API) — see ReferenceViewControllers.swift's makeDialog/
/// makeActionSheet/makeSearchField/makeNavigationBar doc comments for the
/// full rationale. export_references.sh launches one process per case in
/// this mode, screenshots it externally via `simctl io screenshot`, then
/// terminates it — this file never calls exit(0) itself.
enum RealCapture {
    static func present(_ componentCase: ComponentCase, in root: UIViewController, window: UIWindow) {
        let isDark = (componentCase.theme == .cupertinoDark || componentCase.theme == .liquidGlassDark)
        window.overrideUserInterfaceStyle = isDark ? .dark : .light

        // navigationRail replaces the window's root controller outright —
        // UISplitViewController only activates its real column-layout
        // machinery when it's driving the app's own navigation structure,
        // not when embedded as a child of an existing root (confirmed
        // empirically: embedding it as a child rendered nothing at all).
        // safe_area.txt still gets written below using this window, same
        // as every other case.
        if componentCase.builder == .navigationRail {
            addNavigationRail(state: componentCase.state, window: window)
            writeSafeArea(window: window)
            return
        }

        // Same shared backdrop every other reference case uses, full-screen,
        // so a real system control's screenshot is visually comparable.
        let bg = UIImageView(frame: root.view.bounds)
        bg.autoresizingMask = [.flexibleWidth, .flexibleHeight]
        bg.contentMode = .scaleToFill
        if let path = Bundle.main.path(forResource: "liquid_glass_background", ofType: "png"),
           let image = UIImage(contentsOfFile: path) {
            bg.image = image
        }
        root.view.addSubview(bg)

        switch componentCase.builder {
        case .dialog:
            presentDialog(state: componentCase.state, from: root)
        case .actionSheet:
            presentActionSheet(state: componentCase.state, from: root)
        case .searchField:
            addSearchField(state: componentCase.state, to: root)
        case .navigationBar:
            addTabBar(state: componentCase.state, to: root)
        default:
            break
        }

        writeSafeArea(window: window)
    }

    // export_references.sh crops the raw full-screen `simctl io
    // screenshot` down to just the content area. Written to a file (not
    // hardcoded in the host script) so the crop is exact for whichever
    // simulator/device actually ran this, instead of a guessed margin
    // that silently drifts if the device changes. A real launch via
    // `simctl launch` (non-blocking, unlike `--console-pty`) doesn't give
    // the host script a live stdout stream, so a file — the same
    // mechanism ScreenshotExporter already uses for its own output — is
    // the simplest handoff.
    private static func writeSafeArea(window: UIWindow) {
        let insets = window.safeAreaInsets
        let scale = window.screen.scale
        let text = "top=\(Int(insets.top * scale)) bottom=\(Int(insets.bottom * scale))"
        let docs = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask).first!
        try? text.write(to: docs.appendingPathComponent("safe_area.txt"), atomically: true, encoding: .utf8)
    }

    private static func presentDialog(state: String, from root: UIViewController) {
        let alert = UIAlertController(title: "Title", message: "Message", preferredStyle: .alert)
        switch state {
        case "one_action":
            alert.addAction(UIAlertAction(title: "OK", style: .default))
        case "two_actions":
            alert.addAction(UIAlertAction(title: "Cancel", style: .cancel))
            alert.addAction(UIAlertAction(title: "OK", style: .default))
        case "three_actions":
            alert.addAction(UIAlertAction(title: "Cancel", style: .cancel))
            alert.addAction(UIAlertAction(title: "OK", style: .default))
            alert.addAction(UIAlertAction(title: "Delete", style: .destructive))
        default:
            break
        }
        root.present(alert, animated: false)
    }

    private static func presentActionSheet(state: String, from root: UIViewController) {
        let sheet = UIAlertController(title: "Title", message: nil, preferredStyle: .actionSheet)
        sheet.addAction(UIAlertAction(title: "Save", style: .default))
        sheet.addAction(UIAlertAction(title: "Delete", style: .destructive))
        sheet.addAction(UIAlertAction(title: "Cancel", style: .cancel))
        // Required on iPad (action sheets present as a popover there).
        // Previously set unconditionally on the assumption it's a no-op on
        // iPhone — confirmed wrong on iOS 26 (Liquid Glass): configuring
        // popoverPresentationController there makes UIAlertController(
        // .actionSheet) present as an actual floating popover with an
        // arrow pointer instead of the classic full-width bottom sheet,
        // even on a compact-width iPhone. Gating on the real iPad idiom
        // avoids that on iPhone.
        if UIDevice.current.userInterfaceIdiom == .pad, let popover = sheet.popoverPresentationController {
            popover.sourceView = root.view
            popover.sourceRect = CGRect(x: root.view.bounds.midX, y: root.view.bounds.maxY, width: 0, height: 0)
        }
        root.present(sheet, animated: false)
    }

    private static func addSearchField(state: String, to root: UIViewController) {
        let field = UISearchTextField()
        field.placeholder = "Search"
        if state == "filled" { field.text = "query" }
        field.translatesAutoresizingMaskIntoConstraints = false
        root.view.addSubview(field)
        NSLayoutConstraint.activate([
            field.centerXAnchor.constraint(equalTo: root.view.safeAreaLayoutGuide.centerXAnchor),
            field.topAnchor.constraint(equalTo: root.view.safeAreaLayoutGuide.topAnchor, constant: 8),
            field.widthAnchor.constraint(equalToConstant: 280),
        ])
    }

    private static func addTabBar(state: String, to root: UIViewController) {
        let tabBar = UITabBar()
        tabBar.items = [
            UITabBarItem(title: "First", image: UIImage(systemName: "house"), tag: 0),
            UITabBarItem(title: "Second", image: UIImage(systemName: "magnifyingglass"), tag: 1),
            UITabBarItem(title: "Third", image: UIImage(systemName: "person"), tag: 2),
        ]
        tabBar.selectedItem = tabBar.items?.first
        tabBar.translatesAutoresizingMaskIntoConstraints = false
        root.view.addSubview(tabBar)
        // safeAreaLayoutGuide, not root.view.bottomAnchor directly — the
        // latter pins the bar into the home-indicator strip that
        // export_references.sh's crop trims off (using the same
        // safe_area.txt insets this file itself writes below), clipping
        // the bar's bottom edge out of the exported PNG. addSearchField()
        // above already anchors to the safe area for the same reason.
        NSLayoutConstraint.activate([
            tabBar.leadingAnchor.constraint(equalTo: root.view.leadingAnchor),
            tabBar.trailingAnchor.constraint(equalTo: root.view.trailingAnchor),
            tabBar.bottomAnchor.constraint(equalTo: root.view.safeAreaLayoutGuide.bottomAnchor),
        ])
    }

    // NavigationRail's real iPadOS equivalent: tried UITabBarController's
    // `.tabSidebar` mode (iOS 18+) first, since it's the newer/more
    // "official"-sounding API — but empirically it doesn't render a
    // left-edge rail at all in a minimal embedding. A narrow container
    // collapses it to a floating minimized pill; full-screen renders iOS
    // 26's new floating top pill tab bar. Neither is a sidebar.
    //
    // The actual real pattern used by Apple's own sidebar apps (Mail,
    // Notes, Files) is a UISplitViewController with a `.sidebar`-style
    // UICollectionView as its primary column — that's what this builds.
    // Still iPad-only (a compact-width split view just shows one column
    // at a time, no rail), hence the separate iPad export pass.
    private static func addNavigationRail(state: String, window: UIWindow) {
        let listConfig = UICollectionLayoutListConfiguration(appearance: .sidebar)
        let layout = UICollectionViewCompositionalLayout.list(using: listConfig)
        let sidebarVC = UICollectionViewController(collectionViewLayout: layout)
        sidebarVC.collectionView.backgroundColor = .clear

        let dataSource = NavigationRailDataSource()
        sidebarVC.collectionView.dataSource = dataSource
        sidebarVC.collectionView.register(UICollectionViewListCell.self, forCellWithReuseIdentifier: "cell")
        objc_setAssociatedObject(sidebarVC, &navigationRailDataSourceKey, dataSource, .OBJC_ASSOCIATION_RETAIN)
        sidebarVC.collectionView.reloadData()
        sidebarVC.collectionView.selectItem(at: IndexPath(item: 0, section: 0), animated: false, scrollPosition: [])

        // Detail column carries the shared backdrop — visually comparable
        // to every other reference case even though this builder cares
        // only about the sidebar/rail portion of the screenshot.
        let detailVC = UIViewController()
        let bg = UIImageView(frame: UIScreen.main.bounds)
        bg.autoresizingMask = [.flexibleWidth, .flexibleHeight]
        bg.contentMode = .scaleToFill
        if let path = Bundle.main.path(forResource: "liquid_glass_background", ofType: "png"),
           let image = UIImage(contentsOfFile: path) {
            bg.image = image
        }
        detailVC.view.addSubview(bg)

        let split = UISplitViewController(style: .doubleColumn)
        split.viewControllers = [sidebarVC, detailVC]
        split.preferredDisplayMode = .oneBesideSecondary
        split.preferredSplitBehavior = .tile
        // "compact" tests whether a narrow primary-column width alone is
        // enough to trigger UICollectionLayoutListConfiguration(appearance:
        // .sidebar)'s documented adaptive icon-only compaction; "extended"
        // gives it ample width for the icon+label presentation.
        let width: CGFloat = state == "compact" ? 100 : 260
        split.minimumPrimaryColumnWidth = width
        split.maximumPrimaryColumnWidth = width
        split.preferredPrimaryColumnWidth = width

        // Must be the window's actual root controller — embedding it as a
        // child of an existing root rendered nothing at all (confirmed
        // empirically), only replacing the root activates its real
        // column-layout machinery.
        window.rootViewController = split
    }
}

nonisolated(unsafe) private var navigationRailDataSourceKey: UInt8 = 0

// A plain NSObject UICollectionViewDataSource, not the generic diffable-
// data-source + CellRegistration APIs — those generic closures don't
// infer @MainActor isolation cleanly under this project's Swift 6 strict
// concurrency checking (SWIFT_VERSION = 6.0), while ordinary
// UICollectionViewDataSource protocol methods are isolated automatically
// since UIKit declares the protocol itself @MainActor.
private final class NavigationRailDataSource: NSObject, UICollectionViewDataSource {
    let items: [(icon: String, title: String)] = [
        ("house", "Home"), ("magnifyingglass", "Search"), ("person", "Profile"),
    ]

    func collectionView(_ collectionView: UICollectionView, numberOfItemsInSection section: Int) -> Int {
        items.count
    }

    func collectionView(_ collectionView: UICollectionView, cellForItemAt indexPath: IndexPath) -> UICollectionViewCell {
        let cell = collectionView.dequeueReusableCell(withReuseIdentifier: "cell", for: indexPath)
        guard let listCell = cell as? UICollectionViewListCell else { return cell }
        let item = items[indexPath.item]
        var config = listCell.defaultContentConfiguration()
        config.text = item.title
        config.image = UIImage(systemName: item.icon)
        listCell.contentConfiguration = config
        return listCell
    }
}
