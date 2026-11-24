# A30 — 2026.11.7 installation acceptance contract

Reuse the unchanged generic helper at
`docs/preset-evidence/2026.11.5/verification/a41-verify-install-artifact.py`.
Its file SHA-256 is
`803ff4378746937039400e777ff61bfce8ebaeb2ab1bba6699fe76076f520305`.
Do not copy it under a new revision name or change frozen historical evidence.

## Exact source context

| Input | Required value |
| --- | --- |
| Repository | `cheshmakzanoon-ops/yamaha-cs80` |
| `--source` | `c5549f7d095a5c5f49a5ad22ec0ccc819245d201` |
| Source tree | `469244af34233cfa145537266efd29024b66abba` |
| `--revision` | `2026.11.7` |
| `--run-id` | `45385716988` |
| `--attempt` | `1` |
| JUCE | `8.0.11`, commit `c352e2489000ffeb9e397051377d75eef2efe641` |

The compiled source is `c5549f7d…`. The later documentation-only main checkpoint
`e8e29133a14c492a98033871cfe3912ef8978f5d` is not the archive's source argument.
Source/revision are explicit inputs; the helper checks the revision declaration
at that commit and loads its packager and complete bundle checker with
`git show`. It records five source-contract file hashes and the source tree.

The run/API files must be unwrapped JSON objects for the reviewed run and
artifact. The helper checks their repository identities, source, run, expected
target name, URL/ID consistency, nonexpired state, exact outer byte count and
SHA-256. Supplied receipt authentication remains an acquisition responsibility.

## All four target mappings

| Target | `--platform` | `--architecture` | Exact artifact name | Exact inner archive |
| --- | --- | --- | --- | --- |
| Windows x86-64 | `windows` | `x86_64` | `cs80-windows-install-bundle` | `cs80-reference-windows-x86_64-c5549f7d095a.zip` |
| ARM Mac | `macos` | `arm64` | `cs80-macos-15-install-bundle` | `cs80-reference-macos-arm64-c5549f7d095a.tar.gz` |
| Intel Mac | `macos` | `x86_64` | `cs80-macos-15-intel-install-bundle` | `cs80-reference-macos-x86_64-c5549f7d095a.tar.gz` |
| Linux x86-64 | `linux` | `x86_64` | `cs80-linux-install-bundle` | `cs80-reference-linux-x86_64-c5549f7d095a.tar.gz` |

Each outer Actions ZIP must contain exactly the inner archive above,
`cs80-reference-<platform>-<architecture>-c5549f7d095a.manifest.json`, and
`SHA256SUMS`, at its root. `SHA256SUMS` must contain exactly the inner archive
and sidecar manifest checksums. The embedded `manifest.json` must equal the
sidecar bytes.

| Target | Required native payloads, relative to the inner root | Header contract |
| --- | --- | --- |
| Windows x86-64 | `VST3/CS-80 Reference.vst3/Contents/x86_64-win/CS-80 Reference.vst3`; `Standalone/CS-80 Reference.exe` | x86-64 PE32+, machine `0x8664`; executable flag set; DLL flag set only for VST3 |
| ARM Mac | `VST3/CS-80 Reference.vst3/Contents/MacOS/CS-80 Reference`; `AU/CS-80 Reference.component/Contents/MacOS/CS-80 Reference`; `Standalone/CS-80 Reference.app/Contents/MacOS/CS-80 Reference` | ARM64 Mach-O, CPU `0x0100000c`; VST3/AU image kind 6 or 8; standalone kind 2 |
| Intel Mac | Same three Mac bundle paths | x86-64 Mach-O, CPU `0x01000007`; VST3/AU image kind 6 or 8; standalone kind 2 |
| Linux x86-64 | `VST3/CS-80 Reference.vst3/Contents/x86_64-linux/CS-80 Reference.so`; `Standalone/CS-80 Reference` | little-endian ELF64, machine 62; VST3 kind 3; standalone kind 2 or 3 |

Mac inventory also requires each bundle's `Contents/Info.plist`. The helper
requires native executable payloads to be regular files and verifies their
headers without loading them. Mac universal binaries are accepted if the
advertised slice and role pass; this is not proof that a binary has only that
slice. The checked actual ARM archive has three thin ARM64 Mach-O payloads.

## Inventory, permissions and links

The source-pinned bundle checker verifies every declared member's type, size,
SHA-256 and membership; rejects missing, additional, duplicate and unsafe paths;
and requires sidecar/embedded manifest equality. TAR modes must match the
inventory. All Mac/Linux native executables must carry an execute bit. ZIP
Unix-mode equality is intentionally not required for Windows, where native
executable identity comes from PE headers.

Mac/Linux symlinks must remain relative, resolve through inventoried objects
inside their original product bundle and avoid dangling/cyclic targets or
archived descendants beneath a nondirectory. Windows bundles reject symlinks.
This establishes the supported archive structure, not filesystem xattrs, ACLs,
signing validity or installer behavior.

The generic checker establishes a complete **declared** inventory. It does not
derive all expected notices/guides from the source, so its pass alone is
insufficient for those requirements. The current source supplement additionally
requires these 13 metadata files, regular, nonempty and mode `0644`:

- `INSTALL_AND_QUICKSTART.md`, `PRESET_GUIDE.md`, `DISTRIBUTION_STATUS.md`.
- `LICENSE`, `SOURCE.txt`, `evidence/juce-provenance.txt`.
- `licenses/JUCE-LICENSE.md`, `licenses/BarlowCondensed-OFL.txt`,
  `licenses/BarlowCondensed-SOURCE.txt`, `licenses/MTS-ESP-LICENSE.txt`,
  `licenses/libxml2-Copyright.txt`, `licenses/libxml2-SOURCE.txt`,
  `licenses/libxml2-ADDITIONAL-NOTICES.txt`.

`inspect-packaged-docs.py` binds the actual ZIP to a passing A41 receipt before
comparing the three guides with `packaged_markdown` loaded from the exact source
commit. Bundled guide links become local filenames; other relative repository
links become commit-pinned URLs. The function preserves external URLs,
fragments and fenced examples. An independent output scan also rejects any
remaining relative inline link to an unbundled repository file. This is the
source's inline-Markdown contract, not a general Markdown/HTML link validator.

The supplement compares `LICENSE`, both Barlow notices, the MTS notice and the
three libxml2 notices verbatim with their source-commit files. A41 already checks
exact `SOURCE.txt` and clean pinned JUCE provenance. The JUCE license's presence
and inventory hash are checked here; its dependency-source provenance relies on
the original clean JUCE check followed by the source-pinned packager reading
`build/_deps/juce-src/LICENSE.md`. No independent pinned JUCE checkout was
available for a second source-byte comparison in this audit.

The source-time guides intentionally retain pending qualification language and
historical `.4` download records. They are not final release publication. Later
online guide/download updates must be separately reviewed; they do not rewrite
the accepted archive or alter its source identity.

## Exact commands used for the actual ARM archive

Run from the repository root. Existing report paths are never overwritten;
choose fresh output filenames/directories for a deliberate rerun.

```sh
mkdir -p /tmp/cs80-a30-1cee5a docs/preset-evidence/2026.11.7/verification/a30
TMPDIR=/tmp/cs80-a30-1cee5a PYTHONDONTWRITEBYTECODE=1 python3 \
  docs/preset-evidence/2026.11.5/verification/a41-verify-install-artifact.py \
  --repo /workspace/scratch/1cee5a9391a9/yamaha-cs80 \
  --source c5549f7d095a5c5f49a5ad22ec0ccc819245d201 \
  --revision 2026.11.7 --run-id 45385716988 --attempt 1 \
  --run-api docs/preset-evidence/2026.11.7/ci/source-c5549f7d/macos-arm64/run-api-at-platform-completion.json \
  --platform macos --architecture arm64 \
  --archive /tmp/cs80-ci-1cee5a/c5549f7d/macos-arm64-install.zip \
  --artifact-api docs/preset-evidence/2026.11.7/ci/source-c5549f7d/macos-arm64/install-api.json \
  --out docs/preset-evidence/2026.11.7/verification/a30/macos-arm64-install-acceptance.json

TMPDIR=/tmp/cs80-a30-1cee5a PYTHONDONTWRITEBYTECODE=1 python3 \
  docs/preset-evidence/2026.11.7/verification/a30/inspect-packaged-docs.py \
  --repo /workspace/scratch/1cee5a9391a9/yamaha-cs80 \
  --source c5549f7d095a5c5f49a5ad22ec0ccc819245d201 \
  --archive /tmp/cs80-ci-1cee5a/c5549f7d/macos-arm64-install.zip \
  --acceptance docs/preset-evidence/2026.11.7/verification/a30/macos-arm64-install-acceptance.json \
  --out docs/preset-evidence/2026.11.7/verification/a30/macos-arm64-docs-and-notices.json

TMPDIR=/tmp/cs80-a30-1cee5a PYTHONDONTWRITEBYTECODE=1 python3 \
  docs/preset-evidence/2026.11.7/verification/a30/rerun-existing-checks.py \
  --repo /workspace/scratch/1cee5a9391a9/yamaha-cs80 \
  --source c5549f7d095a5c5f49a5ad22ec0ccc819245d201 \
  --revision 2026.11.7 \
  --out-dir docs/preset-evidence/2026.11.7/verification/a30
```

For each remaining target use its row's platform/architecture, the original
downloaded outer ZIP, its own artifact API receipt, the reviewed same-source
run/attempt receipt and a new output path. Then run the same documentation
supplement with that outer ZIP and its passing report. No other actual target
was inspected in A30; the four-target synthetic fixtures are not qualifications.

## Retained results and scope

The actual ARM API ID is `14918661184`; the outer ZIP is **14,989,442 bytes**, SHA-256
`e5839607c8c49d7a9859aee896277c9773ae16359dff59e8b11d1564fa9da545`.
The inner TAR is **14,980,692 bytes**, SHA-256
`eb0a75f60ae203f5d6ddbe3750769ef832490ec63bc0dea7abc8c5a565cc8339`.
The inventory has **39 entries: 26 files, 13 directories, zero symlinks**.
All three native executable headers pass and modes are `0755`.

All three guide comparisons and seven repository-notice comparisons passed.
The guides contain 175 inspected inline targets: 7 local bundled-guide targets,
127 exact-source-pinned targets and 41 existing external/fragment targets.

The unchanged A41 suite was imported and its fixture source/revision constants
rebound only in memory to `.7`. All 11 existing methods passed: four positive
target fixtures and 22 rejected mutations, plus snapshot/publication/scope
controls. Three existing source-pinned methods covering Unix modes, virtual
links and native image roles also passed. Total: **14 methods, zero failures,
zero errors**. Fixture-builder working-tree scripts were byte-compared with
the requested source before use. No new test body or historical file changed.

The passing report preserves `release_qualified: false`,
`native_validation_passed: null`, and
`artifact_attempt_binding_verified: false`. The supplied run snapshot was
`in_progress` with null conclusion; the package/receipt does not declare a
producing attempt. Context matches attempt 1, but upload-attempt binding must
come from the original successful job and upload records.

**Current `.7` gate correction:** the unchanged A41 `remaining_gates` prose says
13 pluginval lanes. The current source workflow requires **15**:
Linux 3 + Windows 2 + ARM Mac 5 + Intel Mac 5. Treat that historical prose as a
known limitation, and use the current workflow/platform reviews for the exact
roster. Do not modify a frozen helper merely to refresh this explanatory count.

Original configure/JUCE/build/Test/package/upload success; complete native test
rosters and reviewed skips; all 15 original validator outcomes and distributed
payload bindings; audition inspection; other-platform results; and current
public download availability remain separate gates. A44 owns the actual ARM
platform qualification and can bind these A30 byte/documentation receipts.

Receipts: [archive inspection](macos-arm64-install-acceptance.json),
[guide/notice supplement](macos-arm64-docs-and-notices.json),
[reused checks](existing-checks.json), [test log](existing-checks.log),
[retained file identities](retained-files.json).
