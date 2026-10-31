#!/usr/bin/env bash
# Build a Debian package from an existing Linux Release build. It never builds,
# signs or validates the plugin; it only stages the VST3 bundle and Standalone.
#
#   installer/linux/make-deb.sh <build-dir> <out-dir>
set -euo pipefail

build="${1:?usage: $0 <build-dir> <out-dir>}"
out="${2:?usage: $0 <build-dir> <out-dir>}"
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
product="CS-80 Reference"
artefacts="$build/CS80Reference_artefacts/Release"
version="$(sed -n 's/^project(CS80Reference VERSION \([0-9.]*\).*/\1/p' "$root/CMakeLists.txt")"
arch="$(dpkg --print-architecture)"

[ -n "$version" ] || { echo "cannot read the project version" >&2; exit 1; }
[ -f "$artefacts/VST3/$product.vst3/Contents/x86_64-linux/$product.so" ] || { echo "missing VST3 in $artefacts" >&2; exit 1; }
[ -x "$artefacts/Standalone/$product" ] || { echo "missing Standalone in $artefacts" >&2; exit 1; }

stage="$(mktemp -d)"
trap 'rm -rf "$stage"' EXIT
chmod 0755 "$stage"
mkdir -p "$stage/DEBIAN" "$stage/usr/lib/vst3" "$stage/usr/bin" "$stage/usr/share/doc/cs80-reference"
cp -a "$artefacts/VST3/$product.vst3" "$stage/usr/lib/vst3/"
install -m 0755 "$artefacts/Standalone/$product" "$stage/usr/bin/cs80-reference"
cp "$root/docs/INSTALL_AND_QUICKSTART.md" "$stage/usr/share/doc/cs80-reference/"
cp "$root/LICENSE" "$stage/usr/share/doc/cs80-reference/copyright"
for juce_license in "$build/_deps/juce-src/LICENSE.md" "$root/deps/juce/LICENSE.md"; do
    if [ -f "$juce_license" ]; then
        cp "$juce_license" "$stage/usr/share/doc/cs80-reference/JUCE-LICENSE.md"
        break
    fi
done
[ -f "$stage/usr/share/doc/cs80-reference/JUCE-LICENSE.md" ] || { echo "JUCE licence not found" >&2; exit 1; }
cat > "$stage/DEBIAN/control" <<CONTROL
Package: cs80-reference
Version: $version
Architecture: $arch
Maintainer: Reference Instruments <noreply@referenceinstruments.invalid>
Depends: libasound2t64 | libasound2, libfreetype6, libfontconfig1, libx11-6, libxext6, libxrandr2, libxinerama1, libxcursor1
Section: sound
Priority: optional
Description: CS-80 Reference synthesizer (VST3 and Standalone)
 Circuit-modelled polyphonic synthesizer. Installs the VST3 bundle into
 /usr/lib/vst3 and the Standalone application as cs80-reference.
CONTROL
mkdir -p "$out"
dpkg-deb --build --root-owner-group "$stage" "$out/cs80-reference_${version}_${arch}.deb"
dpkg-deb --info "$out/cs80-reference_${version}_${arch}.deb" >/dev/null
echo "$out/cs80-reference_${version}_${arch}.deb"
