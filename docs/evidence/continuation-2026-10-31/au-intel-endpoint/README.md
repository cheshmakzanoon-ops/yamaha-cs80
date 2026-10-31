# Native Intel AU endpoint investigation

2026-10-31. Source `56b85819eea8d939d08faebb028f212e475825ed`
compiled on native macOS Intel, then
[run 41238033893, job 124645832711](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41238033893/job/124645832711)
failed 4 of 91 CTests. Cold-idle timing and smoothing failures predate their
already published repairs. The additional failures are the native AU host
fixture and its restored mutation control.

Both report **“AU publishes the current buffer endpoint, not a future ramp
endpoint.”** The same native host's maximum audio differences are
`5.82077e-11` for ramps, `2.55357e-7` for immediate events, and zero for
16-voice mode. The AU macro/platform fixtures, UI smoke and teardown pass.
The missing parameter IDs and numerical readbacks prevent the old log from
distinguishing incorrect publication from a numerical comparison mismatch.
Native arm64 success does not settle that question.

## Read-only source trace and diagnostic proposal

The AU queue clips a crossing ramp at the current render's last frame.
`HostAutomationCurve::commit()` submits that endpoint as Float32 through
`cs80CommitAuParameter()`. JUCE's `AudioParameterFloat::setValue()` stores a
physical Float32 using the parameter's `NormalisableRange`. Its
`getValue()`, and therefore AU `GetParameter`, normalizes the stored physical
float again. The fixture currently compares this readback with the independent
double curve, at the registered **1e-7** normalized limit.

This makes physical storage/inverse-normalization rounding a concrete
alternative to an endpoint-timing defect. For example, pulse width has a
physical range of 0.5 to 0.9; filter cutoffs use a nonlinear range. It is not yet
a proven explanation for the native failure. A stale or incorrectly published
endpoint and approximate-equality setter suppression remain alternatives.

The diagnostics-only change to `tests/AuAutomationHostTests.cpp` retains the
original assertion verbatim. It prints actual/expected values, absolute frame
and block/segment positions, per-lane counts and worst errors. The physical
values it prints are explicitly labelled **projections from normalized values**;
the host has no direct read of the processor's internal physical float. A
JUCE-style range round trip is included to test the numerical hypothesis.
No production source, event schedule, threshold, assertion or mutation changes.

The next native Intel run must supply numerical evidence before a corrective
contract or implementation is evaluated. No threshold relaxation is proposed.

## Evidence

- [Full original native job log](native-before-full-job.log)
- [Provenance and diagnostics plan](diagnostic-plan.json)
- [Original CI artifact 13057572684](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41238033893/artifacts/13057572684)

This record is an open software-protocol investigation, not hardware evidence
or a final Intel qualification.

## Native numerical diagnosis

The diagnostics-only source was subsequently built at
`ce8b127f7ee9466ca6f2627131ff6311bd031b47`. Native Intel
[run 41243861802, job 124663145158](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41243861802/job/124663145158)
passes 90 of 92 CTests; only the native AU host and its restored mutation
control fail. The original endpoint predicate is unchanged.

| Parameter | Failed endpoints | Worst normalized error |
| --- | ---: | ---: |
| Rank I pulse width (`ch1_pw`) | 60 | 1.4898250810801983e-7 |
| Rank II pulse width (`ch2_pw`) | 58 | 1.4709075912833214e-7 |

No other key fails. Pulse width uses an affine physical range, 0.5 to 0.9;
the failure therefore does not require nonlinear `exp`, `log` or `pow`.
Every one of the 64 printed original failures has an actual AU normalized
readback exactly equal to the independently calculated physical Float32
round trip. Every expected/actual physical projection is also identical.

For example, rank I pulse width at frame 272 should be
`0.62851632581695904`; its accepted Float32 is `0.62851631641387939`.
Both values project to physical Float32 `0.75140655040740967`.
Inversely normalizing that float gives `0.62851643562316895`, exactly the
value returned by the native AU. Its error, `1.0980620990252987e-7`, exceeds
the existing `1e-7` gate. Across the 64 records, rounding the accepted endpoint
to Float32 alone differs from the double oracle by at most
`2.977321855723858e-8`; the additional physical round trip contributes up to
`1.1920928955078125e-7` relative to that accepted Float32.

Together with the pinned JUCE setter/getter source, this proves loss during
normalized endpoint publication. The physical values remain explicitly
labelled projections; the host fixture cannot directly inspect internal
parameter storage. This result does not attribute the architecture difference
to a particular compiler instruction.

The same native run has maximum audio differences of `1.16415e-10` for
ramps, `2.90223e-7` for immediate events and zero for sixteen voices. The
audio schedule is already within the original `2e-6` limit.

- [Complete diagnostic job log](native-diagnostic-full-job.log)
- [Unmodified native host's endpoint output](native-diagnostic-endpoints.log)
- [Parsed records, provenance and exact-match checks](native-diagnosis.json)

The separate acceptance file
`tests/parity/tolerances-v2/au-normalized-publication.json` specifies exact
publication of transmitted Float32 knots, superseding edits, lifecycle
invalidation and protected policies. It must be committed before new witness
or repair evaluation. The original interpolated endpoint gate remains `1e-7`;
no oracle, audio tolerance, parameter range or circuit law is changed. The
native Intel qualification of the correction below remains pending.

## Publication correction and local qualification

Acceptance was published first in
[`aa3cadb4147ca9a1cd1cfe843345fcefa3d97432`](https://github.com/cheshmakzanoon-ops/yamaha-cs80/commit/aa3cadb4147ca9a1cd1cfe843345fcefa3d97432),
before repair evaluation. The original native interpolated-value predicate is
retained verbatim. Exact
transmitted Float32 knot checks supplement it, without substituting a looser
oracle.

**Publication chronology correction:** the intended concurrent-reader and
identical-state extension was announced as published in `9dd7762`, but that
commit contained the older acceptance JSON. The first local supplementary
concurrency evaluation used the intended working-tree extension before it
reached the remote repository. Those supplementary results are retrospective
coverage; they must not be described as prospectively registered. The original
zero-error publication/external/protected limits, `1e-7` interpolated limit and
`2e-6` audio limit were correctly published in `aa3cadb` before evaluation.

The failed [integrated CTest log](integrated-ctest-before-contract-correction.log)
reports `missing tolerance incoherent_read_count`, which exposed the mismatch.
The [original qualification record](publication-qualification-original.json)
retains the incorrect registration claim exactly as first recorded. No result
or failure was deleted. The actual extension and explicit correction were
published in
[`bf817e51256037ef63c91efa46dafee853efce8a`](https://github.com/cheshmakzanoon-ops/yamaha-cs80/commit/bf817e51256037ef63c91efa46dafee853efce8a)
before any repaired native Intel/arm64 evaluation. Independent retrieval
confirms that `9dd7762` has the old JSON SHA-256 `6bb67d0c…`, while `bf817e5`
has the intended `8307f727…` content. The
[correction notice](ACCEPTANCE_PUBLICATION_CORRECTION.md) and updated
qualification report preserve this distinction. A later local rerun does not
change the first evaluation's chronology.

The AU wrapper retains the accepted normalized endpoint together with the
parameter getter produced by its existing physical setter. These two floats
share one atomic 64-bit value. A revision captured before the setter identifies
publication ownership; external notifications advance that revision. A
notification arriving during the commit therefore prevents its older token
from reclaiming ownership, even if the physical getter is unchanged.
GetParameter uses a stable matching revision around one coherent pair load.
Changed underlying getters fall back to their current value.

Offset-zero native setters and UI notifications invalidate the affected key.
State restore explicitly invalidates all publications because APVTS may suppress
equal-physical-value notifications. Initialization, reset and cleanup do the
same. Pure and Legacy keep the original storage-based getter. Publication adds
only lock-free atomics; it creates no timer, thread, lock or allocation in the
plugin. The concurrent reader thread exists only in the test fixture.

The existing physical setter, parameter ranges, AU queue initial values,
preparation/scheduling bodies and VST3 generator body remain unchanged. The
queue and preparation/scheduling bodies are byte-identical to their original
sources; the qualification report records those source comparisons.

The exact production AU hook fixture first fails on original sources, at the
accepted-normalized-Float32 assertion. It passes after correction, including
unchanged physical readback after notification, invalidation during commit,
protected policies, lifecycle reset and concurrent pair reads. A deliberately
nonidentity getter projection makes the local witness independent of the
machine's floating-point contraction; the real Intel records above establish
the actual physical-range failure.

| Isolated mutation | Intended detection |
| --- | --- |
| Discard normalized publication | Exact accepted Float32 publication |
| Ignore changed getter | External ownership and concurrent pair association |
| Ignore external invalidation | Same-value notification, reset and intervening edit |
| Remove protected readback | Original Pure/Legacy getter |
| Remove lifecycle reset | Discard old publication ownership |
| Claim the newest revision at commit completion | Intervening edit cannot lose ownership |

All six mutations fail their intended assertions. Fixed and restored controls
pass. The unchanged AU curve gate reports maximum error `2.22045e-16`; the
macro-environment fixture passes; the wrapper platform/source contracts pass
92 checks. The [complete qualification report](publication-qualification.json)
contains compiler commands, source/executable hashes, every assertion log,
mutation patches and the corrected registration chronology. The
[qualification script as executed](qualify-publication-mutations.py) records
the isolated checkout and temporary include overlays used; no mutation switch
enters the plugin.

The native AU host now adds 252 exact knot/ownership checks across all 18 keys,
including identical-physical-state restore and reset. Native mutation CI also
discards normalized publication and suppresses lifecycle reset. The repaired
native Intel and arm64 components, original audio/interpolated gates, new
native mutations and restored controls must still pass before this issue can
be reported closed.
