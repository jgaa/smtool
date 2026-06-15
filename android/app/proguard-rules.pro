# Add project specific ProGuard rules here.
# By default, the flags in this file are appended to flags specified
# in /home/devel/Android/Sdk/tools/proguard/proguard-android.txt
# You can edit the include path and order by changing the proguardFiles
# directive in build.gradle.kts.
#
# For more details, see
#   http://developer.android.com/guide/developing/tools/proguard.html

# Keep whisper.cpp JNI classes and methods
-keep class com.whispercpp.whisper.** { *; }

# Also keep the methods in any class that implements WhisperCallback
-keep interface com.whispercpp.whisper.WhisperCallback {
    *;
}
-keep class * implements com.whispercpp.whisper.WhisperCallback {
    *;
}
