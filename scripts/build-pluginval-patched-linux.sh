#!/usr/bin/env bash
# Build the *host validator*, not the plugin, with JUCE's proven Linux VST3
# RunLoop callback-lifetime repair. Keep pluginval's v1.0.4 tests and CLI intact.
#
# Upstream crash report: https://github.com/Tracktion/pluginval/issues/185
# Upstream fix: https://github.com/juce-framework/JUCE/commit/04e167d64661d3298afd4978f6c2e77f841b5f17
#
# Usage: build-pluginval-patched-linux.sh <source-directory> <build-directory> <output-binary>
set -euo pipefail

if [[ $# -ne 3 ]]; then
    echo "usage: $0 <source-directory> <build-directory> <output-binary>" >&2
    exit 2
fi

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source_dir="$1"
build_dir="$2"
binary="$3"
pluginval_sha=ed19c2c16b57a6d94db391bea3ef4a80b769d5bf
juce_sha=5179f4e720d8406ebd1b5401c86aea8da6cc83c9
patch_file="$repo_dir/scripts/patches/pluginval-juce-vst3-runloop-04e167d.patch"

if [[ -e "$source_dir" || -e "$build_dir" ]]; then
    echo "Refusing to overwrite an existing pluginval source/build directory" >&2
    exit 2
fi

git clone --depth 1 --branch v1.0.4 --recurse-submodules --shallow-submodules \
    https://github.com/Tracktion/pluginval.git "$source_dir"

actual_pluginval_sha="$(git -C "$source_dir" rev-parse HEAD)"
actual_juce_sha="$(git -C "$source_dir/modules/juce" rev-parse HEAD)"
if [[ "$actual_pluginval_sha" != "$pluginval_sha" || "$actual_juce_sha" != "$juce_sha" ]]; then
    echo "Pinned validator checkout mismatch: pluginval=$actual_pluginval_sha JUCE=$actual_juce_sha" >&2
    exit 1
fi

# JUCE 8.0.3 has CRLF source; use whitespace-independent hunk matching while
# preserving all original pluginval validation code and replacing only host
# RunLoop callback ownership. Applying against wrong/unpatched code must fail.
git -C "$source_dir/modules/juce" apply --check --ignore-whitespace "$patch_file"
git -C "$source_dir/modules/juce" apply --ignore-whitespace "$patch_file"
python3 "$repo_dir/scripts/verify-pluginval-runloop-fix.py" \
    "$source_dir/modules/juce/modules/juce_audio_processors/format_types/juce_VST3PluginFormat.cpp"

cmake -S "$source_dir" -B "$build_dir" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DJUCE_BUILD_EXTRAS=OFF -DJUCE_BUILD_EXAMPLES=OFF
# JUCE's large amalgamated translation units have a high per-job RAM peak;
# bound parallelism so hosted Linux runners don't OOM during validation.
cmake --build "$build_dir" --target pluginval --parallel 2

mkdir -p "$(dirname "$binary")"
cp "$build_dir/pluginval_artefacts/Release/pluginval" "$binary"
"$binary" --version
sha256sum "$binary"
