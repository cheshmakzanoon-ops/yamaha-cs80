# Current status

This is the one page that states where the project is **now**. Other documents
are specifications, dated evidence or history; when they disagree with this page
about the current state, this page wins and the other document is stale.

Update this page whenever a phase step in [IMPROVEMENT_PLAN.md](IMPROVEMENT_PLAN.md)
lands. Record exact commits; do not restate test counts without one.

Last updated: 2026-11-12. The latest local core integration checkpoint on this
page tests published `863e6c5b` (retained MPE manager/note pressure and CC74),
with exact scope and results below.
Native CI, downloaded installers, and hardware/DAW qualification are separate
checkpoints; the November 9 remote runs retained below do not cover this newer
source. The sparse solver still exceeds the proposed audio-update budget;
loaded memory controls/receivers remain outside Engine audio, and rejected
one-log span prototypes remain archived outside production source.

Screenshot provenance is recorded in [provenance.json](screenshots/provenance.json)
and [SHA256SUMS](screenshots/SHA256SUMS). Those files identify the actual captured
source and images; they are not a claim that every screenshot renders current
main. Native downloads remain pinned to `0996a15af334f9dcfd63ed8c1d574a22b6804f00`
at the historical inspection checkpoint. No newer binary is qualified here.

For the bounded offline research tools, see the [offline reference guide](OFFLINE_REFERENCE_GUIDE.md).

## Latest local core integration

Tested published source `863e6c5b63d2fc0a2af90cc051fa1387286e065f` completed a
fresh 266-step core-only Release build and all **148/148 registered CTests**,
with zero failures in one serial run (876.07 seconds, explicit exit 0). There
were no CTest-level skips; three Windows-only Python subtests were skipped on Linux.
See the [exact source, reproduction and qualification limits](CORE_INTEGRATION_863E6C5B.md).
The earlier [MTS integration](CORE_INTEGRATION_ACE89DFD.md) and
[note-admission checkpoint](CORE_INTEGRATION_D00D8BF.md) remain historical
results on their own pinned inputs.
No native-host, sanitizer, hardware-fidelity, release or CPU-target claim follows
from this result.

Focused instrumentation on `ccb812e0` separately passed four core suites under
ASan+UBSan with leak detection excluded (119.03 s). The leak-enabled attempt
failed at LeakSanitizer runtime shutdown and remains unqualified. See the
[configuration, distinct outcomes and limits](CORE_SANITIZER_CHECKPOINT_CCB812E.md).

## November 12 scoped native VST3 automation

Published `9b8db027073505ce28aee18b287ae79b48fb14a4` appends both ranks’
SINE/NOISE level controls at lanes74..77, bringing the current roster to
**78 lanes**. Its reviewed candidate passed12 direct native runs:144 full cases
and144 compatibility cases,2736 renders total. All432 actual candidate78/frozen74
no-queue and Legacy/PURE pairs matched byte-for-byte. Maximum partition error
was3.72529e-09 under the unchanged2e-6 threshold, not zero; all144 pre128 prefixes
were exact. All48 PANEL cases had positive effects in held frames[128,3072),
and all96 factory/MEMORY masking comparisons were exact. See [SINE/NOISE
evidence and exact hashes](VST3_SINE_NOISE_AUTOMATION.md).

Existing live smoothing and distinct paths remain unchanged: noise enters
before filters, while sine is added after filtered-path gain. Each case changes
only the selected rank/kind; deterministic reset supports PCM comparison, not
spectral or statistical noise-quality claims. No true-zero/filter-bypass,
simultaneous-lane, whole-project or hardware completion claim is made. Source
authority and DSP remain unchanged. Linux float32 coverage is44.1/48/96kHz,
realtime/offline,8/16 capacities. Direct runs are not CTest or historical-suite
reruns. Broad integration/release qualification and retained candidate
binary/source hashes remain separate from this published source commit.

The preceding RELEASE TIME checkpoint remains historical:

Published `17ba092025f8ff59588e09129e01256f2436e9b3` appends both ranks’
filter/VCA RELEASE TIME controls at lanes70..73, bringing that checkpoint’s roster to
**74 lanes**. Its reviewed candidate passed12 direct native runs:144 full cases
and144 compatibility cases,2736 renders total. All432 actual candidate74/frozen70
no-queue and Legacy/PURE pairs matched byte-for-byte. Maximum partition error
was0; all144 pre3100 prefixes were exact. All48 PANEL cases had positive tail
effects in frames[3100,4096), and all96 factory/MEMORY masking comparisons were
exact. See [RELEASE TIME evidence and exact hashes](VST3_ENVELOPE_RELEASE_AUTOMATION.md).

Existing live timing updates preserve precise state and stage and respect the
max of rank release, key release and gated master sustain. The selected rank
release remains above explicitly controlled floors in this finite post-noteoff
tail. No floor bypass, complete release, calibrated timing, hardware or
simultaneous-lane qualification is claimed. Source authority and DSP remain
unchanged. Linux float32 coverage is44.1/48/96kHz,realtime/offline,8/16 capacities.
Direct runs are not CTest or historical-suite reruns. Broad integration/release
qualification and retained candidate binary/source hashes remain separate from
this published source commit.

The preceding DECAY TIME checkpoint remains historical:

Published `0426928effd993b9dc250540f49acf3e0dad04b6` appends both ranks’
filter/VCA DECAY TIME controls at lanes66..69, bringing that checkpoint’s roster to
**70 lanes**. Its reviewed candidate passed12 direct native runs:144 full cases
and144 compatibility cases,2736 renders total. All432 actual candidate70/frozen66
no-queue and Legacy/PURE pairs matched byte-for-byte. Maximum partition error
was0; all144 pre512 prefixes were exact. All48 PANEL cases had positive effects
in held frames[512,3072), and all96 factory/MEMORY masking comparisons were exact.
See [DECAY TIME evidence and exact hashes](VST3_ENVELOPE_DECAY_AUTOMATION.md).

Existing live timing updates preserve precise state, stage, elapsed time and
anchors. Short fixed attacks and bounded decay coefficients provide a post-attack
held window before decay completion; this is not independent stage-timing or
complete-decay qualification. No post-noteoff decay-selection, calibrated
hardware or simultaneous-lane claim is made. Source authority and DSP remain
unchanged. Linux float32 coverage is44.1/48/96kHz,realtime/offline,8/16 capacities.
Direct runs are not CTest or historical-suite reruns. Broad integration/release
qualification and retained candidate binary/source hashes remain separate from
this published source commit.

The preceding ATTACK TIME checkpoint remains historical:

Published `f4db913db039e9e0c25a9fafda1acfab79e594f8` appends both ranks’
filter/VCA ATTACK TIME controls at lanes62..65, bringing that checkpoint’s roster to
**66 lanes**. Its reviewed candidate passed12 direct native runs:144 full cases
and144 compatibility cases,2736 renders total. All432 actual candidate66/frozen62
no-queue and Legacy/PURE pairs matched byte-for-byte. Maximum partition error
was0; all144 pre128 prefixes were exact. All48 PANEL cases had positive effects
in held frames[128,3072), and all96 factory/MEMORY masking comparisons were exact.
See [ATTACK TIME evidence and exact hashes](VST3_ENVELOPE_ATTACK_AUTOMATION.md).

Existing live timing updates preserve precise state, stage, elapsed time and
attack anchor; unlike changed AL, they do not reanchor the trajectory. Fixed
targets and bounded positive times keep the selected envelope in Attack during
the held witness. No post-noteoff attack-selection, complete-stage, calibrated
timing, hardware or simultaneous-lane qualification is claimed. Source authority
and DSP remain unchanged. Linux float32 coverage is44.1/48/96kHz,realtime/offline,
8/16 capacities. Direct runs are not CTest or historical-suite reruns. Broad
integration/release qualification and retained candidate binary/source hashes
remain separate from this published source commit.

The preceding filter AL checkpoint remains historical:

Published `a8c7de33041365a1531c37e6e62e33d09eb6060b` appends both rank
filter EG AL controls at lanes60/61, bringing that checkpoint’s roster to **62 lanes**.
Its reviewed candidate passed12 direct native runs:72 full cases and72
compatibility cases,1368 renders total. All216 actual candidate62/frozen60
no-queue and Legacy/PURE pairs matched byte-for-byte. Maximum partition error
was0; all72 pre128 prefixes were exact. All24 PANEL cases had positive held-window
effects in frames[128,3072), and all48 factory/MEMORY masking comparisons were
exact. See [filter AL evidence and exact hashes](VST3_FILTER_AL_AUTOMATION.md).

Changed AL during Attack reanchors the existing trajectory from current precise
state and resets its local time without replacing capacitor state or stage.
The long-attack, monotonically increasing-target fixture avoids downward target
crossing; it does not qualify post-noteoff selection, full attack timing or
calibrated hardware. Existing target law and selected-source authority remain
unchanged. Linux float32 coverage is44.1/48/96kHz,realtime/offline,8/16 capacities.
Direct runs are not CTest or historical-suite reruns. Broad integration/release
qualification and retained candidate binary/source hashes remain separate from
this published source commit.

The preceding filter IL checkpoint remains historical:

Published `3f67375012dce7a973c5fa27c2da99922fb8f98b` appends both rank
filter EG IL controls at lanes58/59, bringing that checkpoint’s roster to **60 lanes**.
Its reviewed candidate passed12 direct native runs:72 full cases and72
compatibility cases,1368 renders total. All216 actual candidate60/frozen58
no-queue and Legacy/PURE pairs matched byte-for-byte. Maximum partition error
was0; all72 pre3100 prefixes were exact. All24 PANEL tail cases had positive
effects and all48 factory/MEMORY masking comparisons were exact. See [filter IL
evidence and exact hashes](VST3_FILTER_IL_AUTOMATION.md).

These existing live release-target updates preserve precise filter-envelope
state rather than applying ordinary rank smoothing. The curve starts after
all note-offs, with a longer VCA release retaining an audible tail; selected-source
authority and provisional HP/LP envelope gains remain unchanged. This finite
release-window result does not qualify completed Idle, full release response,
stage timing accuracy, simultaneous-lane behavior or calibrated hardware.
Linux float32 coverage is44.1/48/96kHz,realtime/offline,8/16 capacities.
Direct runs are not CTest or historical-suite reruns. Broad integration/release
qualification and retained candidate binary/source hashes remain separate from
this published source commit.

The preceding VCA SUSTAIN checkpoint remains historical:

Published `d6ca718912791d65f45189682cd906f9da7e6f1f` appends both rank
VCA envelope SUSTAIN controls at lanes56/57, bringing that checkpoint’s roster to
**58 lanes**. Its reviewed candidate passed12 direct native runs:72 full cases
and72 compatibility cases,1368 renders total. All216 actual candidate58/frozen56
no-queue and Legacy/PURE pairs matched byte-for-byte. Maximum partition error
was0; all72 pre512 prefixes were exact. All24 PANEL cases had positive effects
and all48 factory/MEMORY masking comparisons were exact. See [VCA SUSTAIN
evidence and exact hashes](VST3_VCA_SUSTAIN_AUTOMATION.md).

These are live envelope-target updates, not ordinary smoothed gain controls.
Existing precise envelope state, stage selection and decay law remain unchanged,
as does selected-source authority. Short explicit attack/decay settings provide
a held-note target witness, not independent stage timing, complete release,
simultaneous-lane or calibrated hardware qualification. Linux float32 coverage
is44.1/48/96kHz,realtime/offline,8/16 capacities. Direct runs are not CTest or
historical-suite reruns. Broad integration/release qualification and retained
candidate binary/source hashes remain separate from this published source commit.

The preceding VCF LEVEL checkpoint remains historical:

Published `f0be8348906fd844e2ea655685dc2be25ab09ee6` appends both rank
VCF LEVEL controls at lanes54/55, bringing that checkpoint’s roster to **56 lanes**.
Its reviewed candidate passed12 direct native runs:72 full cases and72
compatibility cases,1368 renders total. All216 actual candidate56/frozen54
no-queue and Legacy/PURE pairs matched byte-for-byte. Maximum partition error
was0; all72 pre128 prefixes were exact. All24 PANEL cases had positive effects
and all48 factory/MEMORY masking comparisons were exact. See [VCF LEVEL
evidence and exact hashes](VST3_VCF_LEVEL_AUTOMATION.md).

Existing live smoothing, filtered-path gain before separate sine addition and
source authority remain unchanged. Direct sine contribution was disabled for
PANEL excitation; no sine-only inertness or perfectly pure filter-output claim
is made. These isolated lanes provide finite sounding-path coverage, not
calibrated gain, simultaneous-lane, whole-voice or hardware qualification.
Linux float32 coverage is44.1/48/96kHz,realtime/offline,8/16 capacities.
Direct runs are not CTest or historical-suite reruns. Broad integration/release
qualification and retained candidate binary/source hashes remain separate from
this published source commit.

The preceding PWM checkpoint remains historical:

Published `cec48b334709fffb4dc5803f5798f82e75cf5a4f` appends both ranks’
PWM speed/depth controls at lanes50..53, bringing that checkpoint’s roster to
**54 lanes**. Its reviewed candidate passed12 direct native runs:144 full
cases and144 compatibility cases,2784 renders total. All432 actual
candidate54/frozen50 no-queue and Legacy/PURE pairs matched byte-for-byte.
Maximum partition error was0; all144 pre128 prefixes were exact. All48 PANEL
cases had positive effects and all96 factory/MEMORY masking comparisons were
exact. The24 additional PANEL speed controls at selected-rank PWM depth0 were
PCM-exact, separate from the432 baseline pairs. See [PWM evidence and exact
hashes](VST3_PWM_AUTOMATION.md).

Existing PWM phase, pulsewidth law, smoothing and source authority remain
unchanged. Depth0 establishes PCM equality only, not phase-state or later-depth
equivalence. These isolated lanes provide finite startup/transient coverage,
not full LFO-cycle, calibrated pulsewidth, alias, hardware or simultaneous-lane
qualification. Linux float32 coverage is44.1/48/96kHz,realtime/offline,8/16
capacities. Direct runs are not CTest or historical-suite reruns. Broad
integration/release qualification and retained candidate binary/source hashes
remain separate from this published source commit.

The preceding AFTER TOUCH checkpoint remains historical:

Published `b1019b1fdc74d2e2df1bf30999be318e2f1657a3` appends both ranks’
AFTER TOUCH brilliance/level amounts at lanes46..49, bringing that checkpoint’s
roster to **50 lanes**. Its reviewed candidate passed12 direct native runs:
144 full cases and144 compatibility cases,2832 renders total. All432 actual
candidate50/frozen46 no-queue and Legacy/PURE pairs matched byte-for-byte.
Maximum partition error was3.72529e-09 under the unchanged2e-6 threshold,
not zero; all144 pre256 prefixes were exact. All48 PANEL pressure-ON cases
had positive effects and all96 factory/MEMORY masking comparisons were exact.
The48 additional PANEL pressure-OFF controls were exact, separate from the432
baseline pairs. See [AFTER TOUCH evidence and exact hashes](VST3_AFTER_TOUCH_AUTOMATION.md).

Existing smoothing, modern TouchCircuit pressure nodes and source authority
remain unchanged. Pressure-OFF establishes PCM equality only, not internal
state or later-pressure equivalence. These isolated rank-amount tests do not
qualify simultaneous-rank or calibrated hardware touch behavior. Linux float32
coverage is44.1/48/96kHz,realtime/offline,8/16 capacities. No failures or retries
occurred in this gate; direct runs are not CTest or historical-suite reruns.
Broad integration/release qualification and retained candidate binary/source
hashes remain separate from this published source commit.

The preceding INITIAL TOUCH checkpoint remains historical on its own inputs:

Published `ea6c58b08cc9f9ed466d2f6427b0a4f67c05bc7e` appends both ranks’
INITIAL TOUCH brilliance/level amounts at lanes42..45, bringing that checkpoint’s
roster to **46 lanes**. Its reviewed candidate passed12 direct native runs:
144 full cases and144 compatibility cases,2736 renders total. All432 actual
candidate46/frozen42 no-queue and Legacy/PURE pairs matched byte-for-byte.
Maximum partition error was0 under the unchanged2e-6 threshold, and all144
pre128 prefixes were exact. All48 PANEL cases had positive effects; all96
factory/MEMORY selected-source masking comparisons were exact. See
[INITIAL TOUCH evidence and exact hashes](VST3_INITIAL_TOUCH_AUTOMATION.md).

Existing smoothing and the modern TouchCircuit held b node remain unchanged.
The amount is live for PANEL; factory/MEMORY masks are preserved, not bypassed
or counted as positive amount modulation. These are isolated rank-amount tests,
not simultaneous-rank, hardware-touch calibration or source-switch qualification.
Linux float32 coverage is44.1/48/96kHz,realtime/offline,8/16 capacities. No
failures or retries occurred in this gate; direct runs are not CTest or
historical-suite reruns. Broad integration/release qualification and retained
candidate binary/source hashes remain separate from this published commit.

The preceding BRILLIANCE checkpoint remains historical on its own inputs:

Published `8cdf8a5d38fd81dd19a32f719f4e6751fcd1d7b4` appends existing
keyboard BRILLIANCE LOW/HIGH at lanes40/41, bringing that checkpoint’s roster to
**42 lanes**. Its reviewed candidate passed12 direct native runs:72 full
cases and72 compatibility cases,1368 renders total. All216 actual candidate42/
frozen40 no-queue and Legacy/PURE PCM pairs matched byte-for-byte. Both
isolated lanes had maximum observed partition error0 under the unchanged2e-6
threshold, and all72 pre64 prefixes were exact. LOW effects ranged
.0100588–.104127; HIGH effects .00195321–.0426371. See [BRILLIANCE evidence
and exact hashes](VST3_KBD_BRILLIANCE_AUTOMATION.md).

The levers were tested separately on responsive MIDI24 and62 base cohorts,
with the opposite BRILLIANCE lever and both LEVEL levers0. This does not
qualify simultaneous lanes, independent-key response or calibrated hardware
cutoff/contour behavior. Linux float32 startup runs cover44.1/48/96kHz,
realtime/offline,sources13/0/11 and8/16 capacities. No failures or retries
occurred in this gate; these were direct runs, not CTest or historical-fixture
reruns. Broad integration/release qualification and retained binary/source
identities remain separate from this published source commit.

The preceding HIGH keyboard-level checkpoint remains historical on its own inputs:

Published `b007851c65bb7e015649ab8273505191e84cd9d3` appends existing HIGH
keyboard level at lane39, bringing that checkpoint’s roster to **40 lanes**.
Its reviewed candidate passed12 direct native runs:36 full cases and36
compatibility cases,684 renders total. All108 actual candidate40/frozen39
no-queue and Legacy/PURE PCM pairs were byte-identical. Maximum observed
partition error was0 under the unchanged2e-6 threshold; all36 pre64 prefixes
were exact and later effects ranged .00139232–.0276816. No failed attempt or
runtime retry occurred in this HIGH gate. See [HIGH keyboard-level evidence
and exact hashes](VST3_KBD_LEVEL_HIGH_AUTOMATION.md).

The unchanged note-base helper explicitly selects MIDI62/63 for capacity8
and62..77 for capacity16, source-proven HIGH-responsive physical keys38..53.
This qualifies bounded aggregate waveforms, not independent-key behavior or
calibrated hardware contours. Linux float32 runs cover44.1/48/96kHz and
realtime/offline modes, not CTest, historical-fixture reruns, broad integration
or release qualification. Published source identity does not replace retained
candidate source/binary hashes.

The preceding LOW keyboard-level checkpoint remains historical on its own inputs:

Published `37c6438b087be0b83e0f951884bb07ae1728c396` appends existing LOW
keyboard level at lane38, bringing that checkpoint’s roster to **39 lanes**.
The corrected qualification passed24 direct processes and1116 renders:
36 full cases,36 lower-cohort compatibility cases and72 historical-host
compatibility cases. All216 actual capture pairs matched:108 candidate39/
frozen38 lower-cohort pairs plus108 rebuilt/frozen historical-host default
pairs on the same frozen38 module. Partition error was0 under the unchanged
2e-6 threshold; all36 pre64 prefixes were exact and later effects positive.
See [LOW keyboard-level evidence and exact hashes](VST3_KBD_LEVEL_LOW_AUTOMATION.md).

The original attempt remains a failure: one process,78 renders and six zero
positive-effect checks. Including it, the retained history is25 processes
and1194 renders, not25 passing processes. The default MIDI48..63 cohort did
not excite LOW; the corrected opt-in MIDI24..39 cohort is source-proven
responsive. Default helper notes remain48 onward and received the separate
compatibility-only check, not a full historical feature-suite rerun.

This is bounded Linux float32 startup/transient qualification at44.1/48/96kHz,
realtime/offline,sources13/0/11 and8/16 capacities. It is not independent-key,
full keyboard-contour or hardware calibration, broad integration or release
qualification. The retained evidence discloses a documentation-only change
after build start; executable inputs remained unchanged during the matrix.
Published source identity does not replace exact candidate binary/source hashes.

The preceding INITIAL PITCH BEND checkpoint remains historical on its own inputs:

Published `1611b5b4dc52abc5dc8709ae14ed0ab59ad543ea` appends existing INITIAL PITCH BEND
amount at lane37, bringing that checkpoint’s roster to **38 lanes**. Its reviewed
candidate passed six full direct host runs (36 cases) and six frozen37
compatibility-only runs,684 renders total. All108 actual no-queue and
Legacy/PURE capture pairs were byte-identical. Maximum partition error was
1.49012e-08 under the unchanged2e-6 threshold, not zero. All36 prefixes before
sample64 were exact; later aggregate waveform effects were positive. See
[INITIAL PITCH BEND evidence and exact hashes](VST3_INITIAL_BEND_AUTOMATION.md).

The existing smoothed amount acts live on the modern TouchCircuit transient,
not a note-latched amount or the simple legacy exponential path. This is
bounded Linux float32 startup qualification at44.1/48/96kHz, realtime/offline,
with sources13/0/11 and8/16 capacities. It is not isolated TWS/pitch calibration,
hardware timing, broad integration or release qualification. There is no
bypass-control claim. These were direct runs, not CTest or historical-fixture
reruns; published source identity does not replace candidate binary/source hashes.

The preceding MASTER SUSTAIN checkpoint remains historical on its own inputs:

Published `c0667f5450de3930baedb5e0b7de40d0060b8519` appends existing
MASTER SUSTAIN at lane 36, bringing that checkpoint’s roster to **37 lanes**.
Its reviewed candidate passed six full direct host runs (36 cases) and six
frozen36 compatibility-only runs. All 108 actual no-queue and Legacy/PURE
capture pairs matched byte-for-byte. Observed partition error was zero under
the unchanged 2e-6 threshold. All 36 prefixes before sample 3100 and all 36
assigned-open footswitch controls were exact; positive released-tail effects
were measured after the change. See [MASTER SUSTAIN evidence and exact
input hashes](VST3_MASTER_SUSTAIN_AUTOMATION.md).

This is a bounded live release-time test in static sustain mode 0: all note-offs
precede the automation change, and the performance term stays above the tested
rank/key release floors. It does not establish a calibrated hardware law, full
release behavior, gate-switching or internal-state equivalence. These were
direct Linux float32 runs at 44.1/48/96 kHz in realtime/offline modes, not CTest,
historical-fixture reruns or broad integration/release qualification. Published
source identity does not replace retained candidate source/binary hashes.

The preceding portamento checkpoint remains historical on its own inputs:

Published `a5f46bc3786f3d0f97872c709e05ffc34245a822` appends existing
portamento time at lane35 after the unchanged 35-lane prefix, bringing
that checkpoint’s roster to **36 lanes**. Its reviewed candidate passed six full host
runs (36 cases) and six frozen35 compatibility-only runs. All 108 actual
no-queue and Legacy/PURE capture pairs matched byte-for-byte. Maximum
partition error was 4.47035e-08, below the unchanged 2e-6 threshold, not zero;
all 36 assigned-open footswitch comparisons were PCM-output inert only.
These were direct runs, not CTest or historical-fixture executions. See
[portamento evidence and exact input hashes](VST3_PORTAMENTO_AUTOMATION.md).

This scope is static smooth mode0 with strictly positive time queues and a
reset-pitch60 to first-note48 onset transient, not broad legato behavior,
gate switching or internal-state equivalence. It covers Linux float32,
44.1/48/96 kHz, realtime/offline and 8/16 capacities; it is not broad
integration, hardware/DAW or release qualification. Published source identity
does not replace the retained candidate binary/source hashes.

The preceding CHORUS and TREMOLO checkpoints remain historical on their own inputs:

Published `278d5c5fce7f45f83e17589f17811f515335509c` appends chorus speed/depth
after the unchanged 33-lane prefix, bringing that checkpoint's roster to **35 lanes**.
Its reviewed candidate passed six full CHORUS-mode host runs (72 cases) and
six frozen33 compatibility-only runs, with all 216 actual no-queue and
Legacy/PURE capture pairs byte-identical. Maximum partition errors were
5.96046e-08 for speed and 3.72529e-08 for depth, below the unchanged 2e-6
threshold, not zero. All 72 off-state controls were PCM-output inert only,
not evidence of internal-state identity. These were direct host runs, not
CTest or historical-fixture reruns. See [CHORUS evidence and exact input
hashes](VST3_CHORUS_AUTOMATION.md).

Published `bd4c9c6f743af57a42240b86d43d0f98cfe58f03` separately qualifies
these existing lanes in static TREMOLO mode, without another roster or
production change. Six direct runs passed 72 cases (1,152 renders), with
maximum speed/depth partition errors 1.49012e-08/7.45058e-09. Its 216 retained
capture identities are not baseline comparisons; no baseline replay occurred.
See [TREMOLO evidence and limits](VST3_TREMOLO_AUTOMATION.md).

Both checkpoints are bounded 4,096-frame Linux float32 startup/transient
qualifications at 44.1/48/96 kHz, realtime/offline, with static effect modes.
They do not establish full-cycle, steady-state, mode-switch, combined-effect,
broad integration, hardware/DAW or release qualification. Published source
identities above do not replace the retained candidate binary/source hashes.

The earlier 30-lane RING checkpoint remains historical on its own inputs:

Published `420ac3b8c6674cb004e8a796cc8fecd93a561116` appends RING speed and
amount after the unchanged 28-lane prefix, bringing the roster to 30 lanes.
Its reviewed candidate passed 18 direct full-host runs (180 cases: 72 RING,
36 legacy SUB speed and 72 legacy-combined) with zero observed partition error.
Eighteen separate baseline compatibility runs retained all 540 no-queue and
Legacy/PURE captures byte-for-byte against the frozen 28-lane module. One full
baseline negative control failed exactly 12 partition checks, as expected;
those failures are retained and are not passing qualification. These were
direct runs, not CTest executions. All 19 earlier contracts remain unchanged.
The zero-amount speed control establishes PCM-output equality only, not internal
oscillator-state equivalence. See [exact hashes and limits](VST3_RING_AUTOMATION.md).

The preceding legacy SUB speed checkpoint remains historical on its own inputs:

Published `d5a772e0f0a885f2e76f822cd4484683a2f8639b` appends compatibility-mode
SUB speed after the unchanged 27-lane prefix, bringing the roster to 28 lanes.
Its reviewed candidate passed 18 direct full-host runs (216 cases: 36 new speed,
108 individual touch and 72 combined touch) with zero observed partition error.
Eighteen separate baseline compatibility runs retained all 648 no-queue and
Legacy/PURE captures byte-for-byte against the frozen 27-lane module. One full
baseline negative control failed exactly six partition checks, as expected;
those failures were retained and are not passing qualification. These were
direct runs, not CTest executions. All 18 earlier contracts remain unchanged.
See [exact hashes, build/run evidence and limits](VST3_SUB_LEGACY_SPEED_AUTOMATION.md).

The preceding touch checkpoint remains historical on its own inputs:

Published `72934df6349422a70891a11221cde22ad49bec35` extends the append-only
roster to 27 lanes with SUB touch speed/VCO/VCF amounts. Its reviewed candidate
passed **24/24 selected native CTests** (432 scenarios), including 108 touch
cases, and retained all 816 scoped compatibility captures byte-for-byte against
the frozen 24-lane module. One baseline negative control had exactly 18 expected
partition failures; it was retained and not counted as a passing gate. See
[exact module/source hashes, evidence and limits](VST3_SUB_TOUCH_AUTOMATION.md).

This is bounded Linux float32 protocol/render qualification, not a new full
core/native integration, DAW, GUI, hardware-fidelity or release checkpoint.
Earlier integration and download pins below retain their original scope.

## Latest local native processor/component integration

Pinned source `c8c3dbf44c2aa18ae6719340cc9fef82aefb1015` passed all **13/13 selected
direct CS80Snapshots suites**, zero failures, in one serial run (15.67 seconds).
The current Release target had no outstanding rebuild work. Separate attempts
at display-driven UI smoke and teardown were blocked before application startup
by local display-socket creation restrictions; neither gate passed. See the
[exact suite list, blocker and limits](NATIVE_COMPONENT_INTEGRATION_C8C3DBF.md).
This is not a full native, wrapper/validator, rendered UI or DAW qualification.

## November 10 bounded correctness updates

These changes correct event/state behavior without changing the declared
hardware-fidelity or release-qualification limits:

| Area | Published source | Scope |
| --- | --- | --- |
| MTS live-note admission | `dc086d2` | [Non-real-time tuning affects future notes; addressed real-time changes retain bounded scope](MTS_NON_REALTIME_ADMISSION.md). |
| Tuning-map lifecycle | `58aabe6`, `215b900` | [Excluded strikes preserve Sustain-II tails](UNMAPPED_SUSTAIN_ADMISSION.md); [waiting-key promotion rechecks current mapping](WAITING_TUNING_ADMISSION.md). |
| Arpeggiator state | `03cb6ac`, `cfa9073`, `8d97c92` | [Mode-transition scanner cleanup](ARP_SCANNER_TRANSITIONS.md), [live sequence edits](ARP_LIVE_SEQUENCE_EDITS.md), and [reset/parameter synchronization](ARP_RESET_PARAMETER_SYNC.md). |
| MTS empty mask | `af3309d` | [An empty scale/octave channel mask is a no-op](MTS_EMPTY_CHANNEL_MASK.md); partial nonempty masks retain the documented global-table limitation. |
| MPE panic | `60419bf` | [Active master CC120 clears its current zone](MPE_MASTER_SOUND_OFF.md), with controller state and unrelated channels retained. |
| Malformed input | `19e771b`, `784f94f`, `d00d8bf0` | [Non-finite channel-mode rejection](CHANNEL_MODE_FINITE_ADMISSION.md), [controller-channel admission](CONTROLLER_CHANNEL_ADMISSION.md), and [note/scanner admission](NOTE_EVENT_ADMISSION.md). |
| Editor interaction lifecycle | `acda5b9`, `6f6d3fc`, `47acee1`, `782019d` | [Stale knob menu rejection](KNOB_CONTEXT_LIFECYCLE.md), [inline editor ownership](INLINE_VALUE_EDITOR_LIFECYCLE.md), [MIX percentage-pair validation](MIX_PERCENTAGE_PAIR_ADMISSION.md), and [balanced caption reset gestures](CAPTION_RESET_GESTURE.md). |

Recent follow-ups: [pointer command admission](POINTER_COMMAND_GESTURE_ADMISSION.md)
(`f01ff29`), [arrow admission](ARROW_KEY_ADMISSION.md) (`91bd8d4`),
[slider reset admission](SLIDER_DOUBLE_CLICK_ADMISSION.md) (`4c0999f`), and
[pan typed-value round trips](PAN_TYPED_VALUE_ROUNDTRIP.md) (`c8c3dbf`).

## Changes since the October 31 integration

The original eight-workstream integration is recorded in
[workstreams/HANDOVER.md](workstreams/HANDOVER.md). The October 31 scorecard and
handover PDF remain dated evidence, not current-head measurements.

| Area | Current state | Source / scope |
| --- | --- | --- |
| Sixteen-voice UI ownership | Accepted extension notes retain release and gesture-pressure ownership; conservative alias exclusion spans both instruments | `6e2c171`; [scope and regressions](SIXTEEN_VOICE_UI_OWNERSHIP.md). No source-targeted host/UI note-off expansion; the later meter correction is listed separately. |
| Voice activity display | One coherent audio-owned snapshot covers both instruments; toolbar/monitor add an extension row and report applied 8/16 capacity | `8ade562`; [scope and local evidence](VOICE_ACTIVITY_TELEMETRY.md). Local visual execution was display-blocked; exact-source remote CI and screenshots passed as recorded below. |
| Parameter editing | Shift+F10 opens existing parameter menus; named units require complete tokens rather than accepting arbitrary suffixes | `8ade562`; [interaction and native test scope](workstreams/ws8-studio.md). No new MIDI routing, numeric display law or MIX-ratio syntax. |
| Scala mapping session ownership | KBM imports use current processor SCL source; stale in-flight mapping publication is rejected across restore/clear/replacement | `8ade562`; [scope and regressions](SCALA_KBM_SESSION_OWNERSHIP.md). Native component/processor checks do not automate a platform file picker or qualify a DAW. |
| Transient performance controls | Releases active pitch on controllers-strip hide/destruction and active ribbon on performance-deck destruction; idle host pitch and latched controls are retained | `31a6d5b`; [scope and regression evidence](TRANSIENT_CONTROL_LIFECYCLE.md). Local component/processor checks and independent review, not native capture-loss, ancestor-hide or DAW qualification. |
| Offline sparse memory solve | Retains all 94 source nodes and gates each accepted candidate with an outward full-node residual bound for the prepared coefficient graph | `31a6d5b`; [numerical contract, evidence and timing blocker](analysis/memory-sparse-update.md). Not component-tolerance/hardware proof; cost exceeds the proposed audio-update schedule. No Engine activation. |
| Keyboard performance controls | Pitch/modulation wheels and the expression pedal support Tab and guarded key adjustment; spring release and latched values are preserved | `34358cee`; [controls and local evidence](PERFORMANCE_WHEEL_KEYBOARD.md). Direct event/processor checks, not physical key/focus/repeat, screen-reader or DAW qualification. |
| Preset preview safety | Rejects unrelated/wrong-case XML roots, substitutes finite imported-value fallbacks and clamps release-preview endpoint overshoot | `34358cee`; [scope and fresh evidence](PRESET_PREVIEW_BOUNDARIES.md). Preview/edit-reference boundary only; processor restore, finite-token grammar, state format and DSP unchanged. |
| Offline certificate cost | Centered verified-log series retains outward rounding and an analytic tail while reducing measured certificate cost | `34358cee`; [paired timing and mathematical scope](analysis/memory-certificate-cost.md). Still too costly for the proposed audio schedule; no Engine activation or whole-instrument CPU qualification. |
| Windows validator transfer | Pins the official pluginval 1.0.4 archive and bounds transport attempts before execution | `c7595a4`; [transfer contract and evidence](PLUGINVAL_WINDOWS_DOWNLOAD.md). No validator retry, relaxed gate or Linux crash-repair claim. |
| Validator termination diagnostics | Records raw process return code, termination kind and signal metadata while preserving the original normalized status and failure verdict | `4278da7`; [scope and fixtures](VALIDATOR_DIAGNOSTICS.md). No retry-policy change, crash attribution or repair claim; older normalized exit codes remain ambiguous. |
| Offline interval operations | Point-operand and endpoint-extrema paths avoid redundant arithmetic while retaining observed endpoint/status compatibility | `4278da7`; [point paths](analysis/memory-interval-cost.md), [extrema](analysis/memory-interval-extrema-cost.md). Timing remains mixed/modest and the audio-update budget is not met; no Engine activation. |
| Rejected one-log span research | Both prototypes failed the preregistered availability gate and remain archived outside production source | `4278da7`; [negative result and reproduction](analysis/memory-log-span-cost.md). No timing, sanitizer/profile qualification or audio-activation claim for the rejected prototypes. |
| Tuning admission | Bounds KBM integers before narrowing, validates nominal float-frequency construction and rejects reserved MTS bits before publication | `6e2c171`; [focused tests and exclusions](TUNING_ADMISSION_SAFETY.md). No audible-frequency clamp or Scala/KBM editor-lifetime change. |
| Live dispersion envelope timing | Source Performance envelope timing follows smoothed ratios per internal sample, independently of host block partitioning | `6e2c171`; [regressions and moving-control cost](LIVE_DISPERSION_TIMING.md). Moving performances intentionally change; no whole-instrument CPU qualification. |
| Distribution verification and preflight | Offline bundle inventory/integrity verifier; Windows payload preflight and reparse-point rejection; macOS/Debian staging permissions independent of caller umask | `a6569a0`, `a83acd5` (#104), `c55c216` (#101); [installation and verification](INSTALL_AND_QUICKSTART.md), [qualification scope](release/RELEASE_QUALIFICATION.md). No refreshed downloads or native installation claim. |
| MSEG coordinate import | Rejects overflowing coordinate exponents before JUCE numeric conversion | `ead4d25` ([PR #89](https://github.com/cheshmakzanoon-ops/yamaha-cs80/pull/89)); malformed-input guard, not a new MSEG synthesis law. |
| Preset trash safety | Removes the explicit permanent-delete fallback after a failed trash attempt and reports the failure | `7d42188` ([PR #90](https://github.com/cheshmakzanoon-ops/yamaha-cs80/pull/90)); inert handler checks do not qualify native OS trash services or guarantee recoverability on every volume. Exact-source CI scope is recorded below. |
| Loaded memory-panel reference | Adds offline loaded-port linearisation and programmer-control identity tests; no production audio wiring | `ebe9182` ([PR #103](https://github.com/cheshmakzanoon-ops/yamaha-cs80/pull/103)); [reference and limits](analysis/memory-panel-ports.md). Exact-source CI scope is recorded below. |
| Zero-EI VCA fast path | Avoids `exp2` only for signed-zero exponential drive; nonzero inputs keep the original path | `91bc86b` ([PR #91](https://github.com/cheshmakzanoon-ops/yamaha-cs80/pull/91)); [focused exact-output and bounded timing evidence](IG00151_ZERO_EI.md), not whole-instrument CPU target completion. Exact-source CI scope is recorded below. |
| Offline render timing and validation | Demo events dispatch at their exact sample frames; invalid rate/duration/output arguments are rejected and buffered write failures are reported | `be6eb16` ([PR #97](https://github.com/cheshmakzanoon-ops/yamaha-cs80/pull/97)); offline renderer only, not host-automation expansion. |
| Benchmark preflight | Rejects malformed/unsupported filters and empty case selections before creating output artifacts | `36a148a` ([PR #98](https://github.com/cheshmakzanoon-ops/yamaha-cs80/pull/98)); not a fresh CPU scorecard or target-completion claim. |
| Minimum Hermite delay | Keeps clamp bounds ordered for the supported four-sample minimum buffer | `b5e9d7e` ([PR #99](https://github.com/cheshmakzanoon-ops/yamaha-cs80/pull/99)); separate from the earlier rounded-wrap correction. |
| macOS installer fixtures | Restores required notice inputs and registers success-path architecture fixtures with CTest | `47aad3b` ([PR #102](https://github.com/cheshmakzanoon-ops/yamaha-cs80/pull/102)); inert fixture coverage, not native universal installation, signing or notarization. |
| Package notices | Native archives and Windows/macOS/Linux packaging recipes include the embedded Barlow font licence/source notice and MTS-ESP licence | `1f7b407` ([PR #70](https://github.com/cheshmakzanoon-ops/yamaha-cs80/pull/70)); not a retroactive update to historical artifacts or full licence/release qualification. |
| Binary architecture guards | Native archive gate checks ELF/PE/Mach-O identity and architecture; Debian installer rejects mismatched architecture | `41db04b` ([PR #87](https://github.com/cheshmakzanoon-ops/yamaha-cs80/pull/87)), `de856f7` ([PR #88](https://github.com/cheshmakzanoon-ops/yamaha-cs80/pull/88)); not runtime/loadability qualification. |
| CC88 release prefix | Note Off consumes only its channel's pending prefix, preventing it from changing the next unprefixed strike; positive Note On arithmetic is unchanged and release velocity is not synthesized | `0996a15` ([PR #86](https://github.com/cheshmakzanoon-ops/yamaha-cs80/pull/86)); [scope and focused evidence](CC88_RELEASE_PREFIX.md). Processor integration was source-reviewed, not runtime-tested in the focused audit. |
| Linux installer | Derives versioned runtime dependencies from both staged products; rejects invalid ELF types and primary VST3 payloads outside the shipped bundle | `b363b325` ([PR #83](https://github.com/cheshmakzanoon-ops/yamaha-cs80/pull/83)); [packaging requirements](release/RELEASE_QUALIFICATION.md). Not full loadability or bundle-resource validation. |
| Linux packaging evidence | Scoped native check successfully inspected a temporary package from pinned existing binaries; uploads text/JSON evidence only | `fa7e988` ([PR #84](https://github.com/cheshmakzanoon-ops/yamaha-cs80/pull/84)); [scope and input pins](release/LINUX_PACKAGE_QUALIFICATION.md). No package publication, execution or installation; that historical evidence predates #70 notices. |
| Universal macOS installer | Requires executable VST3, AU and Standalone payloads with x86-64 and arm64 slices before staging | `58ed29e` ([PR #85](https://github.com/cheshmakzanoon-ops/yamaha-cs80/pull/85)); [architecture guard](MACOS_INSTALLER_ARCHITECTURE_GATE.md). Native development archives unchanged; real universal-installer qualification still open. |
| Filter resonance cache | Reuses cutoff-dependent coefficients on Q-only updates; cutoff/rate changes retain the full path and the solver law is unchanged | `465e56c` ([PR #76](https://github.com/cheshmakzanoon-ops/yamaha-cs80/pull/76)); [cache scope and measurements](IG00156_RESONANCE_CACHE.md) |
| External modulation | Sparse FIR reconstruction skips guaranteed-zero products while retaining coefficient design, phase and accumulation order | `228cc8d` ([PR #74](https://github.com/cheshmakzanoon-ops/yamaha-cs80/pull/74)); [scope and measurements](EXTERNAL_INTERPOLATOR_SPARSE.md) |
| Delay-reader safety | A rounded circular-buffer position at capacity is wrapped to zero, preventing an out-of-bounds read | `ff9943e`; [DELAY_LINE_WRAP_SAFETY.md](DELAY_LINE_WRAP_SAFETY.md). Previously valid interpolation retains its arithmetic. |
| Host automation | Seventy-eight key VST3 parameters retain sample offsets in modern circuit policy, including PANEL-routed rank SINE/NOISE levels, filter/VCA attack, decay and release times, filter EG IL/AL, VCA envelope SUSTAIN, VCF LEVEL, PWM speed/depth and INITIAL/AFTER TOUCH amounts, live LOW/HIGH keyboard level and brilliance, initial-bend amount, MASTER SUSTAIN release timing and portamento time; RING attack/decay retain fresh-trigger latch semantics, while protected policies and unsupported parameters keep block-rate behavior | `9b8db027`; [HOST_AUTOMATION.md](HOST_AUTOMATION.md) and [SINE/NOISE qualification](VST3_SINE_NOISE_AUTOMATION.md); historical [RELEASE TIME](VST3_ENVELOPE_RELEASE_AUTOMATION.md), [DECAY TIME](VST3_ENVELOPE_DECAY_AUTOMATION.md), [ATTACK TIME](VST3_ENVELOPE_ATTACK_AUTOMATION.md), [filter AL](VST3_FILTER_AL_AUTOMATION.md), [filter IL](VST3_FILTER_IL_AUTOMATION.md), [VCA SUSTAIN](VST3_VCA_SUSTAIN_AUTOMATION.md), [VCF LEVEL](VST3_VCF_LEVEL_AUTOMATION.md), [PWM](VST3_PWM_AUTOMATION.md), [AFTER TOUCH](VST3_AFTER_TOUCH_AUTOMATION.md), [INITIAL TOUCH](VST3_INITIAL_TOUCH_AUTOMATION.md), [BRILLIANCE](VST3_KBD_BRILLIANCE_AUTOMATION.md), [HIGH keyboard level](VST3_KBD_LEVEL_HIGH_AUTOMATION.md), [LOW keyboard level](VST3_KBD_LEVEL_LOW_AUTOMATION.md), [INITIAL PITCH BEND](VST3_INITIAL_BEND_AUTOMATION.md), [MASTER SUSTAIN](VST3_MASTER_SUSTAIN_AUTOMATION.md), [portamento](VST3_PORTAMENTO_AUTOMATION.md), [CHORUS](VST3_CHORUS_AUTOMATION.md) and [TREMOLO](VST3_TREMOLO_AUTOMATION.md) retain their own evidence. AU scheduled ramps are not enabled. |
| Tuning | Pinned 0BSD MTS-ESP client included, with bounded whole-table updates and missing-master fallback | `73e76ce`; [MTS_ESP_INTEGRATION.md](MTS_ESP_INTEGRATION.md). [Per-channel master tables](MTS_ESP_CHANNEL_MASTER.md) are implemented; live external-master DAW qualification remains open. |
| MIDI and note lifecycle | Channel-scoped pressure/MPE routing, retained release ownership, ordered KAS short notes, bounded CC121 controller defaults and live conventional bend-range edits | [VOICE_RELEASE_OWNERSHIP.md](VOICE_RELEASE_OWNERSHIP.md), [MPE_MASTER_BEND_ROUTING.md](MPE_MASTER_BEND_ROUTING.md), [CONTROLLER_RESET.md](CONTROLLER_RESET.md), [PITCH_BEND_RANGE_AUTOMATION.md](PITCH_BEND_RANGE_AUTOMATION.md) |
| Immediate sixteen-voice release | Cancels one matching waiter or releases the newest comparable held group across both instruments; unison groups and UI source guards are retained | `77dfd74`; [bounded contract](SIXTEEN_VOICE_NOTE_RELEASE.md). Deferred/ARP-origin candidates and active/pending scan paths retain broadcast fallback. No complete sixteen-voice release or realtime qualification claim. |
| MPE initial expression | Finite member-channel bend, pressure and CC74 are retained for accepted onsets, with current range/curve conversion and role/reset invalidation | [Bounded contract and evidence](MPE_MEMBER_ONSET.md). Already-active 8/16-voice scope; full zone reset and inactive-extension handoff remain open. Released-target retention is listed separately. |
| MPE released targets | Ordinary member bend, pressure and CC74 no longer retarget an active voice after its accepted gate closes; the raw cache still feeds future onsets | [Bounded contract and evidence](MPE_RELEASED_TARGETS.md). CC121, managers, global matrix/ribbon and current-role exceptions are explicit; no whole-audio isolation or complete MPE-conformance claim. |
| MSEG and FX | Trigger/sustain boundary fixes, mono onset retained while its routes are unused, restarted poly envelopes' rate history cleared, synced rate phase retained across host blocks, and corrections to tape, plate, Drive and loudness-match transitions | MSEG integration `59b1b3c` ([PR #80](https://github.com/cheshmakzanoon-ops/yamaha-cs80/pull/80)); transport fix `ce423e3`; [MSEG_COMBINED_INTEGRATION.md](MSEG_COMBINED_INTEGRATION.md), [MSEG_TRANSPORT_RATE.md](MSEG_TRANSPORT_RATE.md), [ws5 report](workstreams/ws5-modern.md) |
| Presets and state | A/B and Copy undo history, background-save undo preservation, saved names, metadata limits, malformed-state rejection and precision fixes | `e09ddb2`, `f757909`, `f7d1b75`; [ws6 report](workstreams/ws6-library.md), [BOOLEAN_STATE_RESTORATION.md](BOOLEAN_STATE_RESTORATION.md) |
| Voice onset | Unison circuits share the event's glide origin | `f921f54`; changes the affected modern voice-mode onset, not the hardware model |
| OE1 speed endpoint | Reviewed design evidence, source-conflict notes and a standalone diagnostic probe are preserved; shipping audio is unchanged | `fe16f0a` ([PR #9](https://github.com/cheshmakzanoon-ops/yamaha-cs80/pull/9)); [endpoint design](OE1_SPEED_ENDPOINT_DESIGN.md), [source conflict](OE1_SPEED_SOURCE_CONFLICT.md) |
| Editor evidence | Selected LFO waveform previews and actual-editor screenshots | LFO preview fix `ae3958d`; the current [provenance](screenshots/provenance.json) and [image hashes](screenshots/SHA256SUMS) identify the captured source and artifacts. Images do not qualify sound or a release. |

The optimization and MSEG reports retain draft-stage qualification notes as
historical evidence. Those changes are now merged; the dated post-merge checkpoints below
record their tested inputs. The mono/poly MSEG corrections intentionally change affected routed
performances in Legacy and Circuit, while default/no-route playback and PURE
CS-80 remain unchanged.

### Packaging qualification scope

The native [qualification run 42361342953](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42361342953)
passed for PR #84 head `27f7166`, using synthetic merge checkout `f059e7c`.
The binary source and packager were separately pinned to `635ad6e8`, source build
`42351562098`, Linux bundle `13566771073`. It inspected a temporary `.deb` on
Ubuntu 24.04 and recorded libc6 >= 2.38 and libstdc++6 >= 13.1, retaining a
**verified native diversion warning**, not claiming warning-free packaging.

Its [evidence artifact 13569616693](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42361342953/artifacts/13569616693)
was downloaded and verified: eight UTF-8 text/JSON files, no `.deb`, ELF or
product/source archive, and `package_uploaded=false`. Authenticated access was
rechecked; it expires **November 14, 2026 at 09:50 UTC**. This is packaging evidence
for the named pinned inputs, not qualification of the current `0996a15` downloads
or a newly published installer. That pinned package predates the font/MTS notices now integrated by #70.
Current packaging recipes include those notices, but historical downloads are unchanged;
complete licence, installation, host, loadability and release qualification remain open. #85's macOS guard has inert-fixture coverage, not native universal-installer,
Apple `lipo`, signing or notarization qualification.

The earlier native [qualification run 42360278797](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42360278797)
at `851ecb76` failed its original strict warning gate after temporary package
creation; inspection did not finish and evidence upload was skipped. That failure
remains historical. The packaging report's pre-success "unrun" notes describe
that earlier stage; the verified result above is current.

### Post-merge CI and download checkpoints

The historical remote CI checkpoint below is **`c7595a4`**, verified November 9.
It is not verification of the November 10 source checkpoint.
All three workflows used exact source
`c7595a4ea5171b4bf710e53c635938aa7d363e27`:

| Workflow | Verified result | Scope |
| --- | --- | --- |
| [Build 42574049408](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42574049408) | Passed | All six jobs: four native build/test/package jobs and Linux/Windows VST3 pluginval at strictness 10. Both macOS jobs also passed VST3/AU validation. |
| [DSP 42574049467](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42574049467) | Passed | All five measurement, synthetic-reference, source compatibility, default-off raw-audio and sanitizer jobs. |
| [Screenshots 42574049530](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42574049530) | Passed | Actual-editor capture/publication; screenshot-only child `68fcb88`. |

These runs cover the pinned Windows transfer and preceding diagnostic/offline
interval additions, not later source changes. Downloads remain at `0996a15`;
no newer binary artifacts are inspected or substituted here. The successful
validators do not establish a repair for earlier failures.

Preceding implementation `4278da7c4348c294c57d5232a0b772c40dbe3348`
[build 42566983265](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42566983265)
failed in both attempts 1 and 2 at Windows `Fetch pluginval 1.0.4`; Windows
validation did not run. The other five jobs passed in attempt 1 and were
carried forward into the targeted Windows-only retry.
[DSP 42566983245](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42566983245)
passed all five jobs, and [screenshots 42566983298](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42566983298)
passed, published in `c39d7be`. Those failed download attempts remain failures,
not evidence that Windows plugin validation passed or found a product defect.

The earlier `34358cee9979042b7002f076f5079cd627858e9a` checkpoint remains:

| Workflow | Verified result | Scope |
| --- | --- | --- |
| [Build 42560190680](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42560190680) | **Failed** | Five of six jobs passed: all four native build/test/package jobs and Windows VST3 pluginval. [Linux validation job 128983338914](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42560190680/job/128983338914) failed `Validate VST3 GUI under Xvfb`; headless and fixed-seed `0x2da72db` validation passed. Both macOS jobs passed their VST3/AU validation steps. |
| [DSP 42560190816](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42560190816) | Passed | All five jobs: measurements, synthetic-reference comparison, source circuit V1/audio V2 compatibility, default-off raw audio and sanitizers. |
| [Screenshots 42560190753](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42560190753) | Passed | Actual-editor capture and publication; screenshot-only child `7614b84`. |

The original Linux GUI invocation remains failed. Diagnostic results and later
passes do not establish fault ownership or a repair; new termination metadata
cannot retroactively distinguish a native signal from an ordinary exit code in
older logs.

The preceding fully green implementation
`31a6d5b92a8e38e198bf25294c55bbb40733627d` passed
[build 42555472624](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42555472624)
(all six jobs), [DSP 42555472549](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42555472549)
(all five jobs) and [screenshots 42555472666](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42555472666).
Its `b542c17` screenshot publication is superseded by `7614b84`. That earlier
success neither qualifies later sources nor explains any failed invocation.

The preceding `8ade562f5e3baf1e0803b84249baca6b8e87eef8` checkpoint passed
[build 42550168950](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42550168950),
[DSP 42550169047](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42550169047)
and [screenshots 42550168949](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42550168949),
verified November 9. Its screenshot-only publication `9f89f8a` is superseded by
`b542c17`, not erased from the history.

The preceding `0b340d9aa925141c50f2fd354c936163afe8a4f0` checkpoint also passed
[build 42543080210](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42543080210),
[DSP 42543080241](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42543080241)
and [screenshots 42543080304](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42543080304),
verified November 9 at 02:08 UTC. Its screenshot-only publication `0d84fe3`
was replaced by the `8ade562` captures, not erased from the history.

The following `a6569a0` results are retained **historical observations**, checked
on November 9 at 00:43 UTC, not a claim of live status or the latest remote base:

| Workflow | Result at check | Scope |
| --- | --- | --- |
| [Build 42540498214](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42540498214) | Incomplete | Linux, Windows and Intel macOS jobs pending; Apple Silicon build finished and tests were running. No complete native/validator qualification. |
| [DSP 42540498181](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42540498181) | Incomplete | Numerical measurements, synthetic comparison, circuit V1/audio V2 compatibility and default-off raw audio passed; sanitizers still running. |
| [Screenshots 42540498223](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42540498223) | Passed | Actual-editor capture and publication; source `a6569a0`, screenshot-only descendant `0831c45`. |

The earlier `7d42188` (#90) exact-source
[build 42539522163](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42539522163)
was canceled: all four native jobs canceled and dependent validators skipped.
Its [screenshots 42539522087](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42539522087)
were also canceled. These outcomes are incomplete qualification, not successful
gates or evidence of a product defect.

The reviewed #90 head `cb33f53e792da141244b62f2071e84b75a46cb50` passed its
[PR build 42413160141](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42413160141),
but its [push build 42413149467](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42413149467)
failed Linux headless validation; the later Linux GUI checks were skipped. All
four native jobs and Windows validation passed. The independent inert-handler
UBSan check passed 23 checks; the same harness against the pre-fix main handler
failed five checks, including the permanent-delete call and false success after
trash failure. This verifies handler control flow, not actual OS trash services.
The fix removes the application's explicit fallback only, not native trash
behavior on unsupported filesystems/volumes, and does not guarantee universal
recoverability. Neither the focused test nor a passing PR run erases the failed
push gate or qualifies the merged source.

The preceding `ebe9182` (#103, offline memory reference)
[build](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42539352090),
[DSP](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42539352044) and
[screenshots](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42539352197),
and `91bc86b` (#91, zero-EI VCA fast path)
[build](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42539258668),
[DSP](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42539258711) and
[screenshots](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42539258559)
remain separately scoped. At the November 9 check, #103's build and screenshot
jobs were canceled; four DSP jobs passed but sanitizers were canceled. #91's
build was still incomplete (Linux and Apple Silicon native jobs passed; Windows,
Intel macOS and dependent validation had not completed); its screenshot job was
canceled, and its DSP had three passed jobs plus canceled raw-audio/sanitizer
jobs. No complete pass is inferred. The focused VCA and offline memory-reference
evidence does not substitute for these workflows. The memory model remains
offline, not a shipping audio path.

The preceding implementation checkpoint `47aad3b` has exact-source results:

| Workflow | Result | Scope |
| --- | --- | --- |
| [Build 42516112281](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42516112281) | **Failed** | All four native build/test/package jobs and Windows validation passed. [Linux validation job 128846901563](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42516112281/job/128846901563) failed GUI-under-Xvfb; headless and fixed seed `0x2da72db` passed. The diagnostic GDB step also failed. This is verified job/step status, not a root-cause finding. |
| [DSP 42516112209](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42516112209) | Passed | All five jobs: numerical measurements, synthetic-reference comparison, circuit V1/audio V2 compatibility, default-off raw audio and sanitizers. No hardware-measured or real-DAW qualification is inferred. |
| [Screenshots 42516112410](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42516112410) | Passed | Actual-editor capture and publication; source `47aad3b`, screenshot-only descendant `1b146c4`. |

The latest `c7595a4` success does not erase either the `47aad3b` or `34358cee`
Linux validator failure. The four README download links remain deliberately
at `0996a15`; no newer artifacts replace them here.

The earlier package-notice checkpoint `1f7b407` was merged from reviewed head
`f45f6426bbbe5eeb4cfb4f21c52ebb6231ccff58`, which passed all four pre-merge runs:
[PR build 42441612078](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42441612078),
[push build 42441609396](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42441609396),
[DSP 42441609392](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42441609392),
and [screenshots 42441609423](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42441609423).
The exact merged tree matches that reviewed head. However, its
[post-merge build 42444667238](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42444667238)
failed Linux GUI-under-Xvfb and GUI state regression validation. All four native
build/test/package jobs and Windows validation passed. This is status metadata,
not a root-cause diagnosis; the earlier passing runs do not erase the failure.
No newer artifacts from this failed run replace the README downloads.

The retained download checkpoint `0996a15` passed
[build 42381017495](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42381017495),
[DSP run 42381017482](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42381017482)
and [screenshot run 42381017512](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42381017512).
All six build jobs passed: Linux, Windows, Intel macOS, Apple Silicon macOS,
and the separate Linux and Windows validation jobs.

The README links four native bundles from that exact source. They include the
CC88 release-prefix fix, MSEG integration, delay-reader safety fix, resonance
cache and sparse external interpolation, superseding the `59b1b3c` downloads.
The artifacts were downloaded and checked: all archive checksums, 54 manifest-listed
regular-file hashes and sizes, source stamps and native architecture headers matched.
Authenticated access was verified on November 7. They are unexpired until
**February 5, 2027 at 14:19 UTC** and require a signed-in GitHub account.
Package inspection does not verify code signatures or notarization. Automated
native CI coverage is not DAW or release qualification; no new installer release
was run.

The 68 current documentation screenshots come from `c7595a4`, published in
screenshot-only child `68fcb88`. All image hashes and the exact PNG inventory
were checked November 9 against [SHA256SUMS](screenshots/SHA256SUMS), and
[provenance.json](screenshots/provenance.json) matches successful
[run 42574049530](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42574049530).
The screenshot tree is unchanged from that publication. It replaces the
`4278da7` captures in `c39d7be`, following `34358cee` in `7614b84` and
`31a6d5b` in `b542c17`. These are not captures of the retained downloads;
rendering establishes neither interaction, sound, numerical-model nor release
qualification.

The preceding #85 integration `58ed29e`
[build 42373292449](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42373292449)
remains a historical **failure**. All four platform build jobs and Windows
validation passed, but [Linux validation job 128411618538](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42373292449/job/128411618538)
failed at `Validate GUI state regression seed 0x2da72db`. Only job/step status
metadata establishes that outcome. Neither #85's passing pre-merge runs nor
`0996a15`'s later success proves a root cause, flakiness or a CC88-related repair.

The #83 [main build 42354715740](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42354715740)
at `b363b325` and #84 [main build 42364637787](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42364637787)
at `fa7e988` remain earlier successful six-job runs. The previous `59b1b3c`
[build checkpoint 42334458843](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42334458843)
also passed, but it predates the CC88 correction and is no longer the download
source listed in the README.

The earlier [build 42274477424](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42274477424)
at `228cc8d` remains a historical **failure**: five jobs succeeded and the
[Apple Silicon job 128087142687](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42274477424/job/128087142687)
was canceled, leaving no ARM bundle or completed ARM qualification for that run.
Later passing checkpoints, including `0996a15`, supply successful four-platform
CI and ARM bundles without changing the earlier outcome.

The still-earlier `f921f54` [build run 42175969425](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42175969425)
failed its Linux fixed-seed GUI validation step. Later passing runs do not
establish that failure's cause or resolve the intermittent validator issue.
[README downloads](../README.md#downloads) also retain the October 31 installers
as explicitly historical artifacts. No tagged GitHub Release has been published.

## Where to read what

| Question | Document |
| --- | --- |
| What are we building, in what order? | [IMPROVEMENT_PLAN.md](IMPROVEMENT_PLAN.md) (plan), [PARITY_ROADMAP.md](PARITY_ROADMAP.md) (workstream detail) |
| What did each workstream do, and what is open? | [workstreams/HANDOVER.md](workstreams/HANDOVER.md) and the `wsN-*.md` reports beside it |
| What is the fidelity target and evidence policy? | [ANALOG_FIDELITY_REQUIREMENT.md](ANALOG_FIDELITY_REQUIREMENT.md) |
| Which control is implemented how, and what is still open? | [HARDWARE_COVERAGE.md](HARDWARE_COVERAGE.md) |
| How does audio flow through the engine? | [ENGINE.md](ENGINE.md) |
| What do the render modes mean? | [SOURCE_PERFORMANCE.md](SOURCE_PERFORMANCE.md), [PARITY_ISOLATION.md](PARITY_ISOLATION.md) |
| Editor layout and rules | [UI_DESIGN_SPEC.md](UI_DESIGN_SPEC.md), [PANEL_CONFORMANCE.md](PANEL_CONFORMANCE.md) |
| Release, installers, licence | [release/RELEASE_QUALIFICATION.md](release/RELEASE_QUALIFICATION.md), [../LICENSE](../LICENSE) (AGPLv3) |
| Build, packages, CI evidence | [README.md](../README.md), [CI_DISTRIBUTION_STATUS.md](CI_DISTRIBUTION_STATUS.md) |

## Engines

- **ENGINE: CIRCUIT** (host parameter `source_performance`, "Engine") renders the
  circuit models below. Fresh instances and new presets use it (plan 1.7, state
  schema 3).
- **ENGINE: LEGACY (compat)** retains the older synthesis voicing and is used for
  sessions saved before schema 3 and the Legacy Library. Pinned render gates
  preserve their tested cases; MIDI/modern-layer bug fixes can change affected
  performances, including the optional KAS scanner ordering described below.
- **PURE CS-80** (`parity_mode`) strips every modern addition (MSEG, matrix,
  modern FX, LFOs, arpeggiator, MPE, microtuning, voice modes, MIDI learn,
  Program Change, voice dispersion and drift) and runs the circuit models only.

## Model register

Which implementation produces audio today, by subsystem. "Circuit engine" means
the Source Performance and PURE CS-80 policies.

| Subsystem | Legacy | Circuit engine | Evidence / reference |
| --- | --- | --- | --- |
| Oscillator | PolyBLEP `Oscillator` (`Dsp.h`) | reconstructed event-driven WSC, PWL sine, shared residual basis (`ReconstructedOscillator.h`) | [OSCILLATOR_PROMOTION.md](OSCILLATOR_PROMOTION.md) |
| HP/LP filters | TPT `Svf` with input `softClip` | IG00156 nodal stage: input networks, 1.2 MOhm/180 pF branch, 1 uF couplings, in-loop OTA, declared Q block and cutoff calibration (`Ig00156Filter.h`) | [IG00156_NODAL_MODEL.md](IG00156_NODAL_MODEL.md), truth `reference/IG00156Network.h` |
| Envelopes | `Adsr`/`FilterEnv` float path | inferred single-RC law | [ENVELOPE_RC_MODEL.md](ENVELOPE_RC_MODEL.md) |
| VCA | linear multiply | IG00151: LI node, exponential SUB input, pedestal, AC coupling, constant-power pan (`Vca.h`) | [IG00151_VCA_MODEL.md](IG00151_VCA_MODEL.md) |
| Voice variation | uniform tune/pan trims, 0.031 Hz sine drift | declared 5 % R / 10 % C card draws into tune, cutoff, Q, level, EG times; 1/f thermal drift with warm-up; sine as "worn unit" (`ToleranceModel.h`) | [TOLERANCE_DRIFT_MODEL.md](TOLERANCE_DRIFT_MODEL.md) |
| Idle voices | frozen, reset on reuse | all 16 chains free-run (closed-form idle oscillator, decimated stages, snapshot replay); cold circuits start warm | [workstreams/ws3-keyboard.md](workstreams/ws3-keyboard.md), truth `reference/FreeRunningVoiceTruth.h` |
| Keyboard | MIDI note to channel | KAS scan option, SH7 hold and droop, ninth-key reconsideration, KBC contour network | ws3 report, `KasScanner.h`, `KbcContour.h` |
| Touch | raw MIDI velocity/pressure | TWS a/b/C nodes, TRG5 diode-OR bus | ws3 report, `TouchCircuit.h` |
| Tone tabs | legacy bridge voicings | `SourceFactoryTones.h` (7 complete, 15 inferred; 25 cells are labelled hypotheses) | [PRESET_CIRCUIT_LEDGER.md](PRESET_CIRCUIT_LEDGER.md), ws6 report |
| SUB | legacy `Lfo` or `OriginalSubOscillator` | IG00150 exponential converter, band-limited saw/square | [ORIGINAL_SUB_OSCILLATOR.md](ORIGINAL_SUB_OSCILLATOR.md), ws4 report |
| Ring modulator | provisional law, ideal multiply | ADJ p19-anchored converter, offset multiplier with declared feedthrough | ws4 report |
| Chorus / tremolo | generic `Chorus` | OE2 BBD with true L/R paths; OE1 four-VCA tremolo | [BBD_CHORUS_MODEL.md](BBD_CHORUS_MODEL.md), ws4 report |
| Wah / EXP | SVF band-pass stand-in, linear expression | `PraWahCircuit` with declared LDR optics; expression through the same cell | [REALTIME_PRA_WAH_MODEL.md](REALTIME_PRA_WAH_MODEL.md), ws2/ws4 reports |
| Noise | per-voice uniform draws | one SUB29 generator with declared PSD; mix-bus hiss PSD | ws2 report, truth `reference/NoiseSpectrum.h` |
| Output | scalar LOW trim + soft clip | -6 dB reference, driver ceiling, `OutputTapCircuit`; LISTENING OUTPUT L/R, GENERAL, PHONES | [OUTPUT_REALTIME_PROPOSAL.md](OUTPUT_REALTIME_PROPOSAL.md), ws2 report |
| Smoothing | per-sample constants | 5 ms one-pole on every continuous control (`Smoothing.h`) | ws2 report |
| Oversampling | 2x | Eco 2x / Standard 2x / HQ 8x; offline HQ; 63 + 8/N samples latency | `Oversampling.h`, ws2 report |

Evidence classes: every law cites a schematic or manual page, or is labelled a
declared inference with its alternatives
([ANALOG_FIDELITY_REQUIREMENT.md](ANALOG_FIDELITY_REQUIREMENT.md)). Nothing in
this register is a measurement of a CS-80. The in-app evidence view
(Studio > SETTINGS) and per-control help compile the same register
(`src/plugin/EvidenceRegister.h`).

## Phase progress

| Plan step | State | Commit |
| --- | --- | --- |
| Plan recorded | done | `97cc815` |
| 0.1 Coefficient caching | done; bit-identical | `7b55d34` |
| 0.1 Resonance-only cache and sparse external interpolation | merged numerical optimizations; focused comparisons preserve tested output, whole-instrument CPU target remains open | `465e56c`, `228cc8d`; reports above |
| 0.1 Split, fast math, structure of arrays | partial: bounded-error tan in the nodal stage (`00ceccf`); fast exp2/pow measured no faster than glibc's and dropped; `processInternalSample` split and SoA not done | `00ceccf` |
| 0.2 Parameter smoothing | done (circuit engine), ws2 | `d29f6eb` |
| 0.3 State schema version | done (schema 3 since ws2) | `eb486cb`, `83f8a0e` |
| 0.5 This status page | done | `eb486cb` |
| 1.1 IG00156 filter, first stage | done | `f6b126d` |
| 1.1b IG00156 nodal stage | done with two reported misses (top-octave small signal at 2x/4x; ADJ p11 HP point) | `c9f8cf0`, `cbd2442` |
| 1.2 Oscillator promotion | done; CPU over budget | `a6a574f` |
| 1.3 RC envelopes | done | `131f72b` |
| 1.4 IG00151 VCA | done; model doc and mutation checks `0dfe4ee`; steal criterion met, retrigger 1.51 on one legacy-bank program (scorecard) | `5e857f5`, `1edc6c0`, `0dfe4ee` |
| 1.5 Gain staging | done | `228459c` |
| 1.6 Oversampling quality | done; Standard set to 2x from the measured alias | `ed877d4`, `ebbe75b` |
| 1.7 Engine default flip | done (plugin layer; INIT and toolbar wired) | `83f8a0e`, `230f367` |
| 2.1 Free-running voices | done; ported to the nodal stage and WSC at the merge; cold circuits warm | `69b8db6`, `342b9be`, `3961c1c` |
| 2.2 KAS scan and sample/hold | done (declared scan period, droop) | `2f274b3` |
| 2.3 Keyboard contour | done (declared key map; C2 conflict reported) | `7f36d1b` |
| 2.4 Touch | done (declared acquisition) | `aeb6719` |
| 2.5 Tolerances and drift | done (declared distributions) | `22b6854`, `a0f6dd5` |
| 3 SUB, ring, OE1/OE2, PRA | done | ws4 merge `195e5e3` |
| 3 Output modes and noise | done | `62e18c1` |
| 3 EXP volume optics | done | `3fcff91` |
| 4.1 Tone tabs | done as review records; cells stay hypotheses | `31e3bab` |
| 4.2 Memory panels | modelled with a truth model; not driving audio | `617259d` |
| 4.3 Curated library | done: 301 circuit-engine programs + 222-program Legacy Library; recalibrated for the final audio | `9beed5e`, `82b9e14` |
| 4.4 Program Change, ribbon CC | done | `ec4d853`, `7c65aaf` |
| 5.1-5.2 Conformance audit, re-layout | done | ws7 merge `bd524f9` |
| 5.3-5.4 Toolbar, Studio view, interaction | done; modern and tuning pages mounted | ws8 merge `5af3b77`, `230f367` |
| 5.5-5.7 Readability, code health, UI tests | done; goldens re-recorded on the integrated tree | `bd524f9`, `41363ea` |
| 6 Modern layer | done (MSEG, LFO waves, matrix, FX, tuning, MPE, voice modes, MIDI learn, undo, A/B, randomise, host groups) | ws5 `5263172`, ws3, ws6, ws8 |
| 6 Sample-accurate automation | partial: thirty key VST3 parameters in modern circuit policy; AU and other parameters remain block-rate | `420ac3b8`; [scope](HOST_AUTOMATION.md) |
| 7 Release qualification | packaging safeguards and scoped Linux ABI evidence merged; complete licence/native installation/host/signing qualification and Linux validation failures remain open | `b363b325`, `fa7e988`, `58ed29e`; exact scope and failures above |

## Scorecard (2026-10-31, `00ceccf`)

`cs80_engine_audit --suite all --rates 44100,48000,96000` and
`cs80_source_factory_benchmark` on this container (4 cores, shared). Circuit
engine unless stated.

| Metric | Target | Before the plan (PDF) | Now |
| --- | --- | --- | --- |
| Worst saw/pulse non-harmonic, ring off | <= -70 dB | -33.8 dB (48 kHz) | **-88.3 dB** (48 kHz), -88.5 (44.1), -89.5 (96) |
| Ring-on rows | — | — | -43.0 dB at every factor: ws4's declared RMO carrier feedthrough, a spectral line, not fold-over |
| Worst tab/program peak at default volume | <= 0 dBFS | +3.9 dBFS | **-0.24 dBFS** (Guitar 1 + Guitar 2 tab, 44.1 kHz); met |
| Worst steal click | <= 1 | 1.17 | **0.92** (Harpsichord 1 + 2, 44.1 kHz); met |
| Worst retrigger click | <= 1 | — (1.26 at ws1's last run) | **1.51** (legacy-bank "Bass Stack" forced onto the circuit engine, 44.1 kHz) and 1.01 (Tone I: Guitar 1, 48 kHz; Legacy reads 1.05 on that row); 281 of 283 rows <= 1 |
| CPU, 8 held voices, 48 kHz, 256 frames | 0.15-0.20 RTF | 0.27 (source) | **0.51-0.61 median, p99 0.66-1.00, 0-12 deadline misses per 1024 callbacks** (min of 3 rounds; worst rows Funky 3+4 and the moving-tab/pressure/wah scenario); not met |
| Idle CPU | <= 2 % | 6-7 % | 0.113 RTF (11 %); not met |
| Legacy CPU, same row | — | — | 0.21-0.27 median |

The retrigger outlier ws1 traced to two same-pitch voices (Funky 3+4 at
44.1 kHz, 1.26) no longer appears: with free-running voices the circuit is never
reset at assignment and the VCA output is AC-coupled. The full audit finds a new
one, "Bass Stack" (a legacy-bank program rendered on the circuit engine by the
audit) at 1.51 against 0.63 in Legacy; a [November 11 targeted replay](analysis/bass-stack-retrigger-replay.md)
reproduces the failure and finds a late attack slope, without establishing a
causal DSP fix (open). Note-off click scores (not part
of the plan's criterion) reach 1.65 (Funky 1 + 2, 48 kHz; 16 of 283 rows above 1, Legacy has 25 rows above 1) on percussive programs; open.

A November 9 [prepared-value cache experiment](analysis/oscillator-profile-cache.md)
was rejected rather than shipped: the bit-compatible narrow candidate failed its
predeclared active-cost gate in both GCC profiles. Its archived component timings
are not a refresh of this historical whole-instrument scorecard; production DSP
and the open CPU/idle targets remain unchanged.
The subsequent [exact knee-lookup experiment](analysis/oscillator-knee-lookup.md)
also failed its dual-profile cost gates. Its two complete matrices are retained;
no scalar or AVX2 knee path is enabled.

CPU: the reconstructed oscillator's event solver (about 29 % of instructions)
and the nodal stages' Newton solve (about 22 %) dominate. The bounded-error tan
was the one libm call worth replacing. The realistic route to the target is a
structure-of-arrays, segment-cached fast path for the oscillator (compute the
next branch event once per analytic segment rather than per sample); not done.

## Pinned compatibility gates

- `dsp-measurements.yml` **legacy-compatibility**: default-off audio byte-identical
  to `d56911a`. These pinned audio cases remain a compatibility requirement.
- `dsp-measurements.yml` **source-factory-compatibility**: circuit V1 numerics
  (`22d6a32`) and circuit-engine audio V2, now **`00ceccf`** (the eight-workstream
  integration). Earlier V2 pins, for historical circuit-engine renders:
  `f6b126d` (Phase 1.1 filter), `131f72b` (Phase 1.3 envelopes), `228459c`
  (Phase 1.5 gain staging), `6e5892f` before that.
- `scripts/check-compat-gates.sh --strict-v2` passes at the re-pin commit `ef9bc35`.

These gates cover their defined render cases, not every MIDI performance.
The optional KAS scanner now preserves same-pitch event arrival order, including
across channels. A strike before a release within one scan can change voice
allocation and playback in saved sessions that enable it. See the explicit
compatibility correction in [ws3-keyboard.md](workstreams/ws3-keyboard.md).
Other optional MIDI and modern-layer fixes have the scoped contracts linked above.

### Historical CI on the October 31 integrated tree

At `e048dca` (the head merged into `main` as `ec72eed`):
[build 41208373099](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41208373099)
passed all six jobs (Linux, Windows, macOS arm64 and Intel builds and tests;
pluginval strictness 10 on Linux VST3 headless, GUI and seeded, Windows VST3,
both macOS VST3, AU and AU GUI) and
[DSP measurements 41208373089](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41208373089)
passed all five jobs, including the ASan + UBSan suite (72 tests). Fixed on the
way, without changing an assertion:

- The sanitizer build had stopped at a GCC 13 UBSan constant-evaluation rule in
  `FactoryToneHypotheses.h` since the ws6 merge (`fd0e851`); three tests then
  outlived Release-build timeouts under the sanitizers, scaled x4 there by
  `CS80_TEST_TIMEOUT_SCALE` (`a2b2452`).
- macOS-only UI-smoke failures: ws8's first-run tour opens by itself in a real
  window and covered the keybed of the real-pointer fixture whenever macOS
  window activation outlasted the tour's first timer tick (diagnosed in
  `b2c2302`, fixed in `e048dca`; the window also floats above other
  applications, `a721935`). The same tour covered the 1120x700
  source-performance screenshot (`a26f583`). The F1 help-card check raced
  JUCE's dismissal of call-outs in a background process (the CI harness is
  never the foreground application on macOS); it now checks before any
  message is dispatched (`df4ba22`, after macos-15-intel failed it on `main`
  at `ec72eed`).

On `main` after [#3](https://github.com/cheshmakzanoon-ops/yamaha-cs80/pull/3) (`55bb043`):
[build 41218764120](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41218764120),
[release 41218772353](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41218772353)
(unsigned: no signing secrets) and
[DSP measurements 41218763836](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41218763836)
passed every job. The README retains the installer artifacts as explicitly
historical downloads and links newer native bundles separately.

A passing Linux pluginval run is not a fix for the intermittent exit 139.

### Intentional circuit-engine sound changes in the `00ceccf` pin

- ws1: nodal IG00156 stage (frequency-dependent resonance, 1 uF coupling corners,
  OTA limit on the gm part, calibrated cutoff); reconstructed oscillator (WSC
  rising saw, comparator pulse, shaped sine, reset plateau, coupling memory,
  8 internal samples of delay); IG00151 VCA (SUB tremolo in dB, LI smoothing,
  AC-coupled -52 dB pedestal, constant-power pan); plan 2.5 card tolerances and
  1/f drift instead of the 22-cent sine.
- ws2: 5 ms control smoothing; shared SUB29 noise with declared PSDs; optical
  expression gain; GENERAL/PHONES outputs (selectable; default unchanged).
- ws3: free-running voices with warm cold circuits; Sustain-I hold and droop;
  ninth-key reconsideration; KBC contour network; TWS a/b/C and TRG5 bus.
- ws4: IG00150 SUB converter; ring converter and offset multiplier; OE1 tremolo;
  OE2 L/R paths, loss, hold, noise, clock feedthrough; PRA optics.
- Integration: Standard oversampling 2x (ws2 had 4x); bounded-error tan.

## Integration decisions

- Fresh instances: factory program 0 on the circuit engine with the original SUB
  panel (ws2). INIT (`loadInitPatch`) is the same kind of start: the factory
  "Init Patch" on the circuit engine with the original SUB. The curated
  "Init Circuit" stays a library program.
- Factory recall zeroes the 16 matrix amounts (they are parameters) and keeps the
  matrix routing (sources, destinations, via, ranks), which is session state like
  MIDI learn; a recalled program is therefore modulation-neutral until an amount
  is raised.
- Drift model is a physical-setup parameter (the unit's condition): program recall
  keeps it.
- The goldens show the circuit-engine default.

## Open issues

- Intermittent Linux pluginval termination remains unresolved, including
  historical exit 139 after SUCCESS and the `34358cee` GUI failure.
  Earlier source reviews and teardown probes did not establish its cause.
  [Termination diagnostics](VALIDATOR_DIAGNOSTICS.md) add evidence, not a repair
  or permission to replace a failed invocation with a passing replay.
- CPU and idle targets remain open. The resonance cache, sparse external
  interpolation and zero-EI VCA optimizations are merged, but their focused
  measurements do not replace a new full scorecard for the merged source.
- Historical `c7595a4` native/validator, DSP and screenshot CI passed. The
  `4278da7` Windows download failures and `34358cee` Linux GUI failure remain
  unexplained by that pass. Downloads remain at `0996a15`; actual DAW sessions
  and release qualification remain open.
- Nodal filter top-octave small-signal accuracy at 2x/4x internal; ADJ p11 HP point.
- Note-off click scores above 1 on some percussive circuit-engine programs.
- MTS-ESP now includes the [pinned 0BSD client](../src/vendor/mts-esp/README.md)
  with [channel-specific conventional MIDI polling](MTS_ESP_CHANNEL_MASTER.md).
  MPE retains global tuning; external-master DAW qualification remains open.
- Sixteen-voice UI ownership and activity telemetry now cover both instruments
  ([ownership](SIXTEEN_VOICE_UI_OWNERSHIP.md), [meter](VOICE_ACTIVITY_TELEMETRY.md)).
  Exact-source `c7595a4` native tests, validators and actual-editor captures
  passed. Local native-window execution remains display-blocked; direct wheel
  tests do not exercise physical focus/key routing or OS capture, and no DAW
  qualification is inferred.
- Memory-panel electrical model not wired into audio. The new certified sparse
  solver remains offline and exceeds the proposed audio-update budget despite
  the verified-log reduction and later interval-operation work; receiver
  propagation, scheduling/fallback and eight-versus-sixteen-voice loading remain
  unqualified. The 25 tone-tab cells remain hypotheses; tremolo/chorus
  both-off/both-on states need an engine parameter.
- Parameter-backed illuminated switches, Studio switches, parameter choice boxes,
  LFO destination chips and stored render-policy requests now have scoped
  context actions ([switches](SWITCH_CONTEXT_MENUS.md),
  [Studio switches](STUDIO_SWITCH_CONTEXT_MENUS.md),
  [choices](PARAMETER_COMBO_CONTEXT_MENUS.md),
  [LFO chips](STUDIO_LFO_CONTEXT_MENUS.md),
  [policy requests](RENDER_POLICY_CONTEXT_MENUS.md)). The composite LEGACY action
  has no single MIDI target; memory miniature faders remain outside this coverage.
  The voice monitor retains settings illustrations and also offers
  [read-only callback voice inspection](LIVE_VOICE_INSPECTION.md), not audio taps.
- Plugin undo/redo now brackets effective recorded parameter replays with host
  edit gestures and rejects unsafe replay during an active edit
  ([contract and local tests](UNDO_REPLAY_GESTURES.md)). VST3/AU-wrapper and
  real-DAW automation-recording qualification remain open.
- Sample-offset automation is limited to thirty key VST3 parameters in modern
  circuit policy. AU scheduled ramps, unsupported parameters and DAW-level
  qualification remain open ([HOST_AUTOMATION.md](HOST_AUTOMATION.md)).
- CC121 now resets supported controller targets through existing routes, sustain,
  addressed velocity prefixes and RPN/NRPN selection. Global-controller scope and
  PURE exceptions are explicit; full MIDI/MPE conformance, independent per-channel
  state and real-DAW/controller qualification remain open
  ([CONTROLLER_RESET.md](CONTROLLER_RESET.md)).
- Signing and notarization need the owner's certificates; real-DAW sessions,
  trademark review and AAX are owner actions
  ([release/RELEASE_QUALIFICATION.md](release/RELEASE_QUALIFICATION.md)).
