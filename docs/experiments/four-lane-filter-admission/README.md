# Reuse gathered four-lane filter coefficients during admission

The exact four-stage filter kernel already gathers ten static coefficient
vectors for its Newton solve. It now checks the common finite domain using
those same vectors and integer comparisons of raw representations. It
keeps the previous lane-ordered eligibility checks whenever the quick
certificate cannot prove the result. That fallback still observes custom
rate/network inputs, early rejection order and the DAZ-dependent effect of
comparing a signed subnormal. The kernel still checks host FP support at
entry; no separate or wider filter, new arithmetic or audio-quality change
is introduced. Existing non-AVX2 or unsupported compiler/FP paths remain. GNU builds
explicitly configured with `-fno-trapping-math` compile the original
ordered admission body rather than the fast certificate. The unguarded
candidate could otherwise differ from compiler-reordered stock sticky flags
on rejected special inputs. [Guard verification](profile-receipts.json)
shows the default candidate object was unchanged, and the guarded switch
reproduced the stock objects under both non-trapping O2 and O3.

## Actual VST3 work

[Source and binary identities](exact-git-pin.json) and
[translated work receipts](work-receipts.json) pin the Linux x86-64 GNU
14.2 Release stock `b06bcef4c2e0b8d0d45bba3dadbc370113fb1dd3`
and the filter-only candidate built on it. Four real plug-in callbacks
at 48 kHz/64 frames give these guest counter differences:

| Scene | Instructions | Reads | Writes |
| --- | ---: | ---: | ---: |
| Eight equal-velocity held notes | −3.357% | −2.975% | +0.824% |
| Eight varied-velocity held notes | −3.333% | −2.966% | +0.824% |
| Varied wake | −2.495% | −2.280% | +0.610% |
| Fresh idle | Unchanged | Unchanged | Unchanged |

Independent A/A and extra-callback controls matched exactly for both
modules. In these scenes the entire delta was attributed to the filter
function. Extra guest writes reflect emitted spill traffic; the instruction
counts and simulated reads/writes are not native CPU time or a DAW result.

## Exactness exercised

- [48 kHz native histories](public-parity.json): 112,640 genuine plug-in
  callbacks, 7,208,960 stereo frames and 113 public-state snapshots per
  arm matched, as did the recorded FP/status bytes.
- [Four rates](cross.json): 44.1, 88.2, 96 and 192 kHz, realtime and offline.
  The native host compared matched holds, release and wake with automation and
  state restore. Same-stock and changed-input controls distinguish failure modes.
- [Loaded-ELF filter gates](actual-plugin-fuzz.log): 279,792 raw/finite
  direct kernel calls with all four DAZ/FTZ modes, 432 unsupported-control
  cases, and 3,613 rejected comparisons with a newly observed denormal
  sticky flag. Verdict, output, stage state and flags matched stock.
- All 648 additional native PWM/ring streams across 44.1, 48 and 96 kHz
  in real-time and offline modes matched.
- New permanent signed-subnormal admission and ordered-rejection tests
  are exercised through public stage APIs. Deliberate widened-bound and
  missing-denormal-observation variants fail the corresponding tests.

These observations bind the tested compiler, modules and fixtures; they
do not waive testing on other platforms or establish realtime savings.
