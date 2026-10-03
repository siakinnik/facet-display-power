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
# The manifest written by the build records the SDK version.
manifest="$(dirname "$bin")/manifest.json"
[[ -f "$manifest" ]] || manifest="$root/manifest.json"
install -m644 "$manifest" "$stage/manifest.json"
for f in LICENSE README.md; do
    [[ -f "$root/$f" ]] && install -m644 "$root/$f" "$stage/$f"
done

# The executable is static: the licenses of what is built into it, the
# packages they come from (licenses/STATIC) and the full license texts.
bash "$root/scripts/ci/licenses.sh" "$stage" "${CXX:-g++}" libc.a libstdc++.a libgcc_eh.a
mkdir -p "$out"
name="facet-display-power-$version-linux-$arch.tar.gz"
tar -C "$(dirname "$stage")" -czf "$out/$name" display-power
echo "$out/$name"
