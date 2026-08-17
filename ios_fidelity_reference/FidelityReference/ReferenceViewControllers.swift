import UIKit

/// Produces a deterministic UIViewController for each fidelity case.
enum ReferenceViewControllers {
    static func viewController(for componentCase: ComponentCase) -> UIViewController {
        let vc = UIViewController()

        let isDark = (componentCase.theme == .cupertinoDark || componentCase.theme == .liquidGlassDark)
        vc.view.overrideUserInterfaceStyle = isDark ? .dark : .light

        // Shared non-flat background for all themes so blur/translucency and
        // backdrop effects are visible and every comparison uses the same
        // visually interesting backdrop.
        if let bgView = makeLiquidGlassBackgroundView() {
            bgView.translatesAutoresizingMaskIntoConstraints = false
            vc.view.addSubview(bgView)
            NSLayoutConstraint.activate([
                bgView.topAnchor.constraint(equalTo: vc.view.topAnchor),
                bgView.leadingAnchor.constraint(equalTo: vc.view.leadingAnchor),
                bgView.trailingAnchor.constraint(equalTo: vc.view.trailingAnchor),
                bgView.bottomAnchor.constraint(equalTo: vc.view.bottomAnchor),
            ])
        }

        let container = UIView()
        container.translatesAutoresizingMaskIntoConstraints = false
        vc.view.addSubview(container)
        NSLayoutConstraint.activate([
            container.centerXAnchor.constraint(equalTo: vc.view.centerXAnchor),
            container.centerYAnchor.constraint(equalTo: vc.view.centerYAnchor),
            container.widthAnchor.constraint(equalToConstant: 360),
        ])

        let content = contentView(for: componentCase)
        let bordered = wrapWithRedBorder(content)
        bordered.translatesAutoresizingMaskIntoConstraints = false
        container.addSubview(bordered)

        if usesFullWidthLayout(componentCase.builder) {
            NSLayoutConstraint.activate([
                bordered.topAnchor.constraint(equalTo: container.topAnchor, constant: 16),
                bordered.leadingAnchor.constraint(equalTo: container.leadingAnchor, constant: 16),
                bordered.trailingAnchor.constraint(equalTo: container.trailingAnchor, constant: -16),
                bordered.bottomAnchor.constraint(equalTo: container.bottomAnchor, constant: -16),
            ])
        } else {
            // Intrinsically-sized components must NOT be pinned to both
            // edges here: doing so forces `content` (via wrapWithRedBorder's
            // own required leading/trailing pins) to exactly 328pt wide,
            // which directly conflicts with any content that declares its
            // own required width/size constraint narrower than that (e.g.
            // ratingIndicator's fixed-size star icons, navigationRail's
            // 72pt compact rail). Auto Layout silently breaks one side of
            // that conflict — visibly, as a stretched last star or an
            // oversized "compact" rail. Centering with a max-width cap
            // instead lets `bordered` hug content's real size.
            NSLayoutConstraint.activate([
                bordered.topAnchor.constraint(equalTo: container.topAnchor, constant: 16),
                bordered.bottomAnchor.constraint(equalTo: container.bottomAnchor, constant: -16),
                bordered.centerXAnchor.constraint(equalTo: container.centerXAnchor),
                bordered.widthAnchor.constraint(lessThanOrEqualTo: container.widthAnchor, constant: -32),
            ])
        }

        return vc
    }

    /// Mirrors the C++ harness's `usesFullWidthLayout` — these components
    /// are rendered edge-to-edge inside the 360 pt reference container in
    /// both references, so they skip the centering/max-width treatment
    /// every other (intrinsically-sized) component gets.
    private static func usesFullWidthLayout(_ builder: ComponentCase.Builder) -> Bool {
        switch builder {
        case .appBar, .navigationBar, .bottomSheet, .banner, .actionSheet:
            return true
        default:
            return false
        }
    }

    private static func makeLiquidGlassBackgroundView() -> UIImageView? {
        guard let path = Bundle.main.path(forResource: "liquid_glass_background", ofType: "png") else {
            return nil
        }
        guard let image = UIImage(contentsOfFile: path) else {
            return nil
        }
        let imageView = UIImageView(image: image)
        imageView.contentMode = .scaleToFill
        return imageView
    }

    /// Wraps the component under test in a 1 pt red border with 5 pt internal padding.
    private static func wrapWithRedBorder(_ content: UIView) -> UIView {
        let wrapper = UIView()
        wrapper.layer.borderColor = UIColor.red.cgColor
        wrapper.layer.borderWidth = 1.0
        wrapper.translatesAutoresizingMaskIntoConstraints = false

        content.translatesAutoresizingMaskIntoConstraints = false
        wrapper.addSubview(content)
        NSLayoutConstraint.activate([
            content.topAnchor.constraint(equalTo: wrapper.topAnchor, constant: 5),
            content.leadingAnchor.constraint(equalTo: wrapper.leadingAnchor, constant: 5),
            content.trailingAnchor.constraint(equalTo: wrapper.trailingAnchor, constant: -5),
            content.bottomAnchor.constraint(equalTo: wrapper.bottomAnchor, constant: -5),
        ])

        return wrapper
    }

    private static func contentView(for componentCase: ComponentCase) -> UIView {
        switch componentCase.builder {
        case .button:
            return makeButton(state: componentCase.state, theme: componentCase.theme)
        case .switchControl:
            return makeSwitch(state: componentCase.state)
        case .slider:
            return makeSlider(state: componentCase.state)
        case .textField:
            return makeTextField(state: componentCase.state)
        case .card:
            return makeCard(state: componentCase.state, theme: componentCase.theme)
        case .listTile:
            return makeListTile(state: componentCase.state)
        case .divider:
            return makeDivider(state: componentCase.state)
        case .appBar:
            return makeAppBar(state: componentCase.state)
        case .navigationBar:
            return makeNavigationBar(state: componentCase.state, theme: componentCase.theme)
        case .dialog:
            return makeDialog(state: componentCase.state, theme: componentCase.theme)
        case .popupMenuButton:
            return makePopupMenuButton(state: componentCase.state)
        case .dropdownButton:
            return makeDropdownButton(state: componentCase.state)
        case .primaryActionButton:
            return makePrimaryActionButton(state: componentCase.state, theme: componentCase.theme)
        case .tabBar:
            return makeTabBar(state: componentCase.state)
        case .chip:
            return makeChip(state: componentCase.state)
        case .segmentedButton:
            return makeSegmentedButton(state: componentCase.state, theme: componentCase.theme)
        case .bottomSheet:
            return makeBottomSheet(state: componentCase.state, theme: componentCase.theme)
        case .badge:
            return makeBadge(state: componentCase.state)
        case .iconButton:
            return makeIconButton(state: componentCase.state)
        case .stepper:
            return makeStepper(state: componentCase.state)
        case .ratingIndicator:
            return makeRatingIndicator(state: componentCase.state)
        case .actionSheet:
            return makeActionSheet(state: componentCase.state, theme: componentCase.theme)
        case .searchField:
            return makeSearchField(state: componentCase.state, theme: componentCase.theme)
        case .datePicker:
            return makeDatePicker(state: componentCase.state, theme: componentCase.theme)
        case .timePicker:
            return makeTimePicker(state: componentCase.state, theme: componentCase.theme)
        case .expansionTile:
            return makeExpansionTile(state: componentCase.state)
        case .toggleButtons:
            return makeToggleButtons(state: componentCase.state, theme: componentCase.theme)
        case .banner:
            return makeBanner(state: componentCase.state)
        case .navigationRail:
            return makeNavigationRail(state: componentCase.state)
        case .dataTable:
            return makeDataTable(state: componentCase.state)
        case .confirmationDialog:
            return makeConfirmationDialog(state: componentCase.state, theme: componentCase.theme)
        }
    }

    // MARK: - Common helpers

    private static func label(_ text: String) -> UILabel {
        let l = UILabel()
        l.text = text
        l.font = UIFont.preferredFont(forTextStyle: .body)
        l.textColor = .label
        l.numberOfLines = 0
        return l
    }

    private static func hstack(_ views: [UIView], spacing: CGFloat = 8) -> UIStackView {
        let s = UIStackView(arrangedSubviews: views)
        s.axis = .horizontal
        s.spacing = spacing
        s.alignment = .center
        s.distribution = .fill
        return s
    }

    private static func vstack(_ views: [UIView], spacing: CGFloat = 12, alignment: UIStackView.Alignment = .fill) -> UIStackView {
        let s = UIStackView(arrangedSubviews: views)
        s.axis = .vertical
        s.spacing = spacing
        s.alignment = alignment
        s.distribution = .fill
        return s
    }

    /// The real Liquid Glass material (iOS 26+, distinct from the older
    /// UIBlurEffect family) — falls back to a light blur material on
    /// earlier deployment targets, which this project's 16.0 minimum
    /// still needs to compile against even though every environment this
    /// runs in today is iOS 26.
    private static func glassEffect(isDark: Bool) -> UIVisualEffect {
        if #available(iOS 26.0, *) {
            return UIGlassEffect(style: .regular)
        }
        return UIBlurEffect(style: isDark ? .systemUltraThinMaterialDark : .systemUltraThinMaterialLight)
    }

    private static func hairline() -> UIView {
        let line = UIView()
        line.backgroundColor = .separator
        line.heightAnchor.constraint(equalToConstant: 0.5).isActive = true
        return line
    }

    private static func vhairline() -> UIView {
        let line = UIView()
        line.backgroundColor = .separator
        line.widthAnchor.constraint(equalToConstant: 0.5).isActive = true
        return line
    }

    /// A tappable-looking alert/action-sheet row: a centered label at a
    /// fixed 44pt height, matching real iOS alert row metrics.
    private static func alertActionRow(_ text: String, color: UIColor) -> UIView {
        let l = UILabel()
        l.text = text
        l.textColor = color
        l.font = UIFont.systemFont(ofSize: 17)
        l.textAlignment = .center
        l.translatesAutoresizingMaskIntoConstraints = false
        let wrapper = UIView()
        wrapper.heightAnchor.constraint(equalToConstant: 44).isActive = true
        wrapper.addSubview(l)
        NSLayoutConstraint.activate([
            l.centerXAnchor.constraint(equalTo: wrapper.centerXAnchor),
            l.centerYAnchor.constraint(equalTo: wrapper.centerYAnchor),
        ])
        return wrapper
    }

    private static func systemImage(_ name: String) -> UIImageView {
        let iv = UIImageView(image: UIImage(systemName: name))
        iv.tintColor = .label
        iv.contentMode = .scaleAspectFit
        iv.widthAnchor.constraint(equalToConstant: 24).isActive = true
        iv.heightAnchor.constraint(equalToConstant: 24).isActive = true
        return iv
    }

    /// Wraps `view` in a rounded translucent-material backdrop when `theme`
    /// is a Liquid Glass variant, returning `view` unchanged otherwise.
    /// UIKit has no per-instance "glass" configuration for controls like
    /// UISegmentedControl/UIDatePicker. UIButton.Configuration does have
    /// real .glass()/.prominentGlass() (iOS 26) — but confirmed live, those
    /// compile and apply without rendering their actual translucent
    /// material in this app's offscreen-snapshot pipeline (the same
    /// rendering-pipeline limitation as UIAlertController's own chrome —
    /// see ScreenshotExporter's real-UIWindow comment; apparently real
    /// Liquid Glass compositing needs more than window membership alone).
    /// This UIVisualEffectView-backdrop approximation is what actually
    /// renders, so it's used everywhere Liquid Glass needs to be visible,
    /// including makeButton.
    private static func wrapInGlassBackdropIfNeeded(_ view: UIView, theme: ComponentCase.Theme) -> UIView {
        let isGlass = (theme == .liquidGlassLight || theme == .liquidGlassDark)
        guard isGlass else { return view }

        let isDark = (theme == .liquidGlassDark)
        let container = UIView()
        let blur = UIVisualEffectView(effect: glassEffect(isDark: isDark))
        blur.layer.cornerRadius = 8
        blur.clipsToBounds = true
        blur.translatesAutoresizingMaskIntoConstraints = false
        container.addSubview(blur)

        view.backgroundColor = .clear
        view.translatesAutoresizingMaskIntoConstraints = false
        container.addSubview(view)

        NSLayoutConstraint.activate([
            blur.topAnchor.constraint(equalTo: container.topAnchor),
            blur.leadingAnchor.constraint(equalTo: container.leadingAnchor),
            blur.trailingAnchor.constraint(equalTo: container.trailingAnchor),
            blur.bottomAnchor.constraint(equalTo: container.bottomAnchor),
            view.topAnchor.constraint(equalTo: container.topAnchor),
            view.leadingAnchor.constraint(equalTo: container.leadingAnchor),
            view.trailingAnchor.constraint(equalTo: container.trailingAnchor),
            view.bottomAnchor.constraint(equalTo: container.bottomAnchor),
        ])
        return container
    }

    // MARK: - Button

    private static func makeButton(state: String, theme: ComponentCase.Theme) -> UIView {
        let button = UIButton(type: .system)
        button.setTitle("Button", for: .normal)
        let isGlass = (theme == .liquidGlassLight || theme == .liquidGlassDark)

        if isGlass {
            // See wrapInGlassBackdropIfNeeded's doc comment: UIButton.
            // Configuration's real .glass()/.prominentGlass() (iOS 26)
            // don't actually render translucent in this offscreen-snapshot
            // pipeline, so a manual UIVisualEffectView backdrop is used
            // instead — the same approach already proven for
            // actionSheet/bottomSheet/segmentedButton/datePicker. A
            // semi-transparent tint over the blur approximates
            // "prominent" (colored) vs plain glass.
            button.contentEdgeInsets = UIEdgeInsets(top: 10, left: 20, bottom: 10, right: 20)
            button.layer.cornerRadius = 18
            button.clipsToBounds = true

            let isDark = (theme == .liquidGlassDark)
            let blur = UIVisualEffectView(effect: glassEffect(isDark: isDark))
            blur.isUserInteractionEnabled = false
            blur.translatesAutoresizingMaskIntoConstraints = false
            button.insertSubview(blur, at: 0)
            NSLayoutConstraint.activate([
                blur.topAnchor.constraint(equalTo: button.topAnchor),
                blur.leadingAnchor.constraint(equalTo: button.leadingAnchor),
                blur.trailingAnchor.constraint(equalTo: button.trailingAnchor),
                blur.bottomAnchor.constraint(equalTo: button.bottomAnchor),
            ])

            switch state {
            case "primary":
                button.backgroundColor = UIColor.systemBlue.withAlphaComponent(0.85)
                button.setTitleColor(.white, for: .normal)
            case "secondary", "tertiary":
                button.setTitleColor(.systemBlue, for: .normal)
            case "danger":
                button.backgroundColor = UIColor.systemRed.withAlphaComponent(0.85)
                button.setTitleColor(.white, for: .normal)
            case "disabled":
                button.isEnabled = false
            default:
                break
            }
            return button
        }

        switch state {
        case "primary":
            if #available(iOS 15.0, *) {
                button.configuration = .filled()
            } else {
                button.backgroundColor = .systemBlue
                button.setTitleColor(.white, for: .normal)
                button.layer.cornerRadius = 14
            }
        case "secondary":
            if #available(iOS 15.0, *) {
                button.configuration = .tinted()
            } else {
                button.backgroundColor = UIColor.systemBlue.withAlphaComponent(0.15)
                button.setTitleColor(.systemBlue, for: .normal)
                button.layer.cornerRadius = 14
            }
        case "tertiary":
            if #available(iOS 15.0, *) {
                button.configuration = .plain()
            }
        case "danger":
            if #available(iOS 15.0, *) {
                var cfg = UIButton.Configuration.filled()
                cfg.baseBackgroundColor = .systemRed
                button.configuration = cfg
            } else {
                button.backgroundColor = .systemRed
                button.setTitleColor(.white, for: .normal)
                button.layer.cornerRadius = 14
            }
        case "disabled":
            button.setTitle("Button", for: .normal)
            button.isEnabled = false
        default:
            break
        }
        return button
    }

    // MARK: - Switch

    private static func makeSwitch(state: String) -> UIView {
        let sw = UISwitch()
        switch state {
        case "on": sw.isOn = true
        case "off": sw.isOn = false
        case "disabled": sw.isOn = false; sw.isEnabled = false
        default: break
        }
        return sw
    }

    // MARK: - Slider

    private static func makeSlider(state: String) -> UIView {
        let slider = UISlider()
        slider.value = 0.33
        if state == "disabled" { slider.isEnabled = false }
        return slider
    }

    // MARK: - TextField

    private static func makeTextField(state: String) -> UIView {
        let tf = UITextField()
        tf.borderStyle = .roundedRect
        tf.placeholder = "Placeholder"
        tf.widthAnchor.constraint(equalToConstant: 240).isActive = true
        switch state {
        case "filled": tf.text = "Hello"
        case "disabled": tf.isEnabled = false
        default: break
        }
        return tf
    }

    // MARK: - Card

    private static func makeCard(state: String, theme: ComponentCase.Theme) -> UIView {
        let card = UIView()
        card.layer.cornerRadius = 14
        card.layer.masksToBounds = true

        let isDark = (theme == .cupertinoDark || theme == .liquidGlassDark)
        let isGlass = (theme == .liquidGlassLight || theme == .liquidGlassDark)

        if isGlass && state == "elevated" {
            let blur = UIVisualEffectView(effect: glassEffect(isDark: isDark))
            blur.translatesAutoresizingMaskIntoConstraints = false
            card.addSubview(blur)
            NSLayoutConstraint.activate([
                blur.topAnchor.constraint(equalTo: card.topAnchor),
                blur.leadingAnchor.constraint(equalTo: card.leadingAnchor),
                blur.trailingAnchor.constraint(equalTo: card.trailingAnchor),
                blur.bottomAnchor.constraint(equalTo: card.bottomAnchor),
            ])
            card.layer.shadowColor = UIColor.black.cgColor
            card.layer.shadowOpacity = 0.08
            card.layer.shadowOffset = CGSize(width: 0, height: 2)
            card.layer.shadowRadius = 6
        } else {
            switch state {
            case "filled":
                card.backgroundColor = isDark ? UIColor(hex: 0x1C1C1E) : UIColor(hex: 0xF2F2F7)
            case "outlined":
                card.backgroundColor = isDark ? .black : .white
                card.layer.borderColor = UIColor.separator.cgColor
                card.layer.borderWidth = 1
            default:
                card.backgroundColor = isDark ? .black : .white
                card.layer.shadowColor = UIColor.black.cgColor
                card.layer.shadowOpacity = 0.08
                card.layer.shadowOffset = CGSize(width: 0, height: 2)
                card.layer.shadowRadius = 6
            }
        }

        let label = UILabel()
        label.text = "Card content"
        label.textColor = .label
        label.translatesAutoresizingMaskIntoConstraints = false
        card.addSubview(label)
        NSLayoutConstraint.activate([
            label.topAnchor.constraint(equalTo: card.topAnchor, constant: 16),
            label.leadingAnchor.constraint(equalTo: card.leadingAnchor, constant: 16),
            label.trailingAnchor.constraint(equalTo: card.trailingAnchor, constant: -16),
            label.bottomAnchor.constraint(equalTo: card.bottomAnchor, constant: -16),
        ])
        card.widthAnchor.constraint(equalToConstant: 240).isActive = true
        return card
    }

    // MARK: - ListTile

    private static func makeListTile(state: String) -> UIView {
        let cell = UITableViewCell(style: state == "two_line" ? .subtitle : .default, reuseIdentifier: nil)
        cell.textLabel?.text = "Title"
        cell.detailTextLabel?.text = state == "two_line" ? "Subtitle" : nil
        if state == "with_icon" {
            cell.imageView?.image = UIImage(systemName: "star")
        }
        cell.accessoryType = .disclosureIndicator
        cell.widthAnchor.constraint(equalToConstant: 320).isActive = true
        // A UITableViewCell created outside a UITableView doesn't reliably
        // report/lay out at its natural height via Auto Layout alone (its
        // content view is normally frame-driven by the owning table view,
        // not self-sizing) — without an explicit height here, the
        // red-border wrapper sizes to the wrong (too-small) height while
        // the cell still paints its real content, and the imageView (the
        // first subview to lose out) doesn't get laid out at all.
        cell.heightAnchor.constraint(equalToConstant: state == "two_line" ? 60 : 44).isActive = true
        cell.setNeedsLayout()
        cell.layoutIfNeeded()
        return cell
    }

    // MARK: - Divider

    private static func makeDivider(state: String) -> UIView {
        let line = UIView()
        line.backgroundColor = .separator
        line.heightAnchor.constraint(equalToConstant: 1).isActive = true
        if state == "indented" {
            let stack = UIStackView(arrangedSubviews: [UIView(), line, UIView()])
            stack.axis = .horizontal
            stack.spacing = 0
            stack.distribution = .equalSpacing
            stack.widthAnchor.constraint(equalToConstant: 320).isActive = true
            return stack
        }
        line.widthAnchor.constraint(equalToConstant: 320).isActive = true
        return line
    }

    // MARK: - AppBar

    private static func makeAppBar(state: String) -> UIView {
        // A standalone UINavigationBar, not a full embedded
        // UINavigationController.view — the latter lays out its
        // navigation bar assuming a real window/safe-area context, which
        // doesn't exist here; forcing its container to a hardcoded 44pt
        // then clipped the bar at the wrong vertical offset. UINavigationBar
        // on its own has a real, self-contained intrinsic/fitting height
        // Auto Layout computes correctly.
        let navBar = UINavigationBar()
        let item = UINavigationItem(title: state == "center_title" ? "Title" : "Navigation")
        item.leftBarButtonItem = UIBarButtonItem(image: UIImage(systemName: "chevron.left"), style: .plain, target: nil, action: nil)
        item.rightBarButtonItem = UIBarButtonItem(image: UIImage(systemName: "gear"), style: .plain, target: nil, action: nil)
        navBar.items = [item]
        navBar.widthAnchor.constraint(equalToConstant: 360).isActive = true
        return navBar
    }

    // MARK: - NavigationBar

    private static func makeNavigationBar(state: String, theme: ComponentCase.Theme) -> UIView {
        // Not a real UITabBar: on this SDK its default floating-capsule
        // chrome is already translucent unconditionally, showing up in
        // the plain Cupertino theme too — and unlike UITabBar's classic
        // full-width background, this new floating-pill shape isn't
        // controlled by UITabBarAppearance's standard properties either
        // (tried .configureWithOpaqueBackground(), confirmed live: no
        // visible effect). A manual mockup, matching the pattern already
        // used for actionSheet/dialog/searchField, gives a reliable
        // cupertino/glass split instead.
        let isGlass = (theme == .liquidGlassLight || theme == .liquidGlassDark)
        let isDark = (theme == .cupertinoDark || theme == .liquidGlassDark)
        let items: [(String, String)] = [("house", "First"), ("magnifyingglass", "Second"), ("person", "Third")]

        let container = UIView()
        container.layer.cornerRadius = 32
        container.clipsToBounds = true
        container.widthAnchor.constraint(equalToConstant: 360).isActive = true
        container.heightAnchor.constraint(equalToConstant: 64).isActive = true

        if isGlass {
            let blur = UIVisualEffectView(effect: glassEffect(isDark: isDark))
            blur.translatesAutoresizingMaskIntoConstraints = false
            container.addSubview(blur)
            NSLayoutConstraint.activate([
                blur.topAnchor.constraint(equalTo: container.topAnchor),
                blur.leadingAnchor.constraint(equalTo: container.leadingAnchor),
                blur.trailingAnchor.constraint(equalTo: container.trailingAnchor),
                blur.bottomAnchor.constraint(equalTo: container.bottomAnchor),
            ])
        } else {
            container.backgroundColor = isDark ? UIColor(hex: 0x1C1C1E) : .white
        }

        var itemViews: [UIView] = []
        for (index, item) in items.enumerated() {
            let color: UIColor = index == 0 ? .systemBlue : .label
            let icon = UIImageView(image: UIImage(systemName: item.0))
            icon.tintColor = color
            icon.contentMode = .scaleAspectFit
            icon.widthAnchor.constraint(equalToConstant: 24).isActive = true
            icon.heightAnchor.constraint(equalToConstant: 24).isActive = true

            let title = UILabel()
            title.text = item.1
            title.font = .systemFont(ofSize: 10, weight: .semibold)
            title.textColor = color
            title.textAlignment = .center

            itemViews.append(vstack([icon, title], spacing: 2, alignment: .center))
        }

        let row = hstack(itemViews, spacing: 0)
        row.distribution = .fillEqually
        row.translatesAutoresizingMaskIntoConstraints = false
        container.addSubview(row)
        NSLayoutConstraint.activate([
            row.topAnchor.constraint(equalTo: container.topAnchor, constant: 8),
            row.leadingAnchor.constraint(equalTo: container.leadingAnchor, constant: 16),
            row.trailingAnchor.constraint(equalTo: container.trailingAnchor, constant: -16),
            row.bottomAnchor.constraint(equalTo: container.bottomAnchor, constant: -8),
        ])
        return container
    }

    // MARK: - Dialog

    private static func makeDialog(state: String, theme: ComponentCase.Theme) -> UIView {
        // Not a real UIAlertController: its .view, extracted and
        // force-width-constrained outside of real present(_:animated:)
        // presentation, doesn't lay out its button row correctly (buttons
        // visibly overflowing the card — confirmed live) — the same
        // presentation-machinery dependency actionSheet had. A manual
        // mockup, same pattern as actionSheet, renders reliably. It also
        // makes the opaque-vs-blurred choice explicit: a real
        // UIAlertController is *always* translucent/vibrant system chrome
        // regardless of app theme (true since iOS 7, nothing to do with
        // Liquid Glass) — which reads as "cupertino dialogs are using
        // liquid glass" when compared against a Cupertino theme that's
        // supposed to be opaque elsewhere. This mockup only blurs for the
        // actual Liquid Glass theme variants, matching every other
        // component's cupertino/glass split.
        let isGlass = (theme == .liquidGlassLight || theme == .liquidGlassDark)
        let isDark = (theme == .cupertinoDark || theme == .liquidGlassDark)

        var actions: [(String, UIColor)] = [("OK", .systemBlue)]
        if state == "two_actions" {
            actions = [("Cancel", .systemBlue), ("OK", .systemBlue)]
        } else if state == "three_actions" {
            actions = [("Cancel", .systemBlue), ("OK", .systemBlue), ("Delete", .systemRed)]
        }

        let container = UIView()
        container.layer.cornerRadius = 14
        container.clipsToBounds = true
        container.widthAnchor.constraint(equalToConstant: 270).isActive = true

        if isGlass {
            let blur = UIVisualEffectView(effect: glassEffect(isDark: isDark))
            blur.translatesAutoresizingMaskIntoConstraints = false
            container.addSubview(blur)
            NSLayoutConstraint.activate([
                blur.topAnchor.constraint(equalTo: container.topAnchor),
                blur.leadingAnchor.constraint(equalTo: container.leadingAnchor),
                blur.trailingAnchor.constraint(equalTo: container.trailingAnchor),
                blur.bottomAnchor.constraint(equalTo: container.bottomAnchor),
            ])
        } else {
            container.backgroundColor = isDark ? UIColor(hex: 0x2C2C2E) : .white
        }

        let title = UILabel()
        title.text = "Title"
        title.font = .boldSystemFont(ofSize: 17)
        title.textAlignment = .center
        title.textColor = .label

        let message = UILabel()
        message.text = "Message"
        message.font = .systemFont(ofSize: 13)
        message.textAlignment = .center
        message.textColor = .secondaryLabel
        message.numberOfLines = 0

        let textStack = vstack([title, message], spacing: 4)
        let textPadded = UIView()
        textStack.translatesAutoresizingMaskIntoConstraints = false
        textPadded.addSubview(textStack)
        NSLayoutConstraint.activate([
            textStack.topAnchor.constraint(equalTo: textPadded.topAnchor, constant: 20),
            textStack.leadingAnchor.constraint(equalTo: textPadded.leadingAnchor, constant: 16),
            textStack.trailingAnchor.constraint(equalTo: textPadded.trailingAnchor, constant: -16),
            textStack.bottomAnchor.constraint(equalTo: textPadded.bottomAnchor, constant: -20),
        ])

        // Real iOS alerts lay 1-2 actions side by side (divided by a
        // vertical hairline); 3+ stack vertically, each divided by a
        // horizontal hairline — matches CupertinoDesignSystem::buildDialog's
        // own layout rule.
        let actionArea: UIView
        if actions.count <= 2 {
            var rowChildren: [UIView] = []
            for (i, action) in actions.enumerated() {
                if i > 0 { rowChildren.append(vhairline()) }
                rowChildren.append(alertActionRow(action.0, color: action.1))
            }
            let row = hstack(rowChildren, spacing: 0)
            row.distribution = .fillEqually
            actionArea = row
        } else {
            var colChildren: [UIView] = []
            for (i, action) in actions.enumerated() {
                if i > 0 { colChildren.append(hairline()) }
                colChildren.append(alertActionRow(action.0, color: action.1))
            }
            actionArea = vstack(colChildren, spacing: 0)
        }

        let stack = vstack([textPadded, hairline(), actionArea], spacing: 0)
        stack.translatesAutoresizingMaskIntoConstraints = false
        container.addSubview(stack)
        NSLayoutConstraint.activate([
            stack.topAnchor.constraint(equalTo: container.topAnchor),
            stack.leadingAnchor.constraint(equalTo: container.leadingAnchor),
            stack.trailingAnchor.constraint(equalTo: container.trailingAnchor),
            stack.bottomAnchor.constraint(equalTo: container.bottomAnchor),
        ])
        return container
    }

    // MARK: - PopupMenuButton / DropdownButton

    private static func makePopupMenuButton(state: String) -> UIView {
        let button = UIButton(type: .system)
        button.setTitle("Open Menu", for: .normal)
        if #available(iOS 14.0, *) {
            button.menu = UIMenu(title: "", children: [
                UIAction(title: "One") { _ in },
                UIAction(title: "Two") { _ in },
            ])
            button.showsMenuAsPrimaryAction = true
        }
        return button
    }

    private static func makeDropdownButton(state: String) -> UIView {
        let button = UIButton(type: .system)
        button.setTitle("Select", for: .normal)
        if #available(iOS 14.0, *), state == "open" {
            button.menu = UIMenu(title: "", children: [
                UIAction(title: "Option 1") { _ in },
                UIAction(title: "Option 2") { _ in },
            ])
            button.showsMenuAsPrimaryAction = true
        }
        return button
    }

    // MARK: - PrimaryActionButton

    private static func makePrimaryActionButton(state: String, theme: ComponentCase.Theme) -> UIView {
        let button = UIButton(type: .custom)
        button.frame.size = CGSize(width: 56, height: 56)
        button.widthAnchor.constraint(equalToConstant: 56).isActive = true
        button.heightAnchor.constraint(equalToConstant: 56).isActive = true
        button.layer.cornerRadius = 28
        button.clipsToBounds = true

        let isGlass = (theme == .liquidGlassLight || theme == .liquidGlassDark)
        var blur: UIVisualEffectView?
        if isGlass {
            let isDark = (theme == .liquidGlassDark)
            let b = UIVisualEffectView(effect: glassEffect(isDark: isDark))
            b.frame = button.bounds
            b.autoresizingMask = [.flexibleWidth, .flexibleHeight]
            b.isUserInteractionEnabled = false
            button.addSubview(b)
            blur = b
            button.setTitleColor(.systemBlue, for: .normal)
        } else {
            button.backgroundColor = .systemBlue
            button.setTitleColor(.white, for: .normal)
        }

        if state == "icon" {
            // Not button.setImage(...): confirmed live that UIButton's own
            // built-in imageView doesn't end up visible above the blur
            // here (title-based content in the "label" state renders
            // fine, so this is specific to setImage's imageView, not a
            // general z-order issue — moving the blur's sendSubviewToBack
            // to run after setImage() didn't fix it either). A manual
            // UIImageView subview, the same pattern every other icon in
            // this file uses, sidesteps whatever the underlying quirk is.
            let icon = UIImageView(image: UIImage(systemName: "plus"))
            icon.tintColor = isGlass ? .systemBlue : .white
            icon.contentMode = .scaleAspectFit
            icon.isUserInteractionEnabled = false
            icon.translatesAutoresizingMaskIntoConstraints = false
            button.addSubview(icon)
            NSLayoutConstraint.activate([
                icon.centerXAnchor.constraint(equalTo: button.centerXAnchor),
                icon.centerYAnchor.constraint(equalTo: button.centerYAnchor),
                icon.widthAnchor.constraint(equalToConstant: 24),
                icon.heightAnchor.constraint(equalToConstant: 24),
            ])
        } else {
            button.setTitle("+", for: .normal)
            button.titleLabel?.font = UIFont.systemFont(ofSize: 24)
        }

        if let blur { button.sendSubviewToBack(blur) }
        return button
    }

    // MARK: - TabBar

    private static func makeTabBar(state: String) -> UIView {
        let segmented = UISegmentedControl(items: ["One", "Two"])
        segmented.selectedSegmentIndex = 0
        return segmented
    }

    // MARK: - Chip

    private static func makeChip(state: String) -> UIView {
        let button = UIButton(type: .system)
        button.setTitle("Chip", for: .normal)
        button.layer.cornerRadius = 16
        button.clipsToBounds = true
        button.contentEdgeInsets = UIEdgeInsets(top: 6, left: 12, bottom: 6, right: 12)
        if state == "selected" {
            button.backgroundColor = UIColor.systemBlue.withAlphaComponent(0.15)
            button.setTitleColor(.systemBlue, for: .normal)
        } else {
            button.backgroundColor = UIColor.secondarySystemBackground
            button.setTitleColor(.label, for: .normal)
        }
        return button
    }

    // MARK: - SegmentedButton

    private static func makeSegmentedButton(state: String, theme: ComponentCase.Theme) -> UIView {
        let segmented = UISegmentedControl(items: ["Day", "Week", "Month"])
        segmented.selectedSegmentIndex = 0
        segmented.widthAnchor.constraint(equalToConstant: 280).isActive = true
        return wrapInGlassBackdropIfNeeded(segmented, theme: theme)
    }

    // MARK: - BottomSheet

    private static func makeBottomSheet(state: String, theme: ComponentCase.Theme) -> UIView {
        let view = UIView()
        view.layer.cornerRadius = 20
        view.layer.maskedCorners = [.layerMinXMinYCorner, .layerMaxXMinYCorner]
        view.layer.masksToBounds = true
        view.heightAnchor.constraint(equalToConstant: 200).isActive = true
        view.widthAnchor.constraint(equalToConstant: 360).isActive = true

        let isGlass = (theme == .liquidGlassLight || theme == .liquidGlassDark)
        if isGlass {
            let isDark = (theme == .liquidGlassDark)
            let blur = UIVisualEffectView(effect: glassEffect(isDark: isDark))
            blur.translatesAutoresizingMaskIntoConstraints = false
            view.addSubview(blur)
            NSLayoutConstraint.activate([
                blur.topAnchor.constraint(equalTo: view.topAnchor),
                blur.leadingAnchor.constraint(equalTo: view.leadingAnchor),
                blur.trailingAnchor.constraint(equalTo: view.trailingAnchor),
                blur.bottomAnchor.constraint(equalTo: view.bottomAnchor),
            ])
        } else {
            view.backgroundColor = .secondarySystemBackground
        }

        let handle = UIView()
        handle.backgroundColor = .separator
        handle.layer.cornerRadius = 2.5
        handle.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(handle)
        NSLayoutConstraint.activate([
            handle.topAnchor.constraint(equalTo: view.topAnchor, constant: 8),
            handle.centerXAnchor.constraint(equalTo: view.centerXAnchor),
            handle.widthAnchor.constraint(equalToConstant: 36),
            handle.heightAnchor.constraint(equalToConstant: 5),
        ])
        return view
    }

    // MARK: - Badge

    private static func makeBadge(state: String) -> UIView {
        let icon = systemImage("bell")
        if state == "dot" {
            let dot = UIView()
            dot.backgroundColor = .systemRed
            dot.layer.cornerRadius = 4
            dot.translatesAutoresizingMaskIntoConstraints = false
            icon.addSubview(dot)
            NSLayoutConstraint.activate([
                dot.topAnchor.constraint(equalTo: icon.topAnchor, constant: -2),
                dot.trailingAnchor.constraint(equalTo: icon.trailingAnchor, constant: 2),
                dot.widthAnchor.constraint(equalToConstant: 8),
                dot.heightAnchor.constraint(equalToConstant: 8),
            ])
        } else if state == "number" {
            let badge = UILabel()
            badge.text = "3"
            badge.textColor = .white
            badge.backgroundColor = .systemRed
            badge.font = UIFont.systemFont(ofSize: 10)
            badge.textAlignment = .center
            badge.layer.cornerRadius = 8
            badge.clipsToBounds = true
            badge.translatesAutoresizingMaskIntoConstraints = false
            icon.addSubview(badge)
            NSLayoutConstraint.activate([
                badge.topAnchor.constraint(equalTo: icon.topAnchor, constant: -4),
                badge.trailingAnchor.constraint(equalTo: icon.trailingAnchor, constant: 4),
                badge.widthAnchor.constraint(equalToConstant: 16),
                badge.heightAnchor.constraint(equalToConstant: 16),
            ])
        }
        return icon
    }

    // MARK: - IconButton

    private static func makeIconButton(state: String) -> UIView {
        let button = UIButton(type: .system)
        button.setImage(UIImage(systemName: "heart"), for: .normal)
        if state == "filled" {
            button.backgroundColor = UIColor.systemBlue.withAlphaComponent(0.15)
            button.tintColor = .systemBlue
            button.layer.cornerRadius = 8
        } else if state == "selected" {
            button.tintColor = .systemBlue
            button.setImage(UIImage(systemName: "heart.fill"), for: .normal)
        }
        return button
    }

    // MARK: - Stepper

    private static func makeStepper(state: String) -> UIView {
        let stepper = UIStepper()
        if state == "disabled" { stepper.isEnabled = false }
        return stepper
    }

    // MARK: - RatingIndicator

    private static func makeRatingIndicator(state: String) -> UIView {
        var stars: [UIImageView] = []
        for i in 0..<5 {
            let name = i < 3 ? "star.fill" : "star"
            let iv = UIImageView(image: UIImage(systemName: name))
            iv.tintColor = .systemYellow
            iv.widthAnchor.constraint(equalToConstant: 20).isActive = true
            iv.heightAnchor.constraint(equalToConstant: 20).isActive = true
            stars.append(iv)
        }
        return hstack(stars, spacing: 4)
    }

    // MARK: - ActionSheet

    private static func makeActionSheet(state: String, theme: ComponentCase.Theme) -> UIView {
        // Not a real UIAlertController: its translucent system chrome is
        // tied to the presentation-controller machinery (present(_:
        // animated:)) and does not initialize when its .view is extracted
        // and snapshotted standalone — confirmed live, even after fixing
        // ScreenshotExporter to snapshot from within a real UIWindow (see
        // that file's own comment). A manual mockup with a real
        // UIVisualEffectView backdrop, matching the pattern already used
        // for bottomSheet/card, renders reliably instead.
        let isGlass = (theme == .liquidGlassLight || theme == .liquidGlassDark)
        let isDark = (theme == .cupertinoDark || theme == .liquidGlassDark)

        let container = UIView()
        container.layer.cornerRadius = 14
        container.clipsToBounds = true
        container.widthAnchor.constraint(equalToConstant: 360).isActive = true

        if isGlass {
            let blur = UIVisualEffectView(effect: glassEffect(isDark: isDark))
            blur.translatesAutoresizingMaskIntoConstraints = false
            container.addSubview(blur)
            NSLayoutConstraint.activate([
                blur.topAnchor.constraint(equalTo: container.topAnchor),
                blur.leadingAnchor.constraint(equalTo: container.leadingAnchor),
                blur.trailingAnchor.constraint(equalTo: container.trailingAnchor),
                blur.bottomAnchor.constraint(equalTo: container.bottomAnchor),
            ])
        } else {
            container.backgroundColor = isDark ? UIColor(hex: 0x1C1C1E) : .white
        }

        let stack = vstack([
            alertActionRow("Title", color: .secondaryLabel), hairline(),
            alertActionRow("Save", color: .systemBlue), hairline(),
            alertActionRow("Delete", color: .systemRed),
        ], spacing: 0)
        stack.translatesAutoresizingMaskIntoConstraints = false
        container.addSubview(stack)
        NSLayoutConstraint.activate([
            stack.topAnchor.constraint(equalTo: container.topAnchor),
            stack.leadingAnchor.constraint(equalTo: container.leadingAnchor),
            stack.trailingAnchor.constraint(equalTo: container.trailingAnchor),
            stack.bottomAnchor.constraint(equalTo: container.bottomAnchor),
        ])

        // Real UIActionSheet always separates Cancel into its own group,
        // detached from the action list above — matching
        // CupertinoDesignSystem::buildActionSheet()'s on_cancel handling,
        // which this test case exercises (its harness config always sets
        // on_cancel).
        let cancelContainer = UIView()
        cancelContainer.layer.cornerRadius = 14
        cancelContainer.clipsToBounds = true
        cancelContainer.widthAnchor.constraint(equalToConstant: 360).isActive = true

        if isGlass {
            let blur = UIVisualEffectView(effect: glassEffect(isDark: isDark))
            blur.translatesAutoresizingMaskIntoConstraints = false
            cancelContainer.addSubview(blur)
            NSLayoutConstraint.activate([
                blur.topAnchor.constraint(equalTo: cancelContainer.topAnchor),
                blur.leadingAnchor.constraint(equalTo: cancelContainer.leadingAnchor),
                blur.trailingAnchor.constraint(equalTo: cancelContainer.trailingAnchor),
                blur.bottomAnchor.constraint(equalTo: cancelContainer.bottomAnchor),
            ])
        } else {
            cancelContainer.backgroundColor = isDark ? UIColor(hex: 0x1C1C1E) : .white
        }

        let cancelRow = alertActionRow("Cancel", color: .systemBlue)
        cancelRow.translatesAutoresizingMaskIntoConstraints = false
        cancelContainer.addSubview(cancelRow)
        NSLayoutConstraint.activate([
            cancelRow.topAnchor.constraint(equalTo: cancelContainer.topAnchor),
            cancelRow.leadingAnchor.constraint(equalTo: cancelContainer.leadingAnchor),
            cancelRow.trailingAnchor.constraint(equalTo: cancelContainer.trailingAnchor),
            cancelRow.bottomAnchor.constraint(equalTo: cancelContainer.bottomAnchor),
        ])

        let outer = vstack([container, cancelContainer], spacing: 8)
        outer.translatesAutoresizingMaskIntoConstraints = false
        return outer
    }

    // MARK: - SearchField

    private static func makeSearchField(state: String, theme: ComponentCase.Theme) -> UIView {
        // Not a real UISearchTextField: its own default rounded-capsule
        // background already renders as translucent on this SDK
        // unconditionally — showing up in the plain Cupertino theme too,
        // not just Liquid Glass — and UIKit has no public appearance API
        // to force it back to a classic opaque fill (unlike UITabBar's
        // UITabBarAppearance). A manual mockup, matching the pattern
        // already used for actionSheet/dialog, gives a reliable
        // cupertino/glass split instead.
        let isGlass = (theme == .liquidGlassLight || theme == .liquidGlassDark)
        let isDark = (theme == .cupertinoDark || theme == .liquidGlassDark)

        let container = UIView()
        container.layer.cornerRadius = 10
        container.clipsToBounds = true
        container.widthAnchor.constraint(equalToConstant: 280).isActive = true
        container.heightAnchor.constraint(equalToConstant: 36).isActive = true

        if isGlass {
            let blur = UIVisualEffectView(effect: glassEffect(isDark: isDark))
            blur.translatesAutoresizingMaskIntoConstraints = false
            container.addSubview(blur)
            NSLayoutConstraint.activate([
                blur.topAnchor.constraint(equalTo: container.topAnchor),
                blur.leadingAnchor.constraint(equalTo: container.leadingAnchor),
                blur.trailingAnchor.constraint(equalTo: container.trailingAnchor),
                blur.bottomAnchor.constraint(equalTo: container.bottomAnchor),
            ])
        } else {
            // .secondarySystemFill (tried first) is one of Apple's "Fill"
            // colors — that whole family is intentionally semi-transparent
            // by design (meant to be layered over content), not a solid
            // fill despite the name. .systemGray6 is a genuinely opaque,
            // light/dark-adaptive color instead — what real classic search
            // fields actually use.
            container.backgroundColor = .systemGray6
        }

        let icon = UIImageView(image: UIImage(systemName: "magnifyingglass"))
        icon.tintColor = .secondaryLabel
        icon.contentMode = .scaleAspectFit
        icon.translatesAutoresizingMaskIntoConstraints = false

        let text = UILabel()
        text.text = state == "filled" ? "query" : "Search"
        text.textColor = state == "filled" ? .label : .secondaryLabel
        text.translatesAutoresizingMaskIntoConstraints = false

        container.addSubview(icon)
        container.addSubview(text)
        NSLayoutConstraint.activate([
            icon.leadingAnchor.constraint(equalTo: container.leadingAnchor, constant: 8),
            icon.centerYAnchor.constraint(equalTo: container.centerYAnchor),
            icon.widthAnchor.constraint(equalToConstant: 18),
            icon.heightAnchor.constraint(equalToConstant: 18),
            text.leadingAnchor.constraint(equalTo: icon.trailingAnchor, constant: 6),
            text.trailingAnchor.constraint(lessThanOrEqualTo: container.trailingAnchor, constant: -8),
            text.centerYAnchor.constraint(equalTo: container.centerYAnchor),
        ])
        return container
    }

    // MARK: - DatePicker / TimePicker

    private static func makeDatePicker(state: String, theme: ComponentCase.Theme) -> UIView {
        let picker = UIDatePicker()
        if #available(iOS 14.0, *) {
            picker.preferredDatePickerStyle = .compact
        }
        picker.datePickerMode = .date
        return wrapInGlassBackdropIfNeeded(picker, theme: theme)
    }

    private static func makeTimePicker(state: String, theme: ComponentCase.Theme) -> UIView {
        let picker = UIDatePicker()
        if #available(iOS 14.0, *) {
            picker.preferredDatePickerStyle = .compact
        }
        picker.datePickerMode = .time
        return wrapInGlassBackdropIfNeeded(picker, theme: theme)
    }

    // MARK: - ExpansionTile

    private static func makeExpansionTile(state: String) -> UIView {
        let stack = vstack([
            hstack([label("Settings"), systemImage(state == "expanded" ? "chevron.up" : "chevron.down")]),
            state == "expanded" ? label("Expanded content goes here.") : UIView(),
        ])
        stack.widthAnchor.constraint(equalToConstant: 320).isActive = true
        return stack
    }

    // MARK: - ToggleButtons

    private static func makeToggleButtons(state: String, theme: ComponentCase.Theme) -> UIView {
        let segmented = UISegmentedControl(items: ["A", "B", "C"])
        segmented.selectedSegmentIndex = 0
        segmented.setEnabled(true, forSegmentAt: 0)
        return wrapInGlassBackdropIfNeeded(segmented, theme: theme)
    }

    // MARK: - Banner

    private static func makeBanner(state: String) -> UIView {
        let view = UIView()
        view.backgroundColor = UIColor.systemBlue.withAlphaComponent(0.15)
        view.layer.cornerRadius = 8
        let text = label("A banner message")
        text.textColor = .systemBlue
        text.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(text)
        NSLayoutConstraint.activate([
            text.topAnchor.constraint(equalTo: view.topAnchor, constant: 12),
            text.leadingAnchor.constraint(equalTo: view.leadingAnchor, constant: 16),
            text.trailingAnchor.constraint(equalTo: view.trailingAnchor, constant: -16),
            text.bottomAnchor.constraint(equalTo: view.bottomAnchor, constant: -12),
        ])
        view.widthAnchor.constraint(equalToConstant: 360).isActive = true
        return view
    }

    // MARK: - NavigationRail

    private static func makeNavigationRail(state: String) -> UIView {
        let rail = UIView()
        rail.backgroundColor = .secondarySystemBackground
        rail.widthAnchor.constraint(equalToConstant: state == "extended" ? 200 : 72).isActive = true
        rail.heightAnchor.constraint(equalToConstant: 320).isActive = true

        let items = [("house", "Home"), ("magnifyingglass", "Search"), ("person", "Profile")]
        let stack = UIStackView()
        stack.axis = .vertical
        stack.spacing = 16
        stack.alignment = state == "extended" ? .leading : .center
        stack.distribution = .equalSpacing
        stack.translatesAutoresizingMaskIntoConstraints = false
        rail.addSubview(stack)
        NSLayoutConstraint.activate([
            stack.topAnchor.constraint(equalTo: rail.topAnchor, constant: 16),
            stack.leadingAnchor.constraint(equalTo: rail.leadingAnchor, constant: 8),
            stack.trailingAnchor.constraint(equalTo: rail.trailingAnchor, constant: -8),
        ])

        for (icon, title) in items {
            let item = state == "extended" ? hstack([systemImage(icon), label(title)], spacing: 12) : systemImage(icon)
            stack.addArrangedSubview(item)
        }
        return rail
    }

    // MARK: - DataTable

    private static func makeDataTable(state: String) -> UIView {
        // A bare UITableView with no data source/delegate renders nothing
        // but its own blank background — built as a plain stack of rows
        // instead, matching this file's existing style for other
        // "lightweight mockup" components (navigationRail, banner, etc.)
        // and sidestepping the lifetime hazard of a UITableView's
        // dataSource being a weak reference with nothing else retaining it.
        let columns = ["Name", "Age"]
        let rows = [["Alice", "30"], ["Bob", "25"]]

        func rowView(_ cells: [String], bold: Bool) -> UIView {
            let labels = cells.map { text -> UILabel in
                let l = UILabel()
                l.text = text
                l.font = bold ? .boldSystemFont(ofSize: 15) : .systemFont(ofSize: 15)
                l.textColor = .label
                return l
            }
            let row = hstack(labels, spacing: 12)
            row.distribution = .fillEqually
            return row
        }

        var rowViews: [UIView] = [rowView(columns, bold: true)]
        for r in rows { rowViews.append(rowView(r, bold: false)) }

        let stack = vstack(rowViews, spacing: 12)
        stack.widthAnchor.constraint(equalToConstant: 320).isActive = true
        return stack
    }

    // MARK: - ConfirmationDialog

    /// iOS 26's "remove app" system prompt style: one glass card,
    /// left-aligned title/message, individually-pilled stacked actions —
    /// replicated from a real device screenshot (not a real UIAlertController
    /// snapshot; this exact prompt is SpringBoard-only and has no public API
    /// an app can trigger, so it's hand-mocked like actionSheet/dialog above).
    private static func makeConfirmationDialog(state: String, theme: ComponentCase.Theme) -> UIView {
        // Pre-iOS-26 "remove app" prompts use the exact same
        // UIAlertController chrome as any other classic alert — centered
        // text, hairline-divided action rows, blue/red action colors — not
        // iOS 26's individually pilled buttons (confirmed against a real
        // device screenshot on iOS 18). Mirror makeDialog()'s construction
        // instead of duplicating a second copy of that layout logic.
        if theme == .cupertinoLight || theme == .cupertinoDark {
            let isDark = (theme == .cupertinoDark)

            let container = UIView()
            container.layer.cornerRadius = 14
            container.clipsToBounds = true
            container.widthAnchor.constraint(equalToConstant: 270).isActive = true
            container.backgroundColor = isDark ? UIColor(hex: 0x2C2C2E) : .white

            let title = UILabel()
            title.text = "¿Eliminar Freeform?"
            title.font = .boldSystemFont(ofSize: 17)
            title.textAlignment = .center
            title.textColor = .label
            title.numberOfLines = 0

            let message = UILabel()
            message.text = "Si la eliminas de la pantalla de inicio, la app se guardará en tu biblioteca de apps."
            message.font = .systemFont(ofSize: 13)
            message.textAlignment = .center
            message.textColor = .secondaryLabel
            message.numberOfLines = 0

            let textStack = vstack([title, message], spacing: 4)
            let textPadded = UIView()
            textStack.translatesAutoresizingMaskIntoConstraints = false
            textPadded.addSubview(textStack)
            NSLayoutConstraint.activate([
                textStack.topAnchor.constraint(equalTo: textPadded.topAnchor, constant: 20),
                textStack.leadingAnchor.constraint(equalTo: textPadded.leadingAnchor, constant: 16),
                textStack.trailingAnchor.constraint(equalTo: textPadded.trailingAnchor, constant: -16),
                textStack.bottomAnchor.constraint(equalTo: textPadded.bottomAnchor, constant: -20),
            ])

            let actions: [(String, UIColor)] = [
                ("Eliminar de la pantalla de inicio", .systemBlue),
                ("Eliminar app", .systemRed),
                ("Cancelar", .systemBlue),
            ]
            var colChildren: [UIView] = []
            for (i, action) in actions.enumerated() {
                if i > 0 { colChildren.append(hairline()) }
                colChildren.append(alertActionRow(action.0, color: action.1))
            }
            let actionArea = vstack(colChildren, spacing: 0)

            let stack = vstack([textPadded, hairline(), actionArea], spacing: 0)
            stack.translatesAutoresizingMaskIntoConstraints = false
            container.addSubview(stack)
            NSLayoutConstraint.activate([
                stack.topAnchor.constraint(equalTo: container.topAnchor),
                stack.leadingAnchor.constraint(equalTo: container.leadingAnchor),
                stack.trailingAnchor.constraint(equalTo: container.trailingAnchor),
                stack.bottomAnchor.constraint(equalTo: container.bottomAnchor),
            ])
            return container
        }

        let isGlass = (theme == .liquidGlassLight || theme == .liquidGlassDark)
        let isDark = (theme == .cupertinoDark || theme == .liquidGlassDark)

        let container = UIView()
        container.layer.cornerRadius = 28
        container.clipsToBounds = true
        container.widthAnchor.constraint(equalToConstant: 320).isActive = true

        if isGlass {
            let blur = UIVisualEffectView(effect: glassEffect(isDark: isDark))
            blur.translatesAutoresizingMaskIntoConstraints = false
            container.addSubview(blur)
            NSLayoutConstraint.activate([
                blur.topAnchor.constraint(equalTo: container.topAnchor),
                blur.leadingAnchor.constraint(equalTo: container.leadingAnchor),
                blur.trailingAnchor.constraint(equalTo: container.trailingAnchor),
                blur.bottomAnchor.constraint(equalTo: container.bottomAnchor),
            ])
        } else {
            container.backgroundColor = isDark ? UIColor(hex: 0x1C1C1E) : .white
        }

        let titleLabel = UILabel()
        titleLabel.text = "¿Eliminar Freeform?"
        titleLabel.font = .boldSystemFont(ofSize: 19)
        titleLabel.textColor = .label
        titleLabel.numberOfLines = 0

        let messageLabel = UILabel()
        messageLabel.text = "Si la eliminas de la pantalla de inicio, la app se guardará en tu biblioteca de apps."
        messageLabel.font = .systemFont(ofSize: 15)
        messageLabel.textColor = .secondaryLabel
        messageLabel.numberOfLines = 0

        func pill(_ text: String, color: UIColor) -> UIView {
            let l = UILabel()
            l.text = text
            l.font = .boldSystemFont(ofSize: 17)
            l.textColor = color
            l.textAlignment = .center
            l.numberOfLines = 1
            l.adjustsFontSizeToFitWidth = true
            l.minimumScaleFactor = 0.8

            let bg = UIView()
            bg.backgroundColor = .tertiarySystemFill
            bg.layer.cornerRadius = 25 // fully rounded capsule (height / 2), matches the reference screenshot
            bg.heightAnchor.constraint(equalToConstant: 50).isActive = true

            l.translatesAutoresizingMaskIntoConstraints = false
            bg.addSubview(l)
            NSLayoutConstraint.activate([
                l.leadingAnchor.constraint(greaterThanOrEqualTo: bg.leadingAnchor, constant: 12),
                l.trailingAnchor.constraint(lessThanOrEqualTo: bg.trailingAnchor, constant: -12),
                l.centerXAnchor.constraint(equalTo: bg.centerXAnchor),
                l.centerYAnchor.constraint(equalTo: bg.centerYAnchor),
            ])
            return bg
        }

        let stack = vstack([
            titleLabel,
            messageLabel,
            pill("Eliminar de la pantalla de inicio", color: .label),
            pill("Eliminar app", color: .systemRed),
            pill("Cancelar", color: .label),
        ], spacing: 4, alignment: .fill)
        stack.setCustomSpacing(4, after: titleLabel)
        stack.setCustomSpacing(16, after: messageLabel)

        stack.translatesAutoresizingMaskIntoConstraints = false
        container.addSubview(stack)
        NSLayoutConstraint.activate([
            stack.topAnchor.constraint(equalTo: container.topAnchor, constant: 20),
            stack.leadingAnchor.constraint(equalTo: container.leadingAnchor, constant: 20),
            stack.trailingAnchor.constraint(equalTo: container.trailingAnchor, constant: -20),
            stack.bottomAnchor.constraint(equalTo: container.bottomAnchor, constant: -20),
        ])
        return container
    }
}

// MARK: - UIColor hex helper

extension UIColor {
    convenience init(hex: UInt32) {
        let r = CGFloat((hex >> 16) & 0xFF) / 255.0
        let g = CGFloat((hex >> 8) & 0xFF) / 255.0
        let b = CGFloat(hex & 0xFF) / 255.0
        self.init(red: r, green: g, blue: b, alpha: 1.0)
    }
}
