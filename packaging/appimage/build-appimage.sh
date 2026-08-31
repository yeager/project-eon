#!/usr/bin/env bash
# Build one media-free Project Eon AppImage from an already configured CMake
# tree. This is a packaging adapter only: it stages the normal install layout
# in an AppDir and never scans, copies, extracts, or modifies game media.
set -euo pipefail

usage() {
  echo "usage: $0 --build-dir <configured-cmake-build> --output <new.AppImage> --appimagetool <pinned-tool> --runtime-file <pinned-runtime> [--architecture <arch>]" >&2
  exit 2
}

build_dir=""
output=""
appimagetool=""
runtime_file=""
architecture="${APPIMAGE_ARCH:-x86_64}"
while [ "$#" -gt 0 ]; do
  case "$1" in
    --build-dir) [ "$#" -ge 2 ] || usage; build_dir="$2"; shift 2 ;;
    --output) [ "$#" -ge 2 ] || usage; output="$2"; shift 2 ;;
    --appimagetool) [ "$#" -ge 2 ] || usage; appimagetool="$2"; shift 2 ;;
    --runtime-file) [ "$#" -ge 2 ] || usage; runtime_file="$2"; shift 2 ;;
    --architecture) [ "$#" -ge 2 ] || usage; architecture="$2"; shift 2 ;;
    *) usage ;;
  esac
done

require_absolute_non_tmp_directory() {
  local path="$1"
  case "$path" in
    /tmp|/tmp/*|""|[^/]*) echo "path must be absolute and outside /tmp: $path" >&2; exit 2 ;;
  esac
  if [ ! -d "$path" ] || [ -L "$path" ]; then
    echo "path must be an existing non-symlink directory: $path" >&2
    exit 2
  fi
}

require_absolute_regular_file() {
  local path="$1"
  case "$path" in
    /tmp|/tmp/*|""|[^/]*) echo "path must be absolute and outside /tmp: $path" >&2; exit 2 ;;
  esac
  if [ ! -f "$path" ] || [ -L "$path" ]; then
    echo "path must be a non-symlink regular file: $path" >&2
    exit 2
  fi
}

require_absolute_non_tmp_directory "$build_dir"
require_absolute_regular_file "$appimagetool"
require_absolute_regular_file "$runtime_file"
if [ ! -x "$appimagetool" ]; then
  echo "appimagetool must be executable: $appimagetool" >&2
  exit 2
fi
case "$architecture" in x86_64|aarch64) ;; *) echo "unsupported AppImage architecture: $architecture" >&2; exit 2 ;; esac
case "$output" in
  /tmp|/tmp/*|""|[^/]*) echo "output must be absolute and outside /tmp" >&2; exit 2 ;;
esac
if [ -e "$output" ] || [ -L "$output" ]; then
  echo "output must not already exist: $output" >&2
  exit 2
fi
output_parent=$(dirname -- "$output")
require_absolute_non_tmp_directory "$output_parent"

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repository_root=$(CDPATH= cd -- "$script_dir/../.." && pwd)
appdir="$output_parent/Project-Eon.AppDir"
if [ -e "$appdir" ] || [ -L "$appdir" ]; then
  echo "AppDir must not already exist: $appdir" >&2
  exit 2
fi
mkdir -p "$appdir"

# CMake's normal Linux install route supplies usr/bin, a multiarch private
# library directory, shared cards, bundled fonts, PO catalogues, desktop entry
# and icon.  Do not force a non-native lib path: CMake's INSTALL_RPATH already
# resolves the installed private SDL directory relative to usr/bin.
cmake --install "$build_dir" --prefix "$appdir/usr"
install -m 0755 "$script_dir/AppRun" "$appdir/AppRun"
install -m 0644 "$repository_root/packaging/project-eon.desktop" "$appdir/project-eon.desktop"
install -m 0644 "$repository_root/assets/branding/project-eon-logo-v1.png" "$appdir/project-eon.png"

for required in \
    "$appdir/usr/bin/project-eon" \
    "$appdir/usr/share/project-eon/assets/cards/millennium.png" \
    "$appdir/usr/share/project-eon/assets/branding/project-eon-logo-v1.png" \
    "$appdir/usr/share/project-eon/po/sv.po" \
    "$appdir/AppRun" "$appdir/project-eon.desktop" "$appdir/project-eon.png"; do
  if [ ! -e "$required" ]; then
    echo "AppDir lacks required installed resource: $required" >&2
    exit 1
  fi
done
sdl_loader=$(find "$appdir/usr/lib" -type l -name libSDL3.so.0 -print -quit)
if [ -z "$sdl_loader" ] || [ ! -e "$sdl_loader" ] \
    || ! find "$appdir/usr/lib" -type f -name 'libSDL3.so.*' -print -quit | grep -q .; then
  echo "AppDir lacks the installed private SDL3 runtime" >&2
  exit 1
fi

# A release artifact must not contain original game media even if a caller's
# CMake tree was contaminated. Keep this check lexical and bounded to the
# generated AppDir; it never opens a candidate archive or disk image.
if find "$appdir" -type f \( \
    -iname '*.zip' -o -iname '*.adf' -o -iname '*.adz' -o -iname '*.dms' \
    -o -iname '*.st' -o -iname '*.msa' -o -iname '*.stx' -o -iname '*.img' \
    -o -iname '*.hfe' -o -iname '*.ipf' -o -iname '*.scp' -o -iname '*.ctr' \
    -o -iname '*.lha' -o -iname '*.lzh' -o -iname '*.lzx' -o -iname '*.com' \
    -o -iname '*.exe' \) -print -quit | grep -q .; then
  echo "AppDir contains a prohibited original-media extension" >&2
  exit 1
fi

ARCH="$architecture" APPIMAGE_EXTRACT_AND_RUN=1 "$appimagetool" \
  --runtime-file "$runtime_file" "$appdir" "$output"
if [ ! -s "$output" ] || [ -L "$output" ]; then
  echo "appimagetool did not create a non-empty regular AppImage" >&2
  exit 1
fi
