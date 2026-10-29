# Original foot controls: evidence and implementation boundary

Audit base: 15ddb0b9cf600fe8fe236c886d674e22d0c8fe66 (233 host parameters). Read-only proposal, not a claim of circuit-equivalent production behavior.

**Implementation status (2026-10-29):** the assigner batch described under "Smallest assigner-only production batch", plus a provisional EXP/EXP-WAH stage and an output switch, is implemented with the append-only parameters and policies recorded in [PERFORMANCE_CONTROLS.md](PERFORMANCE_CONTROLS.md). The proposal text below is kept as the evidence record. One deliberate difference: that proposal used four booleans, while the build also added `foot_mode` and `output_level`. The EXP-WAH stage is a labelled stand-in for the PRA network, not the optical RC solution requested below.

## Original footswitch semantics
The owner's manual printed pp11–12,39,45 distinguishes the TS footswitch from the TRS optical expression pedal. One footswitch independently gates the sustain and portamento/glissando effects through their two assigners. An assigner OFF bypasses that gate. With no plug, the switched jack behaves as an active footswitch. A plugged but open dummy plug is therefore not equivalent to no plug.
Define J=plug present, P=physical contact pressed, A=corresponding assigner enabled:
  effectEnabled = !A || !J || P.
Use this separately for sustain and P/G. This is an original control-routing truth table, not a measured transistor transfer model.

Sustain is the panel-controlled release extension, not an indefinite MIDI hold. Pedal release cancels assigned sustain; exact resulting envelope transient still requires circuit/measurement validation. It must not silently destroy the rank's independently programmed release. P/G pedal release terminates the transition and selects the currently played target pitch. Glissando has additional release/sustain-mode behavior (owner summary p39), to be tested separately rather than inferred from a rounded continuous portamento path.

## Circuit corroboration
Panel3p66: SW1 T1=SUB TS1(21), SW2 T1=SUB TS2(26); their B1 contacts share −15 V and their M1 contacts share SUB TS4(24). This corroborates independent selectors sharing one pedal signal.
SUBp29: TS3(25) is the jack input; two 100k paths feed +15 V and Tr6 base. Tr6 collector is +15 V, emitter TS4 has 10k to −15 V. These values alone do not establish switching threshold or analog edge behavior. TS1 and TS2 act on distinct sustain/P-G networks.
EJ/EXP/PFSp67: J5 has a normalled contact and TS3 connection. Revised components must not be guessed from crossed-out drawings.

## Existing implementation is a separate MIDI extension
Engine::releaseKey explicitly implements CC64 held gates, including per-channel and MPE-master rules. CC11 multiplies final output expression gain. Neither is the above assigner circuit. Keep existing saved-state and MIDI behavior intact unless an explicit user-selected adapter activates original controls.

## Proposed bounded implementation
Append global original-control parameters only after the then-current parameter boundary; do not reuse historic IDs or shift AU version hints. Separate footswitch jack presence, physical contact state, sustain assigner, and P/G assigner. A GUI/contact automation source and an explicit optional MIDI-to-physical adapter may drive the physical state; receiving a MIDI CC cannot infer jack insertion.
Apply the original gate to panel sustain and P/G paths, not to legacy CC64 held-note bookkeeping. P/G off transitions must snap existing glides to their targets as well as suppress new glides. Sustain cancellation needs a named envelope operation that does not re-trigger or re-hold notes.
Expression mode should be an original two-choice EXP / EXP-WAH selector, with separately modeled pedal presence and travel. Implement a dedicated PRA audio module rather than pervoice cutoff offsets. Defer its production analog transfer until component identity, optical laws and calibration are verified (coarse harness placement is now verified below); an explicit-resistance reference is a safer first numerical step.

## Regression tests
Exhaust all 16 J/P/assigner combinations across the independent two assigners; confirm no-plug bypass, plugged-open/dummy-plug disable, shared contact, and simultaneous assignments. Cover gate edges during held notes, release tails, mode I/II, repeated notes/chords, key release, unplugging, and P/G target snaps. Preserve legacy CC64 per-channel/MPE and CC11 behavior under old state/defaults. Verify append-only host indices, state round trips, GUI gestures and screenshots.
Quantitative optical resistance-versus-travel, lamp dynamics, switch transients, and exact envelope cancellation time are separate unvalidated analog laws, not established by the discrete truth table.

## EXP/WAH: native PRA53 resolves the routing class
Actual native pixels show WI(23) entering an IC1/IC2 active RC network with photoresistor control and VR1 B25k / VR2 B100k trims. A separate dry branch and processed branch reach FET1 and FET2 respectively and join at WO(65). GW1(63)/GW2(62) control these branches. Thus this is a dedicated PRA audio filter/bypass network, not merely a cutoff-control wire to each M-board voice filter.

EI(52) enters IC3 through 2.2k followed by a 10k shunt and 100k input resistor; feedback is 100k with compensation and LED/optical loading. Other optical branches connect I2(64) to VL1(57) and I3(66) to VL2(54), with VR11/12 B25k trims and RC loading. A linear final-output CC11 multiplier does not reconstruct these networks. Panel3 labels EXP and EXP-WAH; its old WAH-only SW10 drawing is crossed out/revised. Do not add an invented third original mode from the deleted drawing.

**Placement update (2026-10-29):** fresh PRA53/54, Panel2 and OE55/56 scans plus the overall foldout establish rank MIX → PRA sum/ring → MODULATION VR7 → WI/WO wah → OE1/OE2 send-return/mixing → PRA I2/I3 optical expression → stereo VOLUME → output preamps. [Signal-routing evidence](OE1_SIGNAL_ROUTING_EVIDENCE.md) records the reproducible native crops, reciprocal harness pins and unresolved OE2 I(33)/I(53) connector-label discrepancy. The coarse placement gap is closed; OE2 internal dynamics and full analog transfer are not. Neither optical resistance versus pedal travel, LED current versus resistance, hysteresis/time constants, nor calibrated cutoff/Q/gain bounds have been established. The pedal's lamp/photosensor and the PRA optical devices are distinct stages, not one interchangeable potentiometer.

A first reference model may accept explicit positive optical resistances and specified trims, solve the verified linear RC network, and compare AC/transient responses against independent nodal equations. That is conditional circuit math, not physical pedal calibration. Required later measurements include pedal travel→EI, EI→optical resistances (including dynamics), WI→WO response and dry/processed insertion level, I2→VL1 and I3→VL2 matching, and no-plug behavior with the revised jack network. Validate detector/polarity and switch edges without guessing smoothing.

## Primary sources and reproducible pixel provenance
- Yamaha owner's manual, printed pp11–12,39,45: https://synthfool.com/docs/Yamaha/CS_Series/Yamaha%20CS-80%20Owners%20Manual.pdf (text mirror: https://device.report/m/08eee3fd014ab14d5f15fe50a150900b9c3ed22ad75895b5736f46b2e186f9b0).
- Original service PRA p53: https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/53%20-%20PRA%20Circuit%20Diagram.jpg.
- Original service Panel3 p66, SUB p29 and EJ/EXP/PFS p67, with revisions preserved, not erased.
- Native Actions run 40807476007: PRA53 job123229758333, Panel3 job123229757668, source c87b57c8b3b7b1ff79c8a4b4e168926a7a1f05de. PRA markers pra53-native-r0-c0/r0-c1 establish the above route.
- Native Actions run40803699249: SUB29 job123217519172, EJ67 job123217519201. Logs use CS80_CROP_DATA numbered base64 chunks; concatenate each marker's numerically ordered chunks without resizing.
- A second independent reader agreed on the PRA filter/bypass and separate optical level paths. No hardware measurement or new production implementation is claimed by this audit.

## Smallest assigner-only production batch (proposal, not implemented)
Four global booleans suffice for a first functional batch: footswitch_connected=0, footswitch_pressed=0, sustain_assign=0, glide_assign=0. Append IDs and AU version hints after the actual integration baseline. No compatibility-mode parameter is needed to preserve previous defaults: absent plug or assignment OFF leaves the old effect enabled. Keep legacy CC64/MPE bookkeeping unchanged. Provide an explicitly labeled virtual physical footswitch and connection control; defer any optional MIDI-to-physical adapter rather than silently changing CC64.

Cache both effective gates when applying parameters. Use the P/G gate in both startVoice initial glidePitch selection and active-voice sample processing; gate OFF must select targetPitch, including an already-running transition. Gate ON need not resurrect a discarded old transition. Test both continuous glide and quantized glissando.

In releaseVoice, multiply only the performanceRelease contribution by the sustain gate before max(rankRelease,keyRelease,performanceRelease). On enabled-to-disabled transitions, already-released active amp/filter envelopes should have their release duration shortened to the non-performance maximum using setRelease, preserving instantaneous value, stage and elapsed stage time. Existing Dsp.h::noteOff resets stageTime: do not use it for this change because it restarts the bent fast-release window. Do not reset envelopes or cancel MIDI pedalHeld notes. This is an explicit provisional cancellation policy within the existing envelope model, not a measured switch transient.

The existing sustained flag also causes mode-II new-note hard termination and ribbon following. Before implementation, decide and test whether assigned sustain OFF removes these mode-II tail privileges. Prefer a clearly named performance-tail state or a narrowly justified flag transition; do not accidentally change legacy defaults, plain rank release, held-key ribbon behavior or CC64 semantics. Gate re-enable must not resurrect a dead voice or reset a tail. Changes to slider/assigner/connection during a tail need deterministic behavior, not render-partition-dependent parameter polling.

Core tests should compare absent/off cases sample-for-sample against the prior render, including all old factory states; prove gate OFF does not alter attack/held stages; verify tail cancellation preserves envelope value and elapsed bent-release phase; exercise mode-II new notes, CC64/MPE overlaps and allNotesOff. Plugin tests cover four stable appended IDs/defaults/hints, missing-old-state fallback, explicit new state, preset loading/global-control policy, GUI contact automation and screenshots. Optical EXP/WAH is excluded from this first batch.
