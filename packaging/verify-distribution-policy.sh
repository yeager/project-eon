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

for package in "$@"; do
  test -f "$package"
  case "$package" in
    *.deb)
      require_tool lintian
      # mentors.debian.net runs Lintian before accepting an upload. Treat
      # warning-or-higher findings as a failure; informational and pedantic
      # diagnostics remain visible without redefining Debian policy.
      lintian --profile debian --pedantic --fail-on warning "$package"
      ;;
    *.rpm)
      require_tool rpm
      require_tool rpmlint
      require_tool rpmspec
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
