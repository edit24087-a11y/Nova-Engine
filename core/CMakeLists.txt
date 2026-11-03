cmake_minimum_required(VERSION 3.22)
project(NovaEngineCore VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

option(NOVA_BUILD_TESTS "Build Nova Engine core tests" ON)

add_library(nova_core STATIC
    src/json.cpp
    src/util/fs.cpp
    src/scene/Node.cpp
    src/scene/SceneManager.cpp
    src/physics2d/World2D.cpp
    src/script/novascript.cpp
    src/script/novascript_compiler.cpp
    src/render/null_renderer.cpp
    src/engine/Engine.cpp
    src/audio/audio_manager.cpp
)

target_include_directories(nova_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(nova_core PRIVATE -Wall -Wextra)

if(NOVA_BUILD_TESTS)
    enable_testing()
    add_executable(nova_tests tests/test_main.cpp)
    target_link_libraries(nova_tests PRIVATE nova_core)
    target_compile_options(nova_tests PRIVATE -Wall -Wextra)
    add_test(NAME nova_core_tests COMMAND nova_tests)
endif()
