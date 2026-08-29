#!/usr/bin/env bash
# Verify an already-built desktop package contains Project Eon resources only.
# This operates on package metadata; it never opens or needs original media.
set -euo pipefail

if [ "$#" -eq 0 ]; then
  echo "usage: $0 <package.deb|package.rpm> [...]" >&2
  exit 2
fi

# Keep each extracted DEB isolated from the build tree and remove only the
# explicit mktemp directories after validation, including on a failed check.
temporary_directories=()
cleanup() {
  local temporary
  for temporary in "${temporary_directories[@]}"; do
    rm -rf -- "$temporary"
  done
}
trap cleanup EXIT

for package in "$@"; do
  test -f "$package"
  case "$package" in
    *.deb)
      contents=$(dpkg-deb --contents "$package")
      # CPack must declare libraries outside the self-contained SDL payload.
      # Checking the actual control field catches a package that happens to
      # run on the CI image only because its build dependencies remain there.
      dependencies=$(dpkg-deb --field "$package" Depends || true)
      if [ -z "$dependencies" ] || ! printf '%s\n' "$dependencies" | grep -Eq '(^|, )libc6( |\(|,|$)'; then
        echo "$package lacks generated Debian runtime dependencies" >&2
        exit 1
      fi
      if printf '%s\n' "$dependencies" | grep -Eqi '(^|, )libsdl3(-[0-9]+)?( |\(|,|$)'; then
        echo "$package incorrectly depends on a distro SDL3 instead of its private runtime" >&2
        exit 1
      fi

      # Exercise the installed artifact rather than a build-tree executable.
      # The loader trace must resolve all three staged SDL libraries through
      # the executable's $ORIGIN runpath; no game media or display is needed.
      temporary=$(mktemp -d)
      temporary_directories+=("$temporary")
      dpkg-deb --extract "$package" "$temporary"
      executable="$temporary/usr/bin/project-eon"
      if [ ! -x "$executable" ]; then
        echo "$package lacks its installed executable" >&2
        exit 1
      fi
      runtime_trace=$(LD_TRACE_LOADED_OBJECTS=1 "$executable")
      for library in libSDL3.so.0 libSDL3_image.so.0 libSDL3_ttf.so.0; do
        if ! printf '%s\n' "$runtime_trace" | grep -Fq "$temporary/usr/bin/$library"; then
          echo "$package does not resolve $library from its installed runtime" >&2
          exit 1
        fi
      done
      usage=$("$executable" --help 2>&1)
      if ! printf '%s\n' "$usage" | grep -Fq 'Usage:'; then
        echo "$package executable did not load and print CLI usage" >&2
        exit 1
      fi
      ;;
    *.rpm)
      contents=$(rpm -qlp "$package")
      # RPM calculates ELF requirements by default. Keep that metadata visible
      # as a contract. The payload is exercised below without installing it on
      # the Ubuntu CI host, so an RPM repository is not needed for this test.
      dependencies=$(rpm -qp --requires "$package")
      if ! printf '%s\n' "$dependencies" | grep -Fq 'libc.so.6'; then
        echo "$package lacks generated RPM runtime dependencies" >&2
        exit 1
      fi

      # Do not limit RPM validation to its dependency declaration. Extract the
      # artifact into an isolated directory and run the installed executable,
      # just as the DEB path does. This catches a missing private SDL library,
      # a lost $ORIGIN runpath, or a package layout regression before upload.
      temporary=$(mktemp -d)
      temporary_directories+=("$temporary")
      rpm2cpio "$package" | (cd "$temporary" && cpio --quiet -idm)
      executable="$temporary/usr/bin/project-eon"
      if [ ! -x "$executable" ]; then
        echo "$package lacks its installed executable" >&2
        exit 1
      fi
      runtime_trace=$(LD_TRACE_LOADED_OBJECTS=1 "$executable")
      for library in libSDL3.so.0 libSDL3_image.so.0 libSDL3_ttf.so.0; do
        if ! printf '%s\n' "$runtime_trace" | grep -Fq "$temporary/usr/bin/$library"; then
          echo "$package does not resolve $library from its installed runtime" >&2
          exit 1
        fi
      done
      usage=$("$executable" --help 2>&1)
      if ! printf '%s\n' "$usage" | grep -Fq 'Usage:'; then
        echo "$package executable did not load and print CLI usage" >&2
        exit 1
      fi
      ;;
    *) echo "unsupported package: $package" >&2; exit 2 ;;
  esac

  # The generated cards and a catalog prove that the launcher can render and
  # localize after installation.  The original data directory stays absent.
  for required in "assets/cards/millennium.png" "assets/cards/deuteros.png"; do
    if ! printf '%s\n' "$contents" | grep -Fq "$required"; then
      echo "$package lacks required Project Eon resource: $required" >&2
      exit 1
    fi
  done
  # Unicode launcher rendering is entirely bundle-backed.  Verify every
  # reviewed Noto file and its OFL license so installation cannot silently
  # fall back to a workstation font for an otherwise shipped locale.
  for font in NotoSans-Regular.ttf NotoSansArabic-Regular.ttf \
      NotoSansDevanagari-Regular.ttf NotoSansJP-Regular.otf \
      NotoSansKR-Regular.otf NotoSansSC-Regular.otf OFL-1.1.txt; do
    if ! printf '%s\n' "$contents" | grep -Fq "assets/fonts/$font"; then
      echo "$package lacks required launcher font resource: $font" >&2
      exit 1
    fi
  done
  # Every shipped UI language must remain usable after installation. Keep the
  # list explicit and aligned with CMake/iPadOS packaging rather than treating
  # one catalog as proof that a partial localization bundle is acceptable.
  for catalog in ar de el en_GB es fi fr hi it ja ko nl no pl pt_BR ru sv tr uk zh_CN; do
    if ! printf '%s\n' "$contents" | grep -Fq "po/$catalog.po"; then
      echo "$package lacks Project Eon localization catalog: $catalog.po" >&2
      exit 1
    fi
  done
  if printf '%s\n' "$contents" | grep -Eqi '(^|/)(data)(/|$)|\.(zip|adf|adz|dms|st|msa|stx|img|exe|com)([[:space:]]|$)'; then
    echo "$package contains a game-data path or prohibited original-media format" >&2
    exit 1
  fi
done
