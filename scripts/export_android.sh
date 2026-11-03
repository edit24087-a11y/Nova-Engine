#!/usr/bin/env bash
# Nova Engine — Android export pipeline.
#
# Generates a complete standalone Android Studio project for a NOVA ENGINE
# game project and builds a real release APK + AAB with Gradle.
#
# Requirements on the build machine:
#   - Android Studio (or Android SDK + NDK + CMake via command line)
#   - SDK platform 34, build-tools 34, NDK (any recent LTS), CMake 3.22+
#   - JDK 17, Gradle 8.x (wrapper included)
#
# Usage:
#   scripts/export_android.sh <project-name> [--apk-only] [--debug]
#
# Output:
#   builds/<project>/apk/app-release.apk      (or app-debug.apk with --debug)
#   builds/<project>/aab/app-release.aab      (AAB unless --apk-only)

set -euo pipefail

PROJECT_NAME="${1:?usage: export_android.sh <project-name> [--apk-only] [--debug]}"
shift || true

APK_ONLY=0
DEBUG=0
for arg in "$@"; do
    case "$arg" in
        --apk-only) APK_ONLY=1 ;;
        --debug) DEBUG=1 ;;
        *) echo "unknown flag: $arg" >&2; exit 2 ;;
    esac
done

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
APP_DIR="$ROOT_DIR/app"
EXPORT_DIR="$ROOT_DIR/builds/$PROJECT_NAME"

echo "==> Nova Engine Android export: $PROJECT_NAME"
echo "    source:  $APP_DIR"
echo "    output:  $EXPORT_DIR"

# 1. Create the standalone export project (app module + wrapper + config).
rm -rf "$EXPORT_DIR"
mkdir -p "$EXPORT_DIR"

cp -r "$APP_DIR/src" "$EXPORT_DIR/app/src" 2>/dev/null || {
    mkdir -p "$EXPORT_DIR/app"
    cp -r "$APP_DIR/src" "$EXPORT_DIR/app/src"
}
cp "$APP_DIR/build.gradle.kts" "$EXPORT_DIR/app/build.gradle.kts"
cp "$APP_DIR/proguard-rules.pro" "$EXPORT_DIR/app/proguard-rules.pro"

cp "$ROOT_DIR/settings.gradle.kts" "$EXPORT_DIR/settings.gradle.kts"
cp "$ROOT_DIR/build.gradle.kts" "$EXPORT_DIR/build.gradle.kts"
cp "$ROOT_DIR/gradle.properties" "$EXPORT_DIR/gradle.properties"

# 2. Patch the exported app to bundle the game project as raw assets.
#    This embeds project.json, scenes/, scripts/ into the APK's assets/ folder
#    so the runtime can open it directly from the device.
ASSET_DIR="$EXPORT_DIR/app/src/main/assets/NovaEngine/projects/$PROJECT_NAME"
mkdir -p "$ASSET_DIR"

# Copy the game project from the engine workspace if it exists there.
GAME_DIR="$ROOT_DIR/projects/$PROJECT_NAME"
if [ -d "$GAME_DIR" ]; then
    cp -r "$GAME_DIR"/* "$ASSET_DIR/" 2>/dev/null || true
    echo "    bundled game project from $GAME_DIR"
else
    echo "    WARNING: no project found at $GAME_DIR — runtime will use a default scene"
fi

# 3. Ensure Gradle wrapper exists (needed for `gradle` command line builds).
if [ ! -f "$EXPORT_DIR/gradlew" ]; then
    echo "==> generating Gradle wrapper (requires gradle on PATH or Android Studio)"
    if command -v gradle >/dev/null 2>&1; then
        (cd "$EXPORT_DIR" && gradle wrapper --gradle-version 8.7 --distribution-type bin)
    else
        echo "    GRADLE_WRAPPER_NEEDED: run 'gradle wrapper' in $EXPORT_DIR once," \
             "or open the folder in Android Studio (it generates the wrapper)."
    fi
fi

# 4. Build.
VARIANT="release"
TASK="assembleRelease"
if [ "$DEBUG" = "1" ]; then
    VARIANT="debug"
    TASK="assembleDebug"
fi

echo "==> building $VARIANT (this compiles the C++ core via NDK + CMake, then the app)"
if [ ! -x "$EXPORT_DIR/gradlew" ]; then
    echo ""
    echo "    SKIPPED BUILD: Gradle wrapper not available in this sandbox."
    echo "    To build on your machine:"
    echo "      cd $EXPORT_DIR"
    echo "      gradle wrapper --gradle-version 8.7   (if not present)"
    echo "      ./gradlew $TASK"
    echo ""
    echo "    Requirements: Android SDK (platform 34), NDK, CMake 3.22+, JDK 17."
    exit 0
fi

(cd "$EXPORT_DIR" && ./gradlew "$TASK" -PbuildGameProject="$PROJECT_NAME")

APK="$EXPORT_DIR/app/build/outputs/apk/$VARIANT/app-$VARIANT.apk"
if [ ! -f "$APK" ]; then
    echo "ERROR: APK not produced at $APK" >&2
    exit 1
fi

cp "$APK" "$EXPORT_DIR/apk/app-$VARIANT.apk" 2>/dev/null || \
    mkdir -p "$EXPORT_DIR/apk" && cp "$APK" "$EXPORT_DIR/apk/app-$VARIANT.apk"

echo "==> APK: $EXPORT_DIR/apk/app-$VARIANT.apk ($(du -h "$EXPORT_DIR/apk/app-$VARIANT.apk" | cut -f1))"

if [ "$APK_ONLY" = "1" ]; then
    echo "==> done (APK only)"
    exit 0
fi

BUNDLE_TASK="bundleRelease"
[ "$DEBUG" = "1" ] && BUNDLE_TASK="bundleDebug"
(cd "$EXPORT_DIR" && ./gradlew "$BUNDLE_TASK" -PbuildGameProject="$PROJECT_NAME")

AAB="$EXPORT_DIR/app/build/outputs/bundle/$VARIANT/app-$VARIANT.aab"
if [ -f "$AAB" ]; then
    mkdir -p "$EXPORT_DIR/aab"
    cp "$AAB" "$EXPORT_DIR/aab/app-$VARIANT.aab"
    echo "==> AAB: $EXPORT_DIR/aab/app-$VARIANT.aab"
fi

echo "==> export complete"
