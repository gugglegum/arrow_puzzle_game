cmake_minimum_required(VERSION 3.24)

project(ArrowPuzzle VERSION 0.1.0 LANGUAGES CXX)

option(ARROWPUZZLE_BUILD_TESTS "Build Arrow Puzzle tests" ON)
option(ARROWPUZZLE_STATIC_RUNTIME "Use the static MSVC runtime" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC AND ARROWPUZZLE_STATIC_RUNTIME)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
include(FetchContent)
find_package(Threads REQUIRED)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(raylib)

add_library(arrow_core STATIC
    src/Board.cpp
    src/Generator.cpp
    src/MotionKinematics.cpp
    src/MotionResolver.cpp
    src/Persistence.cpp
)

target_include_directories(arrow_core PUBLIC include)
target_compile_features(arrow_core PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(arrow_core PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(arrow_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

set(ARROWPUZZLE_UI_FONT
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/Inter-Medium.ttf"
)
set(ARROWPUZZLE_EMBEDDED_FONT_SOURCE
    "${CMAKE_CURRENT_BINARY_DIR}/generated/EmbeddedUiFont.cpp"
)
add_custom_command(
    OUTPUT "${ARROWPUZZLE_EMBEDDED_FONT_SOURCE}"
    COMMAND
        ${CMAKE_COMMAND}
        "-DINPUT_FILE=${ARROWPUZZLE_UI_FONT}"
        "-DOUTPUT_FILE=${ARROWPUZZLE_EMBEDDED_FONT_SOURCE}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedUiFont.cmake"
    DEPENDS
        "${ARROWPUZZLE_UI_FONT}"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedUiFont.cmake"
    COMMENT "Embedding Inter UI font"
    VERBATIM
)

add_executable(arrow_puzzle WIN32
    "${ARROWPUZZLE_EMBEDDED_FONT_SOURCE}"
    src/Game.cpp
    src/StaticBoardGeometry.cpp
    src/StaticBoardGeometry.hpp
    src/BoardCoverageShader.hpp
    src/main.cpp
    src/UiFontData.hpp
)

target_link_libraries(arrow_puzzle PRIVATE arrow_core raylib Threads::Threads)
target_include_directories(arrow_puzzle PRIVATE src)

if(MSVC)
    target_compile_options(arrow_puzzle PRIVATE /W4 /permissive- /utf-8)
    target_link_options(arrow_puzzle PRIVATE
        "$<$<CONFIG:Release>:/SUBSYSTEM:WINDOWS>"
    )
endif()

if(ARROWPUZZLE_BUILD_TESTS)
    enable_testing()

    add_executable(arrow_tests tests/GeneratorTests.cpp)
    target_link_libraries(arrow_tests PRIVATE arrow_core)

    if(MSVC)
        target_compile_options(arrow_tests PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(arrow_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME arrow_tests COMMAND arrow_tests)

    add_executable(
        arrow_persistence_tests
        tests/PersistenceTests.cpp
    )
    target_link_libraries(arrow_persistence_tests PRIVATE arrow_core)

    if(MSVC)
        target_compile_options(
            arrow_persistence_tests
            PRIVATE /W4 /permissive- /utf-8
        )
    else()
        target_compile_options(
            arrow_persistence_tests
            PRIVATE -Wall -Wextra -Wpedantic
        )
    endif()

    add_test(
        NAME arrow_persistence_tests
        COMMAND arrow_persistence_tests
    )

    # Explicit GPU check: keep the default core tests runnable without a display.
    add_executable(arrow_render_tests EXCLUDE_FROM_ALL
        tests/RenderingTests.cpp
        src/StaticBoardGeometry.cpp
    )
    target_include_directories(arrow_render_tests PRIVATE src)
    target_link_libraries(arrow_render_tests PRIVATE raylib)
    if(MSVC)
        target_compile_options(arrow_render_tests PRIVATE /W4 /permissive- /utf-8)
    endif()
endif()
