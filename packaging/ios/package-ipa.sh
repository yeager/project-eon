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
for required in \
  Resources/assets/cards/millennium.png \
  Resources/assets/cards/deuteros.png \
  Resources/assets/cards/dos-platform-v1.png \
  Resources/assets/cards/amiga-platform-v1.png \
  Resources/assets/cards/atari-st-platform-v1.png \
  Resources/assets/cards/original-profile-v1.png \
  Resources/assets/cards/modern-profile-v1.png \
  Resources/assets/cards/custom-profile-v1.png; do
  if [ ! -f "$app/$required" ]; then
    echo "refusing to package incomplete iPad application: missing $required" >&2
    exit 1
  fi
done
for font in NotoSans-Regular.ttf NotoSansArabic-Regular.ttf \
    NotoSansDevanagari-Regular.ttf NotoSansJP-Regular.otf \
    NotoSansKR-Regular.otf NotoSansSC-Regular.otf OFL-1.1.txt; do
  required="Resources/assets/fonts/$font"
  if [ ! -f "$app/$required" ]; then
    echo "refusing to package incomplete iPad application: missing $required" >&2
    exit 1
  fi
done
for catalog in ar de el en_GB es fi fr hi it ja ko nl no pl pt_BR ru sv tr uk zh_CN; do
  required="Resources/po/$catalog.po"
  if [ ! -f "$app/$required" ]; then
    echo "refusing to package incomplete iPad application: missing $required" >&2
    exit 1
  fi
done
stage=$(mktemp -d); trap 'rm -rf "$stage"' EXIT
mkdir -p "$stage/Payload"
cp -R "$app" "$stage/Payload/ProjectEon.app"
(cd "$stage" && /usr/bin/zip -qr "$ipa" Payload)
if ! python3 "$(dirname "$0")/verify-ipa.py" "$ipa"; then
  # Do not leave a failed archive in a directory that a caller may upload.
  rm -f "$ipa"
  exit 1
fi
