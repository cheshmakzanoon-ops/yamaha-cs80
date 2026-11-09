# Native CS80Snapshots bootstrap

This directory contains locally extracted build dependencies; no system packages or settings were changed.

## Sources and pins

- JUCE: https://github.com/juce-framework/JUCE.git, official 8.0.11 commit `c352e2489000ffeb9e397051377d75eef2efe641`, as pinned by the project's CMakeLists.txt and CI. The checkout is clean; see logs/provenance.txt.
- Debian: https://deb.debian.org/debian/dists/trixie/main/binary-amd64/Packages.xz. The downloaded index SHA256 is recorded in logs/provenance.txt. Every .deb was verified against its SHA256 from that official HTTPS index before extraction. Exact package versions, archive paths and hashes are in package-manifest.json and xvfb-manifest.json.
- Existing local CMake 3.31.6 and Ninja 1.11.1.git.kitware.jobserver-1 are reused from ../pitch-power-recovery/tools. System GCC is Debian 14.2.0-19.

## Reproduce with the retained local inputs

Run from /workspace/scratch/ee447c520988:

    python mpe-native-tools/bootstrap.py
    bash mpe-native-tools/configure.sh
    pitch-power-recovery/tools/cmake/data/bin/cmake --build build-mpe-onset-native --target CS80Snapshots -j 2

The separately retained xvfb .deb was extracted with `dpkg-deb -x` into sysroot. CMake requires xvfb-run to register unrelated locale/UI tests. No X server is needed or started for the focused MPE tests. This bootstrap does not provision the complete runtime dependency set for xvfb or promise full UI-suite support.

The local JUCE source override is intentional for local testing and is not eligible for the CI-only provenance script, which expressly rejects all local overrides. It still references the exact clean official CI commit.

Curl/WebKit are absent and unused because the project defines JUCE_USE_CURL=0 and JUCE_WEB_BROWSER=0.

## Logs

- logs/bootstrap.log: dependency staging
- logs/configure-first.log: initial configure reached successful JUCE helper build, then required xvfb-run
- logs/configure.log: final configure
- logs/build.log: CS80Snapshots build
- logs/provenance.txt: tool and source evidence

Local include paths deliberately use `-idirafter` rather than `-I`, preserving GCC's C++ compatibility header ordering. An initial `-I` attempt caused an unqualified `isnan` error in the vendored MTS client; a minimal reproduction established that it was a bootstrap include-order error, not a product change requirement. The failed attempt is retained as logs/build-include-order-failed.log. Product sources were not modified.
