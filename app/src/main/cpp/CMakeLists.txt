# Nova Engine — native build for the Android app.
# Compiles the C++ engine core (../../../../core) plus the JNI bridge into libnova.so.
cmake_minimum_required(VERSION 3.22.1)
project(nova LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Engine core sources (explicit list — no GLOB so missing files fail loudly)
set(NOVA_CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../core)

set(NOVA_CORE_SRC
    ${NOVA_CORE_DIR}/src/json.cpp
    ${NOVA_CORE_DIR}/src/util/fs.cpp
    ${NOVA_CORE_DIR}/src/scene/Node.cpp
    ${NOVA_CORE_DIR}/src/scene/SceneManager.cpp
    ${NOVA_CORE_DIR}/src/engine/Engine.cpp
    ${NOVA_CORE_DIR}/src/physics2d/World2D.cpp
    ${NOVA_CORE_DIR}/src/script/novascript.cpp
    ${NOVA_CORE_DIR}/src/script/novascript_compiler.cpp
    ${NOVA_CORE_DIR}/src/render/null_renderer.cpp
    ${NOVA_CORE_DIR}/src/audio/audio_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/jni_bridge.cpp
)

add_library(nova SHARED ${NOVA_CORE_SRC})

target_include_directories(nova PRIVATE ${NOVA_CORE_DIR}/include)

find_library(log-lib log)

target_link_libraries(nova
    ${log-lib}
    GLESv3
    EGL
    android
)
