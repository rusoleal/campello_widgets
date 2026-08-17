package systems.leal.fidelityreference

import android.os.Bundle
import android.view.WindowManager
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.ExperimentalMaterial3ExpressiveApi
import androidx.compose.material3.MaterialExpressiveTheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat

/**
 * Single-Activity, single-case reference harness: reads `case`/`theme` from
 * the launch Intent's extras, renders exactly one real M3 Expressive
 * component full-screen (no status/nav bar chrome — hidden below, so the
 * captured screenshot is pure app content, matching the C++ side's plain
 * canvas with nothing to crop-align against), and idles. The driving script
 * (`export_android_references.sh`) launches one case per `am start`,
 * screenshots via `adb shell screencap`, then moves to the next.
 */
class MainActivity : ComponentActivity() {
    @OptIn(ExperimentalMaterial3ExpressiveApi::class)
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        WindowCompat.setDecorFitsSystemWindows(window, false)
        WindowInsetsControllerCompat(window, window.decorView).let { controller ->
            controller.hide(WindowInsetsCompat.Type.systemBars())
        }
        @Suppress("DEPRECATION")
        window.addFlags(WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS)

        val caseId = intent.getStringExtra("case") ?: "button_primary"
        val theme = intent.getStringExtra("theme") ?: "expressive_light"
        val isDark = theme.endsWith("_dark")

        setContent {
            val colorScheme = if (isDark) darkColorScheme() else lightColorScheme()
            MaterialExpressiveTheme(colorScheme = colorScheme) {
                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = MaterialTheme.colorScheme.background
                ) {
                    CaseHost(caseId)
                }
            }
        }
    }
}

@OptIn(ExperimentalMaterial3ExpressiveApi::class)
@Composable
private fun CaseHost(caseId: String) {
    Box(modifier = Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
        ComponentCatalog.render(caseId)
    }
}
