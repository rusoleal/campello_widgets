import UIKit

/// Identifies a single fidelity reference case.
struct ComponentCase: Hashable {
    let theme: Theme
    let builder: Builder
    let state: String

    enum Theme: String, CaseIterable {
        case cupertinoLight = "cupertino_light"
        case cupertinoDark = "cupertino_dark"
        case liquidGlassLight = "liquid_glass_light"
        case liquidGlassDark = "liquid_glass_dark"
    }

    enum Builder: String, CaseIterable {
        case button
        case switchControl = "switch"
        case slider
        case textField
        case card
        case listTile
        case divider
        case appBar
        case navigationBar
        case dialog
        case popupMenuButton
        case dropdownButton
        case primaryActionButton
        case tabBar
        case chip
        case segmentedButton
        case bottomSheet
        case badge
        case iconButton
        case stepper
        case ratingIndicator
        case actionSheet
        case searchField
        case datePicker
        case timePicker
        case expansionTile
        case toggleButtons
        case banner
        case navigationRail
        case dataTable
        case confirmationDialog
    }

    var fileName: String { "\(builder.rawValue)_\(state)" }

    static func allCases() -> [ComponentCase] {
        var cases: [ComponentCase] = []
        for theme in Theme.allCases {
            for builder in Builder.allValuesForPhase1() {
                for state in builder.states() {
                    cases.append(ComponentCase(theme: theme, builder: builder, state: state))
                }
            }
        }
        return cases
    }
}

extension ComponentCase.Builder {
    /// Phase 1 covers the classic controls where UIKit ground truth is strongest.
    static func allValuesForPhase1() -> [ComponentCase.Builder] {
        [
            .button,
            .switchControl,
            .slider,
            .textField,
            .card,
            .listTile,
            .divider,
            .appBar,
            .navigationBar,
            .dialog,
            .popupMenuButton,
            .dropdownButton,
            .primaryActionButton,
            .tabBar,
            .chip,
            .segmentedButton,
            .bottomSheet,
            .badge,
            .iconButton,
            .stepper,
            .ratingIndicator,
            .actionSheet,
            .searchField,
            .datePicker,
            .timePicker,
            .expansionTile,
            .toggleButtons,
            .banner,
            .navigationRail,
            .dataTable,
            .confirmationDialog,
        ]
    }

    func states() -> [String] {
        switch self {
        case .button:
            return ["primary", "secondary", "tertiary", "danger", "disabled"]
        case .switchControl:
            return ["on", "off", "disabled"]
        case .slider:
            return ["value", "disabled"]
        case .textField:
            return ["empty", "filled", "disabled"]
        case .card:
            return ["elevated", "filled", "outlined"]
        case .listTile:
            return ["one_line", "two_line", "with_icon"]
        case .divider:
            return ["default", "indented"]
        case .appBar:
            return ["default", "center_title"]
        case .navigationBar:
            return ["three_items"]
        case .dialog:
            return ["one_action", "two_actions", "three_actions"]
        case .popupMenuButton:
            return ["closed", "open"]
        case .dropdownButton:
            return ["closed", "open"]
        case .primaryActionButton:
            return ["icon", "label"]
        case .tabBar:
            return ["two_tabs"]
        case .chip:
            return ["unselected", "selected"]
        case .segmentedButton:
            return ["three_segments"]
        case .bottomSheet:
            return ["partial"]
        case .badge:
            return ["dot", "number"]
        case .iconButton:
            return ["plain", "filled", "selected"]
        case .stepper:
            return ["default", "disabled"]
        case .ratingIndicator:
            return ["three_of_five"]
        case .actionSheet:
            return ["open"]
        case .searchField:
            return ["empty", "filled"]
        case .datePicker:
            return ["compact"]
        case .timePicker:
            return ["compact"]
        case .expansionTile:
            return ["collapsed", "expanded"]
        case .toggleButtons:
            return ["multi"]
        case .banner:
            return ["default"]
        case .navigationRail:
            return ["compact", "extended"]
        case .dataTable:
            return ["default"]
        case .confirmationDialog:
            return ["remove_app"]
        }
    }
}
