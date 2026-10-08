#!/usr/bin/env python3
"""Publish only complete, verified GitHub Actions packages on manual request."""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import time


ARTIFACTS = {
    "linux-packages-x86_64": "project-eon-linux-artifacts.json",
    "macos-arm64-app": "project-eon-macos-arm64-artifacts.json",
    "macos-x86_64-app": "project-eon-macos-x86_64-artifacts.json",
    "windows-x64-installer": "project-eon-windows-artifacts.json",
    "ipados-arm64-unsigned-ipa": "project-eon-ipados-arm64-unsigned-artifacts.json",
}
JOBS = {"Repository artifact policy", "Gitleaks", "Linux", "macOS", "Windows", "Linux packages",
        "macOS app (arm64)", "macOS app (x86_64)", "Windows Inno Setup",
        "iPadOS sideload IPA"}


def run(*args: str) -> str:
    return subprocess.check_output(args, text=True).strip()


def api(path: str):
    return json.loads(run("gh", "api", path))


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def digest(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def check_artifacts(artifacts: list[dict]) -> None:
    names = [item["name"] for item in artifacts]
    packages = [item for item in artifacts if item["name"] in ARTIFACTS]
    # Gitleaks uploads its scan report alongside the five package groups.
    # The report is not a distributable and must never enter release assets.
    require(len(names) == len(set(names))
            and set(names) <= set(ARTIFACTS) | {"gitleaks-results.sarif"}
            and {item["name"] for item in packages} == set(ARTIFACTS)
            and not any(item["expired"] for item in packages),
            "Complete, unexpired platform artifacts required")


def main() -> None:
    require(os.environ.get("GITHUB_EVENT_NAME") == "workflow_dispatch",
            "Release requires an explicit manual workflow dispatch")
    repo = os.environ["GITHUB_REPOSITORY"]
    version = os.environ["RELEASE_VERSION"]
    run_id = os.environ["RELEASE_BUILD_RUN"]
    require(re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", version) is not None, "Invalid version")
    require(re.fullmatch(r"[0-9]+", run_id) is not None, "Invalid build run ID")
    endpoint = f"repos/{repo}/actions/runs/{run_id}"
    deadline = time.monotonic() + 3600
    while True:
        build = api(endpoint)
        require(build["head_repository"]["full_name"] == repo
                and build["head_branch"] == "main"
                and build["path"] == ".github/workflows/build.yml"
                and build["event"] in {"push", "workflow_dispatch"},
                "Build must be the repository's main Build workflow")
        if build["status"] == "completed":
            break
        require(time.monotonic() < deadline, "Timed out waiting for Build")
        print(f"Waiting for Build {run_id}: {build['status']}", flush=True)
        time.sleep(30)
    require(build["conclusion"] == "success", "Build did not succeed")
    revision = build["head_sha"]
    jobs = api(endpoint + "/jobs?per_page=100")["jobs"]
    require({job["name"] for job in jobs} == JOBS
            and all(job["conclusion"] == "success" for job in jobs),
            "Every required test and packaging job must succeed")
    artifacts = api(endpoint + "/artifacts?per_page=100")["artifacts"]
    check_artifacts(artifacts)
    cache = Path.home() / ".cache/project-eon-tools/release" / os.environ["GITHUB_RUN_ID"]
    cache.mkdir(parents=True, exist_ok=False)
    files: list[Path] = []
    for name, manifest in ARTIFACTS.items():
        directory = cache / name
        run("gh", "run", "download", run_id, "--repo", repo, "--name", name,
            "--dir", str(directory))
        run("python3", "packaging/verify-artifact-manifest.py", "--manifest",
            str(directory / manifest), "--directory", str(directory),
            "--expected-source-revision", revision, "--require-exact-directory")
        files.extend(sorted(directory.iterdir()))
    require(len({path.name for path in files}) == len(files), "Duplicate release asset names")
    names = {path.name for path in files}
    require(f"Project-Eon-{version}-windows-x64.exe" in names,
            "Windows installer version must match the release version")
    require(len(files) == 12 and all(any(name.endswith(suffix) for name in names)
            for suffix in (".deb", ".rpm", ".AppImage", "macos-arm64.zip",
                           "macos-x86_64.zip", "-windows-x64.exe", ".ipa")),
            "All seven platform packages and five manifests are required")
    checksums = cache / "SHA256SUMS.txt"
    checksums.write_text("".join(f"{digest(path)}  {path.name}\n"
                                 for path in sorted(files, key=lambda p: p.name)), encoding="utf-8")
    files.append(checksums)
    expected = {path.name: digest(path) for path in files}
    tag = "v" + version
    notes = cache / "release-notes.md"
    notes.write_text(f"""Project Eon {version}

Preservation-first SDL3 reimplementation of Millennium 2.2 and Deuteros.
This release advances the recovered Millennium DOS and Deuteros Amiga runtime
paths while keeping unsupported behavior behind documented evidence
boundaries. Complete gameplay parity is not yet achieved.

Includes Linux x86_64 DEB, RPM and AppImage; macOS arm64 and x86_64 apps;
Windows x64 installer; and an unsigned iPadOS arm64 IPA requiring sideload signing.
Desktop Apple bundles are not Developer ID notarized.
Original commercial game media is not included.
Supply your own supported media in the documented data directory.

Millennium and Deuteros runtime boundaries remain as documented in the source
and preservation records. These packages do not claim fully playable replacements.

Source: `{revision}`
Verified build: {build['html_url']}

Each platform includes its source-bound integrity manifest. SHA256SUMS.txt
covers all packages and manifests. All required build, test and packaging
jobs passed before publication.
""", encoding="utf-8")
    # Refuse existing releases/tags rather than overwriting published history.
    refs = api(f"repos/{repo}/git/matching-refs/tags/{tag}")
    require(not any(ref["ref"] == f"refs/tags/{tag}" for ref in refs), "Release tag already exists")
    existing = run("gh", "release", "list", "--repo", repo, "--limit", "1000",
                   "--json", "tagName")
    require(tag not in {item["tagName"] for item in json.loads(existing)}, "Release already exists")
    run("gh", "release", "create", tag, "--repo", repo, "--target", revision,
        "--draft", "--title", f"Project Eon {version}", "--notes-file", str(notes))
    run("gh", "release", "upload", tag, "--repo", repo, *(str(path) for path in files))
    downloaded = cache / "published-assets"
    run("gh", "release", "download", tag, "--repo", repo, "--dir", str(downloaded))
    require({path.name: digest(path) for path in downloaded.iterdir()} == expected,
            "Uploaded release assets differ from verified packages")
    run("gh", "release", "edit", tag, "--repo", repo, "--draft=false", "--latest")
    print(run("gh", "release", "view", tag, "--repo", repo, "--json", "url", "--jq", ".url"))


if __name__ == "__main__":
    main()
