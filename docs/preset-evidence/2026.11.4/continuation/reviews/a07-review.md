# Agent 07: complete Keys recipe review

Source: `59d489d1a1f0065ae2a0f42a08c74edc43e94159`; reviewed `src/core/curated/Keys.inc`, its `Program` helper, body initialization and Engine routing. Read-only review; no tracked changes or listening claims.

## Outcome

All 24 existing Keys recipes have coherent intended roles at source level. I found no new definite octave, disabled-velocity-destination, permanently sustained struck-note, or body-size no-op defect requiring a separate recipe change. The known tine strike-normalization issue affects six tine-using Keys recipes, including the mixed Contour Keys and is being handled separately. This source review is not a realism or acoustic-match test.

## Per-program checklist

| Existing program | Source line | Verified construction / conclusion |
|---|---:|---|
| Electric Piano I |29|8-foot tine body and same-pitch high-passed short strike; zero sustain, prompt release; both ranks have initial-touch level. Normalization follow-up applies.|
| Electric Piano II |38|8-foot main plus quieter 4-foot strike; softer cutoff/longer losses and shallow chorus intentionally distinguish I. Normalization follow-up applies.|
| Tine Piano |52|8-foot tine with separate 2 2/3-foot sine = third harmonic, not an erroneous octave. Strike envelope shorter than body. Normalization follow-up applies.|
| Clavichord I |61|Tangent-string model, almost fully modeled lower rank, brief high-passed upper snap; short release and velocity destinations active.|
| Clavichord II |72|Darker cutoff, faster partial loss and stronger residual pulse/resonance distinguish I; zero sustain.|
| Funky Clavichord |83|Intentional string/circuit blend with resonant low-pass sweep and short attack, rather than an acoustic-copy recipe.|
| Harpsichord I |93|Both ranks entirely quill model; fixed body velocity response and near-zero initial level destination. Brief upper component, prompt damping.|
| Harpsichord II |104|8-foot plus 4-foot quill courses; lower rank sustains longer; low velocity dependence is intentional.|
| Lute Harpsichord |115|Covered quill model with lower brightness, fast upper losses, low cutoff, short release; low velocity dependence.|
| Toy Piano |125|4-foot celesta-like modes plus 2-foot bell strike; deliberately high registration agrees with description. No hidden low-rank fundamental.|
| Honky Keys |137|Two 8-foot piano bodies, explicit 5.2-cent detune, independent losses, slight dispersion; beating is intentional.|
| Vibes Keys |148|8-foot vibraphone modes plus 2-foot sine = fourth harmonic; shorter strike and 5 Hz SUB amplitude modulation.|
| Celesta |157|4-foot main and 2-foot strike preserve octave-high registration; modal body decays with zero held sustain.|
| Saw Pulse Piano |167|Piano/circuit blend deliberately retains electronic saw/pulse identity; lower body and upper strike decay separately.|
| Soft Keys |178|Low-brightness piano body, open-enough LP, nearly fully modeled upper strike, longer warm decay and velocity destinations.|
| Chorus Keys |189|Tine main plus high-passed short tine strike, same-pitch ranks, shallow chorus; normalization follow-up applies.|
| Stage Keys |199|Full piano bodies, lower main HP, wide LP, firmer hammer color, long lower body, zero sustain and prompt key damping.|
| Velocity Keys |211|Piano main with low base output and maximum initial-level destinations; upper rank has stronger initial-brilliance route.|
| Dulcimer |222|Steel-string model, 8-foot plus 4-foot course, small detune, long release. Uses authored plucked-steel excitation rather than an independent calibrated hammered-dulcimer model; not a proven acoustic match.|
| Reed Piano |233|Distinct reed model on both ranks, denser upper spectrum, high-passed short bark, velocity and prompt release.|
| Glass Keys |244|4-foot celesta and 2-foot bell with different mode losses, chorus, long tail but zero held sustain; intentional synthetic glass texture.|
| Tremolo Keys |255|Tine body plus short strike; both 4.8 Hz SUB VCA modulation and shallow OE2 tremolo are intentional layers; normalization follow-up applies.|
| Contour Keys |266|Tine main and reed/circuit upper strike; explicit low/high keyboard level and brilliance contour; normalization follow-up applies.|
| Pressure Keys |276|Piano onset with 24% residual main circuit path and 0.14 VCA sustain; main aftertouch brilliance/level remain active after struck modes fade. This is deliberately a hybrid sustained keyboard.|

## Important interpretation checks

- `Program::feet` stores the documented rank footage. The 2 2/3-foot layer is three times the 8-foot pitch and the 2-foot layer is four times; Tine Piano and Vibes Keys therefore match their third/fourth-harmonic descriptions.
- Body audio follows effective HP/LP in `Engine.cpp:3295`, then replaces the oscillator/VCF/sine input by `bodyMix` at `Engine.cpp:3360`. A full-body rank makes its oscillator waveform, sine level and VCF output-level literals inaudible at that mix. This is intentional routing, but those literals must not be counted as an audible factory refinement; cutoff/envelope/touch/rank output still matter.
- All 24 recipes use struck body models. None configures a sustained body's inert partial-decay control; all nondefault size controls here apply to harmonic models where body resonance uses size.
- All but Pressure Keys have zero VCA sustain in both ranks. Pressure Keys' nonzero main sustain and aftertouch are the described performance role, not accidental pad leakage.
- All acoustic-style keys preserve fast onsets. Soft Keys' ~3 ms attack is not a slow pad; long seconds values belong to decay.
- Harpsichord low velocity response is a deliberate exception to the piano/EP velocity requirement, implemented both in `InstrumentBody::noteOn` and recipe touch levels.

## Recommendation

Accept the focused tine normalization correction subject to full-preset velocity/headroom regressions, then qualify the current revoicing. Avoid indiscriminately changing already coherent recipe values just to increase a change count. For future acoustic realism work, Dulcimer's excitation and piano soundboard/sympathetic behavior need reference-guided models; this source review does not justify a speculative replacement.
