package com.novaengine.app.ui.editor

import android.opengl.GLSurfaceView
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.viewinterop.AndroidView
import com.novaengine.app.engine.NovaEngineBridge
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10

/**
 * The editor viewport — a real GLSurfaceView that drives the native engine's
 * update + render loop. The engine core renders the scene (clear color today;
 * full sprite/mesh rendering in the GL backend phase).
 */
@Composable
fun ViewportSurface(projectPath: String, vm: EditorViewModel) {
    val context = LocalContext.current

    AndroidView(
        modifier = Modifier.fillMaxSize(),
        factory = { ctx ->
            GLSurfaceView(ctx.applicationContext).apply {
                setEGLContextClientVersion(3)
                preserveEGLContextOnPause = true
                setRenderer(object : GLSurfaceView.Renderer {
                    override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) {}
                    override fun onSurfaceChanged(gl: GL10?, w: Int, h: Int) {
                        NovaEngineBridge.nativeResize(w, h)
                    }
                    override fun onDrawFrame(gl: GL10?) {
                        NovaEngineBridge.nativeUpdate(0.016f)
                        NovaEngineBridge.nativeRender()
                        vm.refreshStats()
                    }
                })
                renderMode = GLSurfaceView.RENDERMODE_CONTINUOUSLY
            }
        },
        viewCompositionStrategy = androidx.compose.ui.ViewCompositionStrategy.DisposeOnViewTreeLifecycleDestroyed,
    )
}
