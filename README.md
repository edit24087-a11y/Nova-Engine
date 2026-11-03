# NOVA ENGINE

A real native Android game development engine — mobile-first, dark-themed,
Unity/Godot-inspired. Written in Kotlin + Jetpack Compose with a C++17 engine
core compiled through the NDK/CMake and bridged to the app via JNI.

> This is not a game launcher or template app. The engine is a working
> project manager, scene editor, scripting environment, and Android export
> pipeline — with actual source code, real files, and verified functionality.

---

## Architecture

```
┌──────────────────────────────────────────────────────────────┐
│  Kotlin UI (Jetpack Compose)                                  │
│  Dashboard · Editor · Hierarchy · Inspector · Console · ...   │
│  SQLite project registry · Dark theme · Accent colors         │
└──────────────────────────┬───────────────────────────────────┘
                           │ JNI (libnova.so)
┌──────────────────────────▼───────────────────────────────────┐
│  C++17 Engine Core                                            │
│                                                               │
│  Engine facade ── SceneManager (scenes, undo/redo)           │
│       │               Node tree (2D/3D transforms)           │
│       ├── World2D physics (rigid bodies, collisions)         │
│       ├── NovaScript VM (bytecode compiler + VM)             │
│       │     supports func, if/elif/else, while, for, break,  │
│       │     continue, member assignment, builtins            │
│       ├── GL renderer (GL ES 3.0 backend)                   │
│       ├── JSON scene serialization (.novascene)             │
│       ├── Filesystem helpers (mkdir -p, read/write)         │
│       └── Audio manager                                       │
└───────────────────────────────────────────────────────────────┘
```

## Repository layout

```
Nova-Engine/
├── core/                        C++17 engine core (compiled via NDK)
│   ├── include/nova/            public headers
│   │   ├── engine/Engine.hpp        engine facade
│   │   ├── scene/Node.hpp           scene node (2D/3D transforms)
│   │   ├── scene/SceneManager.hpp   scene registry, undo/redo
│   │   ├── script/novascript.hpp    NovaScript VM
│   │   ├── script/novascript_compiler.hpp
│   │   ├── script/Script.hpp        script abstraction
│   │   ├── physics2d/World2D.hpp
│   │   ├── render/RenderDevice.hpp
│   │   └── util/fs.hpp
│   ├── src/                      implementations
│   └── tests/test_main.cpp       111-test suite (all passing)
├── app/                         Android application
│   ├── build.gradle.kts           Gradle config (Compose, NDK, CMake)
│   ├── proguard-rules.pro         keep JNI classes
│   └── src/main/
│       ├── AndroidManifest.xml
│       ├── cpp/
│       │   ├── CMakeLists.txt     builds libnova.so from core/
│       │   └── jni_bridge.cpp     Kotlin ↔ C++ bridge
│       ├── java/com/novaengine/app/
│       │   ├── NovaApplication.kt
│       │   ├── MainActivity.kt    dashboard ↔ editor navigation
│       │   ├── engine/NovaEngineBridge.kt   JNI wrapper
│       │   ├── data/              NovaDatabase (SQLite), ProjectStore
│       │   └── ui/
│       │       ├── theme/         dark theme + accents
│       │       ├── dashboard/     project manager screen
│       │       └── editor/        editor + panels
│       └── res/                   icons, strings, themes
├── scripts/
│   ├── build_core.sh             builds + runs the C++ test suite
│   └── export_android.sh         APK/AAB export pipeline
└── builds/                       exported APK/AAB output (gitignored)
```

## Building the engine core (headless, any machine)

```bash
./scripts/build_core.sh
# compiles core/ into core/build/headless/
# runs the full test suite: PASSED: 111  FAILED: 0
```

The core is pure C++17 (no platform dependencies beyond POSIX), so it compiles
with any g++ ≥ 9 or clang ≥ 10 on Linux/macOS.

## Building the Android app

The Android app requires the Android SDK + NDK + CMake (available on any
Android Studio installation). From the project root:

```bash
# 1. Open in Android Studio, or from command line:
gradle wrapper --gradle-version 8.7     # once
./gradlew :app:assembleDebug

# 2. Or export a game project to APK/AAB:
./scripts/export_android.sh MyGame --debug
# → builds/MyGame/apk/app-debug.apk
```

Requirements:
- Android SDK platform 34, build-tools 34
- NDK (r25+), CMake 3.22+
- JDK 17

## The NovaScript language

Embedded in the engine core — a bytecode VM with a compiler. Supports:

```novascript
func on_ready():
    self.x = self.x + 5
    for i = 1 to 10 do
        if i % 2 == 0 then
            print("even:", i)
        end
    end
end

func update(dt):
    if self.hp <= 0 then
        die()
    end
end
```

Features: optional semicolons, `:`/indentation blocks, `func` with params,
`if/elif/else`, `while`, `for i = start to end`, `break`/`continue`,
member assignment (`self.hp = 50`), host property access (`get_prop`),
builtins (`print`, `get_prop`, `set_position`, `emit_signal`, ...),
top-level vars as globals, recursion (limit 32).

## Editor

- **Dashboard**: project list (SQLite-backed), create projects, templates
- **Editor**: toolbar, hierarchy panel, GL viewport, inspector, console,
  debugger stats (FPS/bodies/draw calls), timeline, asset browser
- **Scene system**: node tree with 2D/3D transforms, undo/redo via
  CommandStack, JSON scene serialization (`.novascene` files)
- **Scripting**: attach NovaScript to nodes, run in the engine VM
- **Theme**: professional dark theme, 4 accent colors (cyan/magenta/green/amber)

## Test suite

`core/tests/test_main.cpp` — 111 tests covering:
- math (vec2/vec3/vec4/quat/mat4)
- JSON (parse/serialize/escape)
- filesystem (read/write/mkdir)
- node tree (hierarchy/transforms/find)
- scene serialization (round-trip JSON)
- undo/redo (CommandStack)
- 2D physics (rigid bodies/collisions)
- NovaScript compiler + VM (arithmetic, functions, recursion, loops,
  break/continue, member assignment, property access, host methods)
- engine loop (init, update, render, stats)

Run: `./scripts/build_core.sh`

## Export pipeline

`scripts/export_android.sh <project>` generates a complete standalone Android
Studio project under `builds/<project>/`, bundles the game project into the
APK's assets, and builds a real release APK (and AAB by default) via Gradle.
The APK contains the full engine core (libnova.so) and the game project —
install it and play.
