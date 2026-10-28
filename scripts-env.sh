#!/usr/bin/env bash
export ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export PATH="$ROOT/deps/tools/bin:$PATH"
export CPATH="$ROOT/deps/sysroot/usr/include/freetype2:$ROOT/deps/sysroot/usr/include:${CPATH:-}"
export CPLUS_INCLUDE_PATH="$ROOT/deps/sysroot/usr/include/freetype2:$ROOT/deps/sysroot/usr/include:${CPLUS_INCLUDE_PATH:-}"
export LIBRARY_PATH="$ROOT/deps/sysroot/usr/lib/x86_64-linux-gnu:${LIBRARY_PATH:-}"
export LD_LIBRARY_PATH="$ROOT/deps/sysroot/usr/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"
export PKG_CONFIG_PATH="$ROOT/deps/sysroot/usr/lib/x86_64-linux-gnu/pkgconfig:$ROOT/deps/sysroot/usr/share/pkgconfig:${PKG_CONFIG_PATH:-}"
export CMAKE_PREFIX_PATH="$ROOT/deps/sysroot/usr:${CMAKE_PREFIX_PATH:-}"

# JUCE's Linux font backend uses fontconfig. A bare CI container has the
# library but no /etc/fonts tree, so register a deterministic font set here.
mkdir -p "$ROOT/deps/font-cache"
cat > "$ROOT/deps/fontconfig-local.conf" <<EOF
<?xml version="1.0"?>
<fontconfig>
  <dir>$ROOT/deps/fonts/usr/share/fonts/truetype/dejavu</dir>
  <dir>/usr/share/fonts</dir>
  <cachedir>$ROOT/deps/font-cache</cachedir>
  <alias><family>sans-serif</family><prefer><family>DejaVu Sans</family></prefer></alias>
  <alias><family>sans</family><prefer><family>DejaVu Sans</family></prefer></alias>
  <alias><family>Helvetica</family><prefer><family>DejaVu Sans</family></prefer></alias>
  <alias><family>Arial</family><prefer><family>DejaVu Sans</family></prefer></alias>
  <alias><family>Default Sans Serif</family><prefer><family>DejaVu Sans</family></prefer></alias>
</fontconfig>
EOF
# JUCE's Linux typeface scanner uses JUCE_FONT_PATH directly, so point it at
# the extracted DejaVu faces. Without this every string renders blank in a
# container whose /etc/fonts is empty.
export JUCE_FONT_PATH="$ROOT/deps/fonts/usr/share/fonts/truetype/dejavu"
export FONTCONFIG_FILE="$ROOT/deps/fontconfig-local.conf"
export FONTCONFIG_PATH="$ROOT/deps/fontconfig/etc/fonts"
