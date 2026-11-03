package com.novaengine.app

import android.app.Application
import com.novaengine.app.engine.NovaEngineBridge

/**
 * Application entry. Creates the native engine once for the process lifetime.
 * The engine is shared by the dashboard and editor screens.
 */
class NovaApplication : Application() {
    override fun onCreate() {
        super.onCreate()
        NovaEngineBridge.nativeCreate()
        NovaEngineBridge.nativeInitialize()
    }
}
