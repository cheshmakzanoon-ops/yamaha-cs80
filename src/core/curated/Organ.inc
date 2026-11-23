// Organ voicings. Included inside namespace curated by CuratedLibrary.inc.
// Existing program names and insertion order are stable host/MIDI identities.
// Stops are sustained and normally independent of key velocity. The quieter
// upper rank supplies a registration, not a second detuned lead.

Program organ()
{
    Program x;
    x.wave(I, false, false, 1.0f).wave(II, false, false, 0.78f)
     .feet(I, f8).feet(II, f4).hpf(Both, 30.0f).lpf(Both, 9000.0f, 0.02f)
     .feg(Both, 0.0f, 0.0f, 0.01f, 0.3f, 0.06f)
     .eg(I, 0.006f, 0.3f, 1.0f, 0.055f).eg(II, 0.008f, 0.3f, 1.0f, 0.045f)
     .level(Both, 0.80f, 0.88f).touch(Both, 0.0f, 0.0f, 0.0f, 0.0f)
     .mix(0.28f).detune(0.0f).res(0.02f)
     .g(P_voiceDispersion, 0.04f).g(P_drift, 0.015f).g(P_analogNoise, 0.0f).volume(0.6f);
    return x;
}

void buildOrgan(Builder& add)
{
    add("Flute Organ 8+4", "Organ", { "feet-layer", "sine" },
        "An 8' flute-like harmonic registration is supported by a lighter 4' stop. Soft fast gates, straight tuning and velocity-independent sustain keep chord voicings clear, with more body than two pure sines.",
        organ()
               .body(I, body::Model::organ, 0.72f, 0.12f, 1.00f)
               .body(II, body::Model::organ, 0.48f, 0.18f, 1.00f));
    add("Flute Organ 16+8", "Organ", { "feet-layer", "sine" },
        "A full 16' flute-like harmonic stop supports a lighter 8' registration. The low rank speaks more slowly, and both hold a steady, velocity-independent level without detuning.",
        organ().feet(I, f16).feet(II, f8).wave(II, false, false, 0.88f).mix(0.34f)
               .eg(I, 0.012f, 0.45f, 1.0f, 0.095f).eg(II, 0.006f, 0.30f, 1.0f, 0.065f)
               .body(I, body::Model::organ, 0.78f, 0.09f, 1.20f)
               .body(II, body::Model::organ, 0.51f, 0.17f, 1.00f));
    add("Percussive Organ", "Organ", { "feet-layer", "percussive" },
        "An 8' harmonic organ body sustains beneath separate second-harmonic percussion at 4'. The short upper strike fades while the registration remains steady; separated notes retrigger it with no velocity dependence.",
        organ().feet(II, f4).wave(II, false, false, 1.0f)
               .eg(I, 0.005f, 0.30f, 1.0f, 0.055f).eg(II, 0.0015f, 1.10f, 0.0f, 0.075f)
               .level(I, 0.80f, 0.88f).level(II, 0.80f, 0.82f).mix(0.30f)
               .body(I, body::Model::organ, 0.18f, 0.08f, 1.00f).eg(II, 0.0012f, 0.94f, 0.0f, 0.060f));
    add("Reed Organ", "Organ", { "dual-rank" },
        "A steady harmonic registration and pulse edge form a reed-organ color. Distinct bellows-like onsets soften the two 8' ranks, while velocity-independent keying and restrained tuning keep held chords stable.",
        organ().wave(Both, false, true).feet(Both, f8).pw(I, 0.5f).pw(II, 0.67f)
               .hpf(I, 80.0f).hpf(II, 120.0f).lpf(I, 1800.0f, 0.07f).lpf(II, 2400.0f, 0.06f)
               .level(I, 0.90f, 0.88f).level(II, 0.65f, 0.78f).mix(0.22f)
               .eg(I, 0.025f, 0.40f, 1.0f, 0.14f).eg(II, 0.037f, 0.40f, 1.0f, 0.11f)
               .body(I, body::Model::organ, 0.66f, 0.59f, 1.00f)
               .body(II, body::Model::organ, 0.31f, 0.77f, 1.00f).eg(I, 0.019f, 0.40f, 1.0f, 0.12f));
    add("Church Organ", "Organ", { "feet-layer" },
        "A full 16' principal-like harmonic registration supports a restrained 4' flute stop. Differently timed pipe-like entrances preserve the ranks, with a dry release that leaves room for a host hall reverb.",
        organ().wave(I, true, false, 0.25f).feet(I, f16).wave(II, false, false, 0.85f)
               .hpf(I, 28.0f).lpf(I, 1800.0f, 0.08f).level(I, 0.74f, 0.85f)
               .eg(I, 0.045f, 0.50f, 1.0f, 0.30f).eg(II, 0.070f, 0.50f, 1.0f, 0.24f).mix(0.30f)
               .body(I, body::Model::organ, 0.93f, 0.64f, 1.30f)
               .body(II, body::Model::organ, 0.68f, 0.35f, 1.10f));
    add("Chapel Organ", "Organ", { "sine", "feet-layer" },
        "A soft 8' flute-like registration meets a small, later 2' stop. Restrained upper harmonics, gentle pipe speech and compact release suit exposed quiet chords without velocity-driven swells.",
        organ().feet(II, f2).wave(II, false, false, 0.45f).mix(0.26f)
               .eg(I, 0.035f, 0.60f, 1.0f, 0.22f).eg(II, 0.047f, 0.45f, 1.0f, 0.17f)
               .level(I, 0.80f, 0.82f).level(II, 0.80f, 0.78f)
               .body(I, body::Model::organ, 0.85f, 0.17f, 1.15f)
               .body(II, body::Model::organ, 0.50f, 0.25f, 1.00f).eg(II, 0.056f, 0.45f, 1.0f, 0.15f));
    add("Theatre Organ", "Organ", { "tremolo", "sub-mod", "feet-layer" },
        "Warm 8' and 4' harmonic registrations combine flute body and a restrained reed edge. A gentle 5.6 Hz amplitude tremulant moves the whole registration, with shallow stereo color and steady rank tuning.",
        organ().wave(I, true, false, 0.55f).wave(II, false, true, 0.45f).pw(II, 0.5f)
               .hpf(I, 55.0f).lpf(I, 2300.0f, 0.05f).lpf(II, 4200.0f, 0.04f)
               .level(I, 0.30f, 0.86f).level(II, 0.20f, 0.80f).mix(0.34f)
               .eg(I, 0.018f, 0.40f, 1.0f, 0.15f).eg(II, 0.023f, 0.40f, 1.0f, 0.12f)
               .tremolo(0.24f, 6.1f).detune(1.0f)
               .body(I, body::Model::organ, 0.71f, 0.45f, 1.00f)
               .body(II, body::Model::organ, 0.45f, 0.57f, 1.00f).tremolo(0.19f, 5.6f)
               .sub(subSine, 5.6f, 0.0f, 0.0f, 0.15f).detune(0.4f));
    add("Tremolo Organ", "Organ", { "tremolo", "sub-mod", "feet-layer" },
        "A weighty 16' harmonic flute foundation supports a lighter 4' stop. Gentle 3.2 Hz SUB amplitude motion keeps the low tremolo audible, with restrained stereo color and stable stop tuning.",
        organ().feet(I, f16).wave(II, false, false, 0.72f).mix(0.25f)
               .eg(I, 0.009f, 0.40f, 1.0f, 0.095f).eg(II, 0.012f, 0.30f, 1.0f, 0.075f)
               .tremolo(0.28f, 3.2f).sub(subSine, 3.2f, 0.0f, 0.0f, 0.20f)
               .body(I, body::Model::organ, 0.70f, 0.13f, 1.20f)
               .body(II, body::Model::organ, 0.43f, 0.24f, 1.00f).g(P_chorusDepth, 0.21f));
    add("Mellow Organ", "Organ", { "sine" },
        "One rounded 8' registration blends a pure center with a very dark harmonic body. Soft keying, straight pitch and a compact release provide clean accompaniment with no upper stop or velocity-dependent attack.",
        organ().mix(0.0f).feet(I, f8).level(I, 0.0f, 0.78f).eg(I, 0.012f, 0.40f, 1.0f, 0.11f)
               .body(I, body::Model::organ, 0.73f, 0.04f, 1.20f).eg(I, 0.009f, 0.40f, 1.0f, 0.09f));
    add("Bright Organ", "Organ", { "feet-layer" },
        "An open 8' principal-like harmonic body carries a small 2' flute stop. Brightness comes from the registration rather than key velocity; crisp gates keep repeated comping clear.",
        organ().wave(I, true, false, 0.18f).hpf(I, 55.0f).lpf(I, 5200.0f, 0.05f)
               .level(I, 0.82f, 0.84f).feet(II, f2).wave(II, false, false, 0.55f).mix(0.18f)
               .eg(I, 0.004f, 0.30f, 1.0f, 0.055f).eg(II, 0.007f, 0.30f, 1.0f, 0.045f)
               .body(I, body::Model::organ, 0.86f, 0.89f, 0.85f)
               .body(II, body::Model::organ, 0.38f, 0.47f, 1.00f).eg(I, 0.003f, 0.30f, 1.0f, 0.045f));
    add("Pipe Organ", "Organ", { "feet-layer" },
        "A rounded 16' harmonic foundation meets a brighter 4' principal-like stop. Separate pipe-like onset and release times define the registration without chorus, ensemble detuning or velocity-dependent loudness.",
        organ().wave(I, false, true, 0.10f).pw(I, 0.5f).feet(I, f16).wave(II, true, false).feet(II, f4)
               .hpf(I, 32.0f).lpf(I, 1900.0f, 0.06f).hpf(II, 110.0f).lpf(II, 6000.0f, 0.05f)
               .level(I, 0.85f, 0.86f).level(II, 0.65f, 0.76f).mix(0.32f)
               .eg(I, 0.055f, 0.50f, 1.0f, 0.28f).eg(II, 0.035f, 0.50f, 1.0f, 0.21f)
               .body(I, body::Model::organ, 0.91f, 0.36f, 1.30f)
               .body(II, body::Model::organ, 0.76f, 0.82f, 1.00f).eg(II, 0.029f, 0.50f, 1.0f, 0.18f));
    add("Combo Organ", "Organ", { "chorus", "feet-layer" },
        "A nasal 8' pulse registration and lighter 4' stop preserve the electronic combo-organ character. More open harmonics, a shallower chorus and straighter tuning sharpen rhythmic comping while the keying remains velocity independent.",
        organ().wave(Both, false, true).pw(I, 0.78f).pw(II, 0.68f)
               .hpf(I, 85.0f).lpf(I, 2800.0f, 0.04f).hpf(II, 130.0f).lpf(II, 5000.0f, 0.04f)
               .level(I, 0.88f, 0.84f).level(II, 0.58f, 0.75f).mix(0.24f)
               .eg(I, 0.003f, 0.30f, 1.0f, 0.045f).eg(II, 0.005f, 0.30f, 1.0f, 0.035f)
               .chorus(0.20f, 0.8f).detune(1.4f)
               .pw(I, 0.74f).pw(II, 0.64f).lpf(I, 3400.0f, 0.035f).lpf(II, 5600.0f, 0.025f).chorus(0.13f, 0.67f).detune(0.45f));
    add("Fifth Organ", "Organ", { "feet-layer", "sine" },
        "An 8' harmonic flute registration supports a quieter 5 1/3' quint in the 3:2 ratio. The upper stop stays clearly subordinate to the root, with straight tuning and steady velocity-independent sustain.",
        organ().feet(II, f513).wave(II, false, false, 0.64f).mix(0.32f)
               .eg(I, 0.008f, 0.35f, 1.0f, 0.075f).eg(II, 0.011f, 0.35f, 1.0f, 0.060f)
               .body(I, body::Model::organ, 0.68f, 0.15f, 1.00f)
               .body(II, body::Model::organ, 0.39f, 0.23f, 1.00f).mix(0.27f));
    add("Organ Vibrato", "Organ", { "sub-mod", "sine", "feet-layer" },
        "An 8'+4' harmonic flute registration carries restrained 6 Hz SUB vibrato. The steady stops retain their clear center and velocity-independent keying while the shallow pitch motion animates held chords.",
        organ().wave(II, false, false, 0.84f).mix(0.37f)
               .eg(I, 0.010f, 0.35f, 1.0f, 0.080f).eg(II, 0.013f, 0.35f, 1.0f, 0.065f)
               .sub(subSine, 6.2f, 0.055f)
               .body(I, body::Model::organ, 0.68f, 0.18f, 1.00f)
               .body(II, body::Model::organ, 0.44f, 0.30f, 1.00f).sub(subSine, 6.0f, 0.047f).mix(0.32f));
}
