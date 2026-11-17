# Explicit source-aware Panel parameter binding (research only)

The immutable `ParameterBindingPolicy` remains `ExactNative=1` by default.
Only request v4 may explicitly select `SourceAwarePanel=2`. Earlier requests
retain exact full-native flush semantics. The standalone driver emits child v4;
private older fixture entry points remain child v1. Query/state/input-topology
policies are independent. This is neither a production change nor qualification.

Policy 2 requires the complete 2,378-row Connected inventory to match the
source-derived classifier: 192 workload parameters, 104 dormant memory controls,
one bypass, one program selector, and 2,080 MIDI pseudo-parameters. Missing,
unknown, duplicate, colliding or mismatched metadata is rejected. Full captured
metadata and current values remain owned and copied into the history evidence.
See `native_observer_parameter_classifier_STATUS.md` for source derivation.

Only a private HistoryOwner-to-ProcessingInstanceOwner operation establishes the
source-aware capability before prepare/start. It stores the exact serialized
initial 192-control and subsequent two-control protocols derived from the frozen
case. Public flush APIs cannot substitute an arbitrary subset, changed value,
wrapper/program/MIDI/memory write, or changed order for these protocols.

Before each A/B/C/D processing segment, after any restore/reactivation, the idle
UI thread reads all 2,186 observe-only residual currents in Connected roster order
and both Panel selector currents (channel I, then II). Residuals must equal their
source-validated baseline, and both selectors must be exactly 1.0 (choice 13,
Panel). There are no corrective writes or retry if any current is stale or
changed. These are controller observations, not proof of arbitrary internal DSP
state. Source readParameters() skips factory/memory substitution on Panel.

Policy 2 history schema is `cs80-instance-history-A-D-v2`. After the five joined
worker counters, it appends u32 policy 2, then four length-prefixed readback blobs.
Each blob contains text `cs80-observe-only-readback-v1`, u32 stage 0..3, u64 count
2188, and exactly that many (u32 ID, f64 current) records. Policy 1 keeps the
previous history v1 byte layout. The child v4 receipt independently carries the
binding policy after topology; request v4 carries it as u64 after topology.

Focused inert owner tests cover complete source-aware A-D and rejection of a
changed residual or Panel selector before every segment, including post-restore
C. They use source-derived fake metadata, not real native execution. Further
public supervisor tests independently check schema, preselection, exact writes,
role partition and full readback evidence. No success on these controls admits
an authentic plugin or establishes CPU benefit/runtime dependency closure.
