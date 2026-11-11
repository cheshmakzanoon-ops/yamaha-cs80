# Keyboard ownership after sample-position ARP program recall

## Reproduced defect

The processor cached effective ARP mode once at callback start. A same-policy
MIDI Program Change could change the accepted engine ARP mode later in that
callback, while subsequent UI/host note decisions still used the old value.

An ARP-enabled UI onset can be accepted as a held source without immediately
allocating a voice. With the stale off flag, the processor failed to record its
conditional release ownership. Its subsequent UI release was dropped and ARP
could continue creating gates. The opposite transition also suppressed valid
same-block per-key pressure after a direct-note allocation.

A real-processor regression on `b40008c3` production source reported **2,760
checks, 16 failures**: both defects across Legacy/Circuit factory programs,
host-first equal-offset and midblock recall, and initial 8/16-voice modes. The
first fixed run passed all 2,760 checks. No tolerance changes were used.

## Bounded correction

The callback reads accepted effective ARP state from the engine, including PURE
masking, and refreshes it immediately after each MIDI Program Change. Changes
revoke keyboard pressure proofs while preserving conditional UI release masks.
The next callback's ARP-edge tracker is updated at the same point. Existing
cross-policy recall still discards the pre-transition UI batch.

The supported sample-accurate automation lanes are continuous controls and do
not include ARP, PURE or engine policy. They cannot independently toggle ARP
within a callback, so the fix adds no per-sample mode query. A future expansion
of those lane types would require corresponding publication-edge handling.

There is no change to engine ARP scheduling, gate length, note selection,
program/bank admission, MIDI/UI tie ordering or preset voice-mode recall.
Initial 16-voice tests are not a claim that a preset preserves that setting:
program recall can retire the extension at the existing block boundary.

## Qualification

The source regression uses actual named factory programs, real MIDI program
selection and the keyboard FIFO/token path. Positive controls require held ARP
gates and finite non-silent PCM before release; the stuck-source oracle checks
that no held gate reappears for several preset-rate periods after release.
Release tails are not mistaken for a stuck gate or required to be silent.

An initial expanded fixture passed 4,127 checks and an 81-step affected rebuild
passed 45/45 selected tests in 159.34 seconds. Review then identified a fixture
reliability hazard: a scheduler pause between the UI onset and queued pressure
could extend the wall-clock batch and shift the onset before the intended
midblock recall. That pass is retained as history, not the final deterministic
midblock qualification. Those cases now use fixed timestamps in the real FIFO through existing test
friendship. The anchor is at 100 ms and onset/pressure at 101 ms, giving sample
64 in a 128-frame callback without sleeps or scheduler assumptions. Tokens and
FIFO admission remain production code; this is explicitly a queue fixture, not
an OS/UI timing test. Public gesture capture remains covered separately by
equal-offset and zero-frame cases.

The final deterministic fixture was rebuilt against the original processor
implementation and reported **4,163 checks, 26 failures**. The fixed processor
source was restored and its saved SHA-256 verified before final rebuilding.
Additional cases use real supported volume curves with Program Change and rapid
same-policy off/on/off recalls before a fresh UI onset. The latter checks final
fresh-onset/pressure admission, not a separate proof about a token owned before
the intermediate edges. No unsupported ARP automation lane is fabricated.

The final restored build completed a 20-step affected rebuild and passed **45/45 selected
tests in 150.46 seconds**, explicit exit 0. The deterministic new fixture passed
**4,163 checks, 0 failures**; allocation remained **204/0**. Selection includes
keyboard queue/epoch/pressure auditing, core and scanner ordering/overflow,
program/keyboard/MPE processor checks, state mirror/undo and the six actual VST3
rate/mode registrations. It is a selected integration run, not every repository
configuration. An independent repeat passed **45/45 in 151.31 seconds**,
explicit exit 0, on the same runtime/test inputs. Independent source and fixture
review found no blocker. A second reviewer independently reran the new ARP,
allocation and keyboard-handoff suites: **3/3 passed in 4.47 seconds**, including
4,163/0 ARP and 204/0 allocation checks. This is processor/FIFO
verification, not an OS mouse/keyboard, GUI, external DAW/controller or complete
ARP/automation qualification. Earlier native validator and full-core checkpoints
refer to their pinned artifacts and are not automatically transferred here.
