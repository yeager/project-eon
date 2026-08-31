#!/usr/bin/env bash
# Verify an already-built Project Eon AppImage without original game media.
set -euo pipefail

if [ "$#" -ne 1 ]; then
  echo "usage: $0 <Project-Eon.AppImage>" >&2
  exit 2
fi

image="$1"
case "$image" in
  /tmp|/tmp/*|""|[^/]*) echo "AppImage path must be absolute and outside /tmp" >&2; exit 2 ;;
esac
if [ ! -f "$image" ] || [ -L "$image" ] || [ ! -x "$image" ]; then
  echo "AppImage must be an executable non-symlink regular file" >&2
  exit 2
fi

scratch_root="${EON_PACKAGE_TEST_TMPDIR:-${HOME}/.cache/project-eon-tools/package-validation}"
case "$scratch_root" in
  /tmp|/tmp/*|""|[^/]*) echo "EON package scratch root must be absolute and outside /tmp" >&2; exit 2 ;;
esac
mkdir -p -- "$scratch_root"
if [ -L "$scratch_root" ] || [ ! -d "$scratch_root" ]; then
  echo "EON package scratch root must be an existing non-symlink directory" >&2
  exit 2
fi
temporary=$(mktemp -d "$scratch_root/eon-appimage.XXXXXXXX")
cleanup() { rm -rf -- "$temporary"; }
trap cleanup EXIT

# APPIMAGE_EXTRACT_AND_RUN avoids a FUSE dependency in CI. The extraction is
# confined to the disposable external cache directory and never touches user
# media or the repository.
(
  cd "$temporary"
  APPIMAGE_EXTRACT_AND_RUN=1 TMPDIR="$scratch_root" "$image" --appimage-extract >/dev/null
)
appdir="$temporary/squashfs-root"
for required in \
    "$appdir/AppRun" "$appdir/project-eon.desktop" "$appdir/project-eon.png" \
    "$appdir/usr/bin/project-eon" \
    "$appdir/usr/share/project-eon/assets/cards/millennium.png" \
    "$appdir/usr/share/project-eon/assets/branding/project-eon-logo-v1.png" \
    "$appdir/usr/share/project-eon/assets/fonts/NotoSans-Regular.ttf" \
    "$appdir/usr/share/project-eon/po/sv.po"; do
  if [ ! -e "$required" ]; then
    echo "AppImage lacks required payload: $required" >&2
    exit 1
  fi
done
if ! find "$appdir/usr/lib" -type f -name libSDL3.so.0 -print -quit | grep -q .; then
  echo "AppImage lacks its installed private SDL3 runtime" >&2
  exit 1
fi
if ! "$appdir/AppRun" --help 2>&1 | grep -Fq 'Usage:'; then
  echo "AppImage launcher did not run its packaged executable" >&2
  exit 1
fi
isolated_home="$temporary/home"
if inspect_output=$(HOME="$isolated_home" "$appdir/AppRun" --inspect 2>&1); then
  echo "AppImage unexpectedly inspected missing default game data" >&2
  exit 1
fi
if ! printf '%s\n' "$inspect_output" | grep -Fq "Data path does not exist: \"$isolated_home/.projecteon\""; then
  echo "AppImage did not report its isolated missing default game-data path" >&2
  exit 1
fi
if [ -e "$isolated_home/.projecteon" ]; then
  echo "AppImage created its default game-data directory during lookup" >&2
  exit 1
fi
if find "$appdir" -type f \( \
    -iname '*.zip' -o -iname '*.adf' -o -iname '*.adz' -o -iname '*.dms' \
    -o -iname '*.st' -o -iname '*.msa' -o -iname '*.stx' -o -iname '*.img' \
    -o -iname '*.hfe' -o -iname '*.ipf' -o -iname '*.scp' -o -iname '*.ctr' \
    -o -iname '*.lha' -o -iname '*.lzh' -o -iname '*.lzx' -o -iname '*.com' \
    -o -iname '*.exe' \) -print -quit | grep -q .; then
  echo "AppImage contains a prohibited original-media extension" >&2
  exit 1
fi
