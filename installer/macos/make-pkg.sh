#!/usr/bin/env bash
# Build a macOS installer package from an existing Release build. The bundles
# are expected to be signed already (Developer ID or ad hoc); this script only
# stages them and, when an installer identity is given, signs the product.
#
#   installer/macos/make-pkg.sh <build-dir> <out-dir> [developer-id-installer-identity]
set -euo pipefail

build="${1:?usage: $0 <build-dir> <out-dir> [installer-identity]}"
out="${2:?usage: $0 <build-dir> <out-dir> [installer-identity]}"
identity="${3:-}"
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
product="CS-80 Reference"
bundle_id="com.referenceinstruments.cs80"
artefacts="$build/CS80Reference_artefacts/Release"
version="$(sed -n 's/^project(CS80Reference VERSION \([0-9.]*\).*/\1/p' "$root/CMakeLists.txt")"
[ -n "$version" ] || { echo "cannot read the project version" >&2; exit 1; }

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
mkdir -p "$work/components" "$out"

component() {
    local name="$1" source="$2" location="$3"
    [ -d "$source" ] || { echo "missing bundle: $source" >&2; exit 1; }
    mkdir -p "$work/root-$name"
    ditto "$source" "$work/root-$name/$(basename "$source")"
    pkgbuild --root "$work/root-$name" --install-location "$location" \
        --identifier "$bundle_id.$name" --version "$version" \
        "$work/components/$name.pkg"
}

component vst3 "$artefacts/VST3/$product.vst3" "/Library/Audio/Plug-Ins/VST3"
component au "$artefacts/AU/$product.component" "/Library/Audio/Plug-Ins/Components"
component app "$artefacts/Standalone/$product.app" "/Applications"

# Licence and install notes, outside the signed bundles.
mkdir -p "$work/root-docs"
juce_license="$build/_deps/juce-src/LICENSE.md"
[ -f "$juce_license" ] || juce_license="$root/deps/juce/LICENSE.md"
cp "$juce_license" "$work/root-docs/JUCE-LICENSE.md"
cp "$root/docs/INSTALL_AND_QUICKSTART.md" "$work/root-docs/"
pkgbuild --root "$work/root-docs" --install-location "/Library/Application Support/Reference Instruments/$product" \
    --identifier "$bundle_id.docs" --version "$version" "$work/components/docs.pkg"

cat > "$work/distribution.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>$product $version</title>
    <options customize="allow" require-scripts="false" hostArchitectures="arm64,x86_64"/>
    <domains enable_localSystem="true"/>
    <choices-outline>
        <line choice="vst3"/>
        <line choice="au"/>
        <line choice="app"/>
        <line choice="docs"/>
    </choices-outline>
    <choice id="vst3" title="VST3 plug-in"><pkg-ref id="$bundle_id.vst3"/></choice>
    <choice id="au" title="Audio Unit plug-in"><pkg-ref id="$bundle_id.au"/></choice>
    <choice id="app" title="Standalone application"><pkg-ref id="$bundle_id.app"/></choice>
    <choice id="docs" title="Licences and notes" enabled="false"><pkg-ref id="$bundle_id.docs"/></choice>
    <pkg-ref id="$bundle_id.vst3">vst3.pkg</pkg-ref>
    <pkg-ref id="$bundle_id.au">au.pkg</pkg-ref>
    <pkg-ref id="$bundle_id.app">app.pkg</pkg-ref>
    <pkg-ref id="$bundle_id.docs">docs.pkg</pkg-ref>
</installer-gui-script>
XML

pkg="$out/CS-80-Reference-$version-macOS-universal.pkg"
sign=()
[ -n "$identity" ] && sign=(--sign "$identity")
productbuild --distribution "$work/distribution.xml" --package-path "$work/components" "${sign[@]}" "$pkg"
pkgutil --check-signature "$pkg" || true
echo "$pkg"
