# Requested releases

Normal pushes only build and test. Publication requires a maintainer's explicit
release request and a manual **Release** workflow dispatch.

1. Confirm the project/package version and review release notes in
   `packaging/publish-release.py`, including current gameplay limitations.
2. Dispatch **Build** on `main` with `package_version` set to that version
   (for example `0.1.3`). Retain its numeric run ID. Avoid newer main pushes
   until that build completes because the build workflow cancels obsolete runs.
3. Dispatch **Release** on `main`, supplying that `build_run_id` and `version`.
   It can wait for a running build. The selected build must belong to this
   repository's main Build workflow and all ten required jobs must succeed.
4. Publication requires all five artifact groups: Linux DEB/RPM/AppImage,
   macOS arm64 and x86_64 ZIPs, Windows x64 installer, and unsigned iPadOS IPA.
   Every package manifest must match the selected source commit, file sizes,
   hashes and exact directory contents. The Windows installer version must
   match the release version.
5. The workflow creates a draft, uploads seven packages, five manifests and
   `SHA256SUMS.txt`, downloads them again, verifies their hashes, then publishes.
   The release tag targets the build's exact source commit. Existing tags or
   releases are never overwritten. A failed upload/check leaves a draft for
   investigation; it does not publish an incomplete release.

No original game media is included. The unsigned IPA needs external signing
for sideloading; desktop Apple packages are not Developer ID notarized.
Successful packaging is not a claim of gameplay parity.
