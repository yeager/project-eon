cmake_minimum_required(VERSION 3.20)
project(project_eon VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SDL3 CONFIG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(SDL3_image CONFIG QUIET)
if(NOT SDL3_image_FOUND)
    include(FetchContent)
    set(SDLIMAGE_INSTALL OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_SAMPLES OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_TESTS OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_AVIF OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_GIF OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_JPG OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_JXL OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_TIF OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_WEBP OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_XCF OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL3_image
        URL https://github.com/libsdl-org/SDL_image/releases/download/release-3.4.4/SDL3_image-3.4.4.tar.gz
        URL_HASH SHA256=29751304a13d25ac513f24305fa25b06a6edd9607718c90129b8350d35fc5573
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(SDL3_image)
endif()

add_executable(project-eon
    src/main.cpp
    src/launcher.cpp
    src/data/amiga_adf.cpp
    src/data/deuteros_amiga_bundle.cpp
    src/data/deuteros_amiga_channel_vm.cpp
    src/data/deuteros_amiga_frame.cpp
    src/data/deuteros_amiga_loader.cpp
    src/data/fat12.cpp
    src/data/sha256.cpp
    src/data/zip_archive.cpp
    src/platform/game_data.cpp
)
target_include_directories(project-eon PRIVATE src)
target_compile_definitions(project-eon PRIVATE EON_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
target_link_libraries(project-eon PRIVATE SDL3::SDL3 SDL3_image::SDL3_image ZLIB::ZLIB)

# Mirror OpenCaptive's development/package layout: launcher artwork is kept
# separate from original game data and copied beside the executable.
add_custom_command(TARGET project-eon POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:project-eon>/assets/cards"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/cards/millennium.png"
        "$<TARGET_FILE_DIR:project-eon>/assets/cards/millennium.png"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/cards/deuteros.png"
        "$<TARGET_FILE_DIR:project-eon>/assets/cards/deuteros.png"
)

if(MSVC)
    target_compile_options(project-eon PRIVATE /W4 /permissive-)
else()
    target_compile_options(project-eon PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(eon-native-tests
        tests/native_tests.cpp
        src/launcher.cpp
        src/data/amiga_adf.cpp
        src/data/deuteros_amiga_bundle.cpp
        src/data/deuteros_amiga_channel_vm.cpp
        src/data/deuteros_amiga_frame.cpp
        src/data/deuteros_amiga_loader.cpp
        src/data/fat12.cpp
        src/data/sha256.cpp
        src/data/zip_archive.cpp
        src/platform/game_data.cpp
    )
    target_include_directories(eon-native-tests PRIVATE src)
    target_link_libraries(eon-native-tests PRIVATE ZLIB::ZLIB)
    target_compile_definitions(eon-native-tests PRIVATE
        EON_REAL_DATA_DIR="${EON_REAL_DATA_DIR}"
    )
    add_test(NAME native-data-detection COMMAND eon-native-tests)
endif()
