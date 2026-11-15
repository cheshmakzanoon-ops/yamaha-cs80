# Independent read-only mapping of reported source-audio failure

The CI-reported `cmp` byte 139501 is one-based. Under this Linux ABI (float/int/enum 4 bytes, double 8), the unchanged `source_factory_compatibility_probe.cpp` record is 3436 bytes:

- audio: 254 floats = 1016 bytes
- active-voice int and four telemetry floats = 20 bytes
- 16 voice/rank records, each 15 floats + two stage enums = 1088 bytes
- two cutoff readouts, each 32 doubles + two loaded-bus doubles = 544 bytes
- shared readout, 16 voice/ranks x six doubles = 768 bytes

139500 = 40*3436 + 2060. This is capture/block index40 (zero-based), first selection {1,13}, first sample rate44100Hz. There are97 captures per selection (96 blocks plus reset capture). Offset2060 = 1036 +15*68 +4: voice index7, rank index1, field a.hpf.ic1, byte0 of its float. Engine::getRankAudioState maps this source-policy field to `(float) hpStage.getState().bp`, i.e. the zBp integrator history, not instantaneous BP output and not an audio sample. Explicit field serialization means this is not struct padding.

The record's254audio floats and preceding metadata/state fields precede the reported first mismatch; if cmp reported its first difference, those bytes match at this point. That does not establish later PCM equality.

Block40 issues CC120(value0,channel2) before rendering127frames. allNotesOffMask(false) deactivates affected voices without clearing filter integrator history. Inactive source-policy circuits continue clocking through the idle path. The current helper repairs finite fifth-evaluation Newton failures for both idle and active filters. This supplies a credible mechanism for changed silent state immediately after panic; causal confirmation still requires paired reproduction and a fallback invocation/capped-residual trace. The reported position alone cannot prove which candidate change caused it, and the existing captured root corpus covered different rates/states.

Circuit V1 passing does not contradict this: source_factory_circuit_compatibility_probe exercises SourceFactoryTones converter/readout equations only; it does not run Engine or Ig00156Stage::process. SOURCE_AUDIO_V2 remains immutable00ceccfacb3cb9845436701da01c4387d53a361a in the workflow, and its gate explicitly compares whole-engine PCM AND numerical history. A justified convergence repair can intentionally break that contract, but no assertion should be relaxed or the failure called passing without a separately reviewed versioned policy decision and retained old-baseline evidence.

Recommended bounded diagnostic: decode all fields in both fresh binary outputs; report earliest PCM mismatch separately from earliest state mismatch; instrument only a copy of the candidate to record fallback attempts with render block, voice/rank, active/idle state, clock, fifth residual and accepted root. Require diagnostic and uninstrumented output bytes match. Revert only the helper call in an isolated control to establish causality rather than assuming it. Do not alter the frozen probe/assertions.

Read-only source inspection; no builds, source mutations or assertion changes performed by this reviewer.
