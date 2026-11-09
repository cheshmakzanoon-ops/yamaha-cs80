# MPE member onset: reproducible evidence

See [the product contract](../../MPE_MEMBER_ONSET.md) and the unchanged
[pre-implementation plan](PLAN.md). All results below concern the frozen code
checkpoint, not a later documentation commit.

## Source identity

- Published base: `eebc94e`.
- Production checkpoint: `2970f46496b268a16995802b3be578449d5c47b9`.
- Native-fixture hardening: `c0186b8c60cfc2ac01318716fb6cd9955ad50c35`.
- Original qualification head: `9c903f1f66a6349dd2f8b9519dcf559da8dc3b38`.
- Final fixture checkpoint: `fcaf4597f34b8ad0180bbbd1ac3600257b868700`.
  Only `tests/TuningMidiTests.cpp` changed after the original head: explicit
  neutral pressure/CC74 before each independent routing case, plus a neutral
  onset assertion. Production, core onset tests, native fixture and fault
  harness are unchanged. Earlier profile/sanitizer/native results apply to
  their unchanged targets; they do not imply the modified tuning fixture
  ran under those earlier profiles.
- Baseline compatibility libraries were built at `4ef0666`, **not** at
  `eebc94e`. Their exact `src` tree equals the published base's tree:
  `b76dba2e547c8829a86dda6d2625f330d5548d26`.

`manifest.json` pins repository source/fixtures and archived evidence by SHA-256.
It separately lists source paths unchanged against the published base.
`external-inputs.json` identifies retained local binaries and other external
inputs: those absolute paths are historical provenance, not portable downloads
or a promise that the binaries are bundled. The two Engine files are the only
production core changes; native snapshot test wiring is a separate source edit.

## Recorded outcomes

- GCC 14.2 and Clang 19, generic and `-march=native`, Release `-O3 -DNDEBUG`:
  each passed 2,501 onset checks and 11,508 existing core checks. AVX2 filter
  batching was disabled for these profiles. This is not cross-profile audio
  equality: each profile independently runs its paired reference checks.
- Full core linked with ASan, UBSan, float-cast-overflow and
  `_GLIBCXX_ASSERTIONS`: the same 2,501 + 11,508 checks passed. Leak detection
  was disabled (`ASAN_OPTIONS=detect_leaks=0`); UBSan halts on error.
- Behavioral negative controls: candidate 0 failures; unchanged baseline 1,004;
  omit bend 410, pressure 198, timbre 606, reset 36, mode edge 8, role change 16;
  broaden mode scope 9; cache already-curved pressure 16. Each completed exactly
  2,501 checks. The driver returned zero because expected behavioral faults were
  detected. Build errors, crashes, truncated summaries and incomplete coverage
  are not accepted as negative-control success.
- Compatibility: each generic/native baseline-candidate pair serialized 27,552
  finite raw float values across six unchanged-path scenarios and was
  byte-identical **within its own profile**. The profiles have different hashes.
  Strict Legacy default/tails, V1 circuit numerics and V2 circuit-engine audio
  gates also report byte identity. This bounded witness is not exhaustive
  engine-state compatibility or a proof about every preset.
- Fresh native JUCE processor qualification: direct 976 checks, zero failures;
  focused CTest 1/1; neighboring undo, controller-reset, voice-activity and
  keyboard-handoff CTests 4/4. Each recorded process status is zero. The native
  build reported no work. This exercises real `processBlock` MIDI ordering,
  same/separate offsets and preceding callbacks, zero-frame offset collapse,
  non-silent continuations, both engines, primary/active extension allocation,
  lower/upper/full-upper members and MIDI-learn ownership. It opens no editor
  and is not external DAW, plugin-format host or hardware qualification.
- Supplemental shared-pressure witness: GCC generic/native each pass 32
  scenarios and 1,853,296 checks. It compares SUB rate/output diagnostics after
  each host frame while rendering normal internal ticks: cache alone supplies
  no bus force before accepted held allocation; retained pressure affects held
  onset; stronger member events add no fresh force after release, including
  pedal-held sound. Existing Circuit residual decay is preserved, not asserted
  instantly zero. This is not direct observation of every internal tick.
- Corrected tuning fixture: candidate and unchanged baseline each pass 22,085
  checks with explicit zero build/run statuses under the generic profile.
  The corrected candidate also passes 22,085 checks under GCC native and with
  the full-core-linked ASan/UBSan/float-cast-overflow/library-assertions profile.
  Extra build/run statuses and hashes are in `tuning-fixture/extra-commands.json`;
  sanitizer leak detection is disabled (`detect_leaks=0`).
- Default core suite: all 143 registered cases have explicit passing verdicts
  across 58 original passes, 36 first-continuation passes and 49 final-continuation
  passes. The final driver returned zero. This is combined coverage across
  recorded attempts, not a successful uninterrupted 143-case CTest invocation.
  The earlier test 95 failure is preserved and is not counted as passed.


`evidence/profiles/commands.json` preserves exact compiler flags, commands,
environments and exit codes. `evidence/native-final/` contains fresh native
statuses and executable identity. `evidence/reviews/` contains independent
production and fault-driver reviews with their original, narrower time-of-review
limits intact. `evidence/final-independent-review/` separately records fresh
GCC and sanitized onset runs, native direct and focused CTest results, with
explicit statuses and checkpoint/source scope; that review holds final bundle
approval pending. A separate supplemental review independently reruns the
corrected generic tuning fixture and native shared-pressure witness and reviews
the six-line fixture delta. Three negatives were independently rerun; the other negative
logs were independently parsed and source-reviewed, not independently rerun.

## Failed and interrupted attempts

The evidence does not erase earlier attempts:

- The first onset fixture reported four failures because it assumed Legacy HQ
  reset behavior incorrectly. The fixture assumption was corrected, not a
  product defect hidden; initial and first-green logs remain under `attempts/`.
- The first local native bootstrap used an include order that produced an
  unqualified `isnan` compile failure in vendored MTS code. Local `-idirafter`
  ordering fixed it; no vendored source patch was needed.
- The earlier native direct log contains 976/0 but lacks a recorded exit status.
  It is corroborative only, superseded for qualification by `native-final/`.
- The original full default CTest invocation was interrupted after 58 completed
  passes. It has no overall verdict. Started-only tests were not counted passed.
- A continuation preflight rejected its own log-matching assumption and launched
  no tests. Its failed status is not a product-test failure or a completed suite.
  The corrected continuation records each remaining test separately.
- That continuation then stopped normally on a real fixture incompatibility:
  `TuningMidiTests` used `allNotesOff(false)` between routing cases and assumed
  it cleared member expression. The new cache deliberately survives that
  note-release operation. The failure log and process status are retained;
  this attempt is not counted as passed. The reviewed test-only correction at
  `fcaf4597f34b8ad0180bbbd1ac3600257b868700` sends neutral pressure and CC74
  before each independent routing case and asserts neutral onset without
  weakening existing routing assertions. Production is unchanged. Corrected
  candidate generic/native/sanitizer and unchanged-baseline generic runs each
  pass 22,085 checks; the final 49-case continuation passes corrected test 95.


The supplemental probe's initial private-API compile error is retained in
`shared-pressure/initial-private-api.*`; it is not a runtime result. The
corrected probe uses ordinary public CC64 for sustain.

## Reproduction and verification

From the repository root:

    python3 docs/experiments/mpe-member-onset/verify.py
    python3 -O docs/experiments/mpe-member-onset/verify.py

The verifier checks byte identity and recorded result structure using explicit
errors, including under optimized Python. It does not rerun builds/tests,
authenticate a publisher, recreate missing external binaries or establish new
runtime results. Use `--external` only on the original retained workspace to
also verify external files. A manifest is an integrity index, not a signature.

For reruns, adapt paths in the archived profile driver and command records.
The negative driver is `tests/test_mpe_member_onset_negative_controls.py`.
The unchanged-path capture is `CompatibilityCapture.cpp`; its archived shell
script uses each baseline/candidate's own headers and library. Native bootstrap
uses clean official JUCE 8.0.11 at
`c352e2489000ffeb9e397051377d75eef2efe641` plus hash-verified official Debian
packages extracted locally. Bootstrap scripts, package pins and provenance are
archived; no system settings were changed. The bootstrap provenance records
its earlier build-start checkpoint `d28ffc6`; it does not identify the final
qualification checkpoint. Fresh native statuses and executable SHA-256 in
`native-final/` are the final-run evidence. Its local source override is not
eligible for the CI-only provenance script. Xvfb was staged for CMake test
registration, not used for focused MPE checks, and this does not provision or
qualify the full UI suite.

The exact official specification URL, hash and selected sections are in
`evidence/selection.json`. Neither the complete PDF nor executables are bundled.
Raw compatibility float captures are data witnesses, not executables.
