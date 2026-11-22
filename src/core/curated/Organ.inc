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
        "An 8' sine body with a quieter 4' flute stop. Straight tuning, a fast soft gate and steady, velocity-independent sustain keep chord voicings clear.",
        organ());
    add("Flute Organ 16+8", "Organ", { "feet-layer", "sine" },
        "A full 16' flute body supported by an 8' sine stop. The lower rank speaks slightly more slowly; both remain steady while held.",
        organ().feet(I, f16).feet(II, f8).wave(II, false, false, 0.88f).mix(0.34f)
               .eg(I, 0.012f, 0.45f, 1.0f, 0.095f).eg(II, 0.006f, 0.30f, 1.0f, 0.065f));
    add("Percussive Organ", "Organ", { "feet-layer", "percussive" },
        "An 8' sustained sine body with true second-harmonic percussion at 4'. The separate percussion rank decays while the fundamental remains held; play separated notes to retrigger it.",
        organ().feet(II, f4).wave(II, false, false, 1.0f)
               .eg(I, 0.005f, 0.30f, 1.0f, 0.055f).eg(II, 0.0015f, 1.10f, 0.0f, 0.075f)
               .level(I, 0.80f, 0.88f).level(II, 0.80f, 0.82f).mix(0.30f));
    add("Reed Organ", "Organ", { "dual-rank" },
        "Two stable 8' pulse ranks combine a hollow square body with a smaller, brighter reed band. A short bellows-like onset and release soften the steady registration.",
        organ().wave(Both, false, true).feet(Both, f8).pw(I, 0.5f).pw(II, 0.67f)
               .hpf(I, 80.0f).hpf(II, 120.0f).lpf(I, 1800.0f, 0.07f).lpf(II, 2400.0f, 0.06f)
               .level(I, 0.90f, 0.88f).level(II, 0.65f, 0.78f).mix(0.22f)
               .eg(I, 0.025f, 0.40f, 1.0f, 0.14f).eg(II, 0.037f, 0.40f, 1.0f, 0.11f));
    add("Church Organ", "Organ", { "feet-layer" },
        "A rounded 16' saw principal with a restrained 4' sine stop. The two pipe-like onsets differ slightly; the dry release leaves room for the host's room or hall reverb.",
        organ().wave(I, true, false, 0.25f).feet(I, f16).wave(II, false, false, 0.85f)
               .hpf(I, 28.0f).lpf(I, 1800.0f, 0.08f).level(I, 0.74f, 0.85f)
               .eg(I, 0.045f, 0.50f, 1.0f, 0.30f).eg(II, 0.070f, 0.50f, 1.0f, 0.24f).mix(0.30f));
    add("Chapel Organ", "Organ", { "sine", "feet-layer" },
        "A soft 8' flute with a small 2' stop adding a clear upper edge. Gentle pipe speech and a compact release suit exposed, quiet chords.",
        organ().feet(II, f2).wave(II, false, false, 0.45f).mix(0.26f)
               .eg(I, 0.035f, 0.60f, 1.0f, 0.22f).eg(II, 0.047f, 0.45f, 1.0f, 0.17f)
               .level(I, 0.80f, 0.82f).level(II, 0.80f, 0.78f));
    add("Theatre Organ", "Organ", { "tremolo", "feet-layer" },
        "Warm 8' and 4' ranks combine flute body and restrained reed harmonics. Moderate tremulant movement and a slight rank offset animate sustained theatre-style chords.",
        organ().wave(I, true, false, 0.55f).wave(II, false, true, 0.45f).pw(II, 0.5f)
               .hpf(I, 55.0f).lpf(I, 2300.0f, 0.05f).lpf(II, 4200.0f, 0.04f)
               .level(I, 0.30f, 0.86f).level(II, 0.20f, 0.80f).mix(0.34f)
               .eg(I, 0.018f, 0.40f, 1.0f, 0.15f).eg(II, 0.023f, 0.40f, 1.0f, 0.12f)
               .tremolo(0.24f, 6.1f).detune(1.0f));
    add("Tremolo Organ", "Organ", { "tremolo", "feet-layer" },
        "A weighty 16' sine with a quieter 4' flute stop, moving under a relaxed tremolo. Stable pitches and restrained depth keep the bass line intelligible.",
        organ().feet(I, f16).wave(II, false, false, 0.72f).mix(0.25f)
               .eg(I, 0.009f, 0.40f, 1.0f, 0.095f).eg(II, 0.012f, 0.30f, 1.0f, 0.075f)
               .tremolo(0.28f, 3.2f));
    add("Mellow Organ", "Organ", { "sine" },
        "One 8' sine rank with a soft gate and no upper registration. Straight pitch, no chorus and a short release give a clean, rounded accompaniment.",
        organ().mix(0.0f).feet(I, f8).level(I, 0.0f, 0.78f).eg(I, 0.012f, 0.40f, 1.0f, 0.11f));
    add("Bright Organ", "Organ", { "feet-layer" },
        "An open 8' saw principal is supported by a small 2' sine stop. Brightness comes from harmonic registration, with a crisp gate and steady level across key velocities.",
        organ().wave(I, true, false, 0.18f).hpf(I, 55.0f).lpf(I, 5200.0f, 0.05f)
               .level(I, 0.82f, 0.84f).feet(II, f2).wave(II, false, false, 0.55f).mix(0.18f)
               .eg(I, 0.004f, 0.30f, 1.0f, 0.055f).eg(II, 0.007f, 0.30f, 1.0f, 0.045f));
    add("Pipe Organ", "Organ", { "feet-layer" },
        "A stopped 16' pulse foundation and a clearer 4' saw principal form a full pipe registration. Separate onset and release times preserve the two stops without ensemble detuning.",
        organ().wave(I, false, true, 0.10f).pw(I, 0.5f).feet(I, f16).wave(II, true, false).feet(II, f4)
               .hpf(I, 32.0f).lpf(I, 1900.0f, 0.06f).hpf(II, 110.0f).lpf(II, 6000.0f, 0.05f)
               .level(I, 0.85f, 0.86f).level(II, 0.65f, 0.76f).mix(0.32f)
               .eg(I, 0.055f, 0.50f, 1.0f, 0.28f).eg(II, 0.035f, 0.50f, 1.0f, 0.21f));
    add("Combo Organ", "Organ", { "chorus", "feet-layer" },
        "A nasal 8' pulse body and a quieter 4' pulse stop, with a restrained chorus. Fast keying keeps rhythmic comping defined while the two pulse widths distinguish the registration.",
        organ().wave(Both, false, true).pw(I, 0.78f).pw(II, 0.68f)
               .hpf(I, 85.0f).lpf(I, 2800.0f, 0.04f).hpf(II, 130.0f).lpf(II, 5000.0f, 0.04f)
               .level(I, 0.88f, 0.84f).level(II, 0.58f, 0.75f).mix(0.24f)
               .eg(I, 0.003f, 0.30f, 1.0f, 0.045f).eg(II, 0.005f, 0.30f, 1.0f, 0.035f)
               .chorus(0.20f, 0.8f).detune(1.4f));
    add("Fifth Organ", "Organ", { "feet-layer", "sine" },
        "An 8' sine and a quieter 5 1/3' quint stop in the harmonic 3:2 ratio. The upper stop stays subordinate, with straight tuning and sustained organ keying.",
        organ().feet(II, f513).wave(II, false, false, 0.64f).mix(0.32f)
               .eg(I, 0.008f, 0.35f, 1.0f, 0.075f).eg(II, 0.011f, 0.35f, 1.0f, 0.060f));
    add("Organ Vibrato", "Organ", { "sub-mod", "sine", "feet-layer" },
        "An 8'+4' flute registration with gentle 6.2 Hz SUB vibrato. The roughly eight-cent pitch motion adds movement without turning held chords into a broad wobble.",
        organ().wave(II, false, false, 0.84f).mix(0.37f)
               .eg(I, 0.010f, 0.35f, 1.0f, 0.080f).eg(II, 0.013f, 0.35f, 1.0f, 0.065f)
               .sub(subSine, 6.2f, 0.055f));
}
