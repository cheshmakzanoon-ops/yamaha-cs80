# Current status

This is the one page that states where the project is **now**. Other documents
are specifications, dated evidence or history; when they disagree with this page
about the current state, this page wins and the other document is stale.

Update this page whenever a phase step in [IMPROVEMENT_PLAN.md](IMPROVEMENT_PLAN.md)
lands. Record exact commits; do not restate test counts without one.

Last updated: 2026-10-31, branch `claude/clever-babbage-sm9ifa`.

## Where to read what

| Question | Document |
| --- | --- |
| What are we building, in what order? | [IMPROVEMENT_PLAN.md](IMPROVEMENT_PLAN.md) (plan), [PARITY_ROADMAP.md](PARITY_ROADMAP.md) (workstream detail) |
| What is the fidelity target and evidence policy? | [ANALOG_FIDELITY_REQUIREMENT.md](ANALOG_FIDELITY_REQUIREMENT.md) |
| Which control is implemented how, and what is still open? | [HARDWARE_COVERAGE.md](HARDWARE_COVERAGE.md) |
| How does audio flow through the engine? | [ENGINE.md](ENGINE.md) |
| What do the three render modes mean? | [SOURCE_PERFORMANCE.md](SOURCE_PERFORMANCE.md), [PARITY_ISOLATION.md](PARITY_ISOLATION.md) |
| Editor layout and rules | [UI_DESIGN_SPEC.md](UI_DESIGN_SPEC.md) |
| Build, packages, CI evidence | [README.md](../README.md), [CI_DISTRIBUTION_STATUS.md](CI_DISTRIBUTION_STATUS.md) |

## Model register

Which implementation produces audio today, by subsystem. "Circuit engine" means
the Source Performance and Isolation policies; "Legacy" is the default-off policy
kept byte-identical for old sessions.

| Subsystem | Legacy | Circuit engine | Reference / experimental (not in audio) |
| --- | --- | --- | --- |
| Oscillator | PolyBLEP `Oscillator` (`Dsp.h`) | same | `experimental/ReconstructedOscillator.h` |
| HP/LP filters | TPT `Svf` with input `softClip` | same | `reference/IG00156Linear.h` |
| Envelopes | `Adsr`/`FilterEnv` float path | double-precision inferred single-RC law ([ENVELOPE_RC_MODEL.md](ENVELOPE_RC_MODEL.md)) | — |
| VCA | linear multiply | loaded LC network gain | `reference/LevelControl.h` |
| Tone tabs | legacy bridge voicings | `SourceFactoryTones.h` (7 complete, 15 inferred) | `HardwareControl.h` matrix |
| SUB | legacy `Lfo` or `OriginalSubOscillator` | same | — |
| Chorus | generic `Chorus` | OE2 `BbdChorus` (tremolo generic) | `reference/Mn3001Packet.h`, `Oe2Filters.h` |
| Wah / EXP | SVF band-pass stand-in | `PraWahCircuit` | `reference/PraWah.h` |
| Output | scalar LOW trim + soft clip | -6 dB reference, inferred driver ceiling, `OutputTapCircuit` | `reference/OutputTaps.h` |
| Idle voice evolution | frozen | active-tail continuity only | `experimental/ContinuousVoiceCircuit.h` |

## Phase progress

| Plan step | State | Commit |
| --- | --- | --- |
| Plan recorded | done | `97cc815` |
| 0.1 Exact coefficient caching | done; bit-identical; source ~23 % and legacy ~10 % faster on a scratch benchmark | `7b55d34` |
| 0.1 Function split, fast approximations | moved to Phase 1: they change pinned circuit-engine bytes | — |
| 0.2 Parameter smoothing | moved to Phase 1 (circuit engine only): both pinned probes move controls mid-render, so smoothing is an intentional audible change | — |
| 0.3 State schema version | done | `eb486cb` |
| 0.4 Shared test helpers | deferred until the first Phase 1 stage gate needs them | — |
| 0.5 This status page | done | `eb486cb` |
| 1.5 Circuit-engine gain staging: -6 dB output reference + inferred driver headroom ceiling | done; intentional circuit-engine change, legacy byte-identical; tab peaks +3.2 -> -2.9 dBFS | `228459c` |
| 1.3 Circuit-engine envelopes as RC circuits; filter EG continuity on reuse | done; intentional circuit-engine change, legacy byte-identical; worst steal click 1.17 -> 0.90 | this change |

## Pinned compatibility gates

- `dsp-measurements.yml` **legacy-compatibility**: default-off audio byte-identical
  to `d56911a`. Legacy must never change.
- `dsp-measurements.yml` **source-factory-compatibility**: circuit V1 numerics
  (`22d6a32`) and circuit-engine audio V2 (`228459c`, re-pinned from `6e5892f`
  for Phase 1.5 gain staging; use `6e5892f` for earlier circuit-engine renders). Any intended circuit-engine
  sound change must re-pin V2 to the commit that introduces it and document the
  change here.

## Open issues that block "release"

- Intermittent Linux pluginval exit 139 after SUCCESS; root cause unknown
  ([VALIDATOR_DIAGNOSTICS.md](VALIDATOR_DIAGNOSTICS.md)).
- No project LICENSE; JUCE licensing and product-name review undecided.
- No signing, notarization, installer or universal macOS binary.
