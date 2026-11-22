# Additional preset verification — 2026.11.1

The [integration index](integration/summary.json) and
[archive selection](integration/archive-selection.json) preserve completed work
from assignments 26 and 29–47. The archived local native checks identify source
`be460b0b93bdcb8dce5eb8925b18b72706b25e19`. The publication candidate has since
moved to `6d3cc220edef5a4a2c7cd968ea8a786c7c83198c`, build run `45044040694`,
after a one-file workflow fingerprint fixture correction. The former run
`45042166237` was cancelled; the correction changes no runtime or preset source.
The publisher records describe installation at `dd78677` and its historical
contract for the superseded run. These local records do **not** certify publisher
retargeting, the new final CI run, release creation, real uploads or public
downloads. The earlier draft summary retains its historically accurate
`DRAFT_READY_NOT_INSTALLED` status.

| Record | Completed observation | Boundary |
| --- | --- | --- |
| [Linux validator diagnostics](validator-diagnostics/summary.json), [independent review](validator-diagnostics/independent-review/summary.json) | Original post-SUCCESS SIGSEGV and a later successful diagnostic rerun are preserved; wrapper tests keep required validation gates | Historical source/run; different rerun seeds. No crash or DSP fix is claimed |
| [Native recall](native-recall/summary.json), [raw exits](native-recall/run-results.json) | 6,607 checks across four native JUCE fixtures; all four direct children exit 0 | The initial Xvfb wrapper failure also occurs with `/bin/true`; its logs remain archived. No named DAW is certified |
| [Sample rates](sample-rates/summary.json), [measurements](sample-rates/measurements.json) | 183 checks, 54 captures/56.7 seconds at 44.1, 48 and 96 kHz preserve six refined layer roles | Bounded final-bank renders, not every program or host |
| [Local final auditions](final-auditions-local/summary.json), [artifact provenance](final-auditions-local/preset-audition/provenance.json) | 24 WAVs/117 seconds and 32 manifest entries verified against actual samples and final factory parameters | Locally generated at source `99f5ebd`, run `local-assignment-32`; not a final CI download. WAVs and the duplicate inventory are omitted here |
| [Performed controls](performance-controls/summary.json), [event history](performance-controls/events.csv) | 60 checks, ten captures/64 seconds cover pressure, wah, vibrato and velocity | Compact results retained; per-sample telemetry and raw audio omitted |
| [Modern roles](modern-roles/summary.json) | 46 static checks across 24 modern-tagged programs | Studio exposes controls; effect routing and Pure masking are distinct DSP behavior |
| [Long sustain](long-sustain/summary.json), [windows](long-sustain/windows.csv) | 96 checks, twelve eight-note captures/216 seconds with ten-second holds and full pressure | All voices retire within the observation horizon; retirement does not imply digital silence |
| [Rapid strikes](rapid-strikes/summary.json), [matched boundaries](rapid-strikes/boundaries.csv) | 96 checks, eight captures/96 seconds, repeated attacks and matched omitted-onset controls | Released-slot reuse; not more than eight simultaneously held physical voices |
| [Phrase gates](phrase-gates/summary.json), [initial short observation](phrase-gates/initial-4s/run.log) | 100 final checks, twelve captures/86 seconds exercise CC64 ring phrase continuity, fresh retrigger, glide and glissando | Initial four-second ring windows failed six retirement checks. Eight-second windows pass without factory/event/gate changes; the initial source and data remain archived |
| [Arpeggiators](arpeggiators/summary.json), [transitions](arpeggiators/transitions.csv) | 64 checks, four captures/36 seconds establish rates, gates, note selection and source-key/CC64 behavior | Bounded four-program engine fixtures |
| [External VST3 boundary](external-vst3/summary.json), [core comparisons](external-vst3/parameters.csv), [physical saved-state values](external-vst3/saved-state-physical.csv) | Actual local Linux VST3: 45,115 assertions, all 523 names/order, and 4,560 normalized comparisons across load/restore of twelve revised programs | Controller-plus-queue selection passes. Separate queue-only attempts retain exit 1/1,336 failures; the following completed diagnosis explains the bounded controller dependency without claiming a fix. Physical values come from saved-state XML; raw native `normalizedParamToPlain` output remains normalized |
| [Program queue diagnosis](program-queue/summary.json), [two-module observations](program-queue/revised/observations.csv), [prior-workspace observations](program-queue/prior_workspace/observations.csv), [source comparison](program-queue/source-comparison.json) | 26 checked observation groups across two Linux modules, five modes each and 48 state snapshots establish pre-existing controller-dependent Program selection | Queue-only loading still fails with handler, audio block/thread and wait controls. Explicit controller synchronization works. Diagnostic exit 0 is not a queue-selection pass; the older module’s exact compile commit is unknown; no runtime fix is claimed |
| [Packaging parser](packaging/revision-parser/summary.json), [install guide](packaging/install-guide/summary.json), [pinned links](packaging/pinned-links/summary.json), [independent packaging review](packaging/independent-review/summary.json) | Patch revision parsing, four synthetic platform shapes, installation wording and 47 actual guide links reviewed; 108 independent assertions, seven notice tests and 46 package fixtures in final review | Local fixtures and source links do not certify native operating-system packages |
| [Publisher draft](publisher-review/draft-authoring/summary.json), [independent review](publisher-review/summary.json), [installed-layout fixtures](publisher-review/installed-layout-fixtures.log) | Eleven offline test methods include nine independent mutation-order/resumption checks; installed implementation identity is recorded | Fake API transactions and synthetic binary headers; real permissions, uploads and public asset GET/hash checks remain operational gates |
| [Claim review](claims-review/summary.json) | 17 checks review source claims and the then-existing 81-file dossier | The earlier review does not implicitly cover this later extension; extension integrity is checked separately |

The original queue-only VST3 failures remain unchanged alongside the passing
controller/state path. Completed assignment 47 confirms that a process-only
Program queue does not load Flute in either tested Linux module, while the
ordinary queued volume control applies. Handler installation, 128-frame
processing, a separate audio thread and a 250 ms wait do not change that result.
Controller synchronization loads the program and the positive control proves
message notifications are serviced. This establishes the bounded pre-existing
controller dependency; it does not certify arbitrary DAW Program automation or
provide a runtime fix. The older binary has pre-refinement Flute values, but its
exact compile commit is not established. See the [full diagnostic method](program-queue/method.txt).

None of these software measurements establishes acoustic instrument equivalence,
original-hardware calibration or a listening-panel verdict.

### Reproducing the additional checks

Restore the desired directory from `reproduce/` to the repository root, preserving
its original `build-wNN-*` name. The corresponding `method.txt` contains the
original author's conditions and invocation; the exact scripts are unchanged.
Build the recorded core/capture executable and generate the final inventory at
`build-audit/inventory-final.json` as described in the [original core reproduction instructions](README.md). The indexes retain the
original input hashes; a new build records its own hashes and provenance.

For example, restore the actual VST3 host fixture and point it at a locally built
module whose source and architecture are recorded separately:

```sh
cp -R docs/preset-evidence/2026.11.1/reproduce/build-w09-vst3-boundary .
python3 build-w09-vst3-boundary/reproduce.py --mode controller \
  --module '/absolute/path/to/CS-80 Reference.vst3/Contents/x86_64-linux/CS-80 Reference.so'
python3 build-w09-vst3-boundary/reproduce.py --mode queue-only \
  --module '/absolute/path/to/CS-80 Reference.vst3/Contents/x86_64-linux/CS-80 Reference.so'
```

This fixture includes the repository's `tests/Vst3AutomationTests.cpp` and needs
its pinned JUCE SDK headers and the final core library. Its queue-only command
reproduces a separately failing observation; it is not a required passing gate.
The [sample-rate method](sample-rates/method.txt),
[performed-control method](performance-controls/method.txt),
[long-sustain method](long-sustain/method.txt),
[rapid-strike method](rapid-strikes/method.txt),
[phrase-gate method](phrase-gates/method.txt) and
[arpeggiator method](arpeggiators/method.txt) describe their own fixtures.

The local audition reproducer explicitly requires checkout `99f5ebd` and a new
working directory; see its [method](final-auditions-local/method.txt) and
[exact source](reproduce/build-w06-final-auditions/reproduce.py). It must not be
relabelled as final CI evidence. For the independent publisher review, restore
`reproduce/build-w08-publisher-review` at the repository root and copy the
recorded installed `scripts/publish-preset-refinement.py` to the historical
`build-w07-durable-release/publish-preset-refinement.py` import path. The
[selection record](integration/archive-selection.json) pins that repository
source and the installed fixture suite to commit `dd78677` and their hashes.
Use the installed offline tests for the current layout; no `--publish` invocation
is part of this evidence reproduction.

For the completed Program queue diagnosis, restore
`reproduce/build-w10-program-queue` to the repository root. Its
[compile command](program-queue/compile-command.json),
[revised-module invocation](program-queue/revised/run-result.json) and
[prior-workspace invocation](program-queue/prior_workspace/run-result.json)
record the exact arguments and source/module hashes. The
[omitted-state index](program-queue/omitted-state-hashes.json) retains all 48
state-blob hashes without embedding binary payloads. The older module’s compile
commit must not be inferred from its checkout’s current HEAD.

Embedded `SHA256SUMS` files retain original experiment payload identities,
including omitted audio and state blobs. They are not inventories of this
compact directory. [Provenance](provenance.json) is the complete archived-file
manifest; [new paths](integration/new-paths.txt) lists this extension's files,
including reproduction sources affected by repository `build-*` ignore rules.
