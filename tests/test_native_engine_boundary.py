#!/usr/bin/env python3
"""Keep capture emulators out of Project Eon's shipped native runtime."""

from __future__ import annotations

import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
RUNTIME_BUILD_INPUTS = (ROOT / "CMakeLists.txt", ROOT / "cmake", ROOT / "src")
FORBIDDEN_EMULATOR_IDENTIFIERS = ("dosbox", "fs-uae", "fsuae")
FORBIDDEN_PROCESS_SPAWN_IDENTIFIERS = ("std::system(", "popen(", "execve(")


def runtime_sources() -> list[pathlib.Path]:
    sources: list[pathlib.Path] = []
    for candidate in RUNTIME_BUILD_INPUTS:
        if candidate.is_file():
            sources.append(candidate)
        elif candidate.is_dir():
            sources.extend(path for path in candidate.rglob("*") if path.is_file())
    return sorted(sources)


class NativeEngineBoundaryTests(unittest.TestCase):
    def test_runtime_build_never_links_or_launches_an_emulator(self) -> None:
        """Emulators are evidence tools only; the shipped runtime is native."""
        offenders: list[str] = []
        for source in runtime_sources():
            text = source.read_text(encoding="utf-8").lower()
            if any(identifier in text for identifier in FORBIDDEN_EMULATOR_IDENTIFIERS):
                offenders.append(str(source.relative_to(ROOT)))
        self.assertEqual(offenders, [])

    def test_runtime_build_never_spawns_an_external_process(self) -> None:
        """No emulator fallback can be hidden behind a generic process API."""
        offenders: list[str] = []
        for source in runtime_sources():
            text = source.read_text(encoding="utf-8").lower()
            if any(identifier in text for identifier in FORBIDDEN_PROCESS_SPAWN_IDENTIFIERS):
                offenders.append(str(source.relative_to(ROOT)))
        self.assertEqual(offenders, [])


if __name__ == "__main__":
    unittest.main()
