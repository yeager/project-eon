#!/usr/bin/env bash
# Verify an already-built desktop package contains Project Eon resources only.
# This operates on package metadata; it never opens or needs original media.
set -euo pipefail

if [ "$#" -eq 0 ]; then
  echo "usage: $0 <package.deb|package.rpm> [...]" >&2
  exit 2
fi

# Keep package inspection outside both the checkout and system temporary
# storage.  A caller can select an existing Project Eon cache root, while the
# default is safe for local and CI runs without game media.
scratch_root="${EON_PACKAGE_TEST_TMPDIR:-${HOME}/.cache/project-eon-tools/package-validation}"
case "$scratch_root" in
  /tmp|/tmp/*|""|[^/]* )
    echo "EON package scratch root must be absolute and outside /tmp" >&2
    exit 2
    ;;
esac
mkdir -p -- "$scratch_root"
if [ -L "$scratch_root" ] || [ ! -d "$scratch_root" ]; then
  echo "EON package scratch root must be an existing non-symlink directory" >&2
  exit 2
fi
make_temporary_directory() {
  mktemp -d "$scratch_root/eon-package.XXXXXXXX"
}

# Remove only explicit directories created under the validated cache root,
# including on a failed check.
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
      temporary=$(make_temporary_directory)
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
      # RPM queries normally consult the host RPM database even for a package
      # file query. Give this verifier a fresh database under the same
      # disposable extraction root so it neither locks nor depends on the
      # workstation's package-manager state.
      temporary=$(make_temporary_directory)
      temporary_directories+=("$temporary")
      rpm_database="$temporary/rpmdb"
      mkdir -p "$rpm_database"
      rpm_query=(rpm --dbpath "$rpm_database")
      contents=$("${rpm_query[@]}" -qlp "$package")
      # RPM calculates ELF requirements by default. Keep that metadata visible
      # as a contract. The payload is exercised below without installing it on
      # the Ubuntu CI host, so an RPM repository is not needed for this test.
      dependencies=$("${rpm_query[@]}" -qp --requires "$package")
      if ! printf '%s\n' "$dependencies" | grep -Fq 'libc.so.6'; then
        echo "$package lacks generated RPM runtime dependencies" >&2
        exit 1
      fi

      # Do not limit RPM validation to its dependency declaration. Extract the
      # artifact into an isolated directory and run the installed executable,
      # just as the DEB path does. This catches a missing private SDL library,
      # a lost $ORIGIN runpath, or a package layout regression before upload.
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

  # Run the installed executable with its Linux default location under an
  # isolated HOME. A distribution payload must neither ship media nor create
  # ~/.projecteon while only looking for it. This is intentionally an
  # end-to-end check: package listing checks alone cannot prove the installed
  # binary retained the read-only default-data boundary.
  isolated_home=$(make_temporary_directory)
  temporary_directories+=("$isolated_home")
  if inspect_output=$(HOME="$isolated_home" "$executable" --inspect 2>&1); then
    echo "$package unexpectedly inspected missing default game data" >&2
    exit 1
  fi
  if ! printf '%s\n' "$inspect_output" | grep -Fq "Data path does not exist: \"$isolated_home/.projecteon\""; then
    echo "$package did not report its isolated missing default game-data path" >&2
    exit 1
  fi
  if [ -e "$isolated_home/.projecteon" ]; then
    echo "$package created its default game-data directory during lookup" >&2
    exit 1
  fi

  # The generated cards and a catalog prove that the launcher can render and
  # localize after installation.  The original data directory stays absent.
  desktop_entry="${temporary}/usr/share/applications/project-eon.desktop"
  if [ ! -f "$desktop_entry" ]; then
    echo "$package lacks its installed desktop launcher entry" >&2
    exit 1
  fi
  if ! desktop-file-validate "$desktop_entry"; then
    echo "$package has an invalid desktop launcher entry" >&2
    exit 1
  fi
  if ! grep -Fxq 'Exec=project-eon' "$desktop_entry"; then
    echo "$package desktop launcher does not start the installed runtime" >&2
    exit 1
  fi
  for required in \
      "assets/cards/millennium.png" "assets/cards/deuteros.png" \
      "assets/cards/dos-platform-v1.png" "assets/cards/amiga-platform-v1.png" \
      "assets/cards/atari-st-platform-v1.png" \
      "assets/cards/original-profile-v1.png" "assets/cards/modern-profile-v1.png" \
      "assets/cards/custom-profile-v1.png"; do
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
  if printf '%s\n' "$contents" | grep -Eqi '(^|/)(data)(/|$)|\.(zip|adf|adz|dms|st|msa|stx|img|hfe|ipf|scp|ctr|lha|lzh|lzx|exe|com)([[:space:]]|$)'; then
    echo "$package contains a game-data path or prohibited original-media format" >&2
    exit 1
  fi
done
