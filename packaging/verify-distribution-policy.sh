#!/usr/bin/env bash
# Enforce the policy tools used by Debian mentors and RPM distributions.
# This examines generated artifacts and their generated RPM spec only; it
# never opens, copies, or needs user-supplied Millennium/Deuteros media.
set -euo pipefail

if [ "$#" -eq 0 ]; then
  echo "usage: $0 <package.deb|package.rpm> [...]" >&2
  exit 2
fi

require_tool() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "required distribution policy tool is unavailable: $1" >&2
    exit 2
  fi
}

# CPack otherwise inherits the build host's umask for implicitly created
# staging directories.  Distribution payload directories are application
# resources, never mutable game-data locations, so reject group/world-writable
# modes before native Debian/RPM policy tooling sees the artifact.
require_755_directories() {
  local format="$1"
  local package="$2"
  local bad_directories
  case "$format" in
    deb)
      require_tool dpkg-deb
      bad_directories="$(dpkg-deb -c "$package" | awk '$1 ~ /^d/ && $1 != "drwxr-xr-x" { print; bad = 1 } END { exit bad }')" || true
      ;;
    rpm)
      bad_directories="$(rpm -qplv "$package" | awk '$1 ~ /^d/ && $1 != "drwxr-xr-x" { print; bad = 1 } END { exit bad }')" || true
      ;;
    *)
      echo "unknown directory permission format: $format" >&2
      exit 2
      ;;
  esac
  if [ -n "$bad_directories" ]; then
    echo "package contains directories that are not 0755: $package" >&2
    printf '%s\n' "$bad_directories" >&2
    exit 1
  fi
}

for package in "$@"; do
  test -f "$package"
  case "$package" in
    *.deb)
      require_tool lintian
      require_755_directories deb "$package"
      # mentors.debian.net runs Lintian before accepting an upload. Treat
      # warning-or-higher findings as a failure; informational and pedantic
      # diagnostics remain visible without redefining Debian policy.
      lintian --profile debian --pedantic --fail-on warning "$package"
      ;;
    *.rpm)
      require_tool rpm
      require_tool rpmlint
      require_tool rpmspec
      require_755_directories rpm "$package"
      # Verify the generated package header/payload, then parse the exact
      # CPack-generated spec rather than a disconnected template.
      rpm --checksig --nogpg "$package"
      spec_root="$(dirname "$package")/_CPack_Packages"
      spec_file="$(find "$spec_root" -type f -path '*/SPECS/*.spec' -print -quit 2>/dev/null || true)"
      if [ -z "$spec_file" ]; then
        echo "RPM package lacks its generated spec for syntax validation: $package" >&2
        exit 1
      fi
      rpmspec --parse "$spec_file" >/dev/null
      rpmlint --strict "$package"
      ;;
    *)
      echo "unsupported package: $package" >&2
      exit 2
      ;;
  esac
done
