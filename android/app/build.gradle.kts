plugins { id("com.android.application") }
android {
    namespace = "org.nephelium.phoneticipa"
    compileSdk = 36
    defaultConfig {
        applicationId = "org.nephelium.phoneticipa"
        minSdk = 29
        targetSdk = 36
        versionCode = 1
        versionName = "0.1.0-prototype"
        testInstrumentationRunner = "org.nephelium.phoneticipa.CoreTests"
    }
    compileOptions { sourceCompatibility = JavaVersion.VERSION_17; targetCompatibility = JavaVersion.VERSION_17 }
    sourceSets.getByName("main").assets.srcDir(rootProject.file("../apple/Generated"))
    androidResources { noCompress += "sqlite" }
}
