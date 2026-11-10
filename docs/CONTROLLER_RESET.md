# CC121: bounded controller defaults and retained ownership

## Why this increment

At `6c560f1`, CC121 released sustain and nulled RPN/NRPN selection, but a
host's reset could leave modulation, expression, pitch bend or pressure at
its last value. This increment advances the roadmap's modern MIDI/performance
layer without changing circuit laws, existing sessions without CC121, or
PURE sound controls. It is MIDI/software behavior, not a physical CS-80
fidelity claim.

## Contract and routing

A finite CC121 on channels 1 through 16 applies the following defaults. As
before, any finite data value is accepted; invalid channels and non-finite
float events do nothing. Targets change at the event position, using the
existing smoothing and optical-expression response, not an instantaneous
smoother/envelope reset.

| State | Reset and scope |
| --- | --- |
| Modulation (CC1) | Zero. This instrument's modulation target is global. |
| Expression (CC11) | Full (1). The target is global, including the processor's expression readback and value reapplied after prepare or extension activation. |
| Pitch bend | Centre through the existing bend-target route, with the later released-target reset exception described below. Conventional bend is global. An MPE manager resets only its zone-wide component, retaining independent member bends; a member resets only its own bend, retaining the manager component. Unassigned channels keep the existing routing. The lower manager (and historical channel-1 fallback) also resets the shared normalized pitch-wheel matrix source; upper-manager and member resets leave that shared source unchanged. |
| Pressure | Zero through the existing pressure-target route, with the later released-target reset exception described below. Own-channel voices and the addressed MPE manager's voices are affected. Conventional channel 1 retains its historical pressure broadcast; other conventional channels affect their own voices. The shared channel-pressure matrix source is global and is also reset. Poly and channel pressure use the same per-voice target, so both clear in that scope. |
| MIDI sustain (CC64) | The addressed pedal is released through the existing pedal-up path. Held keys are not released. MPE members still inherit their manager's pedal; an independently down member pedal remains down when its manager is reset. Other pedals stay unchanged. |
| CC88 velocity prefix | Discard the addressed channel's pending prefix, returning its next strike to ordinary 7-bit velocity. Other channels' prefixes remain pending. This is an explicit device policy, not a claim that RP-015 specifies this later-added prefix. A prefix after CC121 still applies to the next note. |
| RPN/NRPN selection | Null the addressed selector and Data Entry latch, even while MPE is disabled or PURE is enabled. Retain negotiated zones and bend-range values; other selectors stay unchanged. |

**PURE exception:** only the existing selector/latch nulling occurs. No new
sound reset, expression-readback change or CC88 discard occurs. In particular,
PURE retains its physical expression position, pressure and existing pending
high-resolution strike behavior.

The mod/expression defaults are assigned directly. They are **not** synthetic
CC1/11 gestures, and pedal release does not synthesize a CC64 event. Therefore
ribbon mappings to CC1, CC11, CC64 or CC121 cannot move the independent ribbon.
CC74 timbre, ribbon position, bank/program, volume/pan/effects parameters,
MIDI-learn assignments, host parameters and negotiated RPN values are retained.
No MIDI-learn parameter is driven by the reset. No pending UI mailbox is
cleared; a later UI gesture remains eligible at the next callback.

The existing sustain release path still handles value-continuous envelopes,
Circuit Sustain II, ring-gate bookkeeping and waiting-key reconsideration.
This is not a panic or engine reset. No voice-allocation policy, saved-state
format or parameter identifier changes. The added work is bounded target
assignment, existing eight-voice routing per active engine, one atomic
readback store and one prefix-slot write. It introduces no allocation, lock,
file access or new audio-rate computation. This is a source-level statement,
not a measured whole-callback deadline or allocator-instrumentation result.

## Later member-onset retention

The later [bounded MPE member-onset repair](MPE_MEMBER_ONSET.md) adds raw
member-channel bend, pressure and CC74 retention for accepted future allocation.
CC121 uses the existing bend/pressure routes, so an addressed member's retained
raw bend and pressure are reset while its retained CC74 is preserved. A manager
reset still affects its existing active-voice/matrix scope, but does not clear
independent member caches. A later member onset can therefore inherit retained
member pressure even after a manager CC121. This is not full zone termination,
a new manager/member pressure-combination model or complete MPE conformance.
The historical verification results and evidence manifest below remain those of
the original reset increment, not qualification of this later cache extension.

## Later released-target exception

The later [MPE released-target repair](MPE_RELEASED_TARGETS.md) restricts ordinary
member Pitch Bend and Channel Pressure to active accepted held gates. CC121
explicitly bypasses that new held-only guard through private routing helpers:
it still resets its existing active released/pedal-held recipients and raw
member defaults. CC74 remains retained, manager scope and shared matrix sources
are unchanged, and PURE still performs only selector/latch nulling. This is a
preserved reset exception, not ordinary member-event replay or a whole-zone
reset. Its separate evidence is linked from the later repair; the historical
counts below remain those of the original reset increment.

## Primary evidence and limits

- MIDI Association [Response to Reset All Controllers (RP-015)](https://midi.org/response-to-reset-all-controllers),
  approved MMA November 1998 / AMEI May 1999, p. 1. The defaults and preserved
  controller classes above follow this guidance. It permits documented
  exceptions and specifically calls for documenting global-controller scope.
  The official linked [PDF](https://drive.google.com/file/d/1VzU1Bfv8iYLGD23ZfaTJVomEKNTtBh6n/view)
  was retrieved 2026-11-09; SHA-256
  `0b1b75e84ea9ed7429e81a9101afe33683f53914e8161eb6c3dd637b1542e5c5`.
- MIDI Association [MPE v1.1](https://midi.org/mpe-midi-polyphonic-expression),
  M1-100-UM, 14 April 2022, sections 2.3 and Appendices C through E. These
  distinguish manager/member expression and list CC121. The original reset increment uses
  the instrument's existing routes; it does not introduce a new complete
  MPE controller-retention or pressure-combination model. The later bounded
  onset extension is documented above. The official linked
  [PDF](https://drive.google.com/file/d/1QGjiK2QPPbii8YmES3vDEZlP63OGLiKr/view)
  was retrieved 2026-11-09; SHA-256
  `c5f6727af065e454fe9bc4ff80f66414e8b25ffb6c1019ce7795ab4a89da1f1e`.

This remains **bounded support, not complete MIDI/MPE conformance**. Global
CC1/11, conventional bend, channel-1 pressure broadcast and the shared matrix
pressure tap are deliberately documented rather than replaced with a new
per-channel controller architecture. A manager reset does not clear all
independently bent members or independently held member pedals. Unsupported
controllers/modes remain outside this reset increment. Later
[member-onset expression retention](MPE_MEMBER_ONSET.md) adds the bounded
bend/pressure/CC74 initialization described above, not full initial-note MPE
state or complete controller conformance.
Pitch/mod wheel widgets still lack general host-controller readback; only the
existing expression readback is synchronized here. Hardware, real-DAW,
wrapper/validator and external-controller qualification are not claimed.

## Verification

Source/test checkpoint: `dc8de70e3a26a9c10cba6de8489273a2e990c97b`
(production unchanged from `6e639fb`), based on
`6c560f130be649dd4a63cbe146cd5cc54dd057a3`. Local Linux x86-64, GCC 14.2.0,
CMake 4.4.4, official JUCE 8.0.11 at
`c352e2489000ffeb9e397051377d75eef2efe641`. No remote-CI claim is made here.
Raw summaries, commands and content hashes are in
[the evidence directory](evidence/controller-reset-2026-11-09/manifest.json).

- `cs80_controller_reset_tests`: **3,513 checks**, zero failures in Release
  and ASan + UBSan + float-cast-overflow + library assertions. Leak detection
  was disabled for the sandbox sanitizer run. All 16 channels, 44.1/48/96 kHz,
  Legacy/Circuit, ordinary-event bit-exact finite audio (including signed zero)
  with differing render partitions,
  manager/member/gap pressure/bend, matrix-pressure scope, timbre/ribbon
  preservation, sustain overlaps, held/waiting keys, malformed events,
  selector/range retention, per-channel prefix helper and PURE are covered.
- `cs80_controller_reset_host_tests`: **1,408 checks**, zero failures using
  the real processor, without an editor/native peer. Covers 0/32-frame
  callbacks, event offsets and equal-position ordering, all 16 addressed CC88
  channels, active 8/16-voice paths, expression prepare/reactivation,
  later UI expression, learned parameters/program preservation and PURE
  prefix/physical-expression audio identity.
- Nine focused core CTests pass, including the expanded reset test,
  source-performance, smoothing, MPE-manager smoothing/routing, pedal/voice
  release, tuning/MIDI and parity. The separate main core executable passes
  **11,508 checks**. Four neighboring native voice-activity, keyboard-handoff,
  transient-control and performance-wheel CTests pass.
- Strict compatibility probes retain byte-identical Legacy default/tails,
  V1 circuit numerics and V2 circuit-engine audio versus their existing pins.
  These are unchanged-event compatibility witnesses, not a claim that CC121
  audio is unchanged: the newly supported reset behavior intentionally differs.
- Private negative controls require normal test completion and behavioral
  failures, not compile/crash failures. The unchanged base **Engine.cpp**
  produces 438 failures when compiled against the expanded test and current
  behavior-preserving prefix-helper header. Omitting mod/expression/bend/
  pressure resets produces 310/310/320/118 failures; breaking the core PURE
  guard produces 2. Native-only telemetry/prefix omissions produce 16/256
  failures; breaking the processor PURE guard produces 18. No working source
  is modified by either reproduction script.

### Reproduction

```sh
cmake -S . -B build-reset -G Ninja -DCMAKE_BUILD_TYPE=Release -DCS80_BUILD_PLUGIN=OFF
cmake --build build-reset --target cs80_controller_reset_tests
ctest --test-dir build-reset -R '^cs80_controller_reset_tests$' --output-on-failure
python3 tests/test_controller_reset_negative_controls.py --output /tmp/reset-negative

# Configure a normal native JUCE build, then:
cmake --build build-native --target CS80Snapshots
ctest --test-dir build-native -R '^cs80_controller_reset_host_tests$' --output-on-failure
# GCC/Clang + Ninja builds; reuses the completed native objects privately:
python3 tests/test_controller_reset_host_negative_controls.py \
  --build build-native --output /tmp/reset-host-negative
```
