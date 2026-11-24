// Polysynth voicings. Included inside namespace curated by CuratedLibrary.inc.
// Existing program names and insertion order are stable host/MIDI identities.
// A chord body and shorter upper accent have separate articulation. Initial
// touch carries real dynamics because the Circuit VCA adds its level routes.
// Individually balanced fixed and initial-touch levels retain the firm chord
// while giving quiet gestures room to recede; pressure remains independent.

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
              .g(P_chorusOn, 0.0f).mix(0.35f).lpf(I, 1500.0f, 0.10f).lpf(II, 3600.0f, 0.13f)
              .feg(I, 0.0f, 0.51f, 0.003f, 0.88f, 0.15f).feg(II, 0.0f, 0.63f, 0.0015f, 0.18f, 0.065f)
              .eg(I, 0.003f, 1.45f, 0.0f, 0.160f).eg(II, 0.0015f, 0.32f, 0.0f, 0.055f).touch(I, 0.21f, 0.960f, 0.0f, 0.0f)
              .touch(II, 0.32f, 1.000f, 0.0f, 0.0f).level(I, 1.0f, 0.085f).level(II, 0.95f, 0.035f).detune(1.4f)
              .contour(-0.12f, -0.20f, -0.03f, 0.06f));
    add("Poly Brass Stab", "Polysynth", { "velocity", "brilliance-swell", "percussive" },
        "A brief brass-like opening crest relaxes into a short chord body. The ranks speak at slightly different times and decay fully, giving a compact accented stab.",
        poly()
              .g(P_chorusOn, 0.0f).wave(I, true, false, 0.07f).lpf(I, 940.0f, 0.14f).lpf(II, 1880.0f, 0.18f)
              .feg(I, 0.26f, 0.60f, 0.047f, 1.05f, 0.19f).feg(II, 0.33f, 0.72f, 0.069f, 0.51f, 0.12f)
              .eg(I, 0.018f, 1.70f, 0.0f, 0.180f).eg(II, 0.032f, 0.72f, 0.0f, 0.090f).touch(I, 0.26f, 0.960f, 0.0f, 0.0f)
              .touch(II, 0.38f, 1.000f, 0.0f, 0.0f).hpf(II, 190.0f).mix(0.34f).detune(2.1f)
              .contour(-0.08f, -0.17f, -0.035f, 0.06f)
              .level(I, 1.000f, 0.100f)
              .level(II, 0.950f, 0.050f));
    add("PWM Poly", "Polysynth", { "pwm", "velocity", "dual-rank" },
        "Pulse ranks with separate width rates form a round sustained body and brighter moving edge. The initial accent settles into an animated held chord.",
        poly()
              .wave(I, false, true).wave(II, false, true).pan(I, -0.09f).pan(II, 0.15f).pw(I, 0.512f, 0.18f, 0.293f)
              .pw(II, 0.61f, 0.27f, 0.467f).lpf(I, 2100.0f, 0.085f).lpf(II, 3600.0f, 0.11f).hpf(II, 225.0f)
              .eg(I, 0.010f, 1.90f, 0.80f, 0.290f).eg(II, 0.022f, 0.80f, 0.52f, 0.180f)
              .feg(I, 0.0f, 0.34f, 0.008f, 1.75f, 0.27f).feg(II, 0.0f, 0.46f, 0.004f, 0.41f, 0.15f).level(I, 1.0f, 0.100f)
              .level(II, 0.95f, 0.055f).touch(I, 0.15f, 0.910f, 0.14f, 0.24f).touch(II, 0.22f, 0.980f, 0.22f, 0.18f).mix(0.38f)
              .detune(1.6f).chorus(0.13f, 0.30f).contour(-0.10f, -0.18f, -0.035f, 0.065f));
    add("Resonant Poly", "Polysynth", { "resonance", "velocity", "dual-rank" },
        "A sharp resonant upper accent sits over a warmer chord body. Unequal filter decay times distinguish the opening bite from the softer held sound.",
        poly()
              .res(0.34f).g(P_chorusOn, 0.0f).wave(II, false, true).pw(II, 0.63f).lpf(I, 1260.0f, 0.18f)
              .lpf(II, 910.0f, 0.46f).hpf(II, 220.0f).feg(I, 0.0f, 0.34f, 0.006f, 1.50f, 0.22f)
              .feg(II, 0.0f, 0.76f, 0.0018f, 0.22f, 0.08f).eg(I, 0.005f, 1.65f, 0.72f, 0.220f)
              .eg(II, 0.002f, 0.42f, 0.18f, 0.085f).level(I, 1.0f, 0.095f).level(II, 0.95f, 0.050f)
              .touch(I, 0.16f, 0.930f, 0.14f, 0.22f).touch(II, 0.27f, 0.980f, 0.16f, 0.14f).mix(0.24f).detune(1.3f)
              .contour(-0.09f, -0.25f, -0.02f, 0.06f));
    add("Detuned Poly", "Polysynth", { "detune", "dual-rank" },
        "Full saw ranks beat against each other by 17 cents. The second enters slightly later with a lighter sustain; no chorus masks the direct detuned character.",
        poly()
              .wave(I, true, false).wave(II, true, false).detune(17.0f).g(P_chorusOn, 0.0f).g(P_drift, 0.045f).hpf(I, 32.0f)
              .lpf(I, 2650.0f, 0.085f).lpf(II, 3600.0f, 0.10f).hpf(II, 200.0f).eg(I, 0.006f, 2.0f, 0.82f, 0.320f)
              .eg(II, 0.019f, 1.25f, 0.56f, 0.250f).feg(I, 0.02f, 0.32f, 0.008f, 1.8f, 0.27f)
              .feg(II, 0.03f, 0.41f, 0.018f, 0.58f, 0.19f).touch(I, 0.14f, 0.940f, 0.13f, 0.23f)
              .touch(II, 0.21f, 0.980f, 0.18f, 0.19f).mix(0.43f).g(P_voiceDispersion, 0.09f)
              .contour(-0.13f, -0.18f, -0.045f, 0.065f)
              .level(I, 1.000f, 0.130f)
              .level(II, 0.950f, 0.075f));
    add("Poly Sweep", "Polysynth", { "brilliance-swell", "dual-rank" },
        "A quickly established chord opens through staggered filter crests over roughly half a second. Pressure gently brightens the sustain after the opening sweep settles.",
        poly()
              .wave(II, false, true).lpf(I, 900.0f, 0.16f).lpf(II, 1850.0f, 0.21f).feg(I, 0.42f, 0.52f, 0.29f, 3.1f, 0.59f)
              .feg(II, 0.58f, 0.63f, 0.56f, 1.7f, 0.38f).eg(I, 0.018f, 2.6f, 0.87f, 0.520f).eg(II, 0.043f, 1.3f, 0.54f, 0.330f)
              .pw(II, 0.61f, 0.045f, 0.19f).touch(I, 0.12f, 0.930f, 0.20f, 0.28f).touch(II, 0.17f, 0.960f, 0.32f, 0.24f)
              .mix(0.35f).detune(3.1f).chorus(0.20f, 0.27f).contour(-0.10f, -0.19f, -0.035f, 0.07f)
              .level(I, 1.000f, 0.120f)
              .level(II, 0.950f, 0.065f));
    add("Velocity Poly", "Polysynth", { "velocity", "dual-rank" },
        "Soft strikes stay dark and quiet; hard strikes add substantial level and a bright upper accent. Low static VCA levels make velocity the main dynamic control.",
        poly()
              .hpf(I, 25.0f).g(P_chorusOn, 0.0f).wave(II, false, true).touch(I, 0.39f, 1.000f, 0.0f, 0.0f)
              .touch(II, 0.58f, 1.000f, 0.0f, 0.0f).level(I, 1.0f, 0.055f).level(II, 1.0f, 0.025f).lpf(I, 780.0f, 0.10f)
              .lpf(II, 1560.0f, 0.15f).hpf(II, 150.0f).pw(II, 0.67f).feg(I, 0.02f, 0.25f, 0.006f, 1.35f, 0.24f)
              .feg(II, 0.0f, 0.48f, 0.002f, 0.25f, 0.09f).eg(I, 0.005f, 2.2f, 0.83f, 0.260f)
              .eg(II, 0.002f, 0.48f, 0.21f, 0.100f).mix(0.31f).detune(2.4f).contour(-0.12f, -0.16f, -0.04f, 0.08f));
    add("Poly Pluck", "Polysynth", { "percussive", "velocity", "dual-rank" },
        "A short bright pulse attack releases into a longer resonant saw body. Both ranks have zero sustain, so held chords decay naturally instead of becoming a pad.",
        poly()
              .wave(II, false, true).g(P_chorusOn, 0.0f).pw(II, 0.72f).wave(I, true, false, 0.06f).lpf(I, 1300.0f, 0.17f)
              .lpf(II, 3600.0f, 0.15f).hpf(II, 520.0f).feg(I, 0.0f, 0.68f, 0.0015f, 1.4f, 0.24f)
              .feg(II, 0.0f, 0.61f, 0.001f, 0.105f, 0.045f).eg(I, 0.0015f, 3.4f, 0.0f, 0.400f)
              .eg(II, 0.001f, 0.19f, 0.0f, 0.040f).touch(I, 0.26f, 0.970f, 0.0f, 0.0f).touch(II, 0.36f, 1.000f, 0.0f, 0.0f)
              .level(I, 1.0f, 0.070f).level(II, 0.95f, 0.035f).mix(0.18f).detune(0.6f).contour(-0.12f, -0.19f, -0.03f, 0.09f));
    add("Octave Poly", "Polysynth", { "feet-layer", "dual-rank", "velocity" },
        "A full 8' saw chord carries a lighter 4' crown. The upper filter is opened for its register, and its shorter accent settles without overwhelming the sustained root.",
        poly()
              .feet(II, f4).wave(II, false, true).pw(II, 0.57f, 0.045f, 0.31f).lpf(I, 1900.0f, 0.10f).hpf(II, 310.0f)
              .lpf(II, 5300.0f, 0.11f).feg(I, 0.02f, 0.35f, 0.007f, 1.7f, 0.27f).feg(II, 0.0f, 0.41f, 0.003f, 0.31f, 0.12f)
              .eg(I, 0.007f, 1.9f, 0.84f, 0.300f).eg(II, 0.003f, 0.52f, 0.23f, 0.120f).level(I, 1.0f, 0.095f)
              .level(II, 0.95f, 0.040f).touch(I, 0.17f, 0.930f, 0.16f, 0.23f).touch(II, 0.25f, 1.000f, 0.18f, 0.17f).mix(0.24f)
              .detune(1.0f).chorus(0.12f, 0.29f).contour(-0.10f, -0.18f, -0.04f, 0.06f));
    add("Fifth Poly", "Polysynth", { "feet-layer", "dual-rank" },
        "The 8' body and quieter 5 1/3' harmonic fifth form an open chord color. Stable tuning and a lighter sustained upper rank keep the root clear during held voicings.",
        poly()
              .feet(II, f513).wave(II, false, true).lpf(I, 1650.0f, 0.12f).g(P_drift, 0.022f).hpf(II, 300.0f)
              .lpf(II, 3900.0f, 0.115f).pw(II, 0.503f, 0.01f, 0.19f).feg(I, 0.02f, 0.32f, 0.011f, 1.7f, 0.30f)
              .feg(II, 0.0f, 0.36f, 0.006f, 0.50f, 0.17f).eg(I, 0.008f, 1.9f, 0.83f, 0.330f)
              .eg(II, 0.021f, 1.55f, 0.37f, 0.220f).touch(I, 0.14f, 0.930f, 0.15f, 0.25f).touch(II, 0.17f, 0.910f, 0.15f, 0.20f)
              .mix(0.23f).detune(0.0f).chorus(0.10f, 0.26f).contour(-0.08f, -0.17f, -0.035f, 0.065f)
              .level(I, 1.000f, 0.120f)
              .level(II, 0.950f, 0.065f));
    add("Bright Poly", "Polysynth", { "velocity", "dual-rank" },
        "An open saw body and narrow-pulse edge deliver bright, immediate chords. A brief upper accent adds definition while restrained resonance keeps the held sound clear.",
        poly()
              .wave(II, false, true).res(0.07f).detune(3.2f).hpf(I, 45.0f).lpf(I, 4300.0f, 0.06f).lpf(II, 7000.0f, 0.075f)
              .hpf(II, 330.0f).pw(II, 0.70f).feg(I, 0.0f, 0.17f, 0.003f, 1.35f, 0.19f)
              .feg(II, 0.0f, 0.25f, 0.0015f, 0.20f, 0.075f).eg(I, 0.004f, 1.7f, 0.80f, 0.200f)
              .eg(II, 0.0018f, 0.31f, 0.15f, 0.070f).touch(I, 0.16f, 0.950f, 0.13f, 0.23f)
              .touch(II, 0.20f, 0.980f, 0.11f, 0.17f).level(I, 1.0f, 0.100f).level(II, 0.95f, 0.045f).mix(0.27f)
              .chorus(0.12f, 0.31f).contour(-0.06f, -0.26f, -0.02f, 0.08f));
    add("Dark Poly", "Polysynth", { "sine", "dual-rank" },
        "A filtered saw with a small sine foundation holds the chord beneath a quiet dark pulse. The upper rank closes sooner, leaving a rounded, gently extended release.",
        poly()
              .wave(II, false, true).hpf(I, 25.0f).pw(II, 0.53f, 0.035f, 0.21f).wave(I, true, false, 0.13f)
              .lpf(I, 780.0f, 0.085f).lpf(II, 1340.0f, 0.10f).hpf(II, 120.0f).feg(I, 0.08f, 0.19f, 0.020f, 2.5f, 0.48f)
              .feg(II, 0.12f, 0.27f, 0.032f, 0.62f, 0.32f).eg(I, 0.015f, 2.6f, 0.90f, 0.850f)
              .eg(II, 0.028f, 1.3f, 0.42f, 0.410f).touch(I, 0.10f, 0.960f, 0.14f, 0.26f).touch(II, 0.16f, 0.980f, 0.23f, 0.20f)
              .mix(0.24f).detune(1.8f).chorus(0.13f, 0.24f).contour(-0.08f, -0.08f, -0.03f, 0.07f)
              .level(I, 1.000f, 0.140f)
              .level(II, 0.950f, 0.070f));
    add("Hollow Poly", "Polysynth", { "pwm", "dual-rank" },
        "Near-square pulses give a hollow, woody chord center. A quieter, slightly narrower upper rank adds a reedy edge; shallow width movement avoids a thin constant sweep.",
        poly()
              .wave(I, false, true).wave(II, false, true).g(P_chorusOn, 0.0f).pw(I, 0.50f, 0.006f, 0.21f)
              .pw(II, 0.535f, 0.018f, 0.31f).lpf(I, 1770.0f, 0.11f).lpf(II, 2340.0f, 0.22f).hpf(II, 420.0f, 0.085f)
              .feg(I, 0.02f, 0.26f, 0.006f, 1.5f, 0.22f).feg(II, 0.04f, 0.42f, 0.004f, 0.36f, 0.11f)
              .eg(I, 0.006f, 1.85f, 0.81f, 0.260f).eg(II, 0.014f, 0.62f, 0.34f, 0.130f).touch(I, 0.12f, 0.940f, 0.10f, 0.24f)
              .touch(II, 0.21f, 0.980f, 0.18f, 0.17f).mix(0.24f).detune(0.65f).contour(-0.10f, -0.14f, -0.04f, 0.06f)
              .level(I, 1.000f, 0.110f)
              .level(II, 0.950f, 0.055f));
    add("Wide Poly", "Polysynth", { "chorus", "dual-rank" },
        "A solid saw and brighter PWM rank spread across the stereo field. Different accent lengths and a broad chorus give width while the sustained chord remains balanced.",
        poly()
              .wave(II, false, true).feg(I, 0.03f, 0.30f, 0.012f, 1.5f, 0.34f).lpf(I, 2200.0f, 0.085f)
              .lpf(II, 3500.0f, 0.10f).hpf(II, 220.0f).pw(II, 0.59f, 0.13f, 0.27f)
              .feg(II, 0.05f, 0.41f, 0.026f, 0.62f, 0.22f).eg(I, 0.010f, 1.85f, 0.81f, 0.440f)
              .eg(II, 0.028f, 0.92f, 0.52f, 0.280f).touch(I, 0.14f, 0.950f, 0.15f, 0.23f).touch(II, 0.21f, 1.000f, 0.22f, 0.20f)
              .pan(I, -0.28f).pan(II, 0.35f).mix(0.42f).detune(5.8f).chorus(0.40f, 0.33f)
              .contour(-0.10f, -0.18f, -0.04f, 0.07f)
              .level(I, 1.000f, 0.120f)
              .level(II, 0.950f, 0.075f));
    add("Poly Swell", "Polysynth", { "aftertouch", "dual-rank" },
        "A soft initial chord grows under pressure. Low static levels leave room for a substantial dynamic swell, and the brighter second rank responds more strongly than the body.",
        poly()
              .hpf(I, 28.0f).wave(II, false, true).pw(II, 0.59f, 0.08f, 0.25f).hpf(II, 120.0f).lpf(I, 1120.0f, 0.10f)
              .lpf(II, 1950.0f, 0.135f).level(I, 1.0f, 0.090f).level(II, 0.96f, 0.060f).touch(I, 0.035f, 0.090f, 0.24f, 0.94f)
              .touch(II, 0.055f, 0.110f, 0.38f, 1.0f).feg(I, 0.11f, 0.16f, 0.085f, 2.6f, 0.50f)
              .feg(II, 0.20f, 0.25f, 0.175f, 1.45f, 0.34f).eg(I, 0.048f, 2.8f, 0.98f, 0.720f)
              .eg(II, 0.089f, 2.1f, 0.82f, 0.470f).mix(0.35f).detune(2.7f).chorus(0.20f, 0.26f)
              .contour(-0.09f, -0.14f, -0.03f, 0.08f));
    add("Saw Pulse Poly", "Polysynth", { "dual-rank", "velocity" },
        "A sustained saw body pairs with a narrower pulse accent. Their distinct spectral and envelope roles remain audible through the short, lightly chorused release.",
        poly()
              .wave(I, true, false, 0.025f).wave(II, false, true).lpf(I, 1960.0f, 0.09f).lpf(II, 3550.0f, 0.16f)
              .pw(II, 0.66f, 0.02f, 0.23f).hpf(II, 330.0f).feg(I, 0.02f, 0.33f, 0.007f, 1.6f, 0.25f)
              .feg(II, 0.0f, 0.56f, 0.002f, 0.23f, 0.095f).eg(I, 0.007f, 1.9f, 0.84f, 0.280f)
              .eg(II, 0.002f, 0.43f, 0.20f, 0.110f).level(I, 1.0f, 0.095f).level(II, 0.95f, 0.045f)
              .touch(I, 0.16f, 0.920f, 0.16f, 0.23f).touch(II, 0.30f, 1.000f, 0.23f, 0.19f).pan(I, -0.05f).pan(II, 0.15f)
              .mix(0.30f).detune(1.7f).chorus(0.12f, 0.29f).contour(-0.10f, -0.18f, -0.035f, 0.07f));
    add("Synthwave Chords", "Polysynth", { "chorus", "detune", "dual-rank" },
        "Detuned saw chords spread across stereo with a bright opening accent and chorus. The second rank sustains more softly and releases sooner, preserving a warm center in the longer retro tail.",
        poly()
              .wave(I, true, false).wave(II, true, false).mix(0.45f).pan(I, -0.24f).pan(II, 0.32f).lpf(I, 2850.0f, 0.11f)
              .lpf(II, 4100.0f, 0.13f).hpf(II, 170.0f).feg(I, 0.02f, 0.28f, 0.009f, 2.3f, 0.61f)
              .feg(II, 0.0f, 0.47f, 0.004f, 0.57f, 0.33f).eg(I, 0.008f, 2.8f, 0.87f, 1.550f)
              .eg(II, 0.024f, 1.2f, 0.62f, 0.940f).touch(I, 0.15f, 0.940f, 0.16f, 0.24f).touch(II, 0.24f, 1.000f, 0.22f, 0.18f)
              .detune(9.4f).chorus(0.38f, 0.31f).contour(-0.10f, -0.21f, -0.04f, 0.07f)
              .level(I, 1.000f, 0.150f)
              .level(II, 0.950f, 0.085f));
    add("Square Poly", "Polysynth", { "dual-rank", "velocity" },
        "Unmodulated 50-percent pulses give a clear square-wave chord. The upper rank has a shorter accent and brighter filter; light detune adds body without a PWM sweep.",
        poly()
              .wave(I, false, true).wave(II, false, true).pw(I, 0.5f, 0.0f).pw(II, 0.5f, 0.0f).g(P_chorusOn, 0.0f)
              .lpf(I, 2500.0f, 0.085f).lpf(II, 3700.0f, 0.10f).hpf(II, 220.0f).feg(I, 0.0f, 0.29f, 0.006f, 1.8f, 0.25f)
              .feg(II, 0.0f, 0.40f, 0.003f, 0.37f, 0.12f).eg(I, 0.005f, 2.2f, 0.88f, 0.290f)
              .eg(II, 0.011f, 0.62f, 0.35f, 0.130f).level(I, 1.0f, 0.100f).level(II, 0.95f, 0.050f)
              .touch(I, 0.15f, 0.940f, 0.10f, 0.20f).touch(II, 0.21f, 1.000f, 0.16f, 0.15f).mix(0.34f).detune(1.0f)
              .contour(-0.10f, -0.17f, -0.03f, 0.07f));
    add("Sine Poly", "Polysynth", { "sine", "dual-rank", "velocity" },
        "A dominant pure sine body is joined by a small filtered saw attack. The harmonic rank decays to a quiet sustain, leaving rounded chords with meaningful velocity response.",
        poly()
              .wave(II, true, false).feg(I, 0.0f, 0.0f, 0.01f, 0.5f, 0.3f).detune(0.0f).g(P_chorusOn, 0.0f)
              .g(P_analogNoise, 0.0f).wave(I, false, false, 0.91f).lpf(II, 1420.0f, 0.085f).hpf(II, 330.0f)
              .level(I, 0.0f, 0.070f).level(II, 0.55f, 0.040f).mix(0.16f).feg(II, 0.0f, 0.51f, 0.0015f, 0.25f, 0.10f)
              .eg(I, 0.008f, 2.8f, 0.98f, 0.600f).eg(II, 0.002f, 0.48f, 0.04f, 0.090f).touch(I, 0.0f, 1.000f, 0.0f, 0.30f)
              .touch(II, 0.27f, 0.980f, 0.10f, 0.13f).g(P_drift, 0.0f).g(P_voiceDispersion, 0.055f)
              .contour(0.0f, -0.13f, -0.06f, 0.16f));
    add("Contour Poly", "Polysynth", { "kbd-contour", "dual-rank", "velocity" },
        "A rounded bass-register body opens into clearer upper chords through keyboard brilliance contours. A gentle upper level lift supports melody notes without excessive brightness.",
        poly()
              .wave(II, false, true).chorus(0.18f, 0.31f).lpf(I, 1660.0f, 0.095f).lpf(II, 2840.0f, 0.13f).hpf(II, 210.0f)
              .pw(II, 0.58f, 0.024f, 0.21f).feg(I, 0.06f, 0.34f, 0.010f, 1.8f, 0.30f)
              .feg(II, 0.04f, 0.49f, 0.006f, 0.44f, 0.15f).eg(I, 0.011f, 1.95f, 0.82f, 0.300f)
              .eg(II, 0.021f, 0.75f, 0.40f, 0.160f).touch(I, 0.14f, 0.910f, 0.17f, 0.23f).touch(II, 0.25f, 0.980f, 0.25f, 0.19f)
              .mix(0.31f).detune(2.2f).contour(-0.34f, 0.22f, -0.06f, 0.18f)
              .level(I, 1.000f, 0.100f)
              .level(II, 0.950f, 0.050f));
    add("Ring Poly", "Polysynth", { "ring-mod", "dual-rank" },
        "A soft saw body and sine-supported pulse edge acquire modest metallic sidebands from the ring modulator. The dry sound retains its pitch and short chord articulation.",
        poly()
              .g(P_chorusOn, 0.0f).pw(II, 0.59f).wave(I, true, false, 0.09f).wave(II, false, true, 0.08f)
              .lpf(I, 1850.0f, 0.075f).lpf(II, 3200.0f, 0.10f).hpf(II, 260.0f).feg(I, 0.02f, 0.24f, 0.011f, 1.9f, 0.28f)
              .feg(II, 0.0f, 0.37f, 0.003f, 0.36f, 0.14f).eg(I, 0.008f, 2.1f, 0.82f, 0.330f)
              .eg(II, 0.003f, 0.62f, 0.26f, 0.140f).touch(I, 0.15f, 0.940f, 0.14f, 0.24f).touch(II, 0.21f, 0.980f, 0.15f, 0.22f)
              .ring(137.0f, 0.0f, 0.01f, 1.0f, 0.14f).mix(0.30f).detune(0.4f).contour(-0.10f, -0.18f, -0.035f, 0.075f)
              .level(I, 1.000f, 0.120f)
              .level(II, 0.950f, 0.060f));
    add("Glide Poly", "Polysynth", { "glide", "dual-rank" },
        "Sustained saw and pulse chords use gentle polyphonic portamento. Softer onsets let pitch slides speak, while matched rank releases keep the arriving harmony coherent.",
        poly()
              .wave(II, false, true).mix(0.39f).wave(I, true, false, 0.10f).lpf(I, 1720.0f, 0.11f).lpf(II, 2650.0f, 0.14f)
              .pw(II, 0.55f, 0.04f, 0.19f).feg(I, 0.08f, 0.20f, 0.095f, 2.2f, 0.46f)
              .feg(II, 0.16f, 0.30f, 0.155f, 1.6f, 0.41f).eg(I, 0.034f, 2.5f, 0.92f, 0.600f)
              .eg(II, 0.060f, 1.9f, 0.70f, 0.570f).touch(I, 0.10f, 0.900f, 0.18f, 0.31f).touch(II, 0.16f, 0.950f, 0.28f, 0.27f)
              .glide(0.16f).detune(2.7f).chorus(0.17f, 0.28f).contour(-0.09f, -0.13f, -0.035f, 0.07f)
              .level(I, 1.000f, 0.130f)
              .level(II, 0.950f, 0.070f));
    add("Tremolo Poly", "Polysynth", { "tremolo", "sub-mod", "dual-rank" },
        "A sustained saw body and softer square rank pulse at 5.4 Hz through the original SUB's VCA route, with a little stereo tremolo color. The opening accent settles quickly so the held chord's rhythmic level motion stays clear.",
        poly()
              .wave(II, false, true).tremolo(0.37f, 5.4f).sub(subSine, 5.4f, 0.0f, 0.0f, 0.20f).lpf(I, 2150.0f, 0.085f)
              .lpf(II, 3050.0f, 0.10f).hpf(II, 210.0f).pw(II, 0.505f).feg(I, 0.0f, 0.23f, 0.008f, 0.72f, 0.24f)
              .feg(II, 0.0f, 0.31f, 0.003f, 0.25f, 0.12f).eg(I, 0.010f, 1.25f, 0.88f, 0.340f)
              .eg(II, 0.022f, 0.78f, 0.64f, 0.230f).touch(I, 0.13f, 0.950f, 0.12f, 0.24f).touch(II, 0.20f, 0.990f, 0.18f, 0.20f)
              .mix(0.32f).detune(1.4f).contour(-0.09f, -0.17f, -0.035f, 0.075f)
              .level(I, 1.000f, 0.120f)
              .level(II, 0.950f, 0.065f));
    add("Poly Sub Pulse", "Polysynth", { "sub-mod", "resonance", "dual-rank" },
        "A 4 Hz square SUB switches between two filter colors on a sustained chord. The lower saw stays present while the resonant pulse emphasizes the rhythmic brightness change.",
        poly()
              .wave(II, false, true).sub(subSquare, 4.0f, 0.0f, 0.24f).subAt(0.0f, 0.0f, 0.0f).g(P_modWheelSub, 0.0f)
              .g(P_chorusOn, 0.0f).res(0.17f).lpf(I, 1320.0f, 0.14f).lpf(II, 1980.0f, 0.28f).hpf(II, 280.0f).pw(II, 0.61f)
              .feg(I, 0.02f, 0.12f, 0.021f, 1.7f, 0.35f).feg(II, 0.05f, 0.22f, 0.035f, 0.73f, 0.23f)
              .eg(I, 0.014f, 2.3f, 0.96f, 0.370f).eg(II, 0.035f, 1.35f, 0.66f, 0.270f).touch(I, 0.10f, 0.930f, 0.12f, 0.24f)
              .touch(II, 0.15f, 0.990f, 0.18f, 0.22f).mix(0.34f).detune(1.1f).contour(-0.10f, -0.20f, -0.035f, 0.075f)
              .level(I, 1.000f, 0.110f)
              .level(II, 0.950f, 0.060f));
}
