package systems.leal.fidelityreference

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Badge
import androidx.compose.material3.BadgedBox
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Card
import androidx.compose.material3.CenterAlignedTopAppBar
import androidx.compose.material3.ElevatedCard
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ExperimentalMaterial3ExpressiveApi
import androidx.compose.material3.FloatingActionButton
import androidx.compose.material3.TopAppBar
import androidx.compose.foundation.layout.Box
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.Favorite
import androidx.compose.material.icons.filled.Home
import androidx.compose.material.icons.filled.Notifications
import androidx.compose.material.icons.filled.Person
import androidx.compose.material.icons.filled.Search
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material.icons.filled.Star
import androidx.compose.foundation.layout.size
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ExposedDropdownMenuBox
import androidx.compose.material3.ExposedDropdownMenuDefaults
import androidx.compose.material3.FilledIconButton
import androidx.compose.material3.FilterChip
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.ListItem
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.MenuAnchorType
import androidx.compose.material3.ModalBottomSheet
import androidx.compose.material3.MultiChoiceSegmentedButtonRow
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.NavigationRail
import androidx.compose.material3.NavigationRailItem
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedCard
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.SegmentedButton
import androidx.compose.material3.SegmentedButtonDefaults
import androidx.compose.material3.SingleChoiceSegmentedButtonRow
import androidx.compose.material3.Slider
import androidx.compose.material3.Surface
import androidx.compose.material3.Switch
import androidx.compose.material3.Tab
import androidx.compose.material3.TabRow
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

/**
 * Maps a `{builder}_{state}` case id — the same naming convention
 * `themed_component_harness.cpp` uses — to a real M3 Expressive composable.
 * Content/labels mirror that file's cases exactly (see its `button`/
 * `switch`/`card` builder blocks) so the two sides render the same content.
 */
object ComponentCatalog {
    @OptIn(ExperimentalMaterial3ExpressiveApi::class, ExperimentalMaterial3Api::class)
    @Composable
    fun render(caseId: String) {
        when (caseId) {
            "button_primary" -> Button(onClick = {}) { Text("Button") }
            "button_secondary" -> Button(
                onClick = {},
                colors = ButtonDefaults.buttonColors(
                    containerColor = MaterialTheme.colorScheme.secondary,
                    contentColor = MaterialTheme.colorScheme.onSecondary
                )
            ) { Text("Button") }
            "button_tertiary" -> OutlinedButton(onClick = {}) { Text("Button") }
            "button_danger" -> Button(
                onClick = {},
                colors = ButtonDefaults.buttonColors(
                    containerColor = MaterialTheme.colorScheme.error,
                    contentColor = MaterialTheme.colorScheme.onError
                )
            ) { Text("Button") }
            "button_disabled" -> OutlinedButton(onClick = {}, enabled = false) { Text("Button") }

            "switch_on" -> Switch(checked = true, onCheckedChange = {})
            "switch_off" -> Switch(checked = false, onCheckedChange = {})
            "switch_disabled" -> Switch(checked = false, onCheckedChange = {}, enabled = false)

            "card_elevated" -> ElevatedCard(modifier = Modifier.padding(16.dp)) {
                Text("Card content", modifier = Modifier.padding(16.dp))
            }
            "card_filled" -> Card(modifier = Modifier.padding(16.dp)) {
                Text("Card content", modifier = Modifier.padding(16.dp))
            }
            "card_outlined" -> OutlinedCard(modifier = Modifier.padding(16.dp)) {
                Text("Card content", modifier = Modifier.padding(16.dp))
            }

            "slider_value" -> Slider(
                value = 0.33f, onValueChange = {}, modifier = Modifier.width(280.dp)
            )
            "slider_disabled" -> Slider(
                value = 0.33f, onValueChange = {}, enabled = false, modifier = Modifier.width(280.dp)
            )

            "chip_unselected" -> FilterChip(selected = false, onClick = {}, label = { Text("Chip") })
            "chip_selected" -> FilterChip(selected = true, onClick = {}, label = { Text("Chip") })

            "divider_default" -> HorizontalDivider(modifier = Modifier.width(280.dp))
            "divider_indented" -> HorizontalDivider(
                modifier = Modifier.width(280.dp).padding(start = 16.dp, end = 16.dp)
            )

            "listTile_with_icon" -> ListItem(
                headlineContent = { Text("Title") },
                leadingContent = { Icon(Icons.Filled.Star, null) },
                modifier = Modifier.width(280.dp)
            )
            "listTile_one_line" -> ListItem(
                headlineContent = { Text("Title") },
                modifier = Modifier.width(280.dp)
            )
            "listTile_two_line" -> ListItem(
                headlineContent = { Text("Title") },
                supportingContent = { Text("Subtitle") },
                modifier = Modifier.width(280.dp)
            )

            "textField_empty" -> OutlinedTextField(
                value = "", onValueChange = {}, placeholder = { Text("Placeholder") },
                modifier = Modifier.width(240.dp)
            )
            "textField_filled" -> OutlinedTextField(
                value = "Hello", onValueChange = {}, placeholder = { Text("Placeholder") },
                modifier = Modifier.width(240.dp)
            )
            "textField_disabled" -> OutlinedTextField(
                value = "", onValueChange = {}, placeholder = { Text("Placeholder") }, enabled = false,
                modifier = Modifier.width(240.dp)
            )

            "segmentedButton_three_segments" -> {
                val labels = listOf("Day", "Week", "Month")
                SingleChoiceSegmentedButtonRow(modifier = Modifier.width(280.dp)) {
                    labels.forEachIndexed { index, label ->
                        SegmentedButton(
                            selected = index == 0,
                            onClick = {},
                            shape = SegmentedButtonDefaults.itemShape(index = index, count = labels.size)
                        ) { Text(label) }
                    }
                }
            }

            // All three states put every action in the confirmButton slot
            // (never dismissButton) so left-to-right order is fully
            // explicit here, matching themed_component_harness.cpp's
            // "dialog" case exactly (one_action=[OK], two_actions=
            // [Cancel,OK], three_actions=[OK,Delete,Cancel]) rather than
            // relying on AlertDialog's own dismiss/confirm slot ordering.
            "dialog_one_action" -> AlertDialog(
                onDismissRequest = {},
                title = { Text("Title") },
                text = { Text("Message") },
                confirmButton = { TextButton(onClick = {}) { Text("OK") } }
            )
            "dialog_two_actions" -> AlertDialog(
                onDismissRequest = {},
                title = { Text("Title") },
                text = { Text("Message") },
                confirmButton = {
                    Row {
                        TextButton(onClick = {}) { Text("Cancel") }
                        TextButton(onClick = {}) { Text("OK") }
                    }
                }
            )
            "dialog_three_actions" -> AlertDialog(
                onDismissRequest = {},
                title = { Text("Title") },
                text = { Text("Message") },
                confirmButton = {
                    Row {
                        TextButton(onClick = {}) { Text("OK") }
                        TextButton(
                            onClick = {},
                            colors = ButtonDefaults.textButtonColors(
                                contentColor = MaterialTheme.colorScheme.error
                            )
                        ) { Text("Delete") }
                        TextButton(onClick = {}) { Text("Cancel") }
                    }
                }
            )

            "tabBar_two_tabs" -> TabRow(selectedTabIndex = 0, modifier = Modifier.width(280.dp)) {
                Tab(selected = true, onClick = {}, text = { Text("One") })
                Tab(selected = false, onClick = {}, text = { Text("Two") })
            }

            // Only the closed state is captured — the "open" state would
            // need a real anchored DropdownMenu overlay, the same overlay-
            // positioning complexity that's deferred popupMenuButton's
            // "open" state all session (see themed_component_harness.cpp's
            // androidBuilderStates()).
            "dropdownButton_closed" -> {
                var expanded by remember { mutableStateOf(false) }
                ExposedDropdownMenuBox(
                    expanded = expanded,
                    onExpandedChange = { expanded = it },
                    modifier = Modifier.width(240.dp)
                ) {
                    OutlinedTextField(
                        value = "",
                        onValueChange = {},
                        readOnly = true,
                        placeholder = { Text("Select") },
                        trailingIcon = { ExposedDropdownMenuDefaults.TrailingIcon(expanded = expanded) },
                        modifier = Modifier
                            .menuAnchor(MenuAnchorType.PrimaryNotEditable, true)
                            .fillMaxWidth()
                    )
                }
            }

            // Mirrors themed_component_harness.cpp's "badge" case: bell
            // icon, "dot" state = empty Badge (small dot), "number" state
            // = Badge showing "3".
            "badge_dot" -> BadgedBox(badge = { Badge() }) {
                Icon(Icons.Filled.Notifications, null)
            }
            "badge_number" -> BadgedBox(badge = { Badge { Text("3") } }) {
                Icon(Icons.Filled.Notifications, null)
            }

            // Mirrors "iconButton": heart icon. buildIconButton() only has
            // one bool (selected), so "plain"/"filled" both render as a
            // plain (unfilled) IconButton in the C++ side — mirrored
            // faithfully here rather than inventing a distinction the
            // shared config doesn't actually have. "selected" -> filled
            // primary-color container, matching real M3's toggled
            // FilledIconButton appearance.
            "iconButton_plain" -> IconButton(onClick = {}) { Icon(Icons.Filled.Favorite, null) }
            "iconButton_filled" -> IconButton(onClick = {}) { Icon(Icons.Filled.Favorite, null) }
            "iconButton_selected" -> FilledIconButton(onClick = {}) { Icon(Icons.Filled.Favorite, null) }

            // Mirrors "navigationRail" (compact only — see
            // androidBuilderStates()'s comment on why "extended" has no
            // real-capture equivalent): house/magnifyingglass/person,
            // first item selected.
            "navigationRail_compact" -> NavigationRail {
                NavigationRailItem(selected = true, onClick = {}, icon = { Icon(Icons.Filled.Home, null) }, label = { Text("Home") })
                NavigationRailItem(selected = false, onClick = {}, icon = { Icon(Icons.Filled.Search, null) }, label = { Text("Search") })
                NavigationRailItem(selected = false, onClick = {}, icon = { Icon(Icons.Filled.Person, null) }, label = { Text("Profile") })
            }

            // Mirrors "navigationBar" (Material's bottom nav bar, distinct
            // from navigationRail above): house/magnifyingglass/person,
            // labels "First"/"Second"/"Third" (not "Home"/"Search"/
            // "Profile" — those are navigationRail's labels), first item
            // selected.
            "navigationBar_three_items" -> NavigationBar {
                NavigationBarItem(selected = true, onClick = {}, icon = { Icon(Icons.Filled.Home, null) }, label = { Text("First") })
                NavigationBarItem(selected = false, onClick = {}, icon = { Icon(Icons.Filled.Search, null) }, label = { Text("Second") })
                NavigationBarItem(selected = false, onClick = {}, icon = { Icon(Icons.Filled.Person, null) }, label = { Text("Third") })
            }

            // Mirrors "appBar": back chevron leading + one settings action.
            // "default" -> Text("Navigation") (start-aligned title, real
            // TopAppBar's default); "center_title" -> Text("Title")
            // (center-aligned, CenterAlignedTopAppBar).
            "appBar_default" -> TopAppBar(
                title = { Text("Navigation") },
                navigationIcon = { IconButton(onClick = {}) { Icon(Icons.AutoMirrored.Filled.ArrowBack, null) } },
                actions = { IconButton(onClick = {}) { Icon(Icons.Filled.Settings, null) } }
            )
            "appBar_center_title" -> CenterAlignedTopAppBar(
                title = { Text("Title") },
                navigationIcon = { IconButton(onClick = {}) { Icon(Icons.AutoMirrored.Filled.ArrowBack, null) } },
                actions = { IconButton(onClick = {}) { Icon(Icons.Filled.Settings, null) } }
            )

            // Mirrors "primaryActionButton": real M3 FAB defaults to
            // primaryContainer, matching buildPrimaryActionButton()'s own
            // deco.color choice.
            "primaryActionButton_icon" -> FloatingActionButton(onClick = {}) { Icon(Icons.Filled.Add, null) }
            "primaryActionButton_label" -> FloatingActionButton(onClick = {}) { Text("+") }

            "popupMenuButton_closed" -> OutlinedButton(onClick = {}) { Text("Open Menu") }
            "popupMenuButton_open" -> {
                // DropdownMenu(expanded = true) opens immediately with no
                // click needed — real Android's default position (directly
                // below, left-aligned to the anchor Box) needs no explicit
                // offset for an isolated case like this with nothing else
                // on screen to avoid.
                Box {
                    OutlinedButton(onClick = {}) { Text("Open Menu") }
                    DropdownMenu(expanded = true, onDismissRequest = {}) {
                        DropdownMenuItem(text = { Text("One") }, onClick = {})
                        DropdownMenuItem(text = { Text("Two") }, onClick = {})
                    }
                }
            }

            // Mirrors themed_component_harness.cpp's "toggleButtons" case
            // exactly: A and C checked, B unchecked. No fixed width here —
            // unlike segmentedButton's longer "Day"/"Week"/"Month" labels,
            // buildToggleButtons()'s Row shrink-wraps to its buttons'
            // natural content width (MainAxisSize::min), so the real
            // reference should too rather than stretching equally to fill
            // an arbitrary 280dp.
            "toggleButtons_multi" -> {
                val labels = listOf("A", "B", "C")
                val checked = listOf(true, false, true)
                MultiChoiceSegmentedButtonRow {
                    labels.forEachIndexed { index, label ->
                        SegmentedButton(
                            checked = checked[index],
                            onCheckedChange = {},
                            shape = SegmentedButtonDefaults.itemShape(index = index, count = labels.size)
                        ) { Text(label) }
                    }
                }
            }

            // Mirrors "bottomSheet": show_drag_handle=true, content =
            // Text("Sheet content") with no extra padding around it —
            // ModalBottomSheet's default dragHandle already matches
            // buildBottomSheet()'s hand-built 32x4dp pill handle.
            "bottomSheet_partial" -> ModalBottomSheet(onDismissRequest = {}) {
                Text("Sheet content")
            }

            // Mirrors "stepper": no real M3 Stepper composable exists, so
            // buildStepper() itself uses literal "-"/"+" text glyphs (not
            // icons) in a primary-12%-opacity tonal box — mirrored here
            // exactly rather than substituting real icons, so the
            // comparison measures layout/color fidelity to that design,
            // not a separate icon-vs-text decision. value=0 for both
            // states (StepperConfig's default, unchanged by "disabled").
            "stepper_default", "stepper_disabled" -> {
                val content: @Composable () -> Unit = {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Box(
                            modifier = Modifier
                                .background(
                                    MaterialTheme.colorScheme.primary.copy(alpha = 0.12f),
                                    RoundedCornerShape(8.dp)
                                )
                                .padding(horizontal = 12.dp, vertical = 8.dp)
                        ) { Text("-", color = MaterialTheme.colorScheme.primary, fontSize = 16.sp) }
                        Text(
                            "0",
                            modifier = Modifier.padding(horizontal = 12.dp, vertical = 6.dp),
                            fontSize = 14.sp
                        )
                        Box(
                            modifier = Modifier
                                .background(
                                    MaterialTheme.colorScheme.primary.copy(alpha = 0.12f),
                                    RoundedCornerShape(8.dp)
                                )
                                .padding(horizontal = 12.dp, vertical = 8.dp)
                        ) { Text("+", color = MaterialTheme.colorScheme.primary, fontSize = 16.sp) }
                    }
                }
                if (caseId == "stepper_disabled") {
                    Box(modifier = Modifier.alpha(0.4f)) { content() }
                } else {
                    content()
                }
            }

            // Mirrors "ratingIndicator": value=3, max=5 — buildRatingIndicator()
            // uses literal "*"/"-" text glyphs (no real star icon), mirrored
            // exactly here for the same reason as stepper above.
            "ratingIndicator_three_of_five" -> Row {
                val filled = listOf(true, true, true, false, false)
                filled.forEachIndexed { index, isFilled ->
                    Text(
                        if (isFilled) "*" else "-",
                        color = if (isFilled) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.outline,
                        fontSize = 18.sp
                    )
                    if (index < filled.size - 1) Spacer(modifier = Modifier.width(2.dp))
                }
            }

            // Mirrors "actionSheet": MD3's real equivalent is a modal
            // bottom sheet with a plain list of items — title, "Save"
            // (normal), "Delete" (destructive/error-colored), "Cancel" as
            // an ordinary row (MD3 has no separated Cancel button
            // convention, per buildActionSheet()'s own comment).
            "actionSheet_open" -> ModalBottomSheet(onDismissRequest = {}) {
                Column {
                    Text(
                        "Title",
                        modifier = Modifier.padding(start = 16.dp, top = 16.dp, end = 16.dp, bottom = 8.dp)
                    )
                    Text(
                        "Save",
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(vertical = 16.dp, horizontal = 12.dp)
                    )
                    Text(
                        "Delete",
                        color = MaterialTheme.colorScheme.error,
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(vertical = 16.dp, horizontal = 12.dp)
                    )
                    Text(
                        "Cancel",
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(vertical = 16.dp, horizontal = 12.dp)
                    )
                }
            }

            // Mirrors "searchField": buildSearchField() puts a literal
            // "search" text glyph where a real search icon would go (not
            // an Icon), and only shows a clear "x" when on_clear is set —
            // the harness never sets it, so neither state shows one.
            "searchField_empty", "searchField_filled" -> Surface(
                shape = RoundedCornerShape(50),
                color = MaterialTheme.colorScheme.surfaceVariant
            ) {
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    modifier = Modifier.padding(horizontal = 16.dp, vertical = 4.dp)
                ) {
                    Text("search", fontSize = 12.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)
                    Spacer(modifier = Modifier.width(8.dp))
                    Text(
                        if (caseId == "searchField_filled") "query" else "Search",
                        modifier = Modifier.weight(1f),
                        color = if (caseId == "searchField_filled") MaterialTheme.colorScheme.onSurface
                                else MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
            }

            // Mirrors "datePicker"/"timePicker": buildTriggerField() is a
            // plain outlined box (border only, no fill) with the label and
            // a literal trailing text glyph ("date"/"time" — not a real
            // calendar/clock icon), mirrored exactly for the same reason
            // as stepper/ratingIndicator above.
            "datePicker_compact" -> Box(
                modifier = Modifier
                    .border(1.dp, MaterialTheme.colorScheme.outline, RoundedCornerShape(4.dp))
                    .padding(vertical = 16.dp, horizontal = 12.dp)
            ) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Text("Aug 14, 2026", modifier = Modifier.weight(1f))
                    Spacer(modifier = Modifier.width(8.dp))
                    Text("date", fontSize = 12.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)
                }
            }
            "timePicker_compact" -> Box(
                modifier = Modifier
                    .border(1.dp, MaterialTheme.colorScheme.outline, RoundedCornerShape(4.dp))
                    .padding(vertical = 16.dp, horizontal = 12.dp)
            ) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Text("10:30 AM", modifier = Modifier.weight(1f))
                    Spacer(modifier = Modifier.width(8.dp))
                    Text("time", fontSize = 12.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)
                }
            }

            // Mirrors "expansionTile": no real M3 ExpansionTile composable
            // exists, so buildExpansionTile() uses a literal "^"/"v" text
            // glyph for the chevron (not a rotating icon), mirrored
            // exactly. Content only shows when expanded.
            "expansionTile_collapsed", "expansionTile_expanded" -> {
                val expanded = caseId == "expansionTile_expanded"
                Column {
                    Row(
                        verticalAlignment = Alignment.CenterVertically,
                        modifier = Modifier
                            .fillMaxWidth()
                            .clickable {}
                            .padding(vertical = 12.dp, horizontal = 16.dp)
                    ) {
                        Text("Settings", modifier = Modifier.weight(1f))
                        Text(
                            if (expanded) "^" else "v",
                            fontSize = 13.sp,
                            color = MaterialTheme.colorScheme.onSurfaceVariant
                        )
                    }
                    if (expanded) {
                        Text(
                            "Expanded content goes here.",
                            modifier = Modifier.fillMaxWidth()
                        )
                    }
                }
            }

            // Mirrors "banner": no leading icon/actions in the harness's
            // config (cfg.content only) — plain surface background,
            // distinguished by the bottom divider per buildBanner()'s own
            // comment ("MD3 banners sit on plain surface, not
            // surfaceVariant").
            "banner_default" -> Surface(color = MaterialTheme.colorScheme.surface) {
                Column {
                    Row(modifier = Modifier.padding(16.dp)) {
                        Text("A banner message", modifier = Modifier.weight(1f))
                    }
                    HorizontalDivider(color = MaterialTheme.colorScheme.outlineVariant)
                }
            }

            // Mirrors "dataTable": columns=["Name","Age"], rows=[["Alice",
            // "30"],["Bob","25"]] — buildDataTable() puts a divider after
            // *every* row including the last (header divider uses
            // `outline`, row dividers use `outline_variant`).
            "dataTable_default" -> Column {
                Row {
                    Text(
                        "Name",
                        fontWeight = FontWeight.Bold,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                        fontSize = 13.sp,
                        modifier = Modifier.weight(1f).padding(vertical = 16.dp, horizontal = 12.dp)
                    )
                    Text(
                        "Age",
                        fontWeight = FontWeight.Bold,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                        fontSize = 13.sp,
                        modifier = Modifier.weight(1f).padding(vertical = 16.dp, horizontal = 12.dp)
                    )
                }
                HorizontalDivider(color = MaterialTheme.colorScheme.outline)
                listOf("Alice" to "30", "Bob" to "25").forEach { (name, age) ->
                    Row {
                        Text(
                            name,
                            modifier = Modifier.weight(1f).padding(vertical = 16.dp, horizontal = 12.dp)
                        )
                        Text(
                            age,
                            modifier = Modifier.weight(1f).padding(vertical = 16.dp, horizontal = 12.dp)
                        )
                    }
                    HorizontalDivider(color = MaterialTheme.colorScheme.outlineVariant)
                }
            }
        }
    }
}
