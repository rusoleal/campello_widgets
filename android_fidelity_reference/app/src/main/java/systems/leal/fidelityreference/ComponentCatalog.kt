package systems.leal.fidelityreference

import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Card
import androidx.compose.material3.ElevatedCard
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ExperimentalMaterial3ExpressiveApi
import androidx.compose.foundation.layout.Box
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ExposedDropdownMenuBox
import androidx.compose.material3.ExposedDropdownMenuDefaults
import androidx.compose.material3.FilterChip
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.ListItem
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.MenuAnchorType
import androidx.compose.material3.MultiChoiceSegmentedButtonRow
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedCard
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.SegmentedButton
import androidx.compose.material3.SegmentedButtonDefaults
import androidx.compose.material3.SingleChoiceSegmentedButtonRow
import androidx.compose.material3.Slider
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
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp

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

            // "listTile_with_icon" intentionally omitted: the C++ side's
            // icon() helper is a "★" text placeholder (no real Icon widget
            // yet — see themed_component_harness.cpp's TODO), so comparing
            // it against a real Compose icon would test icon-glyph fidelity
            // rather than the ListItem/ListTile chrome this pass covers.
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
        }
    }
}
