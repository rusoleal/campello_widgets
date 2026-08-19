using Microsoft.UI;
using Microsoft.UI.Text;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Controls.Primitives;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Shapes;
using Windows.UI;

namespace FidelityReference;

/// <summary>
/// Maps a `{builder}_{state}` case id — the same naming convention
/// themed_component_harness.cpp uses — to a real Fluent 2 (WinUI 3)
/// control. Content/labels mirror that file's buildWidget() cases exactly
/// (every one of them is a bare `ds.buildXxx(cfg)` call — none use a live
/// dialog/flyout presentation, so every case here is a plain, statically
/// constructed visual too — no ShowAsync()/ShowAt() needed). Structurally
/// mirrors android_fidelity_reference's ComponentCatalog.kt.
/// </summary>
public static class ComponentCatalog
{
    public static FrameworkElement? Render(string caseId)
    {
        switch (caseId)
        {
            case "button_primary":
                return new Button
                {
                    Content = "Button",
                    Style = (Style)Application.Current.Resources["AccentButtonStyle"],
                };
            case "button_secondary":
                return new Button { Content = "Button" };
            case "button_tertiary":
                // Fluent 2 has no built-in "tertiary" button style — closest
                // is a borderless, unfilled Button (text-only), matching
                // Cupertino's/Material's own "text button" convention for
                // this same priority.
                return new Button
                {
                    Content = "Button",
                    Background = new SolidColorBrush(Colors.Transparent),
                    BorderThickness = new Thickness(0),
                };
            case "button_danger":
                return new Button
                {
                    Content = "Button",
                    Background = new SolidColorBrush(Color.FromArgb(0xFF, 0xC4, 0x2B, 0x1C)),
                    Foreground = new SolidColorBrush(Colors.White),
                };
            case "button_disabled":
                return new Button { Content = "Button", IsEnabled = false };

            case "switch_on":
                return new ToggleSwitch { IsOn = true, OnContent = "", OffContent = "" };
            case "switch_off":
                return new ToggleSwitch { IsOn = false, OnContent = "", OffContent = "" };
            case "switch_disabled":
                return new ToggleSwitch { IsOn = false, OnContent = "", OffContent = "", IsEnabled = false };

            case "card_elevated":
                return Card(elevated: true, outlined: false);
            case "card_filled":
                return Card(elevated: false, outlined: false);
            case "card_outlined":
                return Card(elevated: false, outlined: true);

            case "slider_value":
                return new Slider { Minimum = 0, Maximum = 100, Value = 33, Width = 280 };
            case "slider_disabled":
                return new Slider { Minimum = 0, Maximum = 100, Value = 33, Width = 280, IsEnabled = false };

            case "textField_empty":
                return new TextBox { PlaceholderText = "Placeholder", Width = 240 };
            case "textField_filled":
                return new TextBox { PlaceholderText = "Placeholder", Text = "Hello", Width = 240 };
            case "textField_disabled":
                return new TextBox { PlaceholderText = "Placeholder", Width = 240, IsEnabled = false };

            case "listTile_one_line":
                return ListTile("Title", null);
            case "listTile_two_line":
                return ListTile("Title", "Subtitle");
            // "listTile_with_icon" intentionally omitted — the C++ side's
            // icon() helper is a "★" text placeholder (no real Icon widget
            // yet), so comparing it against a real WinUI3 icon would test
            // icon-glyph fidelity rather than the ListView-item chrome this
            // pass covers. Same omission android_fidelity_reference's
            // ComponentCatalog.kt makes, for the identical reason.

            case "divider_default":
                return Divider(indented: false);
            case "divider_indented":
                return Divider(indented: true);

            case "chip_unselected":
                return Chip(selected: false);
            case "chip_selected":
                return Chip(selected: true);

            case "badge_dot":
                // No Value/icon set — InfoBadge's default template already
                // renders as a plain dot in that case, no special style
                // resource needed (there isn't one named "DotInfoBadgeStyle";
                // referencing that nonexistent key is what crashed this
                // case the first time around).
                return new InfoBadge();
            case "badge_number":
                return new InfoBadge { Value = 3 };

            case "iconButton_plain":
                return IconButton(background: false);
            case "iconButton_filled":
                return IconButton(background: true);
            case "iconButton_selected":
                return IconButton(background: true);

            case "stepper_default":
                return new NumberBox
                {
                    Value = 1,
                    SpinButtonPlacementMode = NumberBoxSpinButtonPlacementMode.Inline,
                    Width = 120,
                };
            case "stepper_disabled":
                return new NumberBox
                {
                    Value = 1,
                    SpinButtonPlacementMode = NumberBoxSpinButtonPlacementMode.Inline,
                    Width = 120,
                    IsEnabled = false,
                };

            case "ratingIndicator_three_of_five":
                return new RatingControl { Value = 3, MaxRating = 5 };

            // Tried adding QueryIcon to match FluentDesignSystem::buildSearchField()'s
            // leading search glyph -- AutoSuggestBox only supports a *trailing*
            // QueryIcon via its default template (no built-in leading-icon
            // slot without a full retemplate), so it landed on the opposite
            // side from the C++ side's leading icon and measured slightly
            // worse (5.40%/5.38% -> 5.45%/5.43%). Reverted; the remaining gap
            // here is the icon-position/absence mismatch itself, not
            // something a reference-side tweak can close without a custom
            // ControlTemplate.
            case "searchField_empty":
                return new AutoSuggestBox { PlaceholderText = "Search", Width = 280 };
            case "searchField_filled":
                return new AutoSuggestBox { PlaceholderText = "Search", Text = "query", Width = 280 };

            // No single native "date/time field" control in base WinUI 3 —
            // DatePickerConfig/TimePickerConfig are just a tappable field
            // showing a label, so a plain Button with that label is the
            // closest honest representative.
            case "datePicker_compact":
                return new Button { Content = "Aug 14, 2026" };
            case "timePicker_compact":
                return new Button { Content = "10:30 AM" };

            case "expansionTile_collapsed":
                return ExpansionTile(expanded: false);
            case "expansionTile_expanded":
                return ExpansionTile(expanded: true);

            case "toggleButtons_multi":
                return ToggleButtonsRow();

            case "banner_default":
                return new InfoBar { IsOpen = true, Message = "A banner message" };

            case "tabBar_two_tabs":
                return TabBar();

            case "primaryActionButton_icon":
                return Fab(new FontIcon { Glyph = "\uE710" }); // Segoe MDL2 "Add"
            case "primaryActionButton_label":
                return Fab(new TextBlock { Text = "+", FontSize = 20 });

            case "segmentedButton_three_segments":
                return SegmentedButton();

            case "bottomSheet_partial":
                return BottomSheet();

            case "appBar_default":
                return AppBar("Navigation");
            case "appBar_center_title":
                return AppBar("Title");

            case "navigationBar_three_items":
                return NavigationBar();

            case "navigationRail_compact":
                return NavRail(extended: false);
            case "navigationRail_extended":
                return NavRail(extended: true);

            case "dataTable_default":
                return DataTable();

            case "popupMenuButton_closed":
                return PopupMenuButton(open: false);
            case "popupMenuButton_open":
                return PopupMenuButton(open: true);

            case "dropdownButton_closed":
            case "dropdownButton_open":
                // ComboBox's dropdown renders in its own popup layer, which
                // RenderTargetBitmap (targeting the parent element) can't
                // capture — both states render the closed ComboBox. A known,
                // honest gap rather than a fabricated "open" mockup.
                return new ComboBox
                {
                    PlaceholderText = "Select",
                    // Matches the C++ side's DropdownButton widget, which
                    // fills the full 480px canvas width by default (not
                    // wrapped in a SizedBox::from_width like slider/divider
                    // are) — a narrower ComboBox here was comparing apples
                    // to oranges.
                    Width = 480,
                    ItemsSource = new[] { "Option 1", "Option 2" },
                };

            case "dialog_one_action":
                return Dialog(new[] { ("OK", false) });
            case "dialog_two_actions":
                return Dialog(new[] { ("Cancel", false), ("OK", false) });
            case "dialog_three_actions":
                return Dialog(new[] { ("OK", false), ("Delete", true), ("Cancel", false) });

            case "actionSheet_open":
                return ActionSheet();

            default:
                return null;
        }
    }

    // Fluent 2's actual "Card" visual is a Border using the Fluent system
    // resources (CardBackgroundFillColorDefaultBrush / CardStrokeColorDefaultBrush)
    // — WinUI 3 has no single built-in Card *control* the way Compose
    // Material3 does (ElevatedCard/Card/OutlinedCard), so the three states
    // are hand-approximated here from those same tokens: elevated adds a
    // ThemeShadow, filled is a solid fill with no stroke, outlined is a
    // stroke with a transparent fill.
    private static Border Card(bool elevated, bool outlined)
    {
        var border = new Border
        {
            // FluentDesignSystem::buildCard() fills its parent's full width
            // (no content-hugging Column wraps it) — same class of gap as
            // AppBar/NavBar/TabBar/dropdownButton above: an unset Width here
            // collapses the WinUI3 Border to content size instead.
            Width = 480,
            Padding = new Thickness(16),
            CornerRadius = new CornerRadius(4),
            Background = outlined
                ? new SolidColorBrush(Colors.Transparent)
                : (Brush)Application.Current.Resources["CardBackgroundFillColorDefaultBrush"],
            BorderBrush = (Brush)Application.Current.Resources["CardStrokeColorDefaultBrush"],
            BorderThickness = new Thickness(outlined ? 1 : (elevated ? 0 : 1)),
            Child = new TextBlock { Text = "Card content" },
        };

        if (elevated)
        {
            border.Translation = new System.Numerics.Vector3(0, 0, 16);
            border.Shadow = new ThemeShadow();
        }

        return border;
    }

    private static FrameworkElement ListTile(string title, string? subtitle)
    {
        var stack = new StackPanel { Orientation = Orientation.Vertical, Spacing = 2, Width = 280 };
        stack.Children.Add(new TextBlock { Text = title });
        if (subtitle != null)
            stack.Children.Add(new TextBlock
            {
                Text = subtitle,
                FontSize = 12,
                Foreground = (Brush)Application.Current.Resources["TextFillColorSecondaryBrush"],
            });
        return new Border { Padding = new Thickness(12, 8, 12, 8), Child = stack };
    }

    // No native divider element in WinUI 3 — a 1px Rectangle filled with
    // the Fluent divider token is what a Divider actually is under the hood.
    private static Rectangle Divider(bool indented)
    {
        return new Rectangle
        {
            Height = 1,
            Width = 280,
            Fill = (Brush)Application.Current.Resources["DividerStrokeColorDefaultBrush"],
            Margin = indented ? new Thickness(16, 0, 16, 0) : new Thickness(0),
        };
    }

    // WinUI 3 has no native "Chip" control (that's a Fluent2 addition not
    // yet in base WinUI 3 as of this WindowsAppSDK version) — approximated
    // as a small pill-shaped ToggleButton, matching Fluent 2's actual Chip
    // visual (rounded, selectable, accent-filled when selected).
    private static ToggleButton Chip(bool selected)
    {
        return new ToggleButton
        {
            Content = "Chip",
            IsChecked = selected,
            CornerRadius = new CornerRadius(16),
            Padding = new Thickness(12, 6, 12, 6),
        };
    }

    private static Button IconButton(bool background)
    {
        return new Button
        {
            Content = new FontIcon { Glyph = "\uEB51" }, // Segoe MDL2 "HeartFill"
            CornerRadius = new CornerRadius(20),
            Width = 40,
            Height = 40,
            Background = background
                ? (Brush)Application.Current.Resources["AccentFillColorDefaultBrush"]
                : new SolidColorBrush(Colors.Transparent),
            BorderThickness = new Thickness(0),
        };
    }

    private static Expander ExpansionTile(bool expanded)
    {
        return new Expander
        {
            Header = "Settings",
            Content = new TextBlock { Text = "Expanded content goes here.", Margin = new Thickness(12) },
            IsExpanded = expanded,
            Width = 280,
        };
    }

    private static StackPanel ToggleButtonsRow()
    {
        var row = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 1 };
        row.Children.Add(new ToggleButton { Content = "A", IsChecked = true });
        row.Children.Add(new ToggleButton { Content = "B", IsChecked = false });
        row.Children.Add(new ToggleButton { Content = "C", IsChecked = true });
        return row;
    }

    private static Pivot TabBar()
    {
        // Matches the C++ side's TabBar widget, which fills the full 480px
        // canvas width with a surface-colored background — a narrower,
        // transparent Pivot here was comparing apples to oranges (same
        // class of bug as AppBar/NavBar/dropdownButton below).
        var pivot = new Pivot
        {
            Width = 480,
            Background = (Brush)Application.Current.Resources["CardBackgroundFillColorDefaultBrush"],
        };
        pivot.Items.Add(new PivotItem { Header = "One", Content = new Grid() });
        pivot.Items.Add(new PivotItem { Header = "Two", Content = new Grid() });
        return pivot;
    }

    // No native Floating Action Button in base WinUI 3 — a large,
    // circular AccentButtonStyle button is the closest equivalent (matches
    // how Fluent2's own FAB guidance describes it: accent-filled, fully
    // rounded, icon or short label).
    private static Button Fab(UIElement content)
    {
        return new Button
        {
            Content = content,
            Style = (Style)Application.Current.Resources["AccentButtonStyle"],
            CornerRadius = new CornerRadius(28),
            Width = 56,
            Height = 56,
            HorizontalContentAlignment = HorizontalAlignment.Center,
            VerticalContentAlignment = VerticalAlignment.Center,
        };
    }

    // No native SegmentedControl in this WindowsAppSDK version — a grouped
    // row of ToggleButtons with the first one checked approximates it.
    private static StackPanel SegmentedButton()
    {
        var row = new StackPanel { Orientation = Orientation.Horizontal, Width = 280 };
        row.Children.Add(new ToggleButton { Content = "Day", IsChecked = true, HorizontalAlignment = HorizontalAlignment.Stretch });
        row.Children.Add(new ToggleButton { Content = "Week", IsChecked = false, HorizontalAlignment = HorizontalAlignment.Stretch });
        row.Children.Add(new ToggleButton { Content = "Month", IsChecked = false, HorizontalAlignment = HorizontalAlignment.Stretch });
        return row;
    }

    private static Border BottomSheet()
    {
        var content = new StackPanel { HorizontalAlignment = HorizontalAlignment.Center, Spacing = 12 };
        content.Children.Add(new Rectangle
        {
            Width = 32,
            Height = 4,
            RadiusX = 2,
            RadiusY = 2,
            Fill = (Brush)Application.Current.Resources["DividerStrokeColorDefaultBrush"],
            HorizontalAlignment = HorizontalAlignment.Center,
        });
        content.Children.Add(new TextBlock { Text = "Sheet content" });

        return new Border
        {
            Background = (Brush)Application.Current.Resources["CardBackgroundFillColorDefaultBrush"],
            CornerRadius = new CornerRadius(8, 8, 0, 0),
            Padding = new Thickness(16),
            Width = 280,
            Child = content,
        };
    }

    private static CommandBar AppBar(string title)
    {
        // Width=480 + explicit Background \u2014 CommandBar's default template
        // only paints its background across its actual measured width, and
        // without a Width it just sizes to content and floats, unlike the
        // C++ side's AppBar which always fills the full 480px canvas
        // (same class of bug as TabBar/NavBar/dropdownButton). Also adds
        // the leading icon the C++ side has (cfg.leading = chevron icon) \u2014
        // CommandBar has no built-in "leading" slot, so it's composed
        // directly into Content alongside the title.
        var titleRow = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 12 };
        titleRow.Children.Add(new FontIcon { Glyph = "\uE76B", VerticalAlignment = VerticalAlignment.Center }); // Back/chevron
        titleRow.Children.Add(new TextBlock { Text = title, VerticalAlignment = VerticalAlignment.Center });

        var bar = new CommandBar
        {
            Content = titleRow,
            DefaultLabelPosition = CommandBarDefaultLabelPosition.Collapsed,
            Width = 480,
            Background = (Brush)Application.Current.Resources["CardBackgroundFillColorDefaultBrush"],
        };
        bar.PrimaryCommands.Add(new AppBarButton { Icon = new FontIcon { Glyph = "\uE713" } }); // Setting
        return bar;
    }

    // NavigationView's PaneDisplayMode.Top is really a Pivot-style top menu
    // (left-aligned items, underline indicator) - not a Fluent/Material
    // "bottom navigation bar" (evenly-spaced icon-over-label columns, no
    // native WinUI 3 equivalent exists for that). Hand-composed instead,
    // matching the C++ side's NavigationBarConfig layout exactly (including
    // its "First"/"Second"/"Third" item labels, which differ from
    // NavigationRailConfig's "Home"/"Search"/"Profile" - see themed_
    // component_harness.cpp's buildWidget()).
    private static Border NavigationBar()
    {
        (string label, string glyph)[] items =
        {
            ("First", "\uE80F"),
            ("Second", "\uE721"),
            ("Third", "\uE77B"),
        };

        var row = new StackPanel { Orientation = Orientation.Horizontal, Width = 480 };
        foreach (var (label, glyph) in items)
        {
            var col = new StackPanel
            {
                Orientation = Orientation.Vertical,
                HorizontalAlignment = HorizontalAlignment.Center,
                Width = 480.0 / items.Length,
                Spacing = 2,
            };
            col.Children.Add(new FontIcon { Glyph = glyph, FontSize = 16, HorizontalAlignment = HorizontalAlignment.Center });
            col.Children.Add(new TextBlock { Text = label, HorizontalAlignment = HorizontalAlignment.Center });
            row.Children.Add(col);
        }

        return new Border
        {
            Background = (Brush)Application.Current.Resources["CardBackgroundFillColorDefaultBrush"],
            Padding = new Thickness(0, 8, 0, 8),
            Child = row,
        };
    }

    private static NavigationView NavRail(bool extended)
    {
        // NavigationView's own pane template has an internal MinWidth that
        // silently overrides a plain Width setting smaller than it (this
        // is what actually produced navigationRail_extended's ~38% diff —
        // it rendered edge-to-edge across the full 480px canvas with no
        // visible pane background, instead of the intended bounded,
        // white-surfaced 280px rail). Pinning MinWidth/MaxWidth alongside
        // Width, and giving it an explicit background, forces it to
        // actually respect the intended size.
        double width = extended ? 280 : 80;
        var nav = new NavigationView
        {
            IsBackButtonVisible = NavigationViewBackButtonVisible.Collapsed,
            IsSettingsVisible = false,
            Width = width,
            MinWidth = width,
            MaxWidth = width,
            Height = 280,
            PaneDisplayMode = NavigationViewPaneDisplayMode.Left,
            IsPaneToggleButtonVisible = false,
            OpenPaneLength = 280,
            IsPaneOpen = extended,
            Background = (Brush)Application.Current.Resources["CardBackgroundFillColorDefaultBrush"],
        };

        nav.MenuItems.Add(new NavigationViewItem { Content = "Home", Icon = new FontIcon { Glyph = "\uE80F" }, IsSelected = true });
        nav.MenuItems.Add(new NavigationViewItem { Content = "Search", Icon = new FontIcon { Glyph = "\uE721" } });
        nav.MenuItems.Add(new NavigationViewItem { Content = "Profile", Icon = new FontIcon { Glyph = "\uE77B" } });
        nav.Content = new Grid();
        return nav;
    }

    // No native DataGrid in base WinUI 3 without the Community Toolkit —
    // hand-built with a plain Grid (2 columns x 3 rows: header + 2 data
    // rows), matching DataTableConfig's columns/rows exactly.
    // Matches FluentDesignSystem::buildDataTable() exactly: a card-like
    // outer surface (CardBackgroundFillColorDefault + 1px border + 4dp
    // radius), bold on_surface_variant-colored header text, a 1px divider
    // under the header row, and 1px dividers between data rows — the
    // original version of this helper was a bare Grid with no chrome at
    // all, which is what actually produced dataTable_default's ~25% diff
    // (a reference-app gap, not a campello_fluent bug).
    private static Border DataTable()
    {
        var grid = new Grid();
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
        grid.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
        grid.RowDefinitions.Add(new RowDefinition { Height = new GridLength(1) });
        grid.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
        grid.RowDefinitions.Add(new RowDefinition { Height = new GridLength(1) });
        grid.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });

        var dividerBrush = (Brush)Application.Current.Resources["DividerStrokeColorDefaultBrush"];

        void AddCell(string text, int row, int col, bool header)
        {
            var tb = new TextBlock
            {
                Text = text,
                Padding = new Thickness(16, 12, 16, 12),
                FontWeight = header ? FontWeights.Bold : FontWeights.Normal,
                Foreground = header
                    ? (Brush)Application.Current.Resources["TextFillColorSecondaryBrush"]
                    : (Brush)Application.Current.Resources["TextFillColorPrimaryBrush"],
                FontSize = header ? 13 : 14,
            };
            Grid.SetRow(tb, row);
            Grid.SetColumn(tb, col);
            grid.Children.Add(tb);
        }

        void AddDivider(int row)
        {
            var rect = new Rectangle { Fill = dividerBrush, Height = 1 };
            Grid.SetRow(rect, row);
            Grid.SetColumnSpan(rect, 2);
            grid.Children.Add(rect);
        }

        AddCell("Name", 0, 0, true);
        AddCell("Age", 0, 1, true);
        AddDivider(1);
        AddCell("Alice", 2, 0, false);
        AddCell("30", 2, 1, false);
        AddDivider(3);
        AddCell("Bob", 4, 0, false);
        AddCell("25", 4, 1, false);

        return new Border
        {
            Width = 280,
            Background = (Brush)Application.Current.Resources["CardBackgroundFillColorDefaultBrush"],
            BorderBrush = (Brush)Application.Current.Resources["CardStrokeColorDefaultBrush"],
            BorderThickness = new Thickness(1),
            CornerRadius = new CornerRadius(4),
            Child = grid,
        };
    }

    // Static mockup, not a live MenuFlyout.ShowAt() — the C++ reference
    // (ds.buildPopupMenuButton(cfg)) is itself just a bare, non-live widget
    // tree for both "closed" and "open" states, so a live flyout would not
    // be a fair comparison target anyway.
    private static FrameworkElement PopupMenuButton(bool open)
    {
        var button = new Button { Content = "Open Menu" };
        if (!open) return button;

        var menu = new StackPanel
        {
            Background = (Brush)Application.Current.Resources["CardBackgroundFillColorDefaultBrush"],
            BorderBrush = (Brush)Application.Current.Resources["CardStrokeColorDefaultBrush"],
            BorderThickness = new Thickness(1),
            CornerRadius = new CornerRadius(4),
            Padding = new Thickness(4),
            Width = 160,
        };
        menu.Children.Add(new TextBlock { Text = "One", Padding = new Thickness(8, 6, 8, 6) });
        menu.Children.Add(new TextBlock { Text = "Two", Padding = new Thickness(8, 6, 8, 6) });

        var stack = new StackPanel { Spacing = 4 };
        stack.Children.Add(button);
        stack.Children.Add(menu);
        return stack;
    }

    // Static mockup matching ContentDialog's default chrome (title,
    // content, button row) — same rationale as PopupMenuButton above: the
    // C++ reference never live-presents a dialog either.
    private static Border Dialog((string label, bool destructive)[] actions)
    {
        var content = new StackPanel { Spacing = 12, Width = 280 };
        content.Children.Add(new TextBlock { Text = "Title", FontWeight = FontWeights.Bold, FontSize = 18 });
        content.Children.Add(new TextBlock { Text = "Message" });

        var buttonRow = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8, HorizontalAlignment = HorizontalAlignment.Right };
        foreach (var (label, destructive) in actions)
        {
            buttonRow.Children.Add(new Button
            {
                Content = label,
                Foreground = destructive ? new SolidColorBrush(Color.FromArgb(0xFF, 0xC4, 0x2B, 0x1C)) : null,
            });
        }
        content.Children.Add(buttonRow);

        return new Border
        {
            Background = (Brush)Application.Current.Resources["CardBackgroundFillColorDefaultBrush"],
            CornerRadius = new CornerRadius(8),
            Padding = new Thickness(24),
            Child = content,
        };
    }

    // Rows are flat left-aligned text (no Button chrome/border) — matches
    // FluentDesignSystem::buildActionSheet()'s GestureDetector+Padding rows;
    // Fluent has no separate "action sheet" visual language, just the same
    // flat-surface hairline-border overlay as buildBottomSheet().
    private static Border ActionSheet()
    {
        var content = new StackPanel { Width = 280, Spacing = 0 };
        content.Children.Add(new TextBlock
        {
            Text = "Title",
            HorizontalAlignment = HorizontalAlignment.Left,
            Margin = new Thickness(16, 16, 16, 8),
        });
        content.Children.Add(new TextBlock { Text = "Save", Padding = new Thickness(16, 12, 16, 12) });
        content.Children.Add(new TextBlock
        {
            Text = "Delete",
            Padding = new Thickness(16, 12, 16, 12),
            Foreground = new SolidColorBrush(Color.FromArgb(0xFF, 0xC4, 0x2B, 0x1C)),
        });
        content.Children.Add(new TextBlock { Text = "Cancel", Padding = new Thickness(16, 12, 16, 12) });

        return new Border
        {
            Background = (Brush)Application.Current.Resources["CardBackgroundFillColorDefaultBrush"],
            BorderBrush = (Brush)Application.Current.Resources["CardStrokeColorDefaultBrush"],
            BorderThickness = new Thickness(1),
            CornerRadius = new CornerRadius(8),
            Child = content,
        };
    }
}
