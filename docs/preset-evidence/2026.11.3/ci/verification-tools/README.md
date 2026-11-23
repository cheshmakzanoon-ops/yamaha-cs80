# Actual-archive acceptance tools

These tools require **Python 3.11 or later** and **Git**. They inspect downloaded
GitHub Actions ZIP bytes without executing the plug-in or standalone application
and use no third-party Python packages. For actual archive verification, run
them from the repository root or pass `--repo` explicitly. The selected checkout
must contain the requested full source commit in its Git object database.

The installation and payload tools also call the repository's unchanged
`scripts/verify-ci-bundle.py`, after checking that it and its packaging module
match the requested source commit. Obtain each individual artifact API object,
including its size, digest, source and run, from GitHub; do not substitute a
handwritten expected digest or a surrounding artifact-list object. The audition
tool takes the API digest through `--expected-zip-sha256`, with the `sha256:`
prefix removed. Do not derive that expected digest from the downloaded ZIP
itself: it must come from the independent artifact API record.

| Tool | Acceptance scope |
| --- | --- |
| `verify-install-artifact.py` | Outer ZIP identity; inner archive and complete inventory; source, library revision, run and JUCE provenance; native executable format, role and architecture. |
| `verify-audition-zip.py` | Outer digest; complete source/run/attempt provenance and roster; all parameter vectors and digests; actual WAV headers, frames, duration, finite samples, raw levels and checksums. |
| `verify-validator-payload.py` | Complete selected Windows/Linux VST3 bundle equality between an installation archive and the separate artifact consumed by pluginval, including resources and link targets. The exact Windows compiler sidecar pair is separately identified under the contract below. |

### Windows validator container boundary

The Windows upload contains the complete `Release/VST3` output directory. Its
top-level `CS-80 Reference.exp` export object and `CS-80 Reference.lib` import
library sit beside the `CS-80 Reference.vst3` bundle. Both required Windows
validator commands explicitly select that bundle. The retained checker now
recognizes **only that exact pair of regular top-level Windows files** as
compiler sidecars. It requires bounded, valid AMD64 COFF/export and import-archive
structures, records both files' actual sizes and SHA-256 hashes, and rejects
unknown outside members, a partial pair, and either sidecar on Linux.

When those sidecars are present, pass **`--consumer-log`** with the original
Windows validator job log. The checker reads the workflow at the requested
source commit and requires the log to identify the exact downloaded artifact
ID, size and digest, followed by both resolved bundle-only validation paths.
It records this evidence as `validator_container_contract`. These checks bind
the selected payload; successful original process exits remain a separate gate.

Every file, resource, directory and symbolic link **inside the selected VST3
bundle still participates in strict equality**. An `.exp` or `.lib` name inside
that bundle receives no exception. Parent-directory inference and archive
permission-normalization exclusions retain their original scope.

The actual Windows comparison for source `cbbee6a940985ad3c550768414960d2e1b1e802e`
passed with two identical payload files after this offline-tool correction.
The [correction record](history/windows-sidecar-correction.json) preserves the
unmodified checker's rejection, its original bytes, exact consumer evidence,
the corrected acceptance, and three positive / 22 rejected fixture cases.
No production source, packaging code, workflow or validator gate changed.

Use `--help` for the required archive, API, source, run and output arguments.
The JSON acceptance file is written only after its checks pass. Installation
and payload tools provide `--self-test` with synthetic positive fixtures and
coherent negative mutations. **Run self-tests from the repository root:** their
fixture helpers use the current directory and do not apply the `--repo`
override. Fixture success is not acceptance of a release archive.
Run those synthetic self-tests on Linux or macOS, where their Unix permission
and symbolic-link fixtures are available. Actual archive inspection reads
those attributes from the archive and does not install or execute the payload.

These tools do not establish that a producing job or validator passed. The
release qualification record separately binds each artifact to its successful
producer/upload step and run attempt, the native test results, and the required
original validator process outcomes. Neither checksums nor these inspectors
constitute publisher signing, notarization, acoustic-equivalence measurements
or certification of arbitrary DAWs.

`tool-provenance.json` records the exact retained tool hashes. All three tools
initially received a portable default repository path; the payload checker also
contains the documented Windows container correction above. Its earlier bytes
remain in `history/verify-validator-payload-before-windows-sidecars.py` for the
recorded rejected invocation and are not the current acceptance tool. Actual
release acceptance uses the current retained copies and records their hashes.
