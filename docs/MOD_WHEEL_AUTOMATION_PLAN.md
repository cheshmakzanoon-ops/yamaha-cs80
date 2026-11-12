# Proposed bounded MOD WHEEL automation qualification

Design-only proposal. No production edits or runtime results in this worktree.
Base: edeaaf5f272380e8decfa997cac71d71cf876584 (qualified CC1 fixture).

## Code findings and scope

- `modwheel_sub` / `P_modWheelSub`: range 0..1, default .25, linear normalized mapping.
- `modwheel_vcf` / `P_modWheelVcf`: range 0..1, default 0, linear normalized mapping.
- Append SUB at zero-based lane 81 and VCF at lane 82, total 83. All old lanes
  0..80 remain byte-for-byte ordered, with `P_tuningRef` still at 80.
- Both existing global values are assigned from the parameter snapshot and already
  participate in the continuous-setting smoother bank. SUB adds
  `subValue * modWheel * global.modWheelSub * 2` to pitch; VCF adds
  `modWheel * global.modWheelVcf * 1.5` to filter brilliance. No new DSP is needed.
- Parity mode forces both effective amounts to zero and ignores CC1. Preserve
  this effective policy without altering stored values or introducing new policy.
- Production delta should be the two roster entries only. No metadata/default,
  parameter-ID, DSP, smoothing, UI/fader, or old native-renderer edits.

## New isolated supported-input runner

Keep the historical renderer unchanged. The published CC1 fixture requires a
narrow forward-compatibility edit: change exact count81 to count>=81 AND compare
all81 entries against a literal frozen81 prefix. Apply equivalent explicit
prefix-preserving compatibility handling to the TUNING REF fixture; do not
silently relax counts or change its old contract files. Independently review
these test changes separately from the production two-entry append. New runner uses
its qualified controller lifecycle/mapped CC1 protocol, holding the connection
throughout audio and releasing it only at instance teardown. Default is no
controller/CC1; enabled cases require explicit opt-in. No MIDI CC scanning.

Use CC1=1 at frame zero for the audible driver, held constant thereafter. This
avoids conflating MIDI event-timing qualification with amount automation.
For each newly automated amount, queue finite normalized segments: initial value
through frame 1023, then values at 1535, 2047, 3071 and 4095. Deliberately use
non-block-aligned positions. Sample-1 rendering is the reference for amount
interpolation; blocks 64, 127, 256 must agree within the established host
partition tolerance. Every sample must be finite. The first 1024 frames must
match a fixed-amount driver capture byte-for-byte, and later held audio must
have a positive numerical effect. SUB oscillator settings must be deterministic
and nonzero; VCF cutoff must leave headroom for audible modulation.

Cases: SUB alone, VCF alone, both together, each for manual ranks (13/13) and
mixed source/manual ranks (0/13). Rates: 44100, 48000, 96000; realtime/offline.
Use zero variation, neutral macros, no arp/MPE/other modulation. Add explicit
CC1-zero automation-versus-static exact PCM controls and parity-mode exact PCM
controls. Do not claim an effect for sources where existing DSP policy masks it.

## Frozen compatibility and evidence

Before edits, freeze the 81-lane baseline module and source/header inventory.
Compile the new runner with an explicit compatibility-only mode against the
frozen81 source as well as candidate83. This mode bypasses only new-lane feature
assertions, never supported-input, finite, or capture validation. Its fixed-amount
no-controller/connected-zero/connected-held captures and preceding81 captures do
not drive new-lane curves. Separate protected-policy Legacy/PURE comparisons do
drive new amount curves through the established fallback/inert policies, and
are explicitly distinguished from fixed/noqueue preservation captures. Compare actual captured float bytes across baseline and
candidate, not capture counts or repeated hashes alone.

Also run a frozen81-only driver carrying representative preceding81 automation
against both modules and compare actual PCM. Preserve the existing81 roster,
contracts and historical evidence unchanged; inventory retained source files and
explicitly enumerate the two forward-compatibility test-source edits. Any helper
extraction must be proposed as a separate reviewed change, never folded into a
historical-renderer edit; source/identity audits are not
claimed as historical test reruns. Record module, runner, contract and capture
hashes and exact command/rate/mode/scenario identity.

Independent design review is required before production edits or runtime. After
implementation, independently review source before running only these bounded
normal-input targets. No malformed input, broad historical host/scanner suite,
GUI tests, security reproduction or performance benchmark is included.

## Approved implementation scope and proposed execution (not yet run)

Mandatory review additions applied: capacities8/16 =>12 cases/process,72 per six
rate/mode processes; literal amount endpoints at frames0 and1023; compatibility
SUB sine waveform0/speed8Hz/originalMode0; no other SUB destinations, external
input or touch modulation. Legacy compares sample-offset curves against final
point moved to block-start, separately from PURE amount-inertness.

Frozen baseline: `../mod-wheel-frozen81`, captured before edits. Module hash
`f582e27e6fcbe360c55fe93b940ce10f699e619d91a458166bd5ba386b9a440f`;
4636 tracked identities in inventory hash
`b79b94922a79d19c67de598f4b0f74bf6a7323ffa2ba26f045813761cea569c9`.
Independent retained audit supplied by reviewer covers561source/test blobs and53
historical contracts. Only production file changed: HostAutomationBridge.h,
adding two entries. Existing native renderer unchanged; two prior test runners
receive explicit literal frozen81-prefix forward guards, not contract changes.

Build only `cs80_vst3_mod_wheel_automation_tests` after independent source review.
Run `ctest --test-dir build-native --output-on-failure -R
'^cs80_vst3_mod_wheel_automation_(44100|48000|96000)_(realtime|offline)_tests$'`.
Run baseline compatibility executable for those same six rate/mode combinations
with the frozen baseline module and `--compat-only`. Both receive the unchanged
host contract and new mod-wheel contract. Copy only the newly reviewed runner
and frozen roster header into baseline/tests for compilation; keep baseline's
original renderer, headers and DSP intact. Compile via C++20 with frozen src,
VST3 SDK include paths and coreiids.cpp/funknown.cpp/vstinitiids.cpp, linked -ldl.

Per scenario candidate:3 fixed CC1 configurations +1 preceding81 representative
capture +2 Legacy +2 PURE +4 scalar/partition +1 fixed scalar +2 zero-wheel =15
renders. Baseline compatibility:8 renders. Six captures/scenario each side.
Totals:1080candidate+576baseline=1656renders,432actual paired PCM comparisons
from864captures. Preceding81 representative automation drives volume/masterTune/
tuningRef/rank1pan, not a rerun of every historical lane suite. All53 old contracts
and old evidence retained; identity review cannot replace a runtime claim.

## Reviewed-source execution package

`scripts/qualify-mod-wheel-automation.py` is the exact proposed orchestration.
Do not execute before independent script review. It asserts the reviewed-source
freeze, verifies frozen81 source identities, configures/builds only the candidate
target, and directly compiles an isolated compatibility executable from
`../mod-wheel-frozen81/tests/Vst3ModWheelAutomationTests.cpp`. Only that new runner
and its literal roster header were added there; relative includes resolve the
original81 renderer and `src` headers. Logs must explicitly show compiled_lanes81
for baseline and83 for candidate. The twelve bounded native processes produce
432 pairs compared with actual `read_bytes()` equality, not hash equality alone.
The script preserves failed output, refusing reuse of existing evidence/capture
directories. No broad suite, GUI, scanner or benchmark invocation exists.

Exact approved-next command, after independent review:
`python3 scripts/qualify-mod-wheel-automation.py`

Forward-compatibility CC1 and tuning guards are source-reviewed only in this
package; their historical runtime qualifications must not be represented as
fresh reruns. Update the public capability table/report only after actual new
runtime evidence exists. STATUS81 remains its historical checkpoint.

## Historical plan completion

The approved script subsequently completed once with exit0:72full+72compatibility
cases,1656renders,432actual byte-equal PCM pairs. The earlier design/pending text
above records the pre-execution plan, not current execution status. See
[VST3_MOD_WHEEL_AUTOMATION.md](VST3_MOD_WHEEL_AUTOMATION.md) for observed results
and explicit limits. Tested sources, numerical contracts and freeze remain
unchanged after runtime; only result documentation was added/updated.
