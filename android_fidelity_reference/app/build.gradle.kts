plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
    id("org.jetbrains.kotlin.plugin.compose")
}

android {
    namespace = "systems.leal.fidelityreference"
    compileSdk = 36

    defaultConfig {
        applicationId = "systems.leal.fidelityreference"
        minSdk = 26
        targetSdk = 36
        versionCode = 1
        versionName = "0.1.0"
    }

    buildFeatures {
        compose = true
    }

    buildTypes {
        release {
            isMinifyEnabled = false
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions {
        jvmTarget = "17"
    }
}

dependencies {
    implementation("androidx.core:core-ktx:1.13.1")
    implementation("androidx.activity:activity-compose:1.9.3")
    // M3 Expressive (MaterialExpressiveTheme, ExperimentalMaterial3ExpressiveApi)
    // ships on the material3 alpha channel — not yet in the stable Compose BOM.
    // Pinned to alpha15 specifically: later alphas (18+) bump their transitive
    // androidx.compose.ui dependency to a version requiring compileSdk 37 /
    // AGP 9.1+, which this machine's SDK/tooling can't cleanly support yet
    // (the 37.0 platform release uses non-integer API versioning that AGP
    // 9.1's target-hash lookup doesn't resolve). alpha15 still has
    // MaterialExpressiveTheme and pulls ui 1.8.2, which is compileSdk-36-safe.
    implementation("androidx.compose.material3:material3:1.5.0-alpha15")
    // Core Material Icons (Home/Search/Person/Notifications/Favorite) —
    // used only for the one-off icon_export_* cases that generate
    // campello_widgets' Icon widget assets, see ComponentCatalog.kt.
    implementation("androidx.compose.material:material-icons-core:1.7.6")
}
