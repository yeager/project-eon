"""Static contract for the Linux artifact verifier used in CI."""

from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess
import textwrap
import unittest

from eon_test_paths import temporary_directory


ROOT = Path(__file__).resolve().parents[1]
VERIFIER = ROOT / "packaging" / "verify-desktop-package.sh"
POLICY_VERIFIER = ROOT / "packaging" / "verify-distribution-policy.sh"
MACOS_CLOSURE_VERIFIER = ROOT / "packaging" / "macos" / "verify-dylib-closure.sh"
APPIMAGE_BUILDER = ROOT / "packaging" / "appimage" / "build-appimage.sh"
APPIMAGE_VERIFIER = ROOT / "packaging" / "appimage" / "verify-appimage.sh"
APPIMAGE_RUNNER = ROOT / "packaging" / "appimage" / "AppRun"
WORKFLOW = ROOT / ".github" / "workflows" / "build.yml"


class DesktopPackagingTests(unittest.TestCase):
    def test_verifier_is_valid_shell_and_rejects_media_extensions(self) -> None:
        # The script itself is exercised by the Linux packaging job. Windows
        # still checks the preservation contract below, but has no POSIX shell
        # runtime to parse it with.
        if os.name != "nt":
            subprocess.run(["bash", "-n", str(VERIFIER)], check=True)
            subprocess.run(["bash", "-n", str(POLICY_VERIFIER)], check=True)
            subprocess.run(["bash", "-n", str(APPIMAGE_BUILDER)], check=True)
            subprocess.run(["bash", "-n", str(APPIMAGE_VERIFIER)], check=True)
        source = VERIFIER.read_text(encoding="utf-8")
        for extension in ("zip", "adf", "st", "msa", "stx", "img", "hfe", "ipf", "scp",
                          "ctr", "lha", "lzh", "lzx", "exe", "com"):
            with self.subTest(extension=extension):
                self.assertIn(extension, source)
        self.assertIn("share/project-eon/assets/cards/millennium.png", source)
        self.assertIn("share/project-eon/assets/branding/project-eon-logo-v1.png", source)
        self.assertIn("Icon=project-eon", source)
        self.assertIn("NotoSansSC-Regular.otf", source)
        self.assertIn("OFL-1.1.txt", source)
        self.assertIn('for catalog in ar de el en_GB es fi fr hi it ja ko nl no pl pt_BR ru sv tr uk zh_CN', source)
        self.assertIn("localization catalog", source)
        self.assertIn("generated Debian runtime dependencies", source)
        self.assertIn("private runtime", source)
        self.assertIn("LD_TRACE_LOADED_OBJECTS=1", source)
        self.assertIn("generated RPM runtime dependencies", source)
        self.assertIn("rpm2cpio", source)
        self.assertIn('rpm_query=(rpm --dbpath "$rpm_database")', source)
        self.assertIn("workstation's package-manager state", source)
        self.assertIn("package-relative runpath", source)
        linux_runtime_script = (ROOT / "cmake" / "package_linux_runtime.cmake.in").read_text(encoding="utf-8")
        self.assertIn("Linux packaging requires patchelf", linux_runtime_script)
        self.assertIn("Linux packaging requires strip", linux_runtime_script)
        self.assertIn('COMMAND "${eon_patchelf_executable}"', linux_runtime_script)
        self.assertIn('HOME="$isolated_home" "$executable" --inspect', source)
        self.assertIn("created its default game-data directory during lookup", source)
        self.assertIn("isolated missing default game-data path", source)
        self.assertIn("EON_PACKAGE_TEST_TMPDIR", source)
        self.assertIn("project-eon-tools/package-validation", source)
        self.assertIn('mktemp -d "$scratch_root/eon-package.XXXXXXXX"', source)
        self.assertIn("outside /tmp", source)
        self.assertNotIn("temporary=$(mktemp -d)", source)
        self.assertNotIn("isolated_home=$(mktemp -d)", source)
        workflow = WORKFLOW.read_text(encoding="utf-8")
        self.assertIn("rpm cpio desktop-file-utils lintian rpmlint zlib1g-dev", workflow)

    def test_appimage_route_is_pinned_media_free_and_manifested(self) -> None:
        builder = APPIMAGE_BUILDER.read_text(encoding="utf-8")
        verifier = APPIMAGE_VERIFIER.read_text(encoding="utf-8")
        runner = APPIMAGE_RUNNER.read_text(encoding="utf-8")
        for source in (builder, verifier):
            self.assertIn("outside /tmp", source)
            self.assertIn("prohibited original-media", source)
            self.assertIn(".adf", source)
            self.assertIn(".stx", source)
            self.assertIn(".exe", source)
        self.assertIn("--runtime-file", builder)
        self.assertIn("cmake --install", builder)
        self.assertIn("libSDL3.so.*", builder)
        self.assertIn("libSDL3.so.0", builder)
        self.assertIn('find "$appdir/usr/lib" -type l', builder)
        self.assertIn("APPIMAGE_EXTRACT_AND_RUN=1", builder)
        self.assertIn("AppImage created its default game-data directory during lookup", verifier)
        self.assertIn("--appimage-extract", verifier)
        self.assertIn("libSDL3.so.*", verifier)
        self.assertIn("libSDL3.so.0", verifier)
        self.assertIn('find "$appdir/usr/lib" -type l', verifier)
        self.assertIn("LD_TRACE_LOADED_OBJECTS=1", verifier)
        self.assertIn('"$appdir/usr/bin/project-eon"', verifier)
        self.assertNotIn('LD_TRACE_LOADED_OBJECTS=1 "$appdir/AppRun"', verifier)
        self.assertIn("does not resolve $library from its installed private runtime", verifier)
        self.assertIn("dynamic-library closure leaks into /usr/local", verifier)
        self.assertIn("unresolved dynamic-library dependency", verifier)
        self.assertIn('exec "$appdir/usr/bin/project-eon" "$@"', runner)
        workflow = WORKFLOW.read_text(encoding="utf-8")
        self.assertIn("Fetch pinned AppImage build tools", workflow)
        self.assertIn("a6d71e2b6cd66f8e8d16c37ad164658985e0cf5fcaa950c90a482890cb9d13e0", workflow)
        self.assertIn("1cc49bcf1e2ccd593c379adb17c9f85a36d619088296504de95b1d06215aebbf", workflow)
        self.assertIn("sha256sum --check --strict", workflow)
        self.assertIn("Verify AppImage contents contain no game media", workflow)
        self.assertIn("package/appimage/*.AppImage", workflow)

    @unittest.skipIf(os.name == "nt", "the AppImage verifier requires ELF loader tracing")
    def test_appimage_verifier_proves_the_extracted_private_runtime_closure(self) -> None:
        """Exercise the loader-closure path without an AppImage build tool.

        The fixture is deliberately an ELF program with the three required
        private SONAMEs and a package-relative RPATH.  A small script mimics
        AppImage extraction, allowing the production verifier to inspect the
        same extracted layout it receives in CI while keeping all generated
        bytes under Eon's external test cache.
        """
        compiler = shutil.which("cc")
        if compiler is None:
            self.skipTest("C compiler is unavailable")

        with temporary_directory() as temporary:
            root = Path(temporary)
            source_appdir = root / "source-appdir"
            binary_dir = source_appdir / "usr" / "bin"
            library_dir = source_appdir / "usr" / "lib"
            binary_dir.mkdir(parents=True)
            library_dir.mkdir()

            libraries = (
                ("SDL3", "eon_sdl3"),
                ("SDL3_image", "eon_sdl3_image"),
                ("SDL3_ttf", "eon_sdl3_ttf"),
            )
            for library, symbol in libraries:
                source = root / f"{library}.c"
                source.write_text(f"void {symbol}(void) {{}}\n", encoding="utf-8")
                runtime_library = library_dir / f"lib{library}.so.0.0"
                subprocess.run(
                    [
                        compiler,
                        "-shared",
                        "-fPIC",
                        f"-Wl,-soname,lib{library}.so.0",
                        "-o",
                        str(runtime_library),
                        str(source),
                    ],
                    check=True,
                )
                (library_dir / f"lib{library}.so.0").symlink_to(runtime_library.name)

            executable_source = root / "project-eon.c"
            executable_source.write_text(
                textwrap.dedent(
                    """\
                    #include <stdio.h>
                    #include <stdlib.h>
                    #include <string.h>
                    void eon_sdl3(void);
                    void eon_sdl3_image(void);
                    void eon_sdl3_ttf(void);
                    int main(int argc, char **argv) {
                      eon_sdl3(); eon_sdl3_image(); eon_sdl3_ttf();
                      if (argc > 1 && strcmp(argv[1], "--help") == 0) {
                        puts("Usage:"); return 0;
                      }
                      if (argc > 1 && strcmp(argv[1], "--inspect") == 0) {
                        printf("Data path does not exist: \\\"%s/.projecteon\\\"\\n", getenv("HOME"));
                        return 2;
                      }
                      return 0;
                    }
                    """
                ),
                encoding="utf-8",
            )
            subprocess.run(
                [
                    compiler,
                    "-o",
                    str(binary_dir / "project-eon"),
                    str(executable_source),
                    "-Wl,-rpath,$ORIGIN/../lib",
                    str(library_dir / "libSDL3.so.0"),
                    str(library_dir / "libSDL3_image.so.0"),
                    str(library_dir / "libSDL3_ttf.so.0"),
                ],
                check=True,
            )
            apprun = source_appdir / "AppRun"
            apprun.write_text(
                "#!/usr/bin/env sh\nexec \"$(dirname -- \"$0\")/usr/bin/project-eon\" \"$@\"\n",
                encoding="utf-8",
            )
            apprun.chmod(0o755)
            for relative in (
                "project-eon.desktop",
                "project-eon.png",
                "usr/share/project-eon/assets/cards/millennium.png",
                "usr/share/project-eon/assets/branding/project-eon-logo-v1.png",
                "usr/share/project-eon/assets/fonts/NotoSans-Regular.ttf",
                "usr/share/project-eon/po/sv.po",
            ):
                target = source_appdir / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                target.touch()

            image = root / "Project-Eon.AppImage"
            image.write_text(
                "#!/usr/bin/env bash\nset -eu\n"
                "test \"${1:-}\" = --appimage-extract\n"
                f"cp -a -- {source_appdir!s} squashfs-root\n",
                encoding="utf-8",
            )
            image.chmod(0o755)
            environment = os.environ | {"EON_PACKAGE_TEST_TMPDIR": str(root / "scratch")}
            result = subprocess.run(
                ["bash", str(APPIMAGE_VERIFIER), str(image)],
                text=True,
                capture_output=True,
                env=environment,
            )
            self.assertEqual(result.returncode, 0, result.stderr)

    def test_linux_packaging_job_runs_the_artifact_verifier(self) -> None:
        workflow = WORKFLOW.read_text(encoding="utf-8")
        self.assertIn("Verify package contents contain no game media", workflow)
        self.assertIn("bash packaging/verify-desktop-package.sh package/deb/*.deb package/rpm/*.rpm", workflow)
        self.assertIn("Enforce Debian mentors and RPM package policy", workflow)
        self.assertIn("bash packaging/verify-distribution-policy.sh package/deb/*.deb package/rpm/*.rpm", workflow)

    def test_distribution_policy_gate_uses_debian_and_rpm_native_tools(self) -> None:
        source = POLICY_VERIFIER.read_text(encoding="utf-8")
        self.assertIn("lintian --profile debian --pedantic --fail-on warning", source)
        self.assertIn("require_755_directories", source)
        self.assertIn("dpkg-deb -c", source)
        self.assertIn("rpm -qplv", source)
        self.assertIn("directories that are not 0755", source)
        self.assertIn("rpmspec --parse", source)
        self.assertIn("rpmlint --strict", source)
        self.assertIn("rpm --checksig --nogpg", source)

    def test_debian_package_generates_system_dependencies_without_host_sdl(self) -> None:
        cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn('set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")', cmake)
        self.assertIn("set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)", cmake)
        self.assertIn("CPACK_DEBIAN_PACKAGE_SHLIBDEPS_PRIVATE_DIRS", cmake)
        self.assertIn("CPACK_DEBIAN_PACKAGE_MAINTAINER", cmake)
        self.assertIn("CPACK_DEBIAN_FILE_NAME DEB-DEFAULT", cmake)
        self.assertIn('set(CPACK_DEBIAN_PACKAGE_SECTION "utils")', cmake)
        self.assertIn("SDLTTF_VENDORED OFF", cmake)
        self.assertIn("if(SDLTTF_VENDORED)", cmake)
        runtime_script = (ROOT / "cmake" / "package_linux_runtime.cmake.in").read_text(
            encoding="utf-8"
        )
        self.assertIn("package_linux_runtime.cmake.in", cmake)
        self.assertIn("@EON_INSTALL_LIBDIR@/project-eon", runtime_script)
        self.assertIn('COMMAND "${eon_patchelf_executable}" --set-rpath "$ORIGIN"', runtime_script)
        self.assertIn("--strip-unneeded", runtime_script)
        self.assertIn("project-eon.lintian-overrides", cmake)
        self.assertIn("CPACK_RPM_PACKAGE_URL", cmake)
        self.assertIn("CPACK_RPM_FILE_NAME RPM-DEFAULT", cmake)
        self.assertIn("CMAKE_INSTALL_DEFAULT_DIRECTORY_PERMISSIONS", cmake)
        self.assertIn("DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE", cmake)

    def test_ci_validates_macos_archive_and_windows_runtime_stage(self) -> None:
        workflow = WORKFLOW.read_text(encoding="utf-8")
        self.assertIn('-DCMAKE_OSX_ARCHITECTURES=${{ matrix.arch }}', workflow)
        self.assertIn('built_architectures=$(lipo -archs "$APP/Contents/MacOS/ProjectEon")', workflow)
        self.assertIn('macOS bundle architecture mismatch: expected ${{ matrix.arch }}', workflow)
        self.assertIn('unzip -t "project-eon-macos-${{ matrix.arch }}.zip"', workflow)
        self.assertIn('cp -R assets/fonts "$APP/Contents/MacOS/assets/fonts"', workflow)
        self.assertIn('cp assets/branding/project-eon.icns "$APP/Contents/Resources/project-eon.icns"', workflow)
        self.assertIn('CFBundleIconFile', workflow)
        self.assertIn('Copy-Item assets/fonts dist/assets/fonts -Recurse', workflow)
        self.assertIn('Copy-Item assets/branding/* dist/assets/branding/', workflow)
        self.assertIn("refusing macOS artifact with possible original game data", workflow)
        self.assertIn("macOS bundle unexpectedly inspected missing default game data", workflow)
        self.assertIn('HOME="$isolated_home" "$APP/Contents/MacOS/ProjectEon" --inspect', workflow)
        self.assertIn("macOS bundle created its default game-data directory during lookup", workflow)
        self.assertIn("project-eon-tools/macos-package-validation", workflow)
        self.assertIn('mktemp -d "$package_cache/eon-macos-package.XXXXXXXX"', workflow)
        self.assertIn("-iname '*.hfe'", workflow)
        self.assertIn("-iname '*.lzx'", workflow)
        self.assertIn("Windows package stage lacks libpng runtime DLL", workflow)
        self.assertIn("Windows package stage lacks zlib runtime DLL", workflow)
        self.assertIn("refusing Windows package stage with possible original game data", workflow)
        self.assertIn("zip|adf|adz|dms|st|msa|stx|img|hfe|ipf|scp|ctr|lha|lzh|lzx|exe|com", workflow)
        self.assertIn("Verify installed Inno Setup package and runtime closure", workflow)

    def test_macos_bundle_closure_verifier_checks_rpaths_and_host_libraries(self) -> None:
        if os.name != "nt":
            subprocess.run(["bash", "-n", str(MACOS_CLOSURE_VERIFIER)], check=True)
        source = MACOS_CLOSURE_VERIFIER.read_text(encoding="utf-8")
        self.assertIn("otool -L", source)
        self.assertIn("LC_RPATH", source)
        self.assertIn("@rpath", source)
        self.assertIn("@loader_path", source)
        self.assertIn("@executable_path", source)
        self.assertIn("/System/*|/usr/lib/*", source)
        self.assertIn("non-system dynamic library", source)
        workflow = WORKFLOW.read_text(encoding="utf-8")
        self.assertIn('bash packaging/macos/verify-dylib-closure.sh "$APP"', workflow)
        self.assertIn("resolve_rpath_reference", workflow)
        self.assertIn("brew --prefix sdl3", workflow)
        self.assertIn("own_install_name=$(otool -D", workflow)

    @unittest.skipIf(os.name == "nt", "Git Bash fixtures do not provide portable POSIX executable semantics")
    def test_macos_closure_verifier_resolves_rpath_and_rejects_homebrew(self) -> None:
        # Linux CI cannot execute Apple's inspection tools.  Model their small,
        # documented text interface here so the verifier's decision logic is
        # covered without needing a macOS runner or any game media.
        with temporary_directory() as temporary:
            root = Path(temporary)
            app = root / "ProjectEon.app"
            executable = app / "Contents" / "MacOS" / "ProjectEon"
            framework = app / "Contents" / "Frameworks" / "libexample.dylib"
            tool_dir = root / "tools"
            executable.parent.mkdir(parents=True)
            framework.parent.mkdir(parents=True)
            tool_dir.mkdir()
            executable.touch()
            framework.touch()
            otool = tool_dir / "otool"
            otool.write_text(
                """#!/usr/bin/env bash
if [ \"$1\" = -L ]; then
  case \"$2\" in
    */ProjectEon) printf '%s\\n' \"$2:\" '@rpath/libexample.dylib (compatibility version 0.0.0, current version 0.0.0)' ;;
    *) printf '%s\\n' \"$2:\" '/usr/lib/libSystem.B.dylib (compatibility version 1.0.0, current version 1.0.0)' ;;
  esac
else
  printf '%s\\n' 'Load command 1' '          cmd LC_RPATH' '      cmdsize 56' '         path @executable_path/../Frameworks (offset 12)'
fi
""",
                encoding="utf-8",
            )
            file_command = tool_dir / "file"
            file_command.write_text("#!/usr/bin/env bash\nprintf '%s\\n' 'Mach-O 64-bit executable'\n", encoding="utf-8")
            otool.chmod(0o755)
            file_command.chmod(0o755)
            # The verifier is executed by Bash even on Windows. Bash's PATH
            # parser always uses colons, while Python's os.pathsep is a
            # semicolon on Windows and would hide these fixture tools.
            environment = os.environ | {"PATH": f"{tool_dir}:{os.environ['PATH']}"}

            result = subprocess.run(
                ["bash", str(MACOS_CLOSURE_VERIFIER), str(app)],
                text=True,
                capture_output=True,
                env=environment,
            )
            self.assertEqual(result.returncode, 0, result.stderr)

            otool.write_text(
                """#!/usr/bin/env bash
if [ \"$1\" = -L ]; then
  printf '%s\\n' \"$2:\" '/opt/homebrew/lib/libhost.dylib (compatibility version 0.0.0, current version 0.0.0)'
fi
""",
                encoding="utf-8",
            )
            otool.chmod(0o755)
            rejected = subprocess.run(
                ["bash", str(MACOS_CLOSURE_VERIFIER), str(app)],
                text=True,
                capture_output=True,
                env=environment,
            )
            self.assertNotEqual(rejected.returncode, 0)
            self.assertIn("non-system dynamic library", rejected.stderr)


if __name__ == "__main__":
    unittest.main()
