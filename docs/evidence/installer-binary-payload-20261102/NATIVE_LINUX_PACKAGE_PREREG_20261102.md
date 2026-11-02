# Actual Linux package transport check — preregistration

This bounded local check uses the **already downloaded development artifact**, not
a newly compiled engine and not a release. It must run under the shared heavy
lock on native Linux x86_64. No payload executable is launched, no package is
installed into the system, and no publication or signing is performed.

## Frozen inputs and source separation

- Binary archive: GitHub artifact `13135277790`, build run `41427704633`, published
  source `a6e568543f3d194f10f75af2216bd1dc686ff3e5`.
- Outer ZIP: `cs80-a6-linux-install-bundle.zip`, 12,485,871 bytes,
  SHA-256 `40be0750f5a5606bd6c90decf37b83c5b880a5c35633e8ff277e70b6f69814ee`.
- Reviewed packaging code anchor: `e55540b098af022137c1f90582386673b683bca2`.
  The runner compares all five packaging/helper file bytes with that commit.
- The actual clean packaging checkout SHA is recorded at execution. Its notices
  identify **that packaging checkout**, while the separate receipt preserves the
  older binary input's a6 identity. Equal `src` trees do not independently prove
  compiler/source equivalence.
- The JUCE licence is taken from the pinned archive and must have SHA-256
  `2633539bb26d244f0966fbc4df59400ea99bdf575fa96291851c1c3ff3456146`.

## Registered procedure and outcomes

1. Verify the exact outer ZIP hash/size/CRC, its three entries and nested manifest
   SHA256SUMS. Verify exact nested inventory, safe member types/paths, all file
   hashes/sizes and POSIX modes before materializing only the VST3/Standalone
   components into a fresh build-input directory outside the checkout.
2. Record native `dpkg-deb`, Python/platform and source identities. Run unchanged
   `qualify_native_installer.py --platform linux` from the clean checkout. It must
   actually build and expand one `.deb`, verify documents and binary transport,
   and produce its distinct CI-UNSIGNED receipt. Any command error stops the check.
3. Independently expand the resulting `.deb` a second time with `dpkg-deb`, confirm
   package name/version/architecture and compare the two executables with the
   original archived bytes and independently read their ELF64 little-endian x86_64
   headers (without executing them). Check the five project/MTS/font/quickstart
   notice files against `git show PACKAGING_SHA:path`, plus the pinned JUCE licence and exact
   generated SOURCE text: seven required installer documents in total, plus Debian
   copyright and the generated manifest. The CI-only DISTRIBUTION_STATUS.md is not
   an installer document. Keep raw logs and hashes.
4. On this second expansion only, change one byte of the actual Standalone;
   `verify_installed_payload` must reject specifically for a binary byte/hash
   mismatch. Restore it and require the positive again. Then change its executable
   mode from 0755 to 0644; reject specifically for mode mismatch, restore and pass.
   A mismatched requested source SHA must be refused by the document verifier.
   Exceptions of unrelated type/reason, runtime crashes, skips or missing output
   cannot qualify these controls.
5. Require the runner itself to equal its committed checkout file, reject output
   inside the source checkout, and recheck the five packaging-code bytes at the
   end. Each native command has a 180-second runtime budget after lock acquisition;
   timeout/failure cleanup targets its own process group while the shared lock is
   retained. This is bounded command cleanup, not a hostile-writer sandbox.
6. Freeze a receipt and artifact hashes. Preserve any first failed run unchanged;
   corrections require a new output directory and explicit explanation.

The stopping condition is one actual native package/second-expansion positive and
all three intended negative controls, or a precisely recorded real failure.
This can qualify **Linux package transport only**. It does not establish fresh
compilation, binary execution, audio, CPU, DAW compatibility, universal macOS,
Windows installation, signing, final-main acceptance or a distributable release.

## Registration history and invocation

An earlier uncommitted draft in `cs80-installer-binaries` named the pre-source-origin
anchor fcbb0e7. It was never evaluated. This committed preregistration instead binds
the reviewed raw-notice repair at e555 and the separately committed runner; no
result is inferred from the draft. Runtime and output-directory failures remain
failed observations, not assertion kills. The three negative controls mutate only
the second extracted copy, never the downloaded binary archive or source checkout.

Run from the clean registration commit, under the shared lock:

```sh
flock /workspace/scratch/aa102d573ba3/cs80-heavy-check.lock \
  python3 tests/release/qualify_actual_linux_package.py \
  --source-root /workspace/scratch/aa102d573ba3/cs80-distribution-source \
  --bundle /workspace/scratch/aa102d573ba3/cs80-release-artifacts/a6e5685/native-install-bundles/cs80-a6-linux-install-bundle.zip \
  --out /workspace/scratch/aa102d573ba3/cs80-native-linux-evidence/actual-e555-first
```

The output must be fresh. No full package can be called qualified until actual
execution, independent expansion and all registered controls terminate successfully.
