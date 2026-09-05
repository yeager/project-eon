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

verify_rpm_manpage() {
  local package="$1"
  local expected="/usr/share/man/man6/project-eon.6.gz"
  local members
  require_tool cpio
  require_tool gzip
  require_tool rpm2cpio
  members="$(rpm -qpl "$package")"
  if ! grep -Fxq "$expected" <<<"$members"; then
    echo "RPM package lacks its policy-compressed manual page: $expected" >&2
    return 1
  fi
  if grep -Eq '^/usr/share/man/man6/project-eon\.6($|\.(bz2|xz|zst)$)' <<<"$members"; then
    echo "RPM package contains a conflicting manual-page compression format" >&2
    return 1
  fi
  # Inspect the member bytes instead of trusting its suffix. GNU cpio accepts
  # the archive's leading ./ spelling while rpm -qpl reports absolute paths.
  if ! rpm2cpio "$package" \
      | cpio -i --quiet --to-stdout './usr/share/man/man6/project-eon.6.gz' \
      | gzip -t; then
    echo "RPM manual page is not a valid gzip stream: $expected" >&2
    return 1
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
      verify_rpm_manpage "$package"
      # Verify the generated package header/payload, then parse the exact
      # CPack-generated spec rather than a disconnected template.
      rpm --checksig --nosignature "$package"
      spec_root="$(dirname "$package")/_CPack_Packages"
      spec_file="$(find "$spec_root" -type f -path '*/SPECS/*.spec' -print -quit 2>/dev/null || true)"
      if [ -z "$spec_file" ]; then
        echo "RPM package lacks its generated spec for syntax validation: $package" >&2
        exit 1
      fi
      rpmspec --parse "$spec_file" >/dev/null
      # Keep rpmlint strict for every actionable diagnostic.  The config only
      # records three properties of this non-release CPack artifact which the
      # package cannot truthfully change: it is deliberately unsigned, CPack
      # emits Vendor/Contact but no Packager header, and Ubuntu's rpmlint 2.5
      # license table predates the valid SPDX MIT identifier.
      rpmlint --strict --config "$(dirname "$0")/rpmlint.toml" "$package"
      ;;
    *)
      echo "unsupported package: $package" >&2
      exit 2
      ;;
  esac
done
