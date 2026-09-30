#!/usr/bin/env bash
# Packs a release archive facet-display-power-<version>-linux-<arch>.tar.gz
# whose top directory is the plugin directory (manifest.json + executable),
# as expected by facet-core's `get.sh --plugin`.
#   scripts/ci/package.sh <binary> <version> <arch> <out-dir>
set -euo pipefail
bin="$1" version="$2" arch="$3" out="$4"
root="$(cd "$(dirname "$0")/../.." && pwd)"
stage="$(mktemp -d)/display-power"

install -Dm755 "$bin" "$stage/display-power"
install -m644 "$root/manifest.json" "$stage/manifest.json"
for f in LICENSE README.md; do
    [[ -f "$root/$f" ]] && install -m644 "$root/$f" "$stage/$f"
done

mkdir -p "$out"
name="facet-display-power-$version-linux-$arch.tar.gz"
tar -C "$(dirname "$stage")" -czf "$out/$name" display-power
echo "$out/$name"
