# Assignment 55 — final CI audition verification

**Prepared; actual CI artifact verification is pending.** The captured API
snapshot has no run artifacts and all four native jobs are in `Test`.

The fixed target is source
`6d3cc220edef5a4a2c7cd968ea8a786c7c83198c`, build `45044040694`, attempt `1`.
The exact artifact is
`cs80-curated-preset-audition-6d3cc220edef5a4a2c7cd968ea8a786c7c83198c`.
No final audition bytes were downloaded or certified during preparation.

## Resuming this assignment

Obtain fresh raw GitHub API objects for the build, its Linux job and the named
artifact. Save the individual objects as `run-api.json`, `linux-job-api.json`
and `artifact-api.json` in this directory. Then run from the repository root:

```sh
python3 build-w12-ci-auditions/verify-ci-auditions.py \
  --artifact-json build-w12-ci-auditions/artifact-api.json \
  --run-json build-w12-ci-auditions/run-api.json \
  --job-json build-w12-ci-auditions/linux-job-api.json
```

The script uses Python 3 and NumPy. It downloads through the credential-free,
artifact-ID-pinned nightly.link URL and retains only the outer ZIP in
`build-coordination/artifact-cache/`. It refuses insufficient reserved disk
space, non-HTTPS redirects, unexpected size/digest or incomplete producing
steps. No extracted WAV copies, product execution, API mutations or publication
are involved.

## Checks performed on the actual artifact

- Exact repository, main push, source commit, run and first attempt; Linux
  source/build/test/capture/upload steps must have passed. A later downstream
  validator may still be running, so this is not an overall release decision.
- API artifact identity, expiry status, byte count and SHA-256 of public GET
  bytes, including a separately verified existing cache entry.
- The trusted publisher's pure offline `verify_auditions` function, pinned to
  commit `23e53bbff0e0f4453a7b533bec5ffe346a7941c0` by its script hash. This
  includes the first-attempt guard, ZIP bounds and all 32 checksum entries.
- An independent ZIP/CSV/RIFF inspection of all 24 WAVs: IEEE float32, stereo
  48 kHz, exact `fmt`/`fact`/`data` structure, 5,616,000 frames and 117 seconds.
- All 301 inventory programs and the exact final inventory hash; all 24 selected
  190-value factory digests independently recomputed with float32 FNV-1a.
- Exact selections, provenance capture arguments, MIDI notes, velocity,
  pressure and hold/release excerpt lengths from the reviewed workflow.
- Finite non-silent samples strictly below full scale, and raw WAV peak/RMS
  agreement with each CSV measurement within `1e-7` dB. Levels are inspected
  directly; no audio normalization is performed.

A successful invocation writes `artifact-proof.json`, including every selected
program's measurements, sample count and WAV hash, API/source identity and the
verifier hashes. This proof concerns the CI core-capture artifact. Full native
platform validation and public release qualification remain separate.

## Preparation evidence and qualifications

`preparation-checks.json` records a passing syntax check, NumPy availability,
the exact reviewed publisher hash and rejection of the real pending Linux
step metadata before any download. One older **local** Flute WAV exercises the
independent parser and CSV comparison; its original source is `99f5ebd` and its
run is `local-assignment-32`. It has not been relabeled as a CI download.

The API snapshots are retained in `last-observed-*.json`; `pending-refusal.log`
retains the expected preparation refusal. No actual final `artifact-proof.json`
exists yet. These checks make no listening-panel, acoustic-instrument,
physical-CS-80 or arbitrary-DAW claim. The excerpts use single notes and may
end before a long release is silent.

## Reviewed publisher pin update

The original preparation snapshot is preserved byte-for-byte in
`initial-preparation/`, including the old verifier and its checksums. The current
pin is rebased to publisher commit `23e53bbff0e0f4453a7b533bec5ffe346a7941c0`.
The exact diff in `publisher-note-change.diff` adds only two literal release-note
paragraphs. Replacing that one `main()` release-body value with its original
value makes the entire parsed module identical, including every helper and
acceptance condition. `publisher-pin-review.json` records this comparison, both
hashes and the preserved snapshot inventory.

The current script passes syntax checking and still rejects the saved pending
producer steps before any download. The earlier `preparation-checks.json` is a
historical preparation record; its old pin and local WAV check were preserved,
not rewritten or repeated. Final CI audition bytes remain unverified.
