"""Static contract for the Linux artifact verifier used in CI."""

from __future__ import annotations

import os
from pathlib import Path
import subprocess
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
        self.assertIn("APPIMAGE_EXTRACT_AND_RUN=1", builder)
        self.assertIn("AppImage created its default game-data directory during lookup", verifier)
        self.assertIn("--appimage-extract", verifier)
        self.assertIn('exec "$appdir/usr/bin/project-eon" "$@"', runner)
        workflow = WORKFLOW.read_text(encoding="utf-8")
        self.assertIn("Fetch pinned AppImage build tools", workflow)
        self.assertIn("a6d71e2b6cd66f8e8d16c37ad164658985e0cf5fcaa950c90a482890cb9d13e0", workflow)
        self.assertIn("1cc49bcf1e2ccd593c379adb17c9f85a36d619088296504de95b1d06215aebbf", workflow)
        self.assertIn("sha256sum --check --strict", workflow)
        self.assertIn("Verify AppImage contents contain no game media", workflow)
        self.assertIn("package/appimage/*.AppImage", workflow)

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
        self.assertIn("file(RPATH_REMOVE", cmake)
        self.assertIn("--strip-unneeded", cmake)
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
