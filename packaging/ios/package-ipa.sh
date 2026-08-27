#!/usr/bin/env bash
set -euo pipefail
if [ "$#" -ne 2 ]; then echo "usage: $0 <ProjectEon.app> <output.ipa>" >&2; exit 2; fi
app="$1"; ipa="$2"
test -d "$app"
case "$ipa" in *.ipa) ;; *) echo "output must end in .ipa" >&2; exit 2 ;; esac
stage=$(mktemp -d); trap 'rm -rf "$stage"' EXIT
mkdir -p "$stage/Payload"
cp -R "$app" "$stage/Payload/ProjectEon.app"
(cd "$stage" && /usr/bin/zip -qr "$ipa" Payload)
