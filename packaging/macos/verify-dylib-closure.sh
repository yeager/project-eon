#!/usr/bin/env bash
# Verify that a completed macOS application bundle never falls back to a
# developer machine's Homebrew (or other non-system) dynamic libraries.
#
# `otool -L` prints install names, rather than resolved paths.  In particular,
# an `@rpath` name must be checked against the LC_RPATH entries of the Mach-O
# object that owns it.  This verifier deliberately accepts only references
# resolved within Contents/ plus Apple system locations.
set -euo pipefail

if [ "$#" -ne 1 ]; then
  echo "usage: $0 <ProjectEon.app>" >&2
  exit 64
fi

app_input="$1"
if [ ! -d "$app_input/Contents" ]; then
  echo "not an application bundle: $app_input" >&2
  exit 64
fi

app=$(cd "$app_input" && pwd -P)
contents="$app/Contents"
executable_dir="$contents/MacOS"

fail() {
  echo "macOS runtime closure error: $*" >&2
  exit 1
}

canonical_existing_path() {
  python3 - "$1" <<'PY'
from pathlib import Path
import sys

print(Path(sys.argv[1]).resolve(strict=True))
PY
}

is_inside_contents() {
  local path
  path=$(canonical_existing_path "$1") || return 1
  case "$path" in
    "$contents"/*) return 0 ;;
    *) return 1 ;;
  esac
}

is_system_reference() {
  case "$1" in
    /System/*|/usr/lib/*) return 0 ;;
    *) return 1 ;;
  esac
}

rpaths_for() {
  # Preserve path text instead of splitting it on whitespace.  A path with a
  # space is unusual but legal in an LC_RPATH command.
  otool -l "$1" | sed -n 's/^[[:space:]]*path \(.*\) (offset [0-9][0-9]*)$/\1/p'
}

expand_path_token() {
  local value="$1"
  local owner="$2"
  case "$value" in
    @executable_path/*)
      printf '%s/%s\n' "$executable_dir" "${value#@executable_path/}"
      ;;
    @loader_path/*)
      printf '%s/%s\n' "$(dirname "$owner")" "${value#@loader_path/}"
      ;;
    /*)
      printf '%s\n' "$value"
      ;;
    *)
      return 1
      ;;
  esac
}

verify_resolved_path() {
  local owner="$1"
  local reference="$2"
  local candidate

  case "$reference" in
    @rpath/*)
      local suffix="${reference#@rpath/}"
      local found=0
      while IFS= read -r rpath; do
        candidate=$(expand_path_token "$rpath" "$owner") || \
          fail "$owner has unsupported LC_RPATH '$rpath' for $reference"
        candidate="$candidate/$suffix"
        if [ -e "$candidate" ]; then
          found=1
          is_inside_contents "$candidate" || \
            fail "$owner resolves $reference outside the app bundle: $candidate"
        fi
      done < <(rpaths_for "$owner")
      [ "$found" -eq 1 ] || fail "$owner cannot resolve $reference"
      ;;
    @executable_path/*|@loader_path/*)
      candidate=$(expand_path_token "$reference" "$owner") || \
        fail "$owner has unsupported install name $reference"
      [ -e "$candidate" ] || fail "$owner cannot resolve $reference ($candidate)"
      is_inside_contents "$candidate" || \
        fail "$owner resolves $reference outside the app bundle: $candidate"
      ;;
    /*)
      is_system_reference "$reference" || \
        fail "$owner references non-system dynamic library $reference"
      ;;
    *)
      fail "$owner has unsupported relative install name $reference"
      ;;
  esac
}

verify_macho() {
  local binary="$1"
  local reference
  while IFS= read -r reference; do
    [ -n "$reference" ] || continue
    verify_resolved_path "$binary" "$reference"
  done < <(otool -L "$binary" | awk 'NR > 1 { print $1 }')
}

found_macho=0
while IFS= read -r -d '' candidate; do
  if file -b "$candidate" | grep -q 'Mach-O'; then
    found_macho=1
    verify_macho "$candidate"
  fi
done < <(find "$contents" -type f -print0)

[ "$found_macho" -eq 1 ] || fail "no Mach-O files found in $app"
echo "macOS dynamic-library closure verified for $app"
