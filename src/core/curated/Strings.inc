// Strings voicings. Included inside namespace curated by CuratedLibrary.inc.
// Existing program names and insertion order are stable host/MIDI identities.
// Harmonic bow/string bodies supplement the circuit source where appropriate.
// The body follows real HP/LP voicing now; open body ranks retain their formants
// while upper ranks stay spectrally separate. Explicit geometry, touch and key
// contours distinguish sections and registers. These are authored synth models.

Program strings()
{
    Program x;
    // The saw carries the bow/body; a quieter, later pulse rank supplies motion.
    // Cutoffs are referenced to MIDI 60. Low HP settings retain the fundamental
    // when pressure raises both filter cutoffs. SUB pressure .04 is only 6 cents.
    x.wave(I, true, false).wave(II, false, true).pw(II, 0.57f, 0.20f, 0.33f)
     .hpf(I, 80.0f, 0.02f).hpf(II, 180.0f, 0.03f)
     .lpf(I, 3300.0f, 0.09f).lpf(II, 5200.0f, 0.06f)
     .feg(I, 0.12f, 0.16f, 0.16f, 1.4f, 0.8f).feg(II, 0.20f, 0.10f, 0.30f, 1.8f, 1.0f)
     .eg(I, 0.14f, 0.9f, 0.88f, 0.75f).eg(II, 0.26f, 1.2f, 0.80f, 1.05f)
     .level(I, 0.95f, 0.55f).level(II, 0.88f, 0.50f)
     .touch(I, 0.10f, 0.34f, 0.18f, 0.22f).touch(II, 0.08f, 0.28f, 0.14f, 0.18f)
     .sub(subSine, 5.4f, 0.0f).subAt(0.04f, 0.04f).g(P_modWheelSub, 0.035f)
     .mix(0.42f).chorus(0.36f, 0.28f).detune(5.5f).res(0.10f)
     .pan(I, -0.10f).pan(II, 0.10f)
     .g(P_voiceDispersion, 0.18f).g(P_drift, 0.08f).g(P_analogNoise, 0.012f).volume(0.6f);
    return x;
}

void buildStrings(Builder& add)
{
    add("Ensemble Strings", "Strings", { "pwm", "chorus", "aftertouch" },
        "A modeled bowed body enters before a lighter moving upper section. Different harmonic weights and restrained chorus give the ranks depth; pressure adds bow intensity and gentle vibrato.",
        strings()
               .body(I, body::Model::bowed, 0.64f, 0.50f, 1.10f)
               .body(II, body::Model::bowed, 0.34f, 0.65f, 1.00f).chorus(0.28f, 0.24f)
               .body(I, body::Model::bowed, 0.74f, 0.57f, 1.10f).bodySize(I, 1.20f)
               .body(II, body::Model::bowed, 0.48f, 0.70f, 1.00f).bodySize(II, 1.0f)
               .hpf(I, 50.0f).lpf(I, 5800.0f, 0.075f).lpf(II, 7600.0f, 0.05f)
               .mix(0.41f).chorus(0.24f, 0.19f).detune(4.5f)
               .contour(0.08f, -0.16f, 0.02f, -0.08f));
    add("Slow Strings", "Strings", { "chorus", "brilliance-swell" },
        "A soft bowed harmonic body rises over 0.85 seconds and the brighter section follows over 1.15 seconds. Hold the chord through the opening to hear their separate spectral blooms and long overlapping releases.",
        strings().eg(I, 0.85f, 2.0f, 0.90f, 1.7f).eg(II, 1.15f, 2.4f, 0.84f, 2.1f)
            .feg(I, 0.24f, 0.18f, 0.9f, 2.4f, 1.6f).feg(II, 0.32f, 0.12f, 1.25f, 2.8f, 1.9f)
            .lpf(I, 2800.0f, 0.08f).lpf(II, 4200.0f, 0.06f).chorus(0.40f, 0.22f)
               .body(I, body::Model::bowed, 0.68f, 0.40f, 1.40f)
               .body(II, body::Model::bowed, 0.42f, 0.55f, 1.25f)
               .body(I, body::Model::bowed, 0.78f, 0.45f, 1.40f).bodySize(I, 1.25f)
               .bodySize(II, 1.08f).lpf(I, 4700.0f, 0.065f).lpf(II, 6200.0f, 0.045f)
               .chorus(0.30f, 0.17f).contour(0.06f, -0.18f, 0.01f, -0.08f));
    add("Violins", "Strings", { "feet-layer", "velocity" },
        "An 8' modeled bowed section carries a quieter 4' upper sheen. A quick bow onset, clear upper harmonics and restrained chorus preserve articulated lines, with pressure adding intensity after the attack.",
        strings().feet(II, f4).mix(0.20f).hpf(I, 160.0f, 0.04f).hpf(II, 650.0f, 0.02f)
            .lpf(I, 5000.0f, 0.10f).lpf(II, 8500.0f, 0.04f)
            .eg(I, 0.09f, 0.7f, 0.86f, 0.55f).eg(II, 0.18f, 1.0f, 0.70f, 0.7f)
            .level(II, 0.82f, 0.42f).touch(I, 0.14f, 0.38f, 0.18f, 0.18f)
            .chorus(0.28f, 0.32f).detune(4.0f)
               .body(I, body::Model::bowed, 0.84f, 0.72f, 0.90f)
               .body(II, body::Model::bowed, 0.44f, 0.80f, 0.80f).chorus(0.20f, 0.27f)
               .bodySize(I, 1.0f).bodySize(II, 0.94f)
               .hpf(I, 85.0f, 0.025f).hpf(II, 500.0f, 0.02f)
               .lpf(I, 8000.0f, 0.075f).lpf(II, 11000.0f, 0.035f)
               .eg(I, 0.060f, 0.70f, 0.90f, 0.38f).eg(II, 0.10f, 0.65f, 0.64f, 0.32f)
               .chorus(0.16f, 0.24f).detune(2.8f).contour(0.05f, -0.15f, 0.02f, -0.08f));
    add("Cellos", "Strings", { "feet-layer", "aftertouch" },
        "A larger 16' cello body supports a shorter 8' bow edge; its lower resonant bands remain distinct from the violin section. The darker low spectrum keeps weight beneath the articulation; pressure opens the upper harmonics without a broad ensemble wobble.",
        strings().feet(I, f16).mix(0.25f).hpf(I, 28.0f).hpf(II, 115.0f, 0.03f)
            .lpf(I, 1600.0f, 0.12f).lpf(II, 2900.0f, 0.08f)
            .eg(I, 0.17f, 1.1f, 0.92f, 0.85f).eg(II, 0.10f, 0.7f, 0.48f, 0.55f)
            .feg(II, 0.06f, 0.24f, 0.09f, 0.65f, 0.4f)
            .touch(I, 0.08f, 0.32f, 0.16f, 0.24f).sub(subSine, 4.9f, 0.0f).subAt(0.03f, 0.075f)
            .chorus(0.24f, 0.24f).detune(3.0f)
               .body(I, body::Model::bowed, 0.90f, 0.32f, 1.30f)
               .body(II, body::Model::bowed, 0.36f, 0.56f, 1.00f).chorus(0.17f, 0.20f)
               .body(I, body::Model::bowed, 0.94f, 0.50f, 1.30f).bodySize(I, 2.05f)
               .body(II, body::Model::bowed, 0.45f, 0.62f, 1.00f).bodySize(II, 1.90f)
               .hpf(I, 20.0f).hpf(II, 180.0f, 0.02f).lpf(I, 4200.0f, 0.08f).lpf(II, 5400.0f, 0.06f)
               .eg(I, 0.11f, 1.1f, 0.93f, 0.55f).eg(II, 0.07f, 0.60f, 0.48f, 0.32f)
               .chorus(0.14f, 0.18f).detune(2.3f).contour(0.10f, -0.10f, 0.04f, -0.08f));
    add("Octave Strings", "Strings", { "feet-layer", "chorus" },
        "An 8' harmonic bowed section is joined by a lighter, later 4' section. Different upper-partial weights and envelope timing keep both registers clear without hollowing the lower body.",
        strings().feet(II, f4).mix(0.43f).hpf(I, 90.0f).hpf(II, 230.0f)
            .lpf(I, 3600.0f, 0.08f).lpf(II, 7000.0f, 0.05f)
            .eg(I, 0.16f, 1.0f, 0.88f, 0.9f).eg(II, 0.30f, 1.3f, 0.78f, 1.1f)
            .chorus(0.34f, 0.30f).detune(3.5f)
               .body(I, body::Model::bowed, 0.58f, 0.52f, 1.10f)
               .body(II, body::Model::bowed, 0.40f, 0.67f, 1.00f).mix(0.37f)
               .bodySize(I, 1.16f).bodySize(II, 1.02f)
               .hpf(I, 45.0f).hpf(II, 260.0f).lpf(I, 6200.0f, 0.065f).lpf(II, 9200.0f, 0.045f)
               .level(II, 0.88f, 0.42f).mix(0.30f).eg(II, 0.24f, 1.20f, 0.72f, 0.85f)
               .contour(0.04f, -0.16f, 0.0f, -0.07f));
    add("PWM Strings", "Strings", { "pwm", "chorus" },
        "Two pulse sections evolve at distinct slow rates, with the second filter bloom arriving later. Reduced PWM excursion and chorus preserve a steady pitch center while the harmonic balance keeps moving.",
        strings().wave(Both, false, true).pw(I, 0.53f, 0.32f, 0.29f).pw(II, 0.61f, 0.25f, 0.41f)
            .hpf(I, 100.0f).hpf(II, 180.0f).lpf(I, 3800.0f, 0.07f).lpf(II, 5400.0f, 0.05f)
            .feg(I, 0.10f, 0.08f, 0.20f, 1.5f, 0.9f).feg(II, 0.16f, 0.12f, 0.34f, 1.7f, 1.0f)
            .chorus(0.28f, 0.32f).detune(4.0f)
               .pw(I, 0.51f, 0.23f, 0.23f).pw(II, 0.65f, 0.18f, 0.37f).feg(II, 0.21f, 0.16f, 0.46f, 1.4f, 1.15f).chorus(0.20f, 0.28f).detune(3.2f)
               .pw(I, 0.51f, 0.20f, 0.17f).pw(II, 0.65f, 0.15f, 0.31f)
               .lpf(I, 4100.0f, 0.065f).lpf(II, 5800.0f, 0.045f)
               .eg(I, 0.11f, 0.90f, 0.90f, 0.60f).eg(II, 0.23f, 1.15f, 0.75f, 0.88f)
               .chorus(0.18f, 0.21f).detune(2.8f).contour(0.12f, -0.20f, -0.03f, -0.07f));
    add("Thin Strings", "Strings", {},
        "A high-passed circuit section retains a narrow upper-string texture without losing every low harmonic. The thinner second rank enters later and relaxes sooner, making the distant sound airy rather than brittle.",
        strings().wave(Both, true, false).hpf(I, 700.0f, 0.06f).hpf(II, 1350.0f, 0.04f)
            .lpf(I, 5000.0f, 0.05f).lpf(II, 7000.0f, 0.04f)
            .level(I, 1.0f, 0.68f).level(II, 1.0f, 0.58f)
            .touch(I, 0.05f, 0.32f, 0.08f, 0.18f).touch(II, 0.04f, 0.26f, 0.06f, 0.15f)
            .eg(I, 0.22f, 1.0f, 0.82f, 1.0f).eg(II, 0.38f, 1.4f, 0.74f, 1.3f)
            .chorus(0.30f, 0.24f).detune(3.8f)
               .hpf(I, 570.0f, 0.045f).hpf(II, 1180.0f, 0.025f).lpf(I, 4600.0f, 0.06f).feg(II, 0.24f, 0.13f, 0.52f, 1.1f, 1.15f).chorus(0.23f, 0.19f)
               .hpf(I, 390.0f, 0.035f).hpf(II, 920.0f, 0.02f)
               .lpf(I, 5200.0f, 0.045f).lpf(II, 7200.0f, 0.035f)
               .level(II, 0.92f, 0.45f).eg(II, 0.32f, 1.20f, 0.68f, 0.92f)
               .contour(0.18f, -0.15f, 0.01f, -0.09f));
    add("Dark Strings", "Strings", {},
        "A dark modeled bowed body supports covered pulse overtones. Slow harmonic openings and a restrained chorus keep the low center intact while the long releases join soft, sombre chords.",
        strings().wave(I, true, false, 0.05f).hpf(I, 45.0f).hpf(II, 85.0f)
            .lpf(I, 1100.0f, 0.12f).lpf(II, 1700.0f, 0.08f).brill(-0.15f)
            .feg(I, 0.16f, 0.10f, 0.40f, 1.7f, 1.2f).feg(II, 0.22f, 0.08f, 0.55f, 2.0f, 1.5f)
            .eg(I, 0.36f, 1.4f, 0.91f, 1.4f).eg(II, 0.50f, 1.8f, 0.84f, 1.7f)
            .level(I, 0.95f, 0.9625f).level(II, 0.88f, 0.875f)
            .touch(Both, 0.04f, 0.4375f, 0.12f, 0.35f).chorus(0.28f, 0.18f)
               .body(I, body::Model::bowed, 0.70f, 0.15f, 1.40f)
               .body(II, body::Model::bowed, 0.38f, 0.26f, 1.30f).chorus(0.20f, 0.16f)
               .body(I, body::Model::bowed, 0.84f, 0.25f, 1.40f).bodySize(I, 1.60f)
               .body(II, body::Model::bowed, 0.45f, 0.36f, 1.30f).bodySize(II, 1.35f)
               .hpf(I, 20.0f).hpf(II, 80.0f).lpf(I, 2700.0f, 0.07f).lpf(II, 3500.0f, 0.05f)
               .touch(I, 0.04f, 0.50f, 0.16f, 0.32f).contour(0.10f, -0.20f, 0.04f, -0.08f));
    add("Tremolo Strings", "Strings", { "sub-mod" },
        "A 6.5 Hz rounded level motion suggests repeated bow strokes across a sustained harmonic body. The lighter upper rank responds more quickly, keeping the tremolo animated without abrupt square-wave chopping.",
        strings().sub(subSquare, 6.5f, 0.0f, 0.025f, 0.50f).subAt(0.0f, 0.0f)
            .g(P_modWheelSub, 0.0f).eg(I, 0.08f, 0.8f, 0.90f, 0.5f).eg(II, 0.13f, 1.0f, 0.82f, 0.65f)
            .lpf(I, 3100.0f, 0.08f).lpf(II, 4600.0f, 0.05f).chorus(0.16f, 0.35f).detune(3.0f)
               .body(I, body::Model::bowed, 0.65f, 0.54f, 0.80f).sub(subSine, 6.5f, 0.0f, 0.035f, 0.43f).eg(II, 0.095f, 0.8f, 0.75f, 0.46f)
               .bodySize(I, 1.15f).hpf(I, 70.0f).hpf(II, 270.0f)
               .lpf(I, 5400.0f, 0.065f).lpf(II, 6500.0f, 0.045f)
               .sub(subSine, 6.5f, 0.0f, 0.024f, 0.37f).eg(II, 0.055f, 0.34f, 0.60f, 0.20f)
               .contour(0.08f, -0.15f, 0.02f, -0.09f));
    add("Solo Violin", "Strings", { "aftertouch", "sub-mod", "init-bend", "velocity" },
        "A dry 8' modeled bowed body carries evolving upper harmonics beneath a separate, brief bow-friction onset. Velocity shapes the bow and pressure adds controlled vibrato; the stable single line has no ensemble chorus.",
        strings().mix(0.16f).wave(I, true, false, 0.025f, 0.002f).wave(II, false, false, 0.0f, 0.95f)
            .feet(I, f8).g(P_chorusOn, 0.0f).detune(0.0f).pan(Both, 0.0f)
            .hpf(I, 150.0f, 0.06f).lpf(I, 3800.0f, 0.12f)
            .feg(I, 0.08f, 0.16f, 0.10f, 0.55f, 0.28f).eg(I, 0.045f, 0.65f, 0.85f, 0.28f)
            .level(I, 0.92f, 0.38f).touch(I, 0.18f, 0.62f, 0.18f, 0.285f)
            .hpf(II, 1700.0f, 0.0f).lpf(II, 6800.0f, 0.04f)
            .feg(II, 0.0f, 0.08f, 0.006f, 0.14f, 0.06f)
            .eg(II, 0.006f, 0.24f, 0.0f, 0.06f).level(II, 0.85f, 0.30f)
            .touch(II, 0.0f, 0.90f, 0.0f, 0.0f)
            .sub(subSine, 5.7f, 0.0f).subAt(0.03f, 0.105f).g(P_modWheelSub, 0.065f).bend(0.045f)
            .contour(0.0f, -0.10f, 0.0f, -0.05f)
            .g(P_voiceDispersion, 0.08f).g(P_drift, 0.025f).g(P_analogNoise, 0.003f)
               .body(I, body::Model::bowed, 0.94f, 0.67f, 0.90f).eg(I, 0.035f, 0.62f, 0.86f, 0.23f).g(P_drift, 0.015f)
               .body(I, body::Model::bowed, 0.98f, 0.65f, 0.90f).bodySize(I, 1.0f)
               .hpf(I, 75.0f, 0.025f).lpf(I, 7600.0f, 0.075f).level(I, 0.92f, 0.44f)
               .eg(I, 0.024f, 0.62f, 0.90f, 0.145f).touch(I, 0.16f, 0.64f, 0.20f, 0.28f)
               .wave(II, false, false, 0.0f, 0.42f).hpf(II, 1450.0f).lpf(II, 7200.0f, 0.03f)
               .eg(II, 0.003f, 0.10f, 0.0f, 0.035f).level(II, 0.80f, 0.24f).mix(0.09f)
               .subAt(0.025f, 0.075f).bend(0.015f).g(P_voiceDispersion, 0.045f).g(P_drift, 0.01f).g(P_analogNoise, 0.0f)
               .contour(0.08f, -0.11f, 0.03f, -0.07f));
    add("Solo Cello", "Strings", { "aftertouch", "sub-mod", "velocity" },
        "A dry 16' bowed body uses larger cello resonances while retaining its upper bow band. A warm fundamental follows an independently fading low bow-friction onset. Pressure adds weight and restrained vibrato, with no octave layer or ensemble chorus.",
        strings().mix(0.14f).wave(I, true, false, 0.07f, 0.0015f).wave(II, false, false, 0.0f, 0.85f)
            .feet(I, f16).g(P_chorusOn, 0.0f).detune(0.0f).pan(Both, 0.0f)
            .hpf(I, 28.0f, 0.03f).lpf(I, 1500.0f, 0.14f)
            .feg(I, 0.12f, 0.18f, 0.12f, 0.8f, 0.4f).eg(I, 0.075f, 0.85f, 0.90f, 0.42f)
            .level(I, 0.95f, 0.395f).touch(I, 0.14f, 0.56f, 0.16f, 0.326f)
            .hpf(II, 850.0f, 0.0f).lpf(II, 4200.0f, 0.04f)
            .feg(II, 0.0f, 0.08f, 0.010f, 0.20f, 0.08f)
            .eg(II, 0.010f, 0.32f, 0.0f, 0.08f).level(II, 0.85f, 0.30f)
            .touch(II, 0.0f, 0.90f, 0.0f, 0.0f)
            .sub(subSine, 4.9f, 0.0f).subAt(0.03f, 0.08f).g(P_modWheelSub, 0.055f)
            .contour(0.05f, -0.12f, 0.0f, -0.04f)
            .g(P_voiceDispersion, 0.07f).g(P_drift, 0.025f).g(P_analogNoise, 0.003f)
               .body(I, body::Model::bowed, 0.95f, 0.28f, 1.35f).eg(I, 0.062f, 0.90f, 0.91f, 0.35f).g(P_drift, 0.014f)
               .body(I, body::Model::bowed, 0.99f, 0.47f, 1.35f).bodySize(I, 2.10f)
               .hpf(I, 20.0f, 0.02f).lpf(I, 4500.0f, 0.085f).level(I, 0.95f, 0.45f)
               .eg(I, 0.045f, 0.90f, 0.94f, 0.21f).touch(I, 0.12f, 0.60f, 0.20f, 0.30f)
               .wave(II, false, false, 0.0f, 0.50f).hpf(II, 700.0f).lpf(II, 4800.0f, 0.03f)
               .eg(II, 0.008f, 0.17f, 0.0f, 0.055f).level(II, 0.85f, 0.25f).mix(0.10f)
               .g(P_voiceDispersion, 0.04f).g(P_drift, 0.01f).g(P_analogNoise, 0.0f)
               .contour(0.12f, -0.12f, 0.04f, -0.07f));
    add("Pizzicato Strings", "Strings", { "percussive", "velocity", "dual-rank" },
        "A plucked-string harmonic body follows a shorter upper pluck, with faster losses in the high partials as the note settles. Both ranks decay to silence on a held key; velocity changes the excitation and short release damps the strings.",
        strings().wave(I, true, false, 0.12f).wave(II, false, true).pw(II, 0.76f)
            .feet(II, f4).mix(0.22f).hpf(I, 55.0f).hpf(II, 680.0f)
            .lpf(I, 1800.0f, 0.18f).lpf(II, 6200.0f, 0.08f)
            .feg(I, 0.0f, 0.35f, 0.002f, 0.34f, 0.25f).feg(II, 0.0f, 0.22f, 0.001f, 0.07f, 0.06f)
            .eg(I, 0.0015f, 1.7f, 0.0f, 0.38f).eg(II, 0.001f, 0.17f, 0.0f, 0.09f)
            .level(I, 0.95f, 0.13f).level(II, 0.85f, 0.10f)
            .touch(I, 0.22f, 0.88f, 0.0f, 0.0f).touch(II, 0.28f, 0.55f, 0.0f, 0.0f)
            .g(P_chorusOn, 0.0f).subAt(0.0f, 0.0f).g(P_modWheelSub, 0.0f)
            .pan(Both, 0.0f).detune(0.8f).res(0.14f).g(P_drift, 0.02f).g(P_analogNoise, 0.002f)
               .body(I, body::Model::nylon, 0.88f, 0.37f, 0.75f)
               .body(II, body::Model::steel, 0.44f, 0.65f, 0.45f).eg(I, 0.0018f, 1.95f, 0.0f, 0.32f)
               .body(I, body::Model::nylon, 0.96f, 0.49f, 0.63f).bodySize(I, 1.25f)
               .body(II, body::Model::steel, 0.54f, 0.72f, 0.40f).bodySize(II, 1.05f)
               .hpf(I, 40.0f).hpf(II, 950.0f).lpf(I, 5000.0f, 0.09f).lpf(II, 8000.0f, 0.05f)
               .eg(I, 0.0012f, 1.60f, 0.0f, 0.21f).eg(II, 0.001f, 0.13f, 0.0f, 0.045f)
               .level(I, 0.95f, 0.20f).level(II, 0.85f, 0.08f).mix(0.14f)
               .contour(0.16f, -0.21f, 0.02f, -0.11f));
    add("Swell Strings", "Strings", { "aftertouch" },
        "A quiet modeled bow bed gains harmonic weight and level under pressure. The lighter upper section enters later, and easing pressure returns the same held bow to a soft texture without retriggering.",
        strings().level(I, 0.95f, 0.275f).level(II, 0.88f, 0.20f)
            .touch(I, 0.0f, 0.0f, 0.30f, 0.8125f).touch(II, 0.0f, 0.0f, 0.20f, 0.6875f)
            .hpf(I, 70.0f).hpf(II, 120.0f).lpf(I, 2300.0f, 0.10f).lpf(II, 3500.0f, 0.07f)
            .eg(I, 0.28f, 1.3f, 0.90f, 1.2f).eg(II, 0.42f, 1.7f, 0.84f, 1.5f)
            .chorus(0.30f, 0.26f)
               .body(I, body::Model::bowed, 0.74f, 0.38f, 1.20f)
               .body(II, body::Model::bowed, 0.36f, 0.55f, 1.10f)
               .body(I, body::Model::bowed, 0.82f, 0.46f, 1.20f).bodySize(I, 1.25f).bodySize(II, 1.05f)
               .hpf(I, 45.0f).hpf(II, 180.0f).lpf(I, 5200.0f, 0.075f).lpf(II, 7000.0f, 0.05f)
               .touch(I, 0.0f, 0.0f, 0.26f, 0.8125f).contour(0.06f, -0.14f, 0.02f, -0.07f));
    add("Chamber Strings", "Strings", { "dual-rank" },
        "Two closely tuned modeled bow sections speak at slightly different times without chorus. Distinct harmonic weights and restrained motion suggest a small group while preserving clear note changes.",
        strings().g(P_chorusOn, 0.0f).detune(2.2f).mix(0.30f).pw(II, 0.59f, 0.06f, 0.27f)
            .hpf(I, 100.0f, 0.03f).hpf(II, 180.0f, 0.02f).lpf(I, 3200.0f, 0.10f).lpf(II, 4200.0f, 0.06f)
            .eg(I, 0.08f, 0.7f, 0.88f, 0.5f).eg(II, 0.14f, 0.9f, 0.82f, 0.65f)
            .level(II, 0.88f, 0.40f).touch(II, 0.08f, 0.24f, 0.12f, 0.18f)
            .pan(I, -0.04f).pan(II, 0.04f).subAt(0.04f, 0.05f)
            .g(P_voiceDispersion, 0.10f).g(P_drift, 0.04f)
               .body(I, body::Model::bowed, 0.86f, 0.49f, 1.05f)
               .body(II, body::Model::bowed, 0.62f, 0.62f, 0.95f).detune(1.6f).pw(II, 0.59f, 0.035f, 0.23f)
               .body(I, body::Model::bowed, 0.92f, 0.54f, 1.05f).bodySize(I, 1.08f)
               .body(II, body::Model::bowed, 0.75f, 0.59f, 0.95f).bodySize(II, 1.24f)
               .hpf(I, 55.0f, 0.025f).hpf(II, 200.0f, 0.02f).lpf(I, 6200.0f, 0.075f).lpf(II, 6900.0f, 0.05f)
               .eg(II, 0.105f, 0.80f, 0.82f, 0.42f).detune(1.4f).mix(0.33f)
               .contour(0.07f, -0.14f, 0.01f, -0.08f));
    add("Wide Strings", "Strings", { "chorus", "pwm" },
        "Broad stereo sections combine harmonic bowed depth with a moving pulse sheen. Reduced detuning and chorus preserve the pitch center while the slower upper entrance keeps sustained chords open.",
        strings().mix(0.48f).detune(10.0f).chorus(0.60f, 0.28f).pan(I, -0.36f).pan(II, 0.36f)
            .pw(II, 0.58f, 0.25f, 0.21f).lpf(I, 3600.0f, 0.08f).lpf(II, 5100.0f, 0.05f)
            .eg(I, 0.25f, 1.3f, 0.88f, 1.25f).eg(II, 0.40f, 1.7f, 0.82f, 1.6f)
               .body(I, body::Model::bowed, 0.45f, 0.54f, 1.20f)
               .body(II, body::Model::bowed, 0.35f, 0.64f, 1.10f).detune(7.1f).chorus(0.43f, 0.23f)
               .body(I, body::Model::bowed, 0.58f, 0.62f, 1.20f).bodySize(I, 1.28f)
               .body(II, body::Model::bowed, 0.47f, 0.67f, 1.10f).bodySize(II, 1.08f)
               .hpf(I, 55.0f).hpf(II, 160.0f).lpf(I, 6500.0f, 0.06f).lpf(II, 8200.0f, 0.04f)
               .pan(I, -0.28f).pan(II, 0.28f).chorus(0.34f, 0.18f).detune(5.8f)
               .eg(II, 0.32f, 1.70f, 0.80f, 1.25f).contour(0.08f, -0.20f, 0.0f, -0.08f));
    add("Crystal Strings", "Strings", { "feet-layer", "pwm" },
        "An 8' harmonic bowed body supports a delicate 2' circuit glint. The upper register opens later and stays light, adding a glassy edge without overtaking the string center.",
        strings().feet(II, f2).wave(II, false, true, 0.18f).pw(II, 0.63f, 0.10f, 0.28f)
            .hpf(I, 100.0f).hpf(II, 700.0f, 0.02f).lpf(I, 3500.0f, 0.08f).lpf(II, 10500.0f, 0.04f)
            .mix(0.19f).level(II, 0.65f, 0.55f).touch(II, 0.04f, 0.22f, 0.12f, 0.12f)
            .eg(II, 0.45f, 1.6f, 0.45f, 1.4f).chorus(0.22f, 0.32f).detune(2.5f)
               .body(I, body::Model::bowed, 0.53f, 0.59f, 1.00f).pw(II, 0.66f, 0.065f, 0.21f).eg(II, 0.56f, 1.4f, 0.36f, 1.2f).mix(0.15f)
               .body(I, body::Model::bowed, 0.64f, 0.62f, 1.00f).bodySize(I, 1.0f)
               .lpf(I, 6200.0f, 0.06f).lpf(II, 12000.0f, 0.035f).level(II, 0.65f, 0.32f).mix(0.13f)
               .contour(0.12f, -0.25f, 0.0f, -0.12f));
    add("Fifth Strings", "Strings", { "feet-layer" },
        "A softly entering harmonic fifth joins an 8' modeled bowed body. The fifth has a lighter spectral weight and remains subordinate through the release; sparse voicings reveal the open interval clearly.",
        strings().feet(II, f513).mix(0.28f).hpf(II, 200.0f).lpf(II, 5000.0f, 0.05f)
            .level(II, 0.85f, 0.48f).eg(II, 0.30f, 1.3f, 0.78f, 1.1f)
            .feg(II, 0.15f, 0.08f, 0.35f, 1.6f, 1.0f).chorus(0.30f, 0.25f).detune(1.2f).res(0.08f)
               .body(I, body::Model::bowed, 0.62f, 0.45f, 1.15f)
               .body(II, body::Model::bowed, 0.36f, 0.58f, 1.05f).mix(0.23f)
               .body(I, body::Model::bowed, 0.74f, 0.50f, 1.15f).bodySize(I, 1.18f)
               .body(II, body::Model::bowed, 0.46f, 0.64f, 1.05f).bodySize(II, 1.05f)
               .hpf(I, 50.0f).hpf(II, 280.0f).lpf(I, 5800.0f, 0.065f).lpf(II, 7600.0f, 0.04f)
               .eg(II, 0.26f, 1.15f, 0.70f, 0.85f).mix(0.20f).contour(0.08f, -0.17f, 0.01f, -0.08f));
    add("Analog Orchestra", "Strings", { "feet-layer", "chorus" },
        "A warm 16' circuit section and a lighter 4' upper section span two octaves. Staggered harmonic blooms and reduced chorus keep the orchestral synth texture broad while preserving low-register definition.",
        strings().feet(I, f16).feet(II, f4).mix(0.32f)
            .hpf(I, 40.0f).hpf(II, 200.0f).lpf(I, 2000.0f, 0.10f).lpf(II, 7000.0f, 0.06f)
            .eg(I, 0.25f, 1.4f, 0.90f, 1.2f).eg(II, 0.38f, 1.8f, 0.80f, 1.5f)
            .touch(I, 0.12f, 0.26f, 0.21f, 0.16f).touch(II, 0.07f, 0.22f, 0.16f, 0.12f)
            .chorus(0.48f, 0.25f).detune(4.0f).pan(I, -0.15f).pan(II, 0.15f)
               .feg(I, 0.19f, 0.21f, 0.32f, 1.9f, 1.05f).feg(II, 0.30f, 0.15f, 0.58f, 1.45f, 1.30f).chorus(0.34f, 0.21f).detune(3.2f)
               .hpf(I, 30.0f).hpf(II, 300.0f).lpf(I, 2200.0f, 0.085f).lpf(II, 6500.0f, 0.05f)
               .mix(0.28f).eg(II, 0.31f, 1.50f, 0.72f, 1.15f)
               .touch(I, 0.14f, 0.34f, 0.22f, 0.18f).touch(II, 0.10f, 0.28f, 0.16f, 0.14f)
               .contour(0.14f, -0.18f, 0.03f, -0.10f));
    add("Spiccato Strings", "Strings", { "percussive", "velocity", "dual-rank" },
        "Short modeled bow strokes return quickly to silence: a defined harmonic body follows an even shorter upper bite. Velocity changes the excitation as well as level; detached playing produces clean repeated strokes.",
        strings().wave(I, true, false, 0.04f).wave(II, false, true).pw(II, 0.68f)
            .hpf(I, 110.0f).hpf(II, 420.0f).lpf(I, 2800.0f, 0.12f).lpf(II, 5200.0f, 0.06f)
            .feg(I, 0.0f, 0.30f, 0.012f, 0.14f, 0.1f).feg(II, 0.0f, 0.35f, 0.006f, 0.12f, 0.08f)
            .eg(I, 0.012f, 0.48f, 0.0f, 0.14f).eg(II, 0.006f, 0.18f, 0.0f, 0.08f)
            .level(I, 0.95f, 0.176471f).level(II, 0.88f, 0.153846f)
            .touch(I, 0.22f, 1.0f, 0.0f, 0.0f).touch(II, 0.28f, 1.0f, 0.0f, 0.0f)
            .mix(0.18f).g(P_chorusOn, 0.0f).subAt(0.0f, 0.0f).g(P_modWheelSub, 0.0f)
            .pan(Both, 0.0f).detune(1.0f).g(P_drift, 0.02f).g(P_analogNoise, 0.002f)
               .body(I, body::Model::bowed, 1.0f, 0.62f, 0.65f)
               .body(II, body::Model::bowed, 0.42f, 0.81f, 0.50f).eg(I, 0.009f, 0.42f, 0.0f, 0.11f)
               .body(I, body::Model::bowed, 1.0f, 0.68f, 0.65f).bodySize(I, 1.05f)
               .body(II, body::Model::bowed, 0.55f, 0.85f, 0.50f).bodySize(II, 0.93f)
               .hpf(I, 75.0f).hpf(II, 650.0f).lpf(I, 6600.0f, 0.075f).lpf(II, 9800.0f, 0.04f)
               .eg(I, 0.0065f, 0.30f, 0.0f, 0.065f).eg(II, 0.003f, 0.11f, 0.0f, 0.04f)
               .level(I, 0.95f, 1.0f).level(II, 0.88f, 0.90f).mix(0.14f)
               .contour(0.10f, -0.20f, 0.03f, -0.11f));
    add("Bowed Swell Strings", "Strings", { "brilliance-swell", "aftertouch" },
        "A modeled bow body rises before the two harmonic ranges bloom over 1.1 and 1.5 seconds. Hold the chord to hear the brighter section emerge, then use pressure to increase bow intensity without losing the covered center.",
        strings().hpf(I, 65.0f).hpf(II, 150.0f).lpf(I, 2100.0f, 0.12f).lpf(II, 3600.0f, 0.08f)
            .feg(I, 0.45f, 0.20f, 1.1f, 2.2f, 1.4f).feg(II, 0.55f, 0.18f, 1.5f, 2.6f, 1.8f)
            .eg(I, 0.55f, 1.8f, 0.90f, 1.6f).eg(II, 0.80f, 2.2f, 0.82f, 2.0f)
            .touch(I, 0.06f, 0.22f, 0.25f, 0.22f).touch(II, 0.04f, 0.18f, 0.20f, 0.18f)
            .chorus(0.33f, 0.23f).detune(4.5f)
               .body(I, body::Model::bowed, 0.76f, 0.37f, 1.30f)
               .body(II, body::Model::bowed, 0.44f, 0.55f, 1.15f)
               .body(I, body::Model::bowed, 0.86f, 0.43f, 1.30f).bodySize(I, 1.30f)
               .body(II, body::Model::bowed, 0.56f, 0.62f, 1.15f).bodySize(II, 1.10f)
               .hpf(I, 40.0f).hpf(II, 180.0f).lpf(I, 4800.0f, 0.075f).lpf(II, 6500.0f, 0.05f)
               .contour(0.08f, -0.18f, 0.01f, -0.08f));
    add("Synth Strings Saw", "Strings", { "chorus" },
        "Two circuit saw sections bloom at different rates before settling into a covered synth-string sustain. Restrained chorus, a steadier tuning center and separate long releases keep the texture lush without a smeared attack.",
        strings().wave(Both, true, false).hpf(I, 110.0f).hpf(II, 150.0f)
            .lpf(I, 2600.0f, 0.08f).lpf(II, 3700.0f, 0.06f)
            .feg(I, 0.08f, 0.08f, 0.20f, 1.8f, 1.2f).feg(II, 0.12f, 0.06f, 0.28f, 2.0f, 1.4f)
            .eg(I, 0.18f, 1.3f, 0.91f, 1.3f).eg(II, 0.23f, 1.6f, 0.88f, 1.5f)
            .chorus(0.58f, 0.35f).detune(7.2f).pan(I, -0.12f).pan(II, 0.12f)
               .feg(I, 0.12f, 0.11f, 0.24f, 2.1f, 1.3f).feg(II, 0.20f, 0.14f, 0.47f, 1.55f, 1.6f).chorus(0.44f, 0.29f).detune(5.6f)
               .lpf(I, 2900.0f, 0.065f).lpf(II, 4500.0f, 0.045f)
               .eg(I, 0.14f, 1.30f, 0.92f, 1.10f).eg(II, 0.27f, 1.60f, 0.84f, 1.35f)
               .detune(4.6f).chorus(0.38f, 0.25f).contour(0.10f, -0.20f, -0.02f, -0.10f));
    add("String Machine", "Strings", { "chorus" },
        "Bright circuit saw sections use a fast ensemble chorus, steady sustain and very little touch dependence. A stronger lower foundation, quicker first rank and controlled detuning keep rhythmic chord changes immediate.",
        strings().wave(Both, true, false).hpf(I, 190.0f, 0.03f).hpf(II, 280.0f, 0.02f)
            .lpf(I, 5600.0f, 0.04f).lpf(II, 7200.0f, 0.03f).feg(Both, 0.0f, 0.0f, 0.01f, 0.5f, 0.4f)
            .eg(I, 0.07f, 0.7f, 0.96f, 0.75f).eg(II, 0.11f, 0.9f, 0.96f, 0.85f)
            .level(I, 0.92f, 0.72f).level(II, 0.88f, 0.66f).touch(Both, 0.0f, 0.0f, 0.08f, 0.10f)
            .subAt(0.0f, 0.0f).g(P_modWheelSub, 0.0f).chorus(0.80f, 0.60f).detune(3.0f).res(0.05f)
               .hpf(I, 150.0f, 0.025f).hpf(II, 240.0f, 0.02f).chorus(0.64f, 0.73f).detune(1.8f).eg(I, 0.047f, 0.7f, 0.97f, 0.66f).eg(II, 0.085f, 0.9f, 0.95f, 0.79f)
               .hpf(I, 105.0f, 0.02f).hpf(II, 180.0f, 0.015f)
               .lpf(I, 6400.0f, 0.035f).lpf(II, 8200.0f, 0.025f)
               .eg(I, 0.032f, 0.70f, 0.985f, 0.52f).eg(II, 0.055f, 0.90f, 0.97f, 0.65f)
               .chorus(0.58f, 0.68f).detune(1.2f).contour(0.12f, -0.22f, 0.02f, -0.10f));
    add("Contour Strings", "Strings", { "kbd-contour", "aftertouch" },
        "Modeled bow harmonics and a brighter circuit section share a keyboard contour. The low range stays soft while restrained treble brilliance and level keep wide voicings balanced across the keyboard.",
        strings().contour(-0.15f, 0.20f, -0.10f, -0.08f)
            .hpf(I, 65.0f).hpf(II, 150.0f).lpf(I, 2700.0f, 0.10f).lpf(II, 4800.0f, 0.06f)
            .eg(I, 0.18f, 1.1f, 0.88f, 0.95f).eg(II, 0.30f, 1.5f, 0.80f, 1.2f)
            .mix(0.36f).detune(4.2f)
               .body(I, body::Model::bowed, 0.64f, 0.43f, 1.15f)
               .body(II, body::Model::bowed, 0.38f, 0.61f, 1.00f).contour(-0.12f, 0.16f, -0.06f, -0.10f)
               .body(I, body::Model::bowed, 0.76f, 0.49f, 1.15f).bodySize(I, 1.30f)
               .body(II, body::Model::bowed, 0.48f, 0.66f, 1.00f).bodySize(II, 1.05f)
               .hpf(I, 40.0f).hpf(II, 160.0f).lpf(I, 5500.0f, 0.075f).lpf(II, 7400.0f, 0.045f)
               .mix(0.31f).contour(-0.05f, 0.10f, 0.02f, -0.13f));
    add("Ring Strings", "Strings", { "ring-mod" },
        "A quiet, slowly entering ring component adds level shimmer behind a harmonic-supported ensemble. The warmer center stays stable through the movement and long release, with no metallic fixed pitch imposed on the strings.",
        strings().ring(0.8f, 0.0f, 1.2f, 2.8f, 0.08f)
            .eg(I, 0.28f, 1.3f, 0.90f, 1.2f).eg(II, 0.45f, 1.7f, 0.84f, 1.5f)
            .lpf(I, 3000.0f, 0.08f).lpf(II, 4600.0f, 0.06f).chorus(0.24f, 0.24f).detune(3.0f)
               .body(I, body::Model::bowed, 0.43f, 0.48f, 1.20f).ring(0.8f, 0.0f, 1.65f, 3.2f, 0.065f).chorus(0.18f, 0.20f)
               .body(I, body::Model::bowed, 0.58f, 0.55f, 1.20f).bodySize(I, 1.22f)
               .hpf(I, 45.0f).hpf(II, 180.0f).lpf(I, 5400.0f, 0.065f).lpf(II, 7000.0f, 0.045f)
               .ring(0.8f, 0.0f, 1.40f, 3.20f, 0.05f).chorus(0.16f, 0.17f)
               .contour(0.10f, -0.19f, 0.01f, -0.08f));
    add("Velvet Strings", "Strings", { "chorus" },
        "A soft modeled bowed body carries low pulse overtones with very shallow movement. Dark harmonic weighting and separately shaped long releases make a smooth, warm tail without a bright layer hovering above it.",
        strings().wave(I, true, false, 0.06f).pw(II, 0.54f, 0.16f, 0.24f)
            .hpf(I, 40.0f).hpf(II, 95.0f).lpf(I, 1350.0f, 0.08f).lpf(II, 2100.0f, 0.05f)
            .feg(I, 0.12f, 0.08f, 0.40f, 1.7f, 1.6f).feg(II, 0.16f, 0.06f, 0.55f, 2.0f, 1.9f)
            .eg(I, 0.35f, 1.5f, 0.91f, 2.2f).eg(II, 0.50f, 1.9f, 0.86f, 2.65f)
            .level(II, 0.85f, 0.40f).touch(Both, 0.05f, 0.24f, 0.12f, 0.18f)
            .chorus(0.25f, 0.20f).detune(3.2f)
               .body(I, body::Model::bowed, 0.71f, 0.18f, 1.50f)
               .body(II, body::Model::bowed, 0.32f, 0.30f, 1.35f).pw(II, 0.55f, 0.09f, 0.19f).chorus(0.18f, 0.16f)
               .body(I, body::Model::bowed, 0.84f, 0.28f, 1.50f).bodySize(I, 1.55f)
               .body(II, body::Model::bowed, 0.44f, 0.40f, 1.35f).bodySize(II, 1.30f)
               .hpf(I, 25.0f).hpf(II, 90.0f).lpf(I, 3100.0f, 0.065f).lpf(II, 3900.0f, 0.045f)
               .eg(I, 0.28f, 1.50f, 0.92f, 1.70f).eg(II, 0.42f, 1.90f, 0.83f, 2.10f)
               .chorus(0.15f, 0.14f).contour(0.06f, -0.23f, 0.03f, -0.09f));
    add("Tutti Strings", "Strings", { "feet-layer", "pwm", "velocity" },
        "A strong 16' modeled bow body supports an 8' section with brighter, faster-speaking harmonics. Moderate chorus joins the sections while velocity gives articulated chords a clear opening weight.",
        strings().feet(I, f16).wave(I, true, false, 0.035f).wave(II, false, true).pw(II, 0.60f, 0.22f, 0.38f)
            .hpf(I, 35.0f).hpf(II, 120.0f).lpf(I, 2500.0f, 0.10f).lpf(II, 5000.0f, 0.07f)
            .eg(I, 0.095f, 0.9f, 0.90f, 0.8f).eg(II, 0.14f, 1.2f, 0.86f, 1.0f)
            .level(I, 0.95f, 0.75f).level(II, 0.90f, 0.62f)
            .touch(I, 0.14f, 0.25f, 0.16f, 0.10f).touch(II, 0.12f, 0.30f, 0.12f, 0.10f)
            .mix(0.44f).chorus(0.42f, 0.34f).detune(5.2f)
               .body(I, body::Model::bowed, 0.73f, 0.53f, 1.05f)
               .body(II, body::Model::bowed, 0.49f, 0.69f, 0.90f).chorus(0.30f, 0.28f).detune(3.9f)
               .body(I, body::Model::bowed, 0.84f, 0.58f, 1.05f).bodySize(I, 1.95f)
               .body(II, body::Model::bowed, 0.64f, 0.73f, 0.90f).bodySize(II, 1.10f)
               .hpf(I, 20.0f).hpf(II, 150.0f).lpf(I, 4800.0f, 0.075f).lpf(II, 7800.0f, 0.045f)
               .eg(I, 0.07f, 0.90f, 0.93f, 0.58f).eg(II, 0.095f, 1.00f, 0.86f, 0.74f)
               .touch(I, 0.13f, 0.32f, 0.17f, 0.14f).touch(II, 0.10f, 0.36f, 0.14f, 0.12f)
               .mix(0.37f).chorus(0.26f, 0.24f).detune(3.3f).contour(0.12f, -0.20f, 0.02f, -0.10f));
    add("Marcato Strings", "Strings", { "velocity", "dual-rank" },
        "A firm upper bow burst decays into a quieter modeled harmonic body. Velocity shapes the bite and its weight; the independent accent disappears during the hold, leaving a clear sustained bow.",
        strings().wave(II, true, false, 0.0f, 0.01f).hpf(I, 160.0f).hpf(II, 650.0f)
            .lpf(I, 3200.0f, 0.12f).lpf(II, 7000.0f, 0.08f)
            .feg(I, 0.06f, 0.22f, 0.026f, 0.32f, 0.24f).feg(II, 0.05f, 0.30f, 0.008f, 0.18f, 0.10f)
            .eg(I, 0.026f, 0.34f, 0.52f, 0.30f).eg(II, 0.006f, 0.25f, 0.0f, 0.10f)
            .level(I, 0.95f, 0.32f).level(II, 0.85f, 0.20f)
            .touch(I, 0.22f, 1.0f, 0.12f, 0.16f).touch(II, 0.32f, 0.95f, 0.06f, 0.08f)
            .mix(0.34f).chorus(0.14f, 0.35f).detune(2.0f).subAt(0.02f, 0.03f)
               .body(I, body::Model::bowed, 0.79f, 0.57f, 0.85f)
               .body(II, body::Model::bowed, 0.42f, 0.82f, 0.65f).eg(I, 0.020f, 0.31f, 0.54f, 0.25f)
               .body(I, body::Model::bowed, 0.90f, 0.60f, 0.85f).bodySize(I, 1.10f)
               .body(II, body::Model::bowed, 0.55f, 0.86f, 0.65f).bodySize(II, 1.0f)
               .hpf(I, 70.0f).hpf(II, 1100.0f).lpf(I, 6500.0f, 0.075f).lpf(II, 10000.0f, 0.045f)
               .eg(I, 0.014f, 0.25f, 0.60f, 0.18f).eg(II, 0.004f, 0.13f, 0.0f, 0.055f)
               .level(I, 0.95f, 0.37f).level(II, 0.85f, 0.24f).mix(0.22f)
               .contour(0.08f, -0.18f, 0.02f, -0.10f));
    add("Air Strings", "Strings", { "noise", "dual-rank", "aftertouch" },
        "A modeled bowed body holds beneath a separate filtered friction layer. The air opens quickly and fades to a restrained breath, while pressure supports the pitched body and the release stays clean.",
        strings().wave(I, true, false).wave(II, false, false, 0.0f, 0.55f)
            .hpf(I, 100.0f).hpf(II, 1700.0f).lpf(I, 3000.0f, 0.10f).lpf(II, 6500.0f, 0.04f)
            .feg(II, 0.0f, 0.12f, 0.025f, 0.32f, 0.20f)
            .eg(I, 0.12f, 1.0f, 0.88f, 0.8f).eg(II, 0.045f, 0.60f, 0.20f, 0.35f)
            .level(II, 0.50f, 0.22f).touch(II, 0.06f, 0.35f, 0.0f, 0.12f)
            .mix(0.14f).chorus(0.18f, 0.30f).detune(0.0f).g(P_drift, 0.04f).g(P_analogNoise, 0.004f)
               .body(I, body::Model::bowed, 0.78f, 0.47f, 1.15f).eg(II, 0.032f, 0.48f, 0.14f, 0.27f).hpf(II, 1450.0f)
               .body(I, body::Model::bowed, 0.90f, 0.54f, 1.15f).bodySize(I, 1.20f)
               .hpf(I, 45.0f).lpf(I, 5800.0f, 0.075f)
               .wave(II, false, false, 0.0f, 0.34f).hpf(II, 1900.0f).lpf(II, 7200.0f, 0.035f)
               .eg(II, 0.024f, 0.30f, 0.09f, 0.18f).mix(0.11f)
               .contour(0.08f, -0.20f, 0.02f, -0.08f));
}
