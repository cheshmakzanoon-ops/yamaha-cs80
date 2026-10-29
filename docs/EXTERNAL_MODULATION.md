# Optional EXT modulation input

## Usage and scope

The single mono **External Modulation** input bus is disabled by default; the existing default output remains stereo. Hosts may enable mono input and route a signal to it. Mono/stereo outputs are accepted; stereo/multichannel inputs and other output configurations are rejected rather than silently downmixed. No device permissions or microphone/input capture are requested by this implementation. Host wrappers/DAWs must expose and enable routing.

Select original SUB mode, FUNCTION=EXT, then raise EXT LEVEL (defaultzero). The shared signal reaches the existing VCO/VCF/VCA routes and per-note AT VCO/VCF depths. Base/AT SPEED do not affect it. Input is never mixed directly into output. It is ignored by legacy mode and other functions. UI bus status reflects prepared/processed host layout, **not** proof of a connected cable, signal or physical CS80 input.

## Explicit digital conventions

A nominal peak1 digital sine at full LEVEL produces nominal peak1 **inverted** shared modulation after reconstruction. LEVEL0..1 is a linear software gain, not a measured rear-pot taper. Physical volts-per-digital-full-scale are undefined. No input high-pass is added.

Finite host input is clamped to±1 before interpolation; NaN/infinity becomezero. These are numerical safety policies, not a model of analog clipping or protection diodes. FIR ringing is retained instead of adding a second undocumented clipper. For the current coefficients, the two polyphase absolute coefficient sums imply peaks below2.7 for any bounded input; a conservative3peak numerical regression accompanies this limit. Such transients are not evidence of amplifier headroom.

## Causal2x reconstruction

A dedicated fixed-size255-tap Kaiser FIR uses the existing decimator coefficient design, zero insertion and gain2. It reconstructs host-rate input at the synthesis engine's2x sample rate without the large high-frequency droop/images of linear interpolation. History and coefficients are allocated in fixed arrays; coefficient preparation occurs outside the render loop.

This introduces **127 internal samples =63.5 host samples of external-control delay**. It is a software anti-imaging choice, not a physical circuit latency. It changes the phase relationship between external modulation and the notes/carrier. The instrument's existing reported **audio output latency remains63 host samples**; no claim is made that host output compensation aligns the delayed modulation with external audio.

Tests measure actual output (not coefficient self-comparison) with coherent tones near20Hz,1kHz,20kHz at44.1/48/96kHz: acceptance±0.002dB wanted gain and image below−80dB. These are intended numeric gates until exact-source Actions passes. At lower host rates the normalized filter necessarily has proportionately narrower bandwidth. An impulse independently checks center/centroid127 internal samples.

## Real-time and state behavior

The plugin copies each bounded input chunk into a preallocated mono scratch buffer **before writing outputs**, since JUCE's mono input aliases output channelzero. Input offsets match MIDI segment and oversized-host-block offsets. No render-time vector resize, lock, file operation or filter preparation is added. Fixed scratch capacity is allocated in prepareToPlay.

Engine render accepts an optional separate mono source pointer; callers must provide non-overlapping input/output storage (the plugin's scratch capture enforces this). Null means disconnected: transition clears interpolation history and immediately mutes EXT. A connected zero-valued stream drains the finite FIR tail; reconnecting after disconnect begins with cleared history. prepare/reset also clears history. Zero-frame calls leave DSP history unchanged.

The host owns bus layout; plugin XML does not duplicate it. Existing233 parameters/IDs/order/AU hints/ranges/defaults remain unchanged from original-SUB batch1. EXT LEVEL was already appended there. Existing old229 parameter contracts remain intact.

## Hardware evidence and unresolved differences

SUB29/EJ67 show an inverting100k/10k input amplifier, rear LEVEL electrical wiper fraction a, then noninverting22k/2.2k stage: ideal small-signal gain−110*a. This justifies the sign, **not** our digital normalization. No coupling capacitor is drawn in that chain. Rear-pot resistance/taper, bandwidth, nonlinear swing/slew and physical calibration remain unresolved. Owner50k input impedance conflicts with the shown/printed10k input resistor.

See [original SUB evidence](ORIGINAL_SUB_OSCILLATOR.md) for primary sources and Actions image provenance. This is a functional digital external-modulation route, not a complete nonlinear NJM4558/cable/source-impedance reconstruction.

## Verification gates

Core: actual time-domain passband/image/impulse measurements, finite/extreme input, no directaudio, negative normalization, LEVEL, disconnect/drain/reconnect, partition invariance and existing audio latency.

Plugin/UI: default/accepted/rejected buses; real aliased getBusBuffer input;10003-frame oversized vs partitioned blocks with MIDI on chunk boundaries; mono/stereo outputs; no directaudio; legacy/non-EXT/zeroLEVEL isolation; speed invariance; nonfinite/clampedinput equivalence; enabled/disabled editor status. Batch1 phase/mode/held-gesture regressions remain included.

Static inspection confirms no newly introduced render allocations. No allocator-instrumentation harness was available, so an allocation-free runtime assertion is **not** claimed. Local runtime was blocked by cloud filesystem ENOSPC; exact-source Actions compilation, sanitizers, pluginval and screenshots remain mandatory.
