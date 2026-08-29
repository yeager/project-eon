#!/usr/bin/env bash
# Verify an already-built desktop package contains Project Eon resources only.
# This operates on package metadata; it never opens or needs original media.
set -euo pipefail

if [ "$#" -eq 0 ]; then
  echo "usage: $0 <package.deb|package.rpm> [...]" >&2
  exit 2
fi

for package in "$@"; do
  test -f "$package"
  case "$package" in
    *.deb) contents=$(dpkg-deb --contents "$package") ;;
    *.rpm) contents=$(rpm -qlp "$package") ;;
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
