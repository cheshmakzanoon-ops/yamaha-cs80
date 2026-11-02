# Independently read downloaded a6 native CI archives

The Linux, Windows and Intel macOS development install bundles from source
`a6e568543f3d194f10f75af2216bd1dc686ff3e5`, build run41427704633, were downloaded
through the official GitHub artifact API. Each outer ZIP matches GitHub's artifact
SHA-256 and passes CRC. The pinned one-off read-only verifier separately checks
nested hashes/SHA256SUMS, exact member inventories, embedded/external manifests,
POSIX modes where applicable, safe relative links, eight source notices and
primary executable headers. It does not execute a packaged binary.

Observed inventories: Linux16 members/two ELF64 x86_64 executables, Windows16
two PE x86_64 executables, Intel macOS33/three Mach-O64 x86_64 executables. The
macOS archive is native Intel, not universal. Source notices compare against exact
a6 Git blobs and the pinned upstream JUCE licence. Windows has an observed exact
LF-to-CRLF checkout conversion for affected tracked files; it is **not raw Git-blob
byte identity**. SOURCE.txt is compared independently, not trusted merely because
it is included in its own manifest. Exact original manifests/hashes are retained.

The full build run is not green: its ARM job was cancelled, with no downloadable
job log. The three relevant native jobs and validator jobs succeeded, but their
archive contents alone are not numerical or host qualification. These ZIP/tar
archives are **not** execution of the later unpublished .deb/.pkg/Inno installer
checks or the new binary-input verifier. Signing validity, compiler/source
identity, real DAWs, final CPU/audio gates and release remain unqualified.

No compiled payloads are committed here. The reusable artifact IDs and all
outer/nested hashes are in downloaded-archive-verification.json. The archived
script records the exact local paths used for this bounded independent read.
