#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
doc="$repo/docs/configuration.md"
tmp=$(mktemp)
trap 'rm -f "$tmp"' EXIT HUP INT TERM

find "$repo/src" -type f \( -name '*.c' -o -name '*.cpp' -o -name '*.h' \) -exec grep -hE \
  'get_devcfg_(value|value_hex|string|string_pair_at)|^[[:space:]]*#define[[:space:]][A-Z0-9_]+[[:space:]]+"(DEVICE|DEBUG|EDITOR|SERIAL|DISABLE)[A-Z0-9_]+"|snprintf\([^;]*"(DEVICE|DEBUG|EDITOR|SERIAL|DISABLE)' {} + |
sed -n \
  -e 's/.*get_devcfg_[a-z_]*( *"\([A-Z][A-Z0-9_]*\)".*/\1/p' \
  -e 's/^[[:space:]]*#define[[:space:]][A-Z0-9_]*[[:space:]]*"\([A-Z][A-Z0-9_]*\)".*/\1/p' \
  -e 's/.*snprintf([^;]*"\([A-Z][A-Z0-9_]*%[0-9]*[diu][A-Z0-9_]*\)".*/\1/p' |
sed 's/%[0-9]*[diu]/<n>/g' | sort -u > "$tmp"

missing=0
while IFS= read -r key; do
  if ! grep -Fq "$key" "$doc"; then
    echo "missing configuration key in docs/configuration.md: $key" >&2
    missing=1
  fi
done < "$tmp"

if [ "$missing" -ne 0 ]; then
  exit 1
fi

echo "configuration documentation covers $(wc -l < "$tmp" | tr -d ' ') keys"
