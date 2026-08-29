"""Static contract for the Linux artifact verifier used in CI."""

from __future__ import annotations

import os
from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
VERIFIER = ROOT / "packaging" / "verify-desktop-package.sh"
MACOS_CLOSURE_VERIFIER = ROOT / "packaging" / "macos" / "verify-dylib-closure.sh"
WORKFLOW = ROOT / ".github" / "workflows" / "build.yml"


class DesktopPackagingTests(unittest.TestCase):
    def test_verifier_is_valid_shell_and_rejects_media_extensions(self) -> None:
        # The script itself is exercised by the Linux packaging job. Windows
        # still checks the preservation contract below, but has no POSIX shell
        # runtime to parse it with.
        if os.name != "nt":
            subprocess.run(["bash", "-n", str(VERIFIER)], check=True)
        source = VERIFIER.read_text(encoding="utf-8")
        for extension in ("zip", "adf", "st", "msa", "stx", "img", "exe", "com"):
            with self.subTest(extension=extension):
                self.assertIn(extension, source)
        self.assertIn("assets/cards/millennium.png", source)
        self.assertIn("NotoSansSC-Regular.otf", source)
        self.assertIn("OFL-1.1.txt", source)
        self.assertIn('for catalog in ar de el en_GB es fi fr hi it ja ko nl no pl pt_BR ru sv tr uk zh_CN', source)
        self.assertIn("localization catalog", source)
        self.assertIn("generated Debian runtime dependencies", source)
        self.assertIn("private runtime", source)
        self.assertIn("LD_TRACE_LOADED_OBJECTS=1", source)
        self.assertIn("generated RPM runtime dependencies", source)
        self.assertIn("rpm2cpio", source)
        self.assertIn("package layout regression", source)
        self.assertIn("cpio zlib1g-dev", WORKFLOW.read_text(encoding="utf-8"))

    def test_linux_packaging_job_runs_the_artifact_verifier(self) -> None:
        workflow = WORKFLOW.read_text(encoding="utf-8")
        self.assertIn("Verify package contents contain no game media", workflow)
        self.assertIn("bash packaging/verify-desktop-package.sh package/deb/*.deb package/rpm/*.rpm", workflow)

    def test_debian_package_generates_system_dependencies_without_host_sdl(self) -> None:
        cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn('set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")', cmake)
        self.assertIn("set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)", cmake)
        self.assertIn("CPACK_DEBIAN_PACKAGE_SHLIBDEPS_PRIVATE_DIRS", cmake)

    def test_ci_validates_macos_archive_and_windows_runtime_stage(self) -> None:
        workflow = WORKFLOW.read_text(encoding="utf-8")
        self.assertIn('built_architectures=$(lipo -archs "$APP/Contents/MacOS/ProjectEon")', workflow)
        self.assertIn('macOS bundle architecture mismatch: expected ${{ matrix.arch }}', workflow)
        self.assertIn('unzip -t "project-eon-macos-${{ matrix.arch }}.zip"', workflow)
        self.assertIn('cp -R assets/fonts "$APP/Contents/MacOS/assets/fonts"', workflow)
        self.assertIn('Copy-Item assets/fonts dist/assets/fonts -Recurse', workflow)
        self.assertIn("refusing macOS artifact with possible original game data", workflow)
        self.assertIn("Windows package stage lacks libpng runtime DLL", workflow)
        self.assertIn("Windows package stage lacks zlib runtime DLL", workflow)
        self.assertIn("refusing Windows package stage with possible original game data", workflow)
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

    def test_macos_closure_verifier_resolves_rpath_and_rejects_homebrew(self) -> None:
        # Linux CI cannot execute Apple's inspection tools.  Model their small,
        # documented text interface here so the verifier's decision logic is
        # covered without needing a macOS runner or any game media.
        with tempfile.TemporaryDirectory() as temporary:
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
            environment = os.environ | {"PATH": f"{tool_dir}{os.pathsep}{os.environ['PATH']}"}

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
