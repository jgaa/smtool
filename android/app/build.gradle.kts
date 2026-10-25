plugins {
    alias(libs.plugins.android.application)
}

android {
    namespace = "eu.lastviking.android.smtool"
    compileSdk {
        version = release(36) {
            minorApiLevel = 1
        }
    }

    defaultConfig {
        applicationId = "eu.lastviking.android.smtool.talk"
        minSdk = 31
        targetSdk = 36
        versionCode = 1
        versionName = "1.0"

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
    }

    signingConfigs {
        create("release") {
            storeFile = file("${System.getProperty("user.home")}/lastviking.eu.keystore")
            storePassword = project.findProperty("RELEASE_STORE_PASSWORD") as String?
                ?: project.findProperty("storePassword") as String?
                ?: project.findProperty("MYAPP_RELEASE_STORE_PASSWORD") as String?
                ?: project.findProperty("signing.release.storePassword") as String?
                ?: System.getenv("RELEASE_STORE_PASSWORD") ?: ""

            keyAlias = project.findProperty("RELEASE_KEY_ALIAS") as String?
                ?: project.findProperty("keyAlias") as String?
                ?: project.findProperty("MYAPP_RELEASE_KEY_ALIAS") as String?
                ?: project.findProperty("signing.release.keyAlias") as String?
                ?: System.getenv("RELEASE_KEY_ALIAS") ?: ""

            keyPassword = project.findProperty("RELEASE_KEY_PASSWORD") as String?
                ?: project.findProperty("keyPassword") as String?
                ?: project.findProperty("MYAPP_RELEASE_KEY_PASSWORD") as String?
                ?: project.findProperty("signing.release.keyPassword") as String?
                ?: System.getenv("RELEASE_KEY_PASSWORD") ?: ""
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = true
            isShrinkResources = true
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
            signingConfig = signingConfigs.getByName("release")
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }
    buildFeatures {
        viewBinding = true
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/whisper/CMakeLists.txt")
        }
    }

    packaging {
        jniLibs {
            useLegacyPackaging = true
        }
    }
}

dependencies {
    // implementation(libs.whisper.android) // Removed in favor of embedded whisper.cpp
    implementation(libs.okhttp)
    implementation(libs.kotlinx.coroutines.android)
    implementation(libs.androidx.activity.ktx)
    implementation(libs.androidx.appcompat)
    implementation(libs.androidx.constraintlayout)
    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.navigation.fragment.ktx)
    implementation(libs.androidx.navigation.ui.ktx)
    implementation(libs.material)
    testImplementation(libs.junit)
    androidTestImplementation(libs.androidx.espresso.core)
    androidTestImplementation(libs.androidx.junit)
}