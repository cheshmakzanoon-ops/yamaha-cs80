// Polysynth voicings. Included inside namespace curated by CuratedLibrary.inc.
// Existing program names and insertion order are stable host/MIDI identities.
// A chord body and shorter upper accent have separate articulation. Initial
// touch carries real dynamics because the Circuit VCA adds its level routes.

Program poly()
{
    Program x;
    x.wave(I, true, false, 0.035f).wave(II, true, false)
     .hpf(I, 32.0f).hpf(II, 85.0f).lpf(I, 1600.0f, 0.14f).lpf(II, 2550.0f, 0.18f)
     .feg(I, 0.03f, 0.34f, 0.012f, 1.0f, 0.22f).feg(II, 0.0f, 0.43f, 0.006f, 0.56f, 0.17f)
     .eg(I, 0.007f, 1.5f, 0.70f, 0.35f).eg(II, 0.004f, 0.95f, 0.48f, 0.27f)
     .touch(I, 0.17f, 0.62f, 0.16f, 0.18f).touch(II, 0.24f, 0.72f, 0.22f, 0.15f)
     .level(I, 1.0f, 0.43f).level(II, 0.95f, 0.32f)
     .chorus(0.22f, 0.38f).detune(4.5f).mix(0.40f).res(0.12f)
     .g(P_voiceDispersion, 0.16f).g(P_drift, 0.065f).g(P_analogNoise, 0.008f).volume(0.6f);
    return x;
}

void buildPolysynth(Builder& add)
{
    add("Poly Stab", "Polysynth", { "velocity", "percussive", "dual-rank" },
        "A dry chord hit with a rounded saw body and shorter bright accent. Both ranks decay away under a held key; velocity controls the bite and level.",
        poly()
              .touch(I, 0.18f, 0.70f, 0.0f, 0.0f).touch(II, 0.28f, 0.85f, 0.0f, 0.0f)
              .g(P_chorusOn, 0.0f).mix(0.35f)
              .lpf(I, 1380.0f, 0.12f).lpf(II, 3150.0f, 0.16f).feg(I, 0.0f, 0.46f, 0.005f, 1.10f, 0.19f)
              .feg(II, 0.0f, 0.59f, 0.002f, 0.26f, 0.08f).eg(I, 0.004f, 1.85f, 0.0f, 0.28f).eg(II, 0.002f, 0.42f, 0.0f, 0.11f)
              .detune(2.1f));
    add("Poly Brass Stab", "Polysynth", { "velocity", "brilliance-swell", "percussive" },
        "A brief brass-like opening crest relaxes into a short chord body. The ranks speak at slightly different times and decay fully, giving a compact accented stab.",
        poly().lpf(I, 780.0f, 0.16f).lpf(II, 1450.0f, 0.21f)
              .touch(I, 0.23f, 0.70f, 0.0f, 0.0f)
              .mix(0.39f).detune(3.3f).g(P_chorusOn, 0.0f)
              .feg(I, 0.29f, 0.57f, 0.036f, 1.4f, 0.22f).feg(II, 0.38f, 0.66f, 0.057f, 0.78f, 0.14f)
              .eg(I, 0.014f, 2.1f, 0.0f, 0.30f).eg(II, 0.024f, 0.98f, 0.0f, 0.18f).wave(I, true, false, 0.07f).hpf(II, 135.0f)
              .touch(II, 0.34f, 0.88f, 0.0f, 0.0f));
    add("PWM Poly", "Polysynth", { "pwm", "velocity", "dual-rank" },
        "Pulse ranks with separate width rates form a round sustained body and brighter moving edge. The initial accent settles into an animated held chord.",
        poly().wave(Both, false, true)
              .lpf(I, 1900.0f, 0.10f).lpf(II, 3300.0f, 0.13f)
              .eg(I, 0.012f, 1.6f, 0.77f, 0.42f).eg(II, 0.018f, 1.1f, 0.61f, 0.34f)
              .mix(0.43f).detune(2.8f).chorus(0.18f, 0.35f)
              .pw(I, 0.510f, 0.22f, 0.37f).pw(II, 0.64f, 0.32f, 0.59f).hpf(II, 175.0f).feg(I, 0.0f, 0.31f, 0.010f, 1.55f, 0.30f)
              .feg(II, 0.0f, 0.42f, 0.005f, 0.54f, 0.19f).touch(II, 0.21f, 0.80f, 0.22f, 0.19f).pan(I, -0.09f).pan(II, 0.15f));
    add("Resonant Poly", "Polysynth", { "resonance", "velocity", "dual-rank" },
        "A sharp resonant upper accent sits over a warmer chord body. Unequal filter decay times distinguish the opening bite from the softer held sound.",
        poly().lpf(I, 1150.0f, 0.20f).lpf(II, 830.0f, 0.52f).res(0.34f)
              .eg(I, 0.007f, 1.4f, 0.67f, 0.35f)
              .mix(0.27f).detune(2.2f).g(P_chorusOn, 0.0f)
              .wave(II, false, true).pw(II, 0.63f).hpf(II, 150.0f).feg(I, 0.0f, 0.31f, 0.008f, 1.35f, 0.25f)
              .feg(II, 0.0f, 0.73f, 0.002f, 0.34f, 0.12f).eg(II, 0.002f, 0.52f, 0.24f, 0.18f)
              .touch(II, 0.24f, 0.78f, 0.18f, 0.12f));
    add("Detuned Poly", "Polysynth", { "detune", "dual-rank" },
        "Full saw ranks beat against each other by 17 cents. The second enters slightly later with a lighter sustain; no chorus masks the direct detuned character.",
        poly().wave(Both, true, false).lpf(I, 2400.0f, 0.10f).lpf(II, 3000.0f, 0.12f)
              .eg(I, 0.008f, 1.6f, 0.76f, 0.45f)
              .mix(0.47f).detune(17.0f).g(P_chorusOn, 0.0f).g(P_drift, 0.045f)
              .hpf(I, 32.0f).hpf(II, 140.0f).feg(I, 0.02f, 0.29f, 0.011f, 1.55f, 0.30f)
              .feg(II, 0.05f, 0.38f, 0.023f, 0.72f, 0.22f).eg(II, 0.027f, 1.05f, 0.61f, 0.42f)
              .touch(I, 0.14f, 0.68f, 0.12f, 0.20f).touch(II, 0.21f, 0.73f, 0.18f, 0.17f).contour(-0.08f, -0.10f, -0.04f, 0.04f));
    add("Poly Sweep", "Polysynth", { "brilliance-swell", "dual-rank" },
        "A quickly established chord opens through staggered filter crests over roughly half a second. Pressure gently brightens the sustain after the opening sweep settles.",
        poly().lpf(I, 1000.0f, 0.18f).lpf(II, 1700.0f, 0.24f)
              .mix(0.39f).detune(4.2f).chorus(0.26f, 0.32f)
              .feg(I, 0.38f, 0.47f, 0.36f, 2.9f, 0.55f).feg(II, 0.52f, 0.62f, 0.63f, 2.1f, 0.45f).wave(II, false, true)
              .pw(II, 0.60f, 0.07f, 0.23f).eg(I, 0.024f, 2.2f, 0.83f, 0.72f).eg(II, 0.049f, 1.65f, 0.61f, 0.58f)
              .touch(II, 0.14f, 0.65f, 0.31f, 0.22f));
    add("Velocity Poly", "Polysynth", { "velocity", "dual-rank" },
        "Soft strikes stay dark and quiet; hard strikes add substantial level and a bright upper accent. Low static VCA levels make velocity the main dynamic control.",
        poly().hpf(I, 25.0f).hpf(II, 45.0f).lpf(I, 850.0f, 0.12f).lpf(II, 1400.0f, 0.18f)
              .level(I, 1.0f, 0.16f).level(II, 1.0f, 0.10f)
              .mix(0.37f).detune(3.5f).g(P_chorusOn, 0.0f)
              .wave(II, false, true).pw(II, 0.65f).feg(I, 0.02f, 0.22f, 0.008f, 1.2f, 0.27f)
              .feg(II, 0.0f, 0.41f, 0.003f, 0.38f, 0.13f).eg(I, 0.006f, 1.9f, 0.78f, 0.43f).eg(II, 0.003f, 0.63f, 0.31f, 0.21f)
              .touch(I, 0.39f, 0.94f, 0.0f, 0.0f).touch(II, 0.58f, 1.0f, 0.0f, 0.0f).contour(-0.08f, -0.08f, -0.03f, 0.05f));
    add("Poly Pluck", "Polysynth", { "percussive", "velocity", "dual-rank" },
        "A short bright pulse attack releases into a longer resonant saw body. Both ranks have zero sustain, so held chords decay naturally instead of becoming a pad.",
        poly().wave(II, false, true).lpf(I, 1150.0f, 0.19f).lpf(II, 3200.0f, 0.18f)
              .touch(I, 0.22f, 0.75f, 0.0f, 0.0f).touch(II, 0.30f, 0.92f, 0.0f, 0.0f)
              .mix(0.22f).detune(1.7f).g(P_chorusOn, 0.0f)
              .wave(I, true, false, 0.10f).pw(II, 0.72f).feg(I, 0.0f, 0.64f, 0.002f, 1.65f, 0.28f)
              .feg(II, 0.0f, 0.55f, 0.001f, 0.13f, 0.055f).eg(I, 0.0018f, 3.7f, 0.0f, 0.48f).eg(II, 0.001f, 0.23f, 0.0f, 0.065f)
              .hpf(II, 320.0f).contour(-0.10f, -0.12f, -0.02f, 0.06f));
    add("Octave Poly", "Polysynth", { "feet-layer", "dual-rank", "velocity" },
        "A full 8' saw chord carries a lighter 4' crown. The upper filter is opened for its register, and its shorter accent settles without overwhelming the sustained root.",
        poly().feet(II, f4).wave(II, false, true)
              .lpf(I, 1750.0f, 0.12f).hpf(II, 220.0f).lpf(II, 4800.0f, 0.13f)
              .eg(I, 0.009f, 1.6f, 0.77f, 0.43f)
              .mix(0.28f).detune(1.8f).chorus(0.17f, 0.34f)
              .pw(II, 0.57f, 0.045f, 0.31f).feg(I, 0.02f, 0.32f, 0.010f, 1.45f, 0.30f).feg(II, 0.0f, 0.36f, 0.003f, 0.42f, 0.15f)
              .eg(II, 0.004f, 0.63f, 0.29f, 0.23f).touch(I, 0.17f, 0.67f, 0.16f, 0.20f).touch(II, 0.21f, 0.82f, 0.18f, 0.15f)
              .contour(-0.08f, -0.14f, -0.03f, 0.03f));
    add("Fifth Poly", "Polysynth", { "feet-layer", "dual-rank" },
        "The 8' body and quieter 5 1/3' harmonic fifth form an open chord color. Stable tuning and a lighter sustained upper rank keep the root clear during held voicings.",
        poly().feet(II, f513).wave(II, false, true)
              .lpf(I, 1650.0f, 0.12f).lpf(II, 3500.0f, 0.14f)
              .eg(I, 0.011f, 1.6f, 0.76f, 0.44f)
              .mix(0.28f).detune(0.7f).chorus(0.14f, 0.32f)
              .pw(II, 0.50f, 0.018f, 0.27f).hpf(II, 225.0f).feg(I, 0.03f, 0.29f, 0.014f, 1.5f, 0.30f)
              .feg(II, 0.0f, 0.34f, 0.008f, 0.66f, 0.21f).eg(II, 0.022f, 1.3f, 0.43f, 0.42f)
              .touch(II, 0.16f, 0.58f, 0.16f, 0.19f).g(P_drift, 0.022f));
    add("Bright Poly", "Polysynth", { "velocity", "dual-rank" },
        "An open saw body and narrow-pulse edge deliver bright, immediate chords. A brief upper accent adds definition while restrained resonance keeps the held sound clear.",
        poly().wave(II, false, true).lpf(I, 4100.0f, 0.07f).lpf(II, 6500.0f, 0.09f)
              .eg(I, 0.005f, 1.4f, 0.73f, 0.31f)
              .mix(0.32f).res(0.07f).detune(3.2f)
              .hpf(I, 45.0f).hpf(II, 210.0f).pw(II, 0.72f).feg(I, 0.0f, 0.20f, 0.004f, 1.1f, 0.22f)
              .feg(II, 0.0f, 0.30f, 0.002f, 0.28f, 0.10f).eg(II, 0.002f, 0.44f, 0.27f, 0.17f)
              .touch(II, 0.20f, 0.79f, 0.12f, 0.16f).contour(-0.04f, -0.20f, 0.0f, 0.06f));
    add("Dark Poly", "Polysynth", { "sine", "dual-rank" },
        "A filtered saw with a small sine foundation holds the chord beneath a quiet dark pulse. The upper rank closes sooner, leaving a rounded, gently extended release.",
        poly().wave(II, false, true)
              .hpf(I, 25.0f).hpf(II, 55.0f).lpf(I, 680.0f, 0.10f).lpf(II, 1080.0f, 0.12f)
              .touch(I, 0.12f, 0.60f, 0.10f, 0.20f)
              .mix(0.30f).detune(2.7f).chorus(0.18f, 0.28f)
              .wave(I, true, false, 0.21f).pw(II, 0.53f, 0.035f, 0.21f).feg(I, 0.06f, 0.16f, 0.018f, 2.0f, 0.58f)
              .feg(II, 0.12f, 0.25f, 0.038f, 0.82f, 0.43f).eg(I, 0.019f, 2.1f, 0.88f, 1.55f).eg(II, 0.035f, 1.0f, 0.48f, 0.93f)
              .touch(II, 0.21f, 0.70f, 0.22f, 0.20f));
    add("Hollow Poly", "Polysynth", { "pwm", "dual-rank" },
        "Near-square pulses give a hollow, woody chord center. A quieter, slightly narrower upper rank adds a reedy edge; shallow width movement avoids a thin constant sweep.",
        poly().wave(Both, false, true)
              .lpf(I, 1550.0f, 0.13f)
              .eg(I, 0.009f, 1.5f, 0.77f, 0.36f)
              .mix(0.28f).detune(1.4f).g(P_chorusOn, 0.0f)
              .pw(I, 0.50f, 0.012f, 0.28f).pw(II, 0.55f, 0.028f, 0.41f).hpf(II, 360.0f, 0.11f).lpf(II, 2550.0f, 0.27f)
              .feg(I, 0.02f, 0.23f, 0.008f, 1.3f, 0.25f).feg(II, 0.04f, 0.37f, 0.006f, 0.48f, 0.14f)
              .eg(II, 0.019f, 0.78f, 0.42f, 0.24f));
    add("Wide Poly", "Polysynth", { "chorus", "dual-rank" },
        "A solid saw and brighter PWM rank spread across the stereo field. Different accent lengths and a broad chorus give width while the sustained chord remains balanced.",
        poly().wave(II, false, true)
              .lpf(I, 2400.0f, 0.10f).lpf(II, 3500.0f, 0.12f)
              .eg(I, 0.012f, 1.6f, 0.76f, 0.57f).eg(II, 0.021f, 1.2f, 0.65f, 0.47f)
              .pan(I, -0.35f).pan(II, 0.35f).mix(0.46f).chorus(0.52f, 0.39f).detune(7.5f)
              .pw(II, 0.61f, 0.19f, 0.33f).hpf(II, 170.0f).feg(I, 0.03f, 0.30f, 0.012f, 1.5f, 0.34f)
              .feg(II, 0.05f, 0.38f, 0.022f, 0.77f, 0.28f).touch(I, 0.14f, 0.67f, 0.15f, 0.20f)
              .touch(II, 0.21f, 0.73f, 0.22f, 0.18f).contour(-0.08f, -0.11f, -0.03f, 0.04f));
    add("Poly Swell", "Polysynth", { "aftertouch", "dual-rank" },
        "A soft initial chord grows under pressure. Low static levels leave room for a substantial dynamic swell, and the brighter second rank responds more strongly than the body.",
        poly().lpf(I, 1250.0f, 0.12f).lpf(II, 2100.0f, 0.16f).hpf(I, 28.0f)
              .level(I, 1.0f, 0.22f).level(II, 0.96f, 0.14f)
              .touch(I, 0.04f, 0.14f, 0.30f, 0.76f).touch(II, 0.06f, 0.18f, 0.44f, 0.86f)
              .mix(0.40f).detune(4.0f).chorus(0.28f, 0.31f)
              .wave(II, false, true).pw(II, 0.59f, 0.08f, 0.25f).feg(I, 0.12f, 0.17f, 0.11f, 2.2f, 0.45f)
              .feg(II, 0.23f, 0.27f, 0.21f, 1.65f, 0.37f).eg(I, 0.061f, 2.1f, 0.95f, 0.80f).eg(II, 0.105f, 1.8f, 0.84f, 0.66f)
              .hpf(II, 120.0f).contour(-0.07f, -0.09f, -0.02f, 0.06f));
    add("Saw Pulse Poly", "Polysynth", { "dual-rank", "velocity" },
        "A sustained saw body pairs with a narrower pulse accent. Their distinct spectral and envelope roles remain audible through the short, lightly chorused release.",
        poly().wave(I, true, false, 0.025f).wave(II, false, true)
              .lpf(I, 1800.0f, 0.11f).lpf(II, 3300.0f, 0.19f)
              .eg(I, 0.009f, 1.6f, 0.78f, 0.40f)
              .mix(0.37f).detune(2.7f).chorus(0.16f, 0.36f)
              .pw(II, 0.69f, 0.035f, 0.29f).hpf(II, 240.0f).feg(I, 0.02f, 0.30f, 0.009f, 1.45f, 0.29f)
              .feg(II, 0.0f, 0.49f, 0.003f, 0.32f, 0.13f).eg(II, 0.003f, 0.56f, 0.29f, 0.21f)
              .touch(II, 0.28f, 0.83f, 0.24f, 0.17f).pan(I, -0.07f).pan(II, 0.14f));
    add("Synthwave Chords", "Polysynth", { "chorus", "detune", "dual-rank" },
        "Detuned saw chords spread across stereo with a bright opening accent and chorus. The second rank sustains more softly and releases sooner, preserving a warm center in the longer retro tail.",
        poly().wave(Both, true, false).lpf(I, 2500.0f, 0.13f).lpf(II, 3800.0f, 0.15f)
              .mix(0.45f).detune(11.5f).chorus(0.48f, 0.37f)
              .pan(I, -0.24f).pan(II, 0.32f)
              .feg(I, 0.02f, 0.32f, 0.012f, 1.95f, 0.75f).feg(II, 0.0f, 0.42f, 0.006f, 0.78f, 0.46f)
              .eg(I, 0.010f, 2.3f, 0.83f, 2.15f).eg(II, 0.018f, 1.5f, 0.68f, 1.52f).touch(I, 0.16f, 0.67f, 0.14f, 0.22f)
              .touch(II, 0.24f, 0.75f, 0.22f, 0.19f).contour(-0.08f, -0.15f, -0.03f, 0.04f));
    add("Square Poly", "Polysynth", { "dual-rank", "velocity" },
        "Unmodulated 50-percent pulses give a clear square-wave chord. The upper rank has a shorter accent and brighter filter; light detune adds body without a PWM sweep.",
        poly().wave(Both, false, true).pw(Both, 0.5f, 0.0f)
              .lpf(I, 2300.0f, 0.10f).lpf(II, 3400.0f, 0.12f)
              .mix(0.40f).detune(2.0f).g(P_chorusOn, 0.0f)
              .hpf(II, 165.0f).feg(I, 0.0f, 0.26f, 0.009f, 1.55f, 0.28f).feg(II, 0.0f, 0.36f, 0.004f, 0.52f, 0.16f)
              .eg(I, 0.006f, 1.9f, 0.84f, 0.44f).eg(II, 0.017f, 0.80f, 0.45f, 0.25f).touch(II, 0.23f, 0.75f, 0.19f, 0.17f)
              .contour(-0.08f, -0.12f, -0.02f, 0.04f));
    add("Sine Poly", "Polysynth", { "sine", "dual-rank", "velocity" },
        "A dominant pure sine body is joined by a small filtered saw attack. The harmonic rank decays to a quiet sustain, leaving rounded chords with meaningful velocity response.",
        poly().wave(I, false, false, 0.82f).wave(II, true, false).lpf(II, 1050.0f, 0.10f)
              .feg(I, 0.0f, 0.0f, 0.01f, 0.5f, 0.3f)
              .level(I, 0.0f, 0.28f).level(II, 0.55f, 0.22f)
              .mix(0.20f).detune(0.0f).g(P_chorusOn, 0.0f).g(P_analogNoise, 0.0f).g(P_drift, 0.03f)
              .eg(I, 0.009f, 2.3f, 0.93f, 0.75f).eg(II, 0.003f, 0.63f, 0.11f, 0.21f).hpf(II, 190.0f)
              .feg(II, 0.0f, 0.43f, 0.002f, 0.34f, 0.14f).touch(I, 0.0f, 0.77f, 0.0f, 0.25f)
              .touch(II, 0.24f, 0.86f, 0.12f, 0.12f).contour(0.0f, -0.10f, -0.05f, 0.13f));
    add("Contour Poly", "Polysynth", { "kbd-contour", "dual-rank", "velocity" },
        "A rounded bass-register body opens into clearer upper chords through keyboard brilliance contours. A gentle upper level lift supports melody notes without excessive brightness.",
        poly().wave(II, false, true)
              .lpf(I, 1550.0f, 0.11f).lpf(II, 2650.0f, 0.15f)
              .eg(I, 0.015f, 1.6f, 0.77f, 0.46f)
              .mix(0.35f).detune(3.1f).chorus(0.18f, 0.31f)
              .pw(II, 0.59f, 0.035f, 0.27f).hpf(II, 145.0f).feg(I, 0.06f, 0.30f, 0.014f, 1.55f, 0.34f)
              .feg(II, 0.04f, 0.42f, 0.008f, 0.61f, 0.19f).eg(II, 0.028f, 0.96f, 0.49f, 0.32f)
              .touch(II, 0.26f, 0.77f, 0.21f, 0.18f).contour(-0.30f, 0.18f, -0.04f, 0.17f));
    add("Ring Poly", "Polysynth", { "ring-mod", "dual-rank" },
        "A soft saw body and sine-supported pulse edge acquire modest metallic sidebands from the ring modulator. The dry sound retains its pitch and short chord articulation.",
        poly().wave(I, true, false, 0.06f).wave(II, false, true, 0.12f)
              .lpf(I, 1700.0f, 0.09f).lpf(II, 2900.0f, 0.12f)
              .eg(I, 0.010f, 1.7f, 0.73f, 0.45f)
              .ring(137.0f, 0.0f, 0.01f, 1.0f, 0.17f).mix(0.35f).detune(1.5f).g(P_chorusOn, 0.0f)
              .pw(II, 0.59f).hpf(II, 185.0f).feg(I, 0.02f, 0.20f, 0.014f, 1.7f, 0.31f).feg(II, 0.0f, 0.32f, 0.004f, 0.49f, 0.18f)
              .eg(II, 0.004f, 0.83f, 0.34f, 0.28f).touch(II, 0.19f, 0.73f, 0.16f, 0.20f).contour(-0.07f, -0.14f, -0.02f, 0.05f));
    add("Glide Poly", "Polysynth", { "glide", "dual-rank" },
        "Sustained saw and pulse chords use gentle polyphonic portamento. Softer onsets let pitch slides speak, while matched rank releases keep the arriving harmony coherent.",
        poly().wave(II, false, true).lpf(I, 1600.0f, 0.13f).lpf(II, 2450.0f, 0.16f)
              .glide(0.14f).mix(0.39f).detune(4.5f).chorus(0.23f, 0.33f)
              .wave(I, true, false, 0.10f).pw(II, 0.56f, 0.06f, 0.23f).feg(I, 0.05f, 0.18f, 0.072f, 1.8f, 0.43f)
              .feg(II, 0.14f, 0.27f, 0.115f, 1.35f, 0.38f).eg(I, 0.025f, 2.2f, 0.89f, 0.82f).eg(II, 0.047f, 1.75f, 0.67f, 0.74f)
              .touch(II, 0.18f, 0.63f, 0.29f, 0.24f));
    add("Tremolo Poly", "Polysynth", { "tremolo", "sub-mod", "dual-rank" },
        "A sustained saw body and softer square rank pulse at 5.4 Hz through the original SUB's VCA route, with a little stereo tremolo color. The opening accent settles quickly so the held chord's rhythmic level motion stays clear.",
        poly().wave(II, false, true).pw(II, 0.51f).lpf(I, 2000.0f, 0.10f).lpf(II, 2800.0f, 0.12f)
              .eg(I, 0.012f, 1.1f, 0.84f, 0.48f).eg(II, 0.018f, 0.8f, 0.71f, 0.39f)
              .tremolo(0.37f, 5.4f).sub(subSine, 5.4f, 0.0f, 0.0f, 0.20f)
              .mix(0.36f).detune(2.7f)
              .hpf(II, 150.0f).feg(I, 0.0f, 0.26f, 0.010f, 0.85f, 0.28f).feg(II, 0.0f, 0.35f, 0.004f, 0.32f, 0.15f)
              .touch(I, 0.14f, 0.65f, 0.13f, 0.21f).touch(II, 0.21f, 0.73f, 0.19f, 0.17f).contour(-0.07f, -0.12f, -0.02f, 0.04f));
    add("Poly Sub Pulse", "Polysynth", { "sub-mod", "resonance", "dual-rank" },
        "A 4 Hz square SUB switches between two filter colors on a sustained chord. The lower saw stays present while the resonant pulse emphasizes the rhythmic brightness change.",
        poly().wave(II, false, true).lpf(I, 1200.0f, 0.16f).lpf(II, 1750.0f, 0.30f)
              .sub(subSquare, 4.0f, 0.0f, 0.24f).subAt(0.0f, 0.0f, 0.0f)
              .g(P_modWheelSub, 0.0f).g(P_chorusOn, 0.0f).mix(0.39f).detune(2.5f).res(0.17f)
              .pw(II, 0.64f).hpf(II, 195.0f).feg(I, 0.02f, 0.15f, 0.017f, 1.4f, 0.32f)
              .feg(II, 0.05f, 0.27f, 0.027f, 0.62f, 0.19f).eg(I, 0.015f, 1.9f, 0.94f, 0.52f).eg(II, 0.026f, 1.0f, 0.70f, 0.35f)
              .touch(II, 0.16f, 0.71f, 0.17f, 0.20f));
}
