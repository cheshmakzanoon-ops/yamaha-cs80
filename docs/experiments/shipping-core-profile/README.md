# Current-stock DSP sampling evidence

**Baseline hotspot evidence only. Not a speedup, causal performance, native-plugin, DAW, or release-qualification claim.**

This portable bundle exports six completed stock-only captures: eight sounding voices at 48 kHz / 64 frames, two source-driven factory ranks or Panel, held versus moving fixtures, and two requested SIGPROF periods. It contains 25,613 normalized counted PCs, 36 separately counted outside-core interrupts, source/inline annotations, recorded PCM/voice validity metadata, and a code-identity receipt. No ELF binaries, raw ASLR maps, process IDs, user IDs, or private workspace paths are included.

## Findings

Exclusive sampled shares are ranges across the two periods, not confidence intervals:

|Region|Factory held|Factory moving|Panel moving|
|---|---:|---:|---:|
|Oscillator family|42.4–43.6%|42.7–44.6%|42.2–42.5%|
|Filter setter, inline descendants included|0.5–0.7%|13.4–15.0%|16.1–16.3%|
|Batch filter processing|16.3%|9.6–10.7%|12.6%|
|Source-readout update|7.3–7.8%|4.7–5.1%|absent|
|Unattributed libm|4.6–4.9%|4.9–5.1%|5.0–5.3%|
|PRA self|1.4–1.5%|1.0–1.2%|1.1%|

Physical top-three rankings reproduce in all scenes. Top-five overlap is 5/5 held, 4/5 moving, and 5/5 panel. Moving-helper ordering is less stable. Main-ELF PCs without a bounded physical symbol remain an explicit unknown bucket, 0.20%–0.46% per capture. Library PCs are never assigned to guessed callers. Inline ancestry is an annotation, not extra samples.

`ReconstructedOscillator.h:309` maps to a stack/store materialization block and accounts for 6.5–6.8% held and 4.57–4.59% moving samples. This motivates investigating copy/liveness behavior, not claiming all that latency is removable. Moving filter preparation divides roughly between cutoff calibration (lines 198–211, 7.4–7.8%) and linear elimination (lines 212–222, 5.0–7.0%).

## Scope and controls

- Shipping production source baseline `e9e8175f4278d2457279fcb8809cbba1ddb7be2b`, normal GCC 14.2 Release/PIC flags. A separate `-g` build preserved every allocatable object section and normalized relocation. Linked sampler code/data addresses and bytes matched the stock-linked executable except the build-ID note.
- Single-process **standalone DSP**, not a VST3 load. Eight held notes `36, 43, 48, 55, 60, 64, 67, 72`, velocity 0.8, drift 0.37, dispersion 0.18. Factory selectors 0/0 are String 1 / String 2, both observed source-inferred/source-driven routes; Panel uses 13/13.
- Held is dry/non-MPE. Moving uses the existing MPE+FX profile and pressure/bend/CC74 changes, so held versus moving is **not a one-factor comparison**.
- `setControlFrame` is called once per 64-frame block. Only this **cadence** matches `PluginProcessor.cpp:1033`. The standalone fixture sends moving events immediately **before** its frame; real `processBlock` applies the frame before event dispatch (around line 1234 onward). Neither host-event decoding nor identical callback ordering/full-plugin cost is represented.
- Each capture settles 1,024 blocks, then samples 20 process-CPU seconds. Capture lengths differ; only the fixed 4,096-block PCM prefixes are compared. Every PCM value is checked for finiteness, and each complete capture is required to contain nonzero signal, not necessarily every individual block. Eight active/held voices with the expected notes/channels are checked every block. The exported verifier checks those **receipts**, not a fresh audio render.
- All calibration PCs hit the known arithmetic-loop function. Linux quantizes/coalesces the 1,999 µs / 5,003 µs CPU timers; actual sample counts are retained. The handler perturbs execution; these are statistical self-PC shares, not precise instruction-cycle measurements.
- Six slots completed once, with no retries, deletion, candidate comparison, privileged profiler or system changes. Sampling children used 127.940 CPU seconds / 128.522 wall seconds; compilation used 30.595 CPU seconds. No DSP is executed by export verification.

## Replay, no binaries required

Python 3 standard library is sufficient:

```sh
python3 verify.py --replay
python3 test_negative.py
```

The verifier demands pinned SHA-256 identities for both modules, exact input anchors, complete annotation/executable-range coverage, fixed scene order, calibration containment, per-capture sample conservation, one exclusive owner per PC, and matching aggregate/PCM/voice receipts. Negative tests deliberately bypass file-hash checks to exercise semantic rejection as well as separately testing byte-anchor mutation.

**Replay is not independent binary authentication.** It checks a fixed, internally consistent evidence export against bundled anchors. The manifest is an integrity ledger, not a digital signature or proof of origin. Do not modify the anchors and treat the changed data as this study.

## Verify against the exact external binaries

With Python 3 plus GNU binutils `addr2line` / `c++filt`, supply the original sampled main executable and libm separately:

```sh
python3 verify.py --module main=PATH_TO_EXACT_PROFILE_DEBUG --module libm=PATH_TO_EXACT_LIBM
```

This requires exact module SHA-256 matches, checks actual ELF executable ranges and physical symbols, and reproduces every main-PC DWARF inline/source mapping. It reads the modules; it does not load or run them.

To additionally reproduce the build identity receipt, provide `--identity-root DIRECTORY` containing:

```text
Engine.normal.o          Engine.debug.o
FactoryPresets.normal.o  FactoryPresets.debug.o
Ig00156Batch.normal.o    Ig00156Batch.debug.o
profile-stock           profile-debug
```

The verifier then rechecks exact object hashes, all allocatable bytes, normalized relocations, and linked code/data/address identity. Exact recorded ELFs are intentionally not distributed here. A rebuild at different source/debug paths may have a different whole-file hash even when code agrees; it is a new artifact and cannot silently substitute for a recorded module.

## Files and data format

- `PLAN.txt`: frozen sampling plan plus an explicit exported scope clarification.
- `profile.cpp`, `collect.py`, `BUILD.md`: method source and reconstruction instructions. They are not invoked by replay/tests. Actual sampling was not rerun for this export.
- `modules.json`: pinned module hashes and ELF-relative executable/calibration ranges.
- `pc-counts.json`: global PC rows `[module, hex ELF virtual address, physical-function ID, inline-chain ID]`; each capture stores positive `[PC index, count]` pairs and normalized calibration counts. Addresses are relative to the module image, not ASLR load addresses.
- `symbols.json`: deduplicated function, source-frame and inline-chain dictionaries. System/compiler source categories are portable labels; project locations are relative to `src`.
- `captures.json`: scene metadata and independently replayed exclusive/physical counts. PCM fingerprints are FNV-1a64 diagnostics, not cryptographic hashes.
- `source-hashes.json`, `build-identity.json`: source inventory and code-identity receipt.
- `pins.py`, `manifest.json`: fixed content anchors and complete bundle integrity ledger.
- `validation.json`, `negative-tests.json`: export-time validation receipts, without local paths.
