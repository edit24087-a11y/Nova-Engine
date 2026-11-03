package com.novaengine.app

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.navigation.NavType
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.rememberNavController
import androidx.navigation.navArgument
import com.novaengine.app.ui.editor.EditorScreen
import com.novaengine.app.ui.theme.NovaTheme
import com.novaengine.app.ui.dashboard.DashboardScreen

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            NovaTheme {
                val nav = rememberNavController()
                NavHost(navController = nav, startDestination = "dashboard") {
                    composable("dashboard") {
                        DashboardScreen(onOpenProject = { path ->
                            nav.navigate("editor/${android.net.Uri.encode(path)}")
                        })
                    }
                    composable(
                        "editor/{projectPath}",
                        arguments = listOf(navArgument("projectPath") { type = NavType.StringType })
                    ) { entry ->
                        val path = entry.arguments?.getString("projectPath").orEmpty()
                        EditorScreen(
                            projectPath = android.net.Uri.decode(path),
                        )
                    }
                }
            }
        }
    }
}
