cmake_minimum_required(VERSION 3.24)

project(ArrowPuzzle VERSION 0.4.0 LANGUAGES CXX)

option(ARROWPUZZLE_BUILD_TESTS "Build Arrow Puzzle tests" ON)
option(ARROWPUZZLE_STATIC_RUNTIME "Use the static MSVC runtime" ON)
option(ARROWPUZZLE_RELEASE_PREVIEW "Show the project version before creating a release tag" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC AND ARROWPUZZLE_STATIC_RUNTIME)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
include(FetchContent)
find_package(Threads REQUIRED)
find_package(Git QUIET)

# Check Git on every build, including after a commit or a new release tag.
# The script preserves the header's timestamp when its contents are unchanged.
set(ARROWPUZZLE_BUILD_VERSION_HEADER
    "${CMAKE_CURRENT_BINARY_DIR}/generated/BuildVersion.hpp"
)
add_custom_target(arrow_build_version
    COMMAND
        ${CMAKE_COMMAND}
        "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
        "-DOUTPUT_FILE=${ARROWPUZZLE_BUILD_VERSION_HEADER}"
        "-DRELEASE_VERSION=${PROJECT_VERSION}"
        "-DRELEASE_PREVIEW=${ARROWPUZZLE_RELEASE_PREVIEW}"
        "-DGIT_EXECUTABLE=${GIT_EXECUTABLE}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateBuildVersion.cmake"
    BYPRODUCTS "${ARROWPUZZLE_BUILD_VERSION_HEADER}"
    COMMENT "Updating build version"
    VERBATIM
)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG c1ab645ca298a2801097931d1079b10ff7eb9df8 # raylib 5.5
    GIT_SHALLOW FALSE
)
FetchContent_MakeAvailable(raylib)

add_library(arrow_core STATIC
    src/Board.cpp
    src/Generator.cpp
    src/FrontierGenerator.cpp
    src/MotionKinematics.cpp
    src/MotionResolver.cpp
    src/Persistence.cpp
)

target_include_directories(arrow_core PUBLIC include)
target_compile_features(arrow_core PUBLIC cxx_std_20)
target_link_libraries(arrow_core PUBLIC Threads::Threads)

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

function(arrow_embed_sound filename symbol)
    set(source "${CMAKE_CURRENT_BINARY_DIR}/generated/${symbol}.cpp")
    add_custom_command(
        OUTPUT "${source}"
        COMMAND
            ${CMAKE_COMMAND}
            "-DINPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/assets/sounds/${filename}"
            "-DOUTPUT_FILE=${source}"
            "-DHEADER_NAME=AudioData.hpp"
            "-DSYMBOL_NAME=${symbol}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedBinary.cmake"
        DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/sounds/${filename}"
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedBinary.cmake"
        COMMENT "Embedding ${filename}"
        VERBATIM
    )
    set(ARROWPUZZLE_EMBEDDED_SOUND_SOURCES
        ${ARROWPUZZLE_EMBEDDED_SOUND_SOURCES} "${source}" PARENT_SCOPE)
endfunction()

arrow_embed_sound(b2-light-pistol.wav kArrowLaunchWav)
arrow_embed_sound(c3-metal-clack.wav kCollisionWav)
arrow_embed_sound(d4-retro-fail.wav kDefeatWav)
arrow_embed_sound(v3-glass-chord.wav kVictoryWav)
arrow_embed_sound(firework-burst.wav kFireworkWav)

foreach(size 16 24 32 48 64 128 256)
    set(source "${CMAKE_CURRENT_BINARY_DIR}/generated/AppIcon${size}.cpp")
    add_custom_command(
        OUTPUT "${source}"
        COMMAND
            ${CMAKE_COMMAND}
            "-DINPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/arrow-puzzle-${size}.png"
            "-DOUTPUT_FILE=${source}"
            "-DHEADER_NAME=AppIconData.hpp"
            "-DSYMBOL_NAME=kAppIcon${size}Png"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedBinary.cmake"
        DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/arrow-puzzle-${size}.png"
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedBinary.cmake"
        COMMENT "Embedding ${size}px application icon"
        VERBATIM
    )
    list(APPEND ARROWPUZZLE_EMBEDDED_ICON_SOURCES "${source}")
endforeach()

add_executable(arrow_puzzle WIN32
    "${ARROWPUZZLE_EMBEDDED_FONT_SOURCE}"
    ${ARROWPUZZLE_EMBEDDED_SOUND_SOURCES}
    ${ARROWPUZZLE_EMBEDDED_ICON_SOURCES}
    src/AppIcon.cpp
    src/AppIcon.hpp
    src/AppIconData.hpp
    src/GameAudio.cpp
    src/GameAudio.hpp
    src/EndScreen.cpp
    src/EndScreen.hpp
    src/MenuScreen.cpp
    src/MenuScreen.hpp
    src/MenuSelection.hpp
    src/AudioData.hpp
    src/BoardPalette.hpp
    src/BoardPointerGesture.hpp
    src/Game.cpp
    src/StaticBoardGeometry.cpp
    src/StaticBoardGeometry.hpp
    src/ArrowStroke.hpp
    src/BoardCoverageShader.hpp
    src/main.cpp
    src/UiFontData.hpp
    src/UiText.cpp
    src/UiText.hpp
)

if(WIN32)
    enable_language(RC)
    set(ARROWPUZZLE_WINDOWS_ICON "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/arrow-puzzle.ico")
    set(icon_resource "${CMAKE_CURRENT_BINARY_DIR}/generated/WindowsIcon.rc")
    configure_file(src/WindowsIcon.rc.in "${icon_resource}" @ONLY)
    set_source_files_properties("${icon_resource}" PROPERTIES
        OBJECT_DEPENDS "${ARROWPUZZLE_WINDOWS_ICON}")
    target_sources(arrow_puzzle PRIVATE "${icon_resource}")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(X11 REQUIRED)
    target_sources(arrow_puzzle PRIVATE src/LinuxWindow.cpp)
    target_link_libraries(arrow_puzzle PRIVATE glfw X11::X11)
endif()

add_dependencies(arrow_puzzle arrow_build_version)
target_link_libraries(arrow_puzzle PRIVATE arrow_core raylib)
target_include_directories(arrow_puzzle PRIVATE
    src
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
)

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

    # Explicit measurement tool: run each seed in a fresh process. It reports
    # generation time/peak working set separately from independent verification.
    add_executable(arrow_generator_benchmark EXCLUDE_FROM_ALL tests/GeneratorBenchmark.cpp)
    target_link_libraries(arrow_generator_benchmark PRIVATE arrow_core)
    if(WIN32)
        target_link_libraries(arrow_generator_benchmark PRIVATE psapi)
    endif()
    if(MSVC)
        target_compile_options(arrow_generator_benchmark PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(arrow_generator_benchmark PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_executable(arrow_menu_selection_tests tests/MenuSelectionTests.cpp)
    target_include_directories(arrow_menu_selection_tests PRIVATE src)
    if(MSVC)
        target_compile_options(arrow_menu_selection_tests PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(arrow_menu_selection_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME arrow_menu_selection_tests COMMAND arrow_menu_selection_tests)

    add_executable(arrow_board_pointer_tests tests/BoardPointerGestureTests.cpp)
    target_include_directories(arrow_board_pointer_tests PRIVATE src)
    if(MSVC)
        target_compile_options(arrow_board_pointer_tests PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(arrow_board_pointer_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME arrow_board_pointer_tests COMMAND arrow_board_pointer_tests)

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

    # State/timing tests need no window. An optional output directory enables
    # explicit screenshots on a machine with a graphics context.
    add_executable(arrow_end_screen_tests
        tests/EndScreenTests.cpp
        src/EndScreen.cpp
        src/UiText.cpp
        "${ARROWPUZZLE_EMBEDDED_FONT_SOURCE}"
    )
    target_include_directories(arrow_end_screen_tests PRIVATE src)
    target_link_libraries(arrow_end_screen_tests PRIVATE raylib)
    if(MSVC)
        target_compile_options(arrow_end_screen_tests PRIVATE /W4 /permissive- /utf-8)
    endif()
    add_test(NAME arrow_end_screen_tests COMMAND arrow_end_screen_tests)

    # Explicit GPU check: keep the default core tests runnable without a display.
    add_executable(arrow_render_tests EXCLUDE_FROM_ALL
        tests/RenderingTests.cpp
        src/StaticBoardGeometry.cpp
    )
    target_include_directories(arrow_render_tests PRIVATE src include)
    target_link_libraries(arrow_render_tests PRIVATE raylib)
    if(MSVC)
        target_compile_options(arrow_render_tests PRIVATE /W4 /permissive- /utf-8)
    endif()

    add_executable(arrow_ui_text_tests EXCLUDE_FROM_ALL
        tests/UiTextTests.cpp
        src/UiText.cpp
        "${ARROWPUZZLE_EMBEDDED_FONT_SOURCE}"
    )
    target_include_directories(arrow_ui_text_tests PRIVATE src)
    target_link_libraries(arrow_ui_text_tests PRIVATE raylib)
    if(MSVC)
        target_compile_options(arrow_ui_text_tests PRIVATE /W4 /permissive- /utf-8)
    endif()

    # Explicit check requiring a playback device; default core tests stay headless.
    add_executable(arrow_audio_tests EXCLUDE_FROM_ALL
        tests/AudioTests.cpp
        src/GameAudio.cpp
        ${ARROWPUZZLE_EMBEDDED_SOUND_SOURCES}
    )
    target_include_directories(arrow_audio_tests PRIVATE src)
    target_link_libraries(arrow_audio_tests PRIVATE raylib)
    if(MSVC)
        target_compile_options(arrow_audio_tests PRIVATE /W4 /permissive- /utf-8)
    endif()
endif()
