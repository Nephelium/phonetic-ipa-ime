plugins { id("com.android.application") }
android {
    namespace = "org.nephelium.phoneticipa"
    compileSdk = 36
    defaultConfig {
        applicationId = "org.nephelium.phoneticipa"
        minSdk = 29
        targetSdk = 36
        versionCode = 2
        versionName = "0.1.1"
        testInstrumentationRunner = "org.nephelium.phoneticipa.CoreTests"
    }
    compileOptions { sourceCompatibility = JavaVersion.VERSION_17; targetCompatibility = JavaVersion.VERSION_17 }
    sourceSets.getByName("main").assets.srcDir(rootProject.file("../apple/Generated"))
    sourceSets.getByName("androidTest").assets.srcDir(rootProject.file("../updates/testdata"))
    androidResources { noCompress += "sqlite" }
}
