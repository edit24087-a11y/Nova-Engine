# Keep JNI bridge classes — Kotlin <-> native method names are resolved reflectively.
-keep class com.novaengine.app.engine.NovaEngineBridge { *; }
-keepclassmembers class com.novaengine.app.engine.NovaEngineBridge { *; }
-keepclasseswithmembers class * {
    native <methods>;
}
