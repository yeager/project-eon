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
  for required in "assets/cards/millennium.png" "assets/cards/deuteros.png" "po/sv.po"; do
    if ! printf '%s\n' "$contents" | grep -Fq "$required"; then
      echo "$package lacks required Project Eon resource: $required" >&2
      exit 1
    fi
  done
  if printf '%s\n' "$contents" | grep -Eqi '(^|/)(data)(/|$)|\.(zip|adf|adz|dms|st|msa|stx|img|exe|com)([[:space:]]|$)'; then
    echo "$package contains a game-data path or prohibited original-media format" >&2
    exit 1
  fi
done
