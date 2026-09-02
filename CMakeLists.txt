cmake_minimum_required(VERSION 3.20)
project(project_eon VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# iPadOS IPAs are self-contained application bundles. Build all dependencies
# fetched below statically for that target; package-ipa.sh deliberately never
# invents or omits a Frameworks directory. Desktop targets retain their shared
# SDL runtime libraries for the existing DEB/RPM/Windows/macOS packagers.
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0" CACHE STRING "" FORCE)
endif()

find_package(SDL3 CONFIG REQUIRED)
find_package(ZLIB REQUIRED)
include(FetchContent)
find_package(SDL3_image CONFIG QUIET)
if(NOT SDL3_image_FOUND)
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
    # Project Eon loads its launcher cards through SDL_image at run time. Be
    # explicit about desktop shared libraries so their packagers can stage
    # them, while iPadOS retains its self-contained static application.
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
        set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
    endif()
    # CI supplies zlib as an installed dependency. The release tarball does
    # not include SDL_image's vendored third-party source tree, so never ask
    # CMake to use it as a fallback.
    set(SDLIMAGE_VENDORED OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL3_image
        URL https://github.com/libsdl-org/SDL_image/releases/download/release-3.4.4/SDL3_image-3.4.4.tar.gz
        URL_HASH SHA256=29751304a13d25ac513f24305fa25b06a6edd9607718c90129b8350d35fc5573
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(SDL3_image)
endif()

# SDL_ttf's source archive intentionally omits its FreeType/HarfBuzz gitlinks.
# The portable iPadOS/macOS/Windows routes fetch all three source snapshots by
# immutable object ID and SHA-256, then stage the two explicitly pinned
# dependencies in SDL_ttf's expected source tree. This avoids CMake's
# `GIT_SHALLOW` fallback to `--no-single-branch`, which fetches the whole
# SDL_ttf history when resolving a raw commit. Linux packages use their
# declared distribution dependencies and therefore do not fetch unused
# vendored copies.
# Linux packages link SDL_ttf against distribution FreeType/HarfBuzz rather
# than embedding those libraries. Desktop builds on macOS/Windows retain the
# pinned portable fallback because their packaging routes have no compatible
# system dependency contract.
if(UNIX AND NOT APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(SDLTTF_VENDORED OFF CACHE BOOL "" FORCE)
else()
    set(SDLTTF_VENDORED ON CACHE BOOL "" FORCE)
endif()
set(SDLTTF_HARFBUZZ ON CACHE BOOL "" FORCE)
set(SDLTTF_PLUTOSVG OFF CACHE BOOL "" FORCE)
set(SDLTTF_SAMPLES OFF CACHE BOOL "" FORCE)
set(SDLTTF_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3_ttf
    URL https://codeload.github.com/libsdl-org/SDL_ttf/tar.gz/a1ce3670aec736ecbf0936c43f2f0cc53aa61e5b
    URL_HASH SHA256=5024f953a5ce207e87ae73e09644ec3e5357d9907952209035b88bdaeac7edeb
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
if(SDLTTF_VENDORED)
    FetchContent_Declare(eon_sdl3_ttf_freetype
        URL https://codeload.github.com/libsdl-org/freetype/tar.gz/9973564cfa63763a3e4ac67c09147899539b1e07
        URL_HASH SHA256=026a05a49d114a1235d2926f4c03a9330e4b1a6efe7c217ec9607904c32907d4
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_Declare(eon_sdl3_ttf_harfbuzz
        URL https://codeload.github.com/libsdl-org/harfbuzz/tar.gz/564bf9818a18709776856533829c0c04950773d6
        URL_HASH SHA256=a448dd6c22d8e1e1cf39438c662251c1f97f810b8780eed4a6d6ada948c99ddc
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
endif()
# FetchContent_MakeAvailable would add SDL_ttf before its portable gitlinks
# can be staged. Populate the verified source snapshots first, then add it.
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()
FetchContent_Populate(SDL3_ttf)
if(SDLTTF_VENDORED)
    FetchContent_Populate(eon_sdl3_ttf_freetype)
    FetchContent_Populate(eon_sdl3_ttf_harfbuzz)
    file(COPY "${eon_sdl3_ttf_freetype_SOURCE_DIR}/"
        DESTINATION "${sdl3_ttf_SOURCE_DIR}/external/freetype")
    file(COPY "${eon_sdl3_ttf_harfbuzz_SOURCE_DIR}/"
        DESTINATION "${sdl3_ttf_SOURCE_DIR}/external/harfbuzz")
endif()
add_subdirectory("${sdl3_ttf_SOURCE_DIR}" "${sdl3_ttf_BINARY_DIR}" EXCLUDE_FROM_ALL)

add_executable(project-eon
    src/main.cpp
    src/display_geometry.cpp
    src/launcher_text.cpp
    src/i18n.cpp
    src/launcher.cpp
    src/presentation_preferences.cpp
    src/presentation/modern_presentation_pipeline.cpp
    src/engine/deuteros_amiga_opening.cpp
    src/engine/deuteros_amiga_opening_runner.cpp
    src/engine/menu_runtime_launch.cpp
    src/engine/native_session_controller.cpp
    src/engine/release_runtime.cpp
    src/engine/release_runtime_capability.cpp
    src/engine/runtime_session.cpp
    src/engine/deuteros_amiga_title_stage_session.cpp
    src/engine/deuteros_amiga_paula.cpp
    src/engine/deuteros_atari_bootstrap_session.cpp
    src/engine/millennium_dos_sound_selection_session.cpp
    src/engine/millennium_dos_title_session.cpp
    src/engine/millennium_dos_game_session.cpp
    src/engine/millennium_dos_gx_startup_session.cpp
    src/engine/millennium_dos_gx_startup_trace_admission.cpp
    src/engine/millennium_dos_save_session.cpp
    src/engine/millennium_atari_bootstrap_session.cpp
    src/engine/millennium_amiga_bootstrap_session.cpp
    src/data/amiga_adf.cpp
    src/data/amiga_ofs.cpp
    src/data/atari_st_prg.cpp
    src/data/atari_st_stx.cpp
    src/data/creative_voice.cpp
    src/data/deuteros_amiga_bundle.cpp
    src/data/deuteros_amiga_data_disk.cpp
    src/data/deuteros_amiga_audio.cpp
    src/data/deuteros_amiga_channel_vm.cpp
    src/data/deuteros_amiga_alternate_renderer.cpp
    src/data/deuteros_amiga_frame.cpp
    src/data/deuteros_amiga_loader.cpp
    src/data/deuteros_amiga_title_stage.cpp
    src/data/deuteros_amiga_reference_trace.cpp
    src/data/deuteros_amiga_main_stage_reference_trace.cpp
    src/data/deuteros_amiga_title_display_reference_trace.cpp
    src/data/deuteros_amiga_title_bridge_reference_trace.cpp
    src/data/deuteros_atari_boot.cpp
    src/data/deuteros_atari_reference_trace.cpp
    src/data/deuteros_atari_stage.cpp
    src/data/fat12.cpp
    src/data/millennium_dos_bitmap.cpp
    src/data/millennium_control_text.cpp
    src/data/millennium_dos_game_data.cpp
    src/data/millennium_dos_game_flow.cpp
    src/data/millennium_dos_gameplay_screen.cpp
    src/data/millennium_dos_gx_catalog.cpp
    src/data/millennium_dos_last_screen.cpp
    src/data/millennium_save_comparison.cpp
    src/data/modern_asset_pack.cpp
    src/data/modern_pixel_reconstruction.cpp
    src/data/millennium_dos_lib.cpp
    src/data/millennium_dos_title_flow.cpp
    src/data/millennium_dos_title_exit.cpp
    src/data/millennium_dos_title_transition.cpp
    src/data/millennium_dos_title_presentation.cpp
    src/data/millennium_dos_video_driver.cpp
    src/data/millennium_dos_sound_driver.cpp
    src/data/millennium_dos_reference_trace.cpp
    src/data/millennium_amiga_reference_trace.cpp
    src/data/millennium_amiga_loader.cpp
    src/data/reference_trace.cpp
    src/data/reference_trace_registry.cpp
    src/data/function_map.cpp
    src/data/recovery_map.cpp
    src/data/runtime_diagnostics.cpp
    src/data/release_manifest.cpp
    src/data/static_control_flow.cpp
    src/data/startup_boundary.cpp
    src/data/sha256.cpp
    src/data/zip_archive.cpp
    src/platform/game_data.cpp
    src/platform/platform_coverage.cpp
)
target_include_directories(project-eon PRIVATE src)
target_compile_definitions(project-eon PRIVATE
    EON_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
    EON_FONT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts"
    EON_LOCALE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/po"
    EON_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(project-eon PRIVATE SDL3::SDL3 SDL3_image::SDL3_image SDL3_ttf::SDL3_ttf ZLIB::ZLIB)

# The native preservation suite exercises every runtime module except the SDL
# entry point.  Derive that module set from the shipped target instead of
# maintaining a second hand-written source list: a new recovered parser or
# session cannot be linked into the application while silently escaping the
# native test binary.  Capture this before iPad-only bundle resources are
# added below, since those are application packaging inputs, not C++ modules.
get_target_property(EON_RUNTIME_SOURCES project-eon SOURCES)
list(REMOVE_ITEM EON_RUNTIME_SOURCES src/main.cpp)

# iPadOS uses the same SDL3 code as the desktop builds, with Project Eon's
# own cards and catalogs embedded as resources. Supplied game archives are
# deliberately not copied into the application bundle or IPA.
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set_target_properties(project-eon PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "Project Eon"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.yeager.projecteon"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/packaging/ios/Info.plist.in"
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "2"
        XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "15.0"
        XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"
    )
    foreach(resource IN ITEMS
        assets/cards/millennium.png
        assets/cards/deuteros.png
        assets/cards/dos-platform-v1.png
        assets/cards/amiga-platform-v1.png
        assets/cards/atari-st-platform-v1.png
        assets/cards/original-profile-v1.png
        assets/cards/modern-profile-v1.png
        assets/cards/custom-profile-v1.png)
        set_source_files_properties("${resource}" PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources/assets/cards")
        target_sources(project-eon PRIVATE "${resource}")
    endforeach()
    set_source_files_properties("assets/branding/project-eon-logo-v1.png" PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources/assets/branding")
    target_sources(project-eon PRIVATE "assets/branding/project-eon-logo-v1.png")
    # The UI bundle is licensed separately from original game media. Keep the
    # exact reviewed font files in the app resource directory so iPadOS never
    # resolves text through a host font installation.
    foreach(font IN ITEMS
        assets/fonts/NotoSans-Regular.ttf
        assets/fonts/NotoSansArabic-Regular.ttf
        assets/fonts/NotoSansDevanagari-Regular.ttf
        assets/fonts/NotoSansJP-Regular.otf
        assets/fonts/NotoSansKR-Regular.otf
        assets/fonts/NotoSansSC-Regular.otf
        assets/fonts/OFL-1.1.txt
        assets/fonts/README.md)
        set_source_files_properties("${font}" PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources/assets/fonts")
        target_sources(project-eon PRIVATE "${font}")
    endforeach()
    foreach(catalog IN ITEMS ar de el en_GB es fi fr hi it ja ko nl no pl pt_BR ru sv tr uk zh_CN)
        set_source_files_properties("po/${catalog}.po" PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources/po")
        target_sources(project-eon PRIVATE "po/${catalog}.po")
    endforeach()
endif()

# Mirror OpenCaptive's development/package layout: launcher artwork is kept
# separate from original game data and copied beside the executable.
add_custom_command(TARGET project-eon POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:project-eon>/assets/cards"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/cards"
        "$<TARGET_FILE_DIR:project-eon>/assets/cards"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/branding"
        "$<TARGET_FILE_DIR:project-eon>/assets/branding"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts"
        "$<TARGET_FILE_DIR:project-eon>/assets/fonts"
)

if(MSVC)
    target_compile_options(project-eon PRIVATE /W4 /permissive-)
else()
    target_compile_options(project-eon PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()

# Embed the reviewed Project Eon mark in the native Windows executable. The
# resource is launcher artwork, not an original game asset or data container.
if(WIN32)
    target_sources(project-eon PRIVATE packaging/windows/project-eon.rc)
endif()

# GCC 15 can crash internally while compiling a small set of deliberately
# large provenance/report and parser translation units as PIE. Keep the
# workaround narrowly scoped to the observed sources; the executable remains
# PIE-hardened and the rest of the runtime retains the configured optimisation
# level. This is a compiler reliability guard, not a change to recovered game
# behaviour.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set_source_files_properties(
        src/main.cpp
        src/launcher.cpp
        src/data/millennium_dos_game_flow.cpp
        src/engine/deuteros_atari_bootstrap_session.cpp
        PROPERTIES COMPILE_OPTIONS "-O1")
    # The monolithic preservation corpus test hits a distinct GCC 15 RTL
    # optimiser crash even at -O1. It is test-only code, so disable
    # optimisation for this one translation unit rather than weakening the
    # executable or recovered parsers.
    set_source_files_properties(tests/native_tests.cpp PROPERTIES COMPILE_OPTIONS "-O0")
endif()

include(CTest)
if(BUILD_TESTING)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    # An externally retained checkpoint can join a developer's CTest run only
    # through an explicit absolute directory. It remains opaque to the native
    # tests and is never copied into the build tree or a package.
    set(EON_REPLAY_FIXTURE_DIR "" CACHE PATH
        "Absolute external Project Eon replay-fixture directory to verify with CTest")
    # This corpus test deliberately requires every canonical archive-layout
    # release, including both Millennium DOS languages. Installed direct data
    # is a distinct input shape with its own focused test below.
    set(EON_REAL_DATA_DIR "" CACHE PATH
        "Absolute original-media root containing the complete six-release canonical archive corpus")
    set(EON_DIRECT_DATA_DIR "" CACHE PATH
        "Absolute original-media root containing recognised installed direct media")
    add_executable(eon-native-tests
        tests/native_tests.cpp
        ${EON_RUNTIME_SOURCES}
    )
    target_include_directories(eon-native-tests PRIVATE src)
    target_link_libraries(eon-native-tests PRIVATE SDL3::SDL3 SDL3_ttf::SDL3_ttf ZLIB::ZLIB)
    target_compile_definitions(eon-native-tests PRIVATE
        EON_LOCALE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/po"
        EON_REAL_DATA_DIR="${EON_REAL_DATA_DIR}"
    )
    # Native tests deliberately use assert() for their byte-exact preservation
    # contracts. Keep those assertions enabled even when the application is
    # built as Release, where CMake otherwise defines NDEBUG globally.
    if(MSVC)
        target_compile_options(eon-native-tests PRIVATE /UNDEBUG)
    else()
        target_compile_options(eon-native-tests PRIVATE -UNDEBUG)
        # GCC 15 on aarch64 can ICE during RTL expansion for the deliberately
        # large native preservation test translation unit when PIE codegen is
        # enabled. The installed runtime remains PIE-hardened; this applies
        # only to the local, non-distributed test executable.
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_compile_options(eon-native-tests PRIVATE -fno-pie)
            target_link_options(eon-native-tests PRIVATE -no-pie)
        endif()
    endif()
    # Native fixtures never use the system temporary directory. This protects
    # constrained preservation workstations and keeps test artefacts outside
    # both the checkout and user-supplied game media.
    # Test scratch remains outside the checkout and all original-media roots.
    # Callers may provide an explicit external path; otherwise select the
    # platform's user cache rather than a CMake build directory or /tmp.
    if(DEFINED ENV{EON_TEST_TMPDIR} AND NOT "$ENV{EON_TEST_TMPDIR}" STREQUAL "")
        set(EON_TEST_TMPDIR "$ENV{EON_TEST_TMPDIR}")
    elseif(WIN32 AND DEFINED ENV{LOCALAPPDATA} AND NOT "$ENV{LOCALAPPDATA}" STREQUAL "")
        set(EON_TEST_TMPDIR "$ENV{LOCALAPPDATA}/ProjectEon/tests")
    elseif(DEFINED ENV{HOME} AND NOT "$ENV{HOME}" STREQUAL "")
        set(EON_TEST_TMPDIR "$ENV{HOME}/.cache/project-eon-tools/tests")
    else()
        message(FATAL_ERROR "Set EON_TEST_TMPDIR to an external test scratch directory")
    endif()
    file(MAKE_DIRECTORY "${EON_TEST_TMPDIR}")
    add_test(NAME native-data-detection COMMAND eon-native-tests)
    set_tests_properties(native-data-detection PROPERTIES
        ENVIRONMENT "EON_TEST_TMPDIR=${EON_TEST_TMPDIR}")
    add_test(NAME cli-default-data-read-only
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_default_data_test.py"
            "$<TARGET_FILE:project-eon>")
    set_tests_properties(cli-default-data-read-only PROPERTIES
        ENVIRONMENT "EON_TEST_TMPDIR=${EON_TEST_TMPDIR}")
    if(EON_REPLAY_FIXTURE_DIR)
        add_test(NAME verify-external-replay-fixture
            COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/verify_replay_fixture.py"
                --fixture "${EON_REPLAY_FIXTURE_DIR}")
    endif()
    # Commercial game data is never available in CI. A developer build that
    # supplies the complete canonical archive corpus receives a separate
    # black-box launch test for every supported platform path. Do not point
    # this at an installed direct-data directory: that has a focused test.
    if(EON_REAL_DATA_DIR)
        # Keep test-only manifests and fixture media out of /tmp as well as
        # out of user-supplied original media. The test script rejects a
        # missing value rather than falling back to the system temp directory.
        add_test(NAME cli-real-platform-starts
            COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_launch_test.py"
                "$<TARGET_FILE:project-eon>"
                "${EON_REAL_DATA_DIR}")
        set_tests_properties(cli-real-platform-starts PROPERTIES
            ENVIRONMENT "EON_TEST_TMPDIR=${EON_TEST_TMPDIR}"
            # This black-box scan opens every supplied archive and exercises
            # each admitted platform.  It normally completes in under a
            # minute; keep enough room for slow physical media while turning
            # a future scanner or launch-admission hang into a useful failure.
            TIMEOUT 180)
    endif()
    if(EON_DIRECT_DATA_DIR)
        add_test(NAME cli-real-direct-directory-start
            COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_direct_directory_test.py"
                "$<TARGET_FILE:project-eon>"
                "${EON_DIRECT_DATA_DIR}")
        set_tests_properties(cli-real-direct-directory-start PROPERTIES
            ENVIRONMENT "EON_TEST_TMPDIR=${EON_TEST_TMPDIR}"
            TIMEOUT 180)
    endif()
endif()

# Distribution packages deliberately contain no commercial game media. The
# executable creates/uses its data directory and reads user-supplied archives
# and disk images in place through the verified scanners.
include(GNUInstallDirs)
# Do not inherit a developer workstation's umask into a distributable DEB or
# RPM.  In particular, a group-writable `/usr/share` tree is rejected by
# Debian-quality tooling and is not an appropriate permission contract for a
# read-only launcher resource bundle.  This affects only staging directories;
# it never changes user-supplied original media.
set(CMAKE_INSTALL_DEFAULT_DIRECTORY_PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE)
# Apple bundle targets require a BUNDLE destination.  Keep it separate from
# the desktop runtime rule so an iPadOS cross-build can configure and stage
# ProjectEon.app before the unsigned IPA packaging guard examines it.
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    install(TARGETS project-eon BUNDLE DESTINATION ".")
else()
    install(TARGETS project-eon RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif()
# Linux CI builds SDL3 and SDL_image from source, outside the distribution's
# package database.  Ship those two runtime libraries with the executable so
# DEB/RPM artifacts remain launchable without proprietary media or a
# workstation-local /usr/local SDL install.
if(UNIX AND NOT APPLE)
    # Keep bundled runtime libraries out of /usr/bin.  Their package-private
    # directory is both a normal Linux layout and an explicit loader boundary.
    set_target_properties(project-eon PROPERTIES
        INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}/project-eon")
    install(FILES
        "$<TARGET_FILE:SDL3::SDL3>"
        "$<TARGET_SONAME_FILE:SDL3::SDL3>"
        "$<TARGET_FILE:SDL3_image::SDL3_image>"
        "$<TARGET_SONAME_FILE:SDL3_image::SDL3_image>"
        "$<TARGET_FILE:SDL3_ttf::SDL3_ttf>"
        "$<TARGET_SONAME_FILE:SDL3_ttf::SDL3_ttf>"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/project-eon")
    # Desktop packages must expose the media-free card launcher through the
    # host application menu. The entry runs only Project Eon; it names no
    # original archive, data path, or mutable save location.
    install(FILES packaging/project-eon.desktop
        DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
    install(FILES assets/branding/project-eon-logo-v1.png
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps"
        RENAME "project-eon.png")
endif()
if(UNIX AND NOT APPLE)
    # Installed Linux applications use shared data, never /usr/bin/assets.
    install(DIRECTORY assets/cards
        DESTINATION "${CMAKE_INSTALL_DATADIR}/project-eon/assets"
        DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
    install(FILES assets/branding/project-eon-logo-v1.png
        DESTINATION "${CMAKE_INSTALL_DATADIR}/project-eon/assets/branding")
    install(DIRECTORY assets/fonts
        DESTINATION "${CMAKE_INSTALL_DATADIR}/project-eon/assets"
        DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
else()
    install(DIRECTORY assets/cards DESTINATION "${CMAKE_INSTALL_BINDIR}/assets"
        DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
    install(FILES assets/branding/project-eon-logo-v1.png
        DESTINATION "${CMAKE_INSTALL_BINDIR}/assets/branding")
    install(DIRECTORY assets/fonts DESTINATION "${CMAKE_INSTALL_BINDIR}/assets"
        DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
endif()
install(DIRECTORY po/ DESTINATION "${CMAKE_INSTALL_DATADIR}/project-eon/po"
    DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

# PO files are read directly by the launcher, so a package can otherwise carry
# a syntactically damaged or stale translation without the compiler noticing.
# Keep validation opt-in and non-generating: it writes temporary .mo probes
# only beneath the build tree and never rewrites a translator's source catalog.
find_program(EON_MSGFMT_EXECUTABLE NAMES msgfmt)
find_program(EON_MSGCMP_EXECUTABLE NAMES msgcmp)
if(EON_MSGFMT_EXECUTABLE AND EON_MSGCMP_EXECUTABLE)
    add_custom_target(l10n
        COMMAND "${CMAKE_COMMAND}"
            -DPO_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}/po"
            -DOUTPUT_DIRECTORY="${CMAKE_CURRENT_BINARY_DIR}/eon-l10n"
            -DMSGFMT_EXECUTABLE="${EON_MSGFMT_EXECUTABLE}"
            -DMSGCMP_EXECUTABLE="${EON_MSGCMP_EXECUTABLE}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_l10n.cmake"
        COMMENT "Validating Project Eon's 20 launcher translation catalogs")
else()
    add_custom_target(l10n
        COMMAND "${CMAKE_COMMAND}" -E echo
            "l10n validation requires gettext tools msgfmt and msgcmp"
        COMMAND "${CMAKE_COMMAND}" -E false
        COMMENT "Cannot validate Project Eon translation catalogs")
endif()

install(FILES README.md
    DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/project-eon")
install(FILES packaging/debian/copyright
    DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/project-eon"
    RENAME copyright)
install(FILES packaging/debian/changelog.Debian
    DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/project-eon")
install(FILES packaging/debian/project-eon.lintian-overrides
    DESTINATION "${CMAKE_INSTALL_DATADIR}/lintian/overrides"
    RENAME project-eon)
if(UNIX AND NOT APPLE)
    install(FILES docs/project-eon.6
        DESTINATION "${CMAKE_INSTALL_DATADIR}/man/man6")
    # Debian policy expects packaged changelogs and manual pages compressed.
    # This runs only against CMake's package staging root, never user media.
    # Keep CMAKE_INSTALL_PREFIX unresolved until CPack executes the install;
    # a developer configure may use /usr/local while a DEB package stages /usr.
    install(CODE [[
execute_process(
  COMMAND gzip -9n -f
    "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/share/doc/project-eon/changelog.Debian"
    "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/share/man/man6/project-eon.6"
  RESULT_VARIABLE eon_gzip_result)
if(NOT eon_gzip_result EQUAL 0)
  message(FATAL_ERROR "Unable to compress Debian policy documentation")
endif()
]])
endif()

if(UNIX AND NOT APPLE)
    # CPack stages SDL runtime libraries built against the CI's /usr/local
    # SDL3 prefix. Rewrite that build-host search path to the package-private
    # directory and strip only staged executable/private libraries before
    # packaging. The application retains its explicit package-relative
    # RUNPATH; neither operation opens or changes user-supplied media.
    #
    # CMAKE_INSTALL_LIBDIR can be multiarch (for example
    # lib/x86_64-linux-gnu), so resolve it while configuring rather than with
    # a lib* glob that misses the staged runtime payload.
    set(EON_INSTALL_LIBDIR "${CMAKE_INSTALL_LIBDIR}")
    set(EON_INSTALL_BINDIR "${CMAKE_INSTALL_BINDIR}")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/package_linux_runtime.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/package_linux_runtime.cmake"
        @ONLY)
    install(SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/package_linux_runtime.cmake")
endif()

set(CPACK_PACKAGE_NAME "project-eon")
set(CPACK_PACKAGE_VENDOR "Project Eon contributors")
set(CPACK_PACKAGE_CONTACT "https://github.com/yeager/project-eon")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "SDL3 reimplementation project for Millennium 2.2 and Deuteros")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
# The launcher is exposed as a Game/Simulation desktop application and the
# RPM metadata carries the equivalent Amusements/Games classification.  Keep
# the Debian control field aligned with that user-facing package identity;
# ``utils`` would incorrectly classify it in Debian archives and mentors.
set(CPACK_DEBIAN_PACKAGE_SECTION "games")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER
    "Daniel Nylander <1206564+yeager@users.noreply.github.com>")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/yeager/project-eon")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION
    "Preservation-first SDL3 launcher for Millennium 2.2 and Deuteros.\n It reads user-supplied original media in place.\n It never distributes game data.")
set(CPACK_DEBIAN_PACKAGE_RELEASE "1")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
# DEB/RPM artifacts install below /usr, independent of the developer's local
# CMAKE_INSTALL_PREFIX.  SDL3, SDL_image, and SDL_ttf are private payload
# libraries in that directory; tell dpkg-shlibdeps where to find them so it
# does not replace them with a host libsdl3 dependency.  It can then generate
# the real system-library dependencies required by a fresh installation.
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS_PRIVATE_DIRS
    "${CPACK_PACKAGING_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/project-eon")
set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RPM_PACKAGE_GROUP "Amusements/Games")
set(CPACK_RPM_PACKAGE_URL "https://github.com/yeager/project-eon")
set(CPACK_RPM_PACKAGE_DESCRIPTION
    "Preservation-first SDL3 reimplementation launcher for Millennium 2.2 and Deuteros. Project Eon reads user-supplied original media in place and never distributes game data.")
set(CPACK_RPM_PACKAGE_RELEASE "1")
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
include(CPack)
