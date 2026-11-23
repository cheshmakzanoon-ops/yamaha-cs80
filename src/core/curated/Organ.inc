// Organ voicings. Included inside namespace curated by CuratedLibrary.inc.
// Existing program names and insertion order are stable host/MIDI identities.
// Stops are sustained and normally independent of key velocity. The quieter
// upper rank supplies a registration, not a second detuned lead.
// Panel filtering now also shapes the harmonic bodies; each stop has its own
// passband, speaking time and keyboard contour. No velocity routes are added.

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
        organ().hpf(I, 20.0f).hpf(II, 42.0f).lpf(I, 3200.0f, 0.015f).lpf(II, 7400.0f, 0.015f)
               .body(I, body::Model::organ, 0.78f, 0.06f, 1.0f).body(II, body::Model::organ, 0.42f, 0.14f, 1.0f)
               .eg(I, 0.010f, 0.30f, 1.0f, 0.075f).eg(II, 0.016f, 0.30f, 1.0f, 0.052f).mix(0.24f)
               .contour(-0.06f, -0.14f, -0.03f, 0.035f));
    add("Flute Organ 16+8", "Organ", { "feet-layer", "sine" },
        "A full 16' flute-like harmonic stop supports a lighter 8' registration. The low rank speaks more slowly, and both hold a steady, velocity-independent level without detuning.",
        organ().feet(I, f16).feet(II, f8).wave(II, false, false, 0.88f).hpf(I, 20.0f).hpf(II, 30.0f)
               .lpf(I, 2500.0f, 0.012f).lpf(II, 6000.0f, 0.015f).body(I, body::Model::organ, 0.84f, 0.06f, 1.20f)
               .body(II, body::Model::organ, 0.57f, 0.16f, 1.0f).eg(I, 0.024f, 0.45f, 1.0f, 0.120f)
               .eg(II, 0.012f, 0.30f, 1.0f, 0.080f).mix(0.31f).contour(-0.08f, -0.12f, -0.07f, 0.035f));
    add("Percussive Organ", "Organ", { "feet-layer", "percussive" },
        "An 8' harmonic organ body sustains beneath separate second-harmonic percussion at 4'. The short upper strike fades while the registration remains steady; separated notes retrigger it with no velocity dependence.",
        organ().feet(II, f4).wave(II, false, false, 1.0f).hpf(I, 24.0f).hpf(II, 32.0f).lpf(I, 3800.0f, 0.012f)
               .lpf(II, 9000.0f, 0.010f).body(I, body::Model::organ, 0.14f, 0.035f, 1.0f)
               .eg(I, 0.004f, 0.30f, 1.0f, 0.048f).eg(II, 0.0010f, 0.82f, 0.0f, 0.052f).level(I, 0.80f, 0.84f)
               .level(II, 0.80f, 0.88f).mix(0.28f).contour(-0.035f, -0.13f, -0.025f, 0.03f));
    add("Reed Organ", "Organ", { "dual-rank" },
        "A steady harmonic registration and pulse edge form a reed-organ color. Distinct bellows-like onsets soften the two 8' ranks, while velocity-independent keying and restrained tuning keep held chords stable.",
        organ().wave(I, false, true).wave(II, false, true).feet(I, f8).feet(II, f8).level(I, 0.90f, 0.88f)
               .level(II, 0.65f, 0.78f).pw(I, 0.51f).pw(II, 0.645f).hpf(I, 56.0f).hpf(II, 92.0f).lpf(I, 3600.0f, 0.045f)
               .lpf(II, 5200.0f, 0.040f).body(I, body::Model::organ, 0.70f, 0.66f, 1.0f)
               .body(II, body::Model::organ, 0.36f, 0.82f, 1.0f).eg(I, 0.022f, 0.40f, 1.0f, 0.100f)
               .eg(II, 0.039f, 0.40f, 1.0f, 0.082f).mix(0.19f).contour(-0.045f, -0.20f, -0.035f, 0.03f));
    add("Church Organ", "Organ", { "feet-layer" },
        "A full 16' principal-like harmonic registration supports a restrained 4' flute stop. Differently timed pipe-like entrances preserve the ranks, with a dry release that leaves room for a host hall reverb.",
        organ().wave(I, true, false, 0.25f).feet(I, f16).wave(II, false, false, 0.85f).hpf(I, 20.0f).hpf(II, 90.0f)
               .lpf(I, 6000.0f, 0.035f).lpf(II, 10000.0f, 0.015f).body(I, body::Model::organ, 0.96f, 0.70f, 1.30f)
               .body(II, body::Model::organ, 0.74f, 0.29f, 1.10f).eg(I, 0.055f, 0.50f, 1.0f, 0.260f)
               .eg(II, 0.038f, 0.50f, 1.0f, 0.170f).level(I, 0.74f, 0.86f).level(II, 0.80f, 0.70f).mix(0.23f)
               .contour(-0.10f, -0.16f, -0.09f, 0.025f));
    add("Chapel Organ", "Organ", { "sine", "feet-layer" },
        "A soft 8' flute-like registration meets a small, later 2' stop. Restrained upper harmonics, gentle pipe speech and compact release suit exposed quiet chords without velocity-driven swells.",
        organ().feet(II, f2).wave(II, false, false, 0.45f).level(I, 0.80f, 0.82f).hpf(I, 22.0f).hpf(II, 80.0f)
               .lpf(I, 3100.0f, 0.015f).lpf(II, 9000.0f, 0.010f).body(I, body::Model::organ, 0.87f, 0.10f, 1.15f)
               .body(II, body::Model::organ, 0.44f, 0.18f, 1.0f).eg(I, 0.045f, 0.60f, 1.0f, 0.180f)
               .eg(II, 0.061f, 0.45f, 1.0f, 0.110f).level(II, 0.80f, 0.66f).mix(0.18f)
               .contour(-0.055f, -0.19f, -0.04f, 0.025f));
    add("Theatre Organ", "Organ", { "tremolo", "sub-mod", "feet-layer" },
        "Warm 8' and 4' harmonic registrations combine flute body and a restrained reed edge. A gentle 5.6 Hz amplitude tremulant moves the whole registration, with shallow stereo color and steady rank tuning.",
        organ().wave(I, true, false, 0.55f).wave(II, false, true, 0.45f).pw(II, 0.5f).level(I, 0.30f, 0.86f)
               .level(II, 0.20f, 0.80f).hpf(I, 32.0f).hpf(II, 85.0f).lpf(I, 4800.0f, 0.035f).lpf(II, 6500.0f, 0.025f)
               .body(I, body::Model::organ, 0.79f, 0.55f, 1.0f).body(II, body::Model::organ, 0.53f, 0.64f, 1.0f)
               .eg(I, 0.025f, 0.40f, 1.0f, 0.115f).eg(II, 0.018f, 0.40f, 1.0f, 0.092f).mix(0.29f).tremolo(0.17f, 5.6f)
               .sub(subSine, 5.6f, 0.0f, 0.0f, 0.14f).detune(0.0f).g(P_drift, 0.012f)
               .contour(-0.05f, -0.16f, -0.03f, 0.035f));
    add("Tremolo Organ", "Organ", { "tremolo", "sub-mod", "feet-layer" },
        "A weighty 16' harmonic flute foundation supports a lighter 4' stop. Gentle 3.2 Hz SUB amplitude motion keeps the low tremolo audible, with restrained stereo color and stable stop tuning.",
        organ().feet(I, f16).wave(II, false, false, 0.72f).sub(subSine, 3.2f, 0.0f, 0.0f, 0.20f).hpf(I, 20.0f)
               .hpf(II, 70.0f).lpf(I, 3100.0f, 0.015f).lpf(II, 7800.0f, 0.015f)
               .body(I, body::Model::organ, 0.78f, 0.10f, 1.20f).body(II, body::Model::organ, 0.50f, 0.25f, 1.0f)
               .eg(I, 0.018f, 0.40f, 1.0f, 0.110f).eg(II, 0.009f, 0.30f, 1.0f, 0.060f).mix(0.22f).tremolo(0.18f, 3.2f)
               .g(P_chorusDepth, 0.18f).contour(-0.08f, -0.14f, -0.06f, 0.035f));
    add("Mellow Organ", "Organ", { "sine" },
        "One rounded 8' registration blends a pure center with a very dark harmonic body. Soft keying, straight pitch and a compact release provide clean accompaniment with no upper stop or velocity-dependent attack.",
        organ().mix(0.0f).feet(I, f8).hpf(I, 22.0f).lpf(I, 1400.0f, 0.010f)
               .body(I, body::Model::organ, 0.81f, 0.02f, 1.20f).eg(I, 0.013f, 0.40f, 1.0f, 0.105f).level(I, 0.0f, 0.84f)
               .g(P_drift, 0.006f).g(P_voiceDispersion, 0.02f).contour(-0.04f, -0.18f, -0.015f, 0.04f));
    add("Bright Organ", "Organ", { "feet-layer" },
        "An open 8' principal-like harmonic body carries a small 2' flute stop. Brightness comes from the registration rather than key velocity; crisp gates keep repeated comping clear.",
        organ().wave(I, true, false, 0.18f).feet(II, f2).wave(II, false, false, 0.55f).hpf(I, 38.0f).hpf(II, 110.0f)
               .lpf(I, 7600.0f, 0.030f).lpf(II, 11000.0f, 0.015f).body(I, body::Model::organ, 0.90f, 0.82f, 0.85f)
               .body(II, body::Model::organ, 0.45f, 0.54f, 1.0f).eg(I, 0.0045f, 0.30f, 1.0f, 0.050f)
               .eg(II, 0.006f, 0.30f, 1.0f, 0.032f).level(I, 0.82f, 0.82f).level(II, 0.80f, 0.72f).mix(0.16f)
               .contour(-0.035f, -0.23f, -0.02f, 0.045f));
    add("Pipe Organ", "Organ", { "feet-layer" },
        "A rounded 16' harmonic foundation meets a brighter 4' principal-like stop. Separate pipe-like onset and release times define the registration without chorus, ensemble detuning or velocity-dependent loudness.",
        organ().wave(I, false, true, 0.10f).pw(I, 0.5f).feet(I, f16).wave(II, true, false).feet(II, f4)
               .level(I, 0.85f, 0.86f).level(II, 0.65f, 0.76f).hpf(I, 20.0f).hpf(II, 62.0f).lpf(I, 5200.0f, 0.030f)
               .lpf(II, 9800.0f, 0.020f).body(I, body::Model::organ, 0.96f, 0.44f, 1.30f)
               .body(II, body::Model::organ, 0.84f, 0.78f, 1.0f).eg(I, 0.042f, 0.50f, 1.0f, 0.220f)
               .eg(II, 0.022f, 0.50f, 1.0f, 0.130f).mix(0.26f).contour(-0.10f, -0.18f, -0.075f, 0.035f));
    add("Combo Organ", "Organ", { "chorus", "feet-layer" },
        "A nasal 8' pulse registration and lighter 4' stop preserve the electronic combo-organ character. More open harmonics, a shallower chorus and straighter tuning sharpen rhythmic comping while the keying remains velocity independent.",
        organ().wave(I, false, true).wave(II, false, true).level(I, 0.88f, 0.84f).level(II, 0.58f, 0.75f).pw(I, 0.72f)
               .pw(II, 0.61f).hpf(I, 55.0f).hpf(II, 95.0f).lpf(I, 4200.0f, 0.025f).lpf(II, 7200.0f, 0.020f)
               .eg(I, 0.0025f, 0.30f, 1.0f, 0.032f).eg(II, 0.004f, 0.30f, 1.0f, 0.025f).chorus(0.095f, 0.67f)
               .detune(0.18f).mix(0.21f).contour(-0.035f, -0.15f, -0.02f, 0.035f));
    add("Fifth Organ", "Organ", { "feet-layer", "sine" },
        "An 8' harmonic flute registration supports a quieter 5 1/3' quint in the 3:2 ratio. The upper stop stays clearly subordinate to the root, with straight tuning and steady velocity-independent sustain.",
        organ().feet(II, f513).wave(II, false, false, 0.64f).hpf(I, 20.0f).hpf(II, 45.0f).lpf(I, 3400.0f, 0.015f)
               .lpf(II, 7100.0f, 0.015f).body(I, body::Model::organ, 0.75f, 0.12f, 1.0f)
               .body(II, body::Model::organ, 0.36f, 0.20f, 1.0f).eg(I, 0.009f, 0.35f, 1.0f, 0.065f)
               .eg(II, 0.014f, 0.35f, 1.0f, 0.048f).mix(0.23f).contour(-0.05f, -0.15f, -0.03f, 0.03f));
    add("Organ Vibrato", "Organ", { "sub-mod", "sine", "feet-layer" },
        "An 8'+4' harmonic flute registration carries restrained 5.9 Hz SUB vibrato. The steady stops retain their clear center and velocity-independent keying while the shallow pitch motion animates held chords.",
        organ().wave(II, false, false, 0.84f).hpf(I, 22.0f).hpf(II, 60.0f).lpf(I, 3600.0f, 0.015f)
               .lpf(II, 7600.0f, 0.015f).body(I, body::Model::organ, 0.74f, 0.15f, 1.0f)
               .body(II, body::Model::organ, 0.40f, 0.24f, 1.0f).eg(I, 0.008f, 0.35f, 1.0f, 0.070f)
               .eg(II, 0.012f, 0.35f, 1.0f, 0.048f).sub(subSine, 5.9f, 0.035f).mix(0.27f)
               .contour(-0.05f, -0.14f, -0.025f, 0.035f));

}
