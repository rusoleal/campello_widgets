package systems.leal.fidelityreference

import android.os.Bundle
import android.view.WindowManager
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.ExperimentalMaterial3ExpressiveApi
import androidx.compose.material3.MaterialExpressiveTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.res.painterResource
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
                Box(modifier = Modifier.fillMaxSize().background(colorScheme.background)) {
                    // Same colourful non-flat backdrop the C++ harness's
                    // Android path now paints (see renderAndroidCase() in
                    // themed_component_harness.cpp) — a plain
                    // colorScheme.background fill hid translucency/scrim/
                    // blur differences behind a uniform color. Both sides
                    // stretch the identical liquid_glass_background.png
                    // asset to their own canvas size, so the comparison
                    // still measures real content differences, not
                    // background noise.
                    //
                    // The PNG itself has genuinely semi-transparent pixels
                    // (some circles sit at ~47% alpha over the gradient),
                    // so whatever backs the Image matters: the C++ side
                    // clears its render target to colorScheme.background
                    // before drawing the image over it
                    // (renderAndroidCase()), so this Box needs the same
                    // background modifier — without it, the Image fell
                    // through to the window's default dark background
                    // instead, producing a systematic color mismatch
                    // everywhere the source PNG wasn't fully opaque.
                    Image(
                        painter = painterResource(R.drawable.liquid_glass_background),
                        contentDescription = null,
                        contentScale = ContentScale.FillBounds,
                        modifier = Modifier.fillMaxSize()
                    )
                    Surface(
                        modifier = Modifier.fillMaxSize(),
                        color = Color.Transparent,
                        contentColor = colorScheme.onBackground
                    ) {
                        CaseHost(caseId)
                    }
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
