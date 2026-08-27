#!/usr/bin/env bash
set -euo pipefail
if [ "$#" -ne 2 ]; then echo "usage: $0 <ProjectEon.app> <output.ipa>" >&2; exit 2; fi
app="$1"; ipa="$2"
test -d "$app"
case "$ipa" in *.ipa) ;; *) echo "output must end in .ipa" >&2; exit 2 ;; esac
ipa="$(cd "$(dirname "$ipa")" && pwd)/$(basename "$ipa")"
# An IPA must never redistribute user-supplied commercial media. The build
# bundle contains only executable/runtime resources, so reject common game
# media rather than trusting a caller's staging directory.
if find "$app" -type d -name data -print -quit | grep -q . \
    || find "$app" -type f \( -iname '*.zip' -o -iname '*.adf' -o -iname '*.st' \
        -o -iname '*.msa' -o -iname '*.stx' -o -iname '*.img' -o -iname '*.exe' \
        -o -iname '*.com' \) -print -quit | grep -q .; then
  echo "refusing to package possible original game data" >&2
  exit 1
fi
stage=$(mktemp -d); trap 'rm -rf "$stage"' EXIT
mkdir -p "$stage/Payload"
cp -R "$app" "$stage/Payload/ProjectEon.app"
(cd "$stage" && /usr/bin/zip -qr "$ipa" Payload)
