import java.util.Properties

plugins { id("com.android.application") }
// Release signing is read from android/keystore.properties (ignored by git):
// storeFile, storePassword, keyAlias, keyPassword. Without it the release APK
// is built unsigned and must be signed separately; it is never debuggable.
val releaseSigning = Properties().apply {
    val file = rootProject.file("keystore.properties")
    if (file.isFile) file.inputStream().use { load(it) }
}
android {
    namespace = "org.nephelium.phoneticipa"
    compileSdk = 36
    buildFeatures { buildConfig = true }
    defaultConfig {
        applicationId = "org.nephelium.phoneticipa"
        minSdk = 29
        targetSdk = 36
        versionCode = 9
        versionName = "0.1.8"
        testInstrumentationRunner = "org.nephelium.phoneticipa.CoreTests"
    }
    signingConfigs {
        if (!releaseSigning.isEmpty) create("release") {
            storeFile = rootProject.file(releaseSigning.getProperty("storeFile"))
            storePassword = releaseSigning.getProperty("storePassword")
            keyAlias = releaseSigning.getProperty("keyAlias")
            keyPassword = releaseSigning.getProperty("keyPassword")
        }
    }
    buildTypes {
        getByName("release") {
            isDebuggable = false
            isMinifyEnabled = false
            if (!releaseSigning.isEmpty) signingConfig = signingConfigs.getByName("release")
        }
    }
    compileOptions { sourceCompatibility = JavaVersion.VERSION_17; targetCompatibility = JavaVersion.VERSION_17 }
    sourceSets.getByName("main").assets.srcDir(rootProject.file("../apple/Generated"))
    sourceSets.getByName("androidTest").assets.srcDir(rootProject.file("../updates/testdata"))
    androidResources { noCompress += "sqlite" }
}
