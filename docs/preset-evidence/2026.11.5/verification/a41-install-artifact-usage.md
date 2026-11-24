# Parameterized native installation acceptance

This checker inspects the actual outer GitHub Actions ZIP for any of the four
supported installation targets. It does not execute a downloaded product. A
passing report always retains `release_qualified: false`,
`native_validation_passed: null`, and
`artifact_attempt_binding_verified: false`.

## Invocation

Run from the repository, with the exact compiled source object present in Git.
The artifact API JSON must be the unwrapped artifact object, and the run API
JSON must be the original run/attempt object. Obtain both through authenticated
GitHub reads; this offline checker cannot establish their authenticity.

```sh
python3 docs/preset-evidence/2026.11.5/verification/a41-verify-install-artifact.py \
  --repo /absolute/path/to/yamaha-cs80 \
  --source FULL_40_CHARACTER_COMPILED_SOURCE_SHA \
  --revision 2026.11.5 \
  --run-id ACTUAL_BUILD_RUN_ID \
  --attempt ACTUAL_REVIEWED_ATTEMPT \
  --run-api /absolute/evidence/run-attempt-api.json \
  --platform windows \
  --architecture x86_64 \
  --archive /absolute/downloads/windows-install.zip \
  --artifact-api /absolute/evidence/windows-install-api.json \
  --out /absolute/evidence/windows-install-acceptance.json
```

The output path must be new. An existing regular file or symlink is never
replaced. Parent directories are created as needed.

| Target | `--platform` | `--architecture` | Required artifact name |
|---|---|---|---|
| Windows x64 | `windows` | `x86_64` | `cs80-windows-install-bundle` |
| Linux x86-64 | `linux` | `x86_64` | `cs80-linux-install-bundle` |
| Apple Silicon Mac | `macos` | `arm64` | `cs80-macos-15-install-bundle` |
| Intel Mac | `macos` | `x86_64` | `cs80-macos-15-intel-install-bundle` |

The expected repository defaults to `cheshmakzanoon-ops/yamaha-cs80`.
`--repository` permits an explicit alternative; repository API identities,
artifact URLs and packaged `SOURCE.txt` must agree with it.

## Established by a successful byte inspection

The three external files are read once into bounded immutable snapshots. Their
hashes identify the same bytes that are parsed or inspected. The checker rejects
nonregular external files and unexpected symlinks, duplicate JSON keys, unsafe
or unexpected outer ZIP topology, encryption and excessive sizes. The outer
ZIP must contain exactly the expected inner archive, sidecar manifest and
`SHA256SUMS`.

The complete bundle verifier and native header checker come directly from the
requested Git commit. The checker records their SHA-256 values, the source tree,
that commit's workflow, JUCE source-check script and factory declaration. The
explicit expected factory revision must match the declaration. No current
working-tree verification script is used.

The source-pinned bundle verifier checks every inner inventory entry, byte hash,
size, mode and supported link target, together with complete archive membership,
embedded/sidecar manifest equality and checksums. The A41 layer additionally
checks source/revision/run declarations, the known JUCE 8.0.11 commit and clean
source provenance, exact `SOURCE.txt`, and all advertised native executable
headers and roles. Linux and Mac executables must retain executable inventory
modes. The report includes the complete manifest inventory and per-executable
sizes/hashes/header observations.

A source-pinned verifier is trusted repository code. Loading it executes that
verification code; it never loads or executes a downloaded plugin binary.

## Separate original-execution gates

A supplied run API object binds the expected run and attempt as **context**.
Current artifact receipts and package manifests do not bind their producing
attempt. Even a successful native run in that supplied object cannot qualify
these bytes by itself. Independently require:

1. Authentic original producer and upload records binding artifact ID, digest,
   byte count and creation/upload times to the reviewed source/run/attempt.
2. Successful configure, clean JUCE verification, build, complete native CTest
   roster, reviewed skips, packaging and upload steps for each platform.
3. All thirteen original pluginval lanes with version, strictness, program
   counts, seeds, GUI coverage, exit propagation and eligible preserved retries.
4. The independent Windows/Linux and Mac checks that bind distributed VST3/AU
   bundles to the exact inputs actually validated.
5. The actual audition archive, remaining platforms and current download
   availability before publishing a consolidated release qualification.

The report records current API expiry metadata. It does not independently test
a public download route. Header inspection proves advertised architecture and
image role, not runtime validity, exclusive universal-binary slices, DAW
compatibility or signing/notarization. No acoustic-equivalence or listening
approval is inferred.

## Retained bounded synthetic verification

`a41-test-install-artifact.py` contains eleven test methods. The retained run
passed all eleven, including four positive inert target fixtures and twenty-two
rejected mutations. It exercises coherent wrong-CPU artifacts, external/API
identity failures, revision mismatch, duplicate metadata, outer membership,
inner checksum corruption, immutable snapshots and no-overwrite publication.
A synthetic failed run remains recorded as failure while byte inspection still
leaves every release/native gate open.

The probe uses the historical helper only to construct inert header/archive
fixtures in temporary directories, with an explicitly assigned fixture revision.
No historical source or evidence is modified or accepted as a current result.
The actual checker loading test uses committed source
`6094bce18c8b19da804c2fbc9a00e31969a7f579`.

The test runner writes a fresh `a41-synthetic-results.json`; preserve or move a
prior result before deliberately rerunning it. Synthetic fixture results are
not native product, CI or download qualifications.
