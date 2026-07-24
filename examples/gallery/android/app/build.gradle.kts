plugins {
    alias(libs.plugins.android.application)
}

android {
    namespace = "systems.leal.campello_widgets.gallery"
    compileSdk = 36

    defaultConfig {
        applicationId = "systems.leal.campello_widgets.gallery"
        minSdk = 33
        targetSdk = 36
        versionCode = 1
        versionName = "0.1.0"

        externalNativeBuild {
            cmake {
                cppFlags += "-std=c++20"
                arguments += listOf("-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON")
                // Without this, AGP builds every top-level CMake target it
                // discovers — including host-only tool executables pulled
                // in transitively (e.g. campello_image -> basis_universal's
                // `basisu` CLI encoder), which fails to link on Android
                // (no standalone -lpthread; it's part of libc there).
                // Restricting to the one target we actually need still
                // pulls in its real link dependencies (campello_widgets,
                // campello_gpu, campello_image, ...) — just not unrelated
                // executables alongside them.
                targets += "campello_gallery"
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }
}

dependencies {
    testImplementation(libs.junit)
    androidTestImplementation(libs.androidx.junit)
    androidTestImplementation(libs.androidx.espresso.core)
}
