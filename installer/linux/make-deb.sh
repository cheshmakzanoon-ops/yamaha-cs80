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

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
stage="$work/package"
mkdir -m 0755 "$stage"
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
# Infer ABI minimums from both delivered ELF files. The explicit audio/window
# libraries below also cover JUCE libraries loaded dynamically (not DT_NEEDED).
# Do not guess or publish a package when dependency metadata is unavailable.
command -v dpkg-shlibdeps >/dev/null || { echo "dpkg-shlibdeps is required (install dpkg-dev)" >&2; exit 1; }
command -v readelf >/dev/null || { echo "readelf is required (install binutils)" >&2; exit 1; }
command -v realpath >/dev/null || { echo "realpath is required (install coreutils)" >&2; exit 1; }
# cp -a preserves VST3 links, while the scanner follows them. Only scan a payload
# actually delivered inside the bundle, including relative in-bundle links.
stage_real="$(cd "$stage" && pwd -P)"
plugin_real="$(realpath -e -- "$stage/usr/lib/vst3/$product.vst3/Contents/x86_64-linux/$product.so")" \
    || { echo "unresolvable packaged VST3 payload" >&2; exit 1; }
case "$plugin_real" in
    "$stage_real/usr/lib/vst3/$product.vst3/"*) ;;
    *) echo "VST3 payload resolves outside packaged bundle: $plugin_real" >&2; exit 1 ;;
esac
# dpkg-shlibdeps only warns for some non-runtime inputs, including relocatable
# ELF objects. One invalid product must not be hidden by the other's dependencies.
check_elf_type() {
    local binary="$1" role="$2" header elf_type
    header="$(LC_ALL=C readelf -h "$binary")" || { echo "invalid ELF product: $binary" >&2; exit 1; }
    elf_type="$(printf '%s\n' "$header" | sed -n 's/^[[:space:]]*Type:[[:space:]]*\([^[:space:]]*\).*/\1/p')"
    case "$role:$elf_type" in
        VST3:DYN|Standalone:EXEC|Standalone:DYN) ;;
        *) echo "invalid ELF type for $role: $elf_type ($binary)" >&2; exit 1 ;;
    esac
}
check_elf_type "$stage/usr/lib/vst3/$product.vst3/Contents/x86_64-linux/$product.so" VST3
check_elf_type "$stage/usr/bin/cs80-reference" Standalone
mkdir "$work/debian"
printf 'Source: cs80-reference\n\nPackage: cs80-reference\nArchitecture: any\nDescription: CS-80 Reference\n' > "$work/debian/control"
runtime_deps="$(cd "$work" && dpkg-shlibdeps -O \
    -e"$stage/usr/lib/vst3/$product.vst3/Contents/x86_64-linux/$product.so" \
    -e"$stage/usr/bin/cs80-reference")"
runtime_deps="$(printf '%s\n' "$runtime_deps" | sed -n 's/^shlibs:Depends=//p')"
[ -n "$runtime_deps" ] || { echo "no ELF runtime dependencies found" >&2; exit 1; }
cat > "$stage/DEBIAN/control" <<CONTROL
Package: cs80-reference
Version: $version
Architecture: $arch
Maintainer: Reference Instruments <noreply@referenceinstruments.invalid>
Depends: $runtime_deps, libasound2t64 | libasound2, libfreetype6, libfontconfig1, libx11-6, libxext6, libxrandr2, libxinerama1, libxcursor1
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
