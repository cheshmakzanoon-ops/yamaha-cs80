// Brass voicings. Included inside namespace curated by CuratedLibrary.inc.
// Existing program names and insertion order are stable host/MIDI identities.
// These are programmed synthesizer interpretations, not sampled instruments.
// Cutoffs are referenced to MIDI 60. Low HP settings leave room for the
// upward HP movement caused by initial/after-touch brilliance. The two ranks
// deliberately have different spectral and articulation roles.

Program brass()
{
    Program x;
    x.wave(I, true, false, 0.035f).wave(II, true, false)
     .hpf(I, 42.0f).hpf(II, 68.0f).lpf(I, 760.0f, 0.18f).lpf(II, 1120.0f, 0.14f)
     .feg(I, 0.24f, 0.52f, 0.12f, 1.7f, 0.38f).feg(II, 0.18f, 0.40f, 0.085f, 1.3f, 0.30f)
     .eg(I, 0.035f, 1.8f, 0.82f, 0.42f).eg(II, 0.045f, 1.4f, 0.76f, 0.34f)
     .touch(I, 0.20f, 0.58f, 0.32f, 0.25f).touch(II, 0.26f, 0.62f, 0.40f, 0.22f)
     .level(I, 1.0f, 0.48f).level(II, 0.94f, 0.40f)
     .sub(subSine, 5.1f, 0.0f).subAt(0.04f, 0.065f)
     .g(P_modWheelSub, 0.055f).g(P_voiceDispersion, 0.18f)
     .g(P_drift, 0.08f).g(P_analogNoise, 0.012f)
     .mix(0.43f).res(0.16f).detune(3.2f).volume(0.6f);
    return x;
}

void buildBrass(Builder& add)
{
    add("Brass Swell I", "Brass", { "brilliance-swell", "aftertouch", "dual-rank" },
        "A rounded body arrives first, then a brighter second rank blooms over roughly half a second. Pressure opens the held chord; the wheel adds restrained vibrato.",
        brass().lpf(I, 720.0f, 0.19f).lpf(II, 1080.0f, 0.16f)
               .feg(I, 0.32f, 0.62f, 0.52f, 2.4f, 0.65f).feg(II, 0.40f, 0.48f, 0.67f, 2.0f, 0.52f)
               .eg(I, 0.075f, 2.2f, 0.86f, 0.82f).eg(II, 0.14f, 1.9f, 0.79f, 0.68f)
               .touch(I, 0.16f, 0.52f, 0.42f, 0.28f).touch(II, 0.22f, 0.58f, 0.52f, 0.24f)
               .mix(0.45f).detune(4.8f));
    add("Brass Swell II", "Brass", { "brilliance-swell", "aftertouch", "feet-layer" },
        "A 16' foundation supports an 8' brass crown. Their staggered filter rises give a broad low swell; pressure brings forward the upper rank.",
        brass().feet(I, f16).wave(I, true, false, 0.06f)
               .hpf(I, 26.0f).hpf(II, 58.0f).lpf(I, 580.0f, 0.18f).lpf(II, 1080.0f, 0.22f)
               .feg(I, 0.30f, 0.55f, 0.68f, 2.7f, 0.75f).feg(II, 0.42f, 0.64f, 0.88f, 2.1f, 0.70f)
               .eg(I, 0.095f, 2.1f, 0.88f, 0.85f).eg(II, 0.18f, 1.8f, 0.78f, 0.7f)
               .touch(II, 0.20f, 0.60f, 0.48f, 0.32f).mix(0.38f).detune(3.7f).res(0.22f));
    add("Pressure Swell Brass", "Brass", { "aftertouch", "brilliance-swell" },
        "A quiet, dark chord waits under the fingers. Pressure supplies most of its level and brightness; releasing pressure brings it back to the soft bed.",
        brass().lpf(I, 590.0f, 0.15f).lpf(II, 870.0f, 0.18f).hpf(I, 28.0f).hpf(II, 42.0f)
               .feg(I, 0.05f, 0.08f, 0.18f, 1.5f, 0.5f).feg(II, 0.06f, 0.10f, 0.23f, 1.2f, 0.45f)
               .eg(I, 0.09f, 1.8f, 0.90f, 0.75f).eg(II, 0.13f, 1.5f, 0.85f, 0.65f)
               .level(I, 1.0f, 0.12f).level(II, 1.0f, 0.09f)
               .touch(I, 0.03f, 0.05f, 0.62f, 0.90f).touch(II, 0.05f, 0.07f, 0.76f, 0.94f)
               .subAt(0.025f, 0.04f).detune(4.0f));
    add("Resonant Swell Brass", "Brass", { "brilliance-swell", "resonance" },
        "A focused resonant edge rises above a warmer body, then settles into a sustained brass chord. The two filter peaks arrive at different times.",
        brass().lpf(I, 610.0f, 0.24f).lpf(II, 850.0f, 0.46f).res(0.36f)
               .feg(I, 0.30f, 0.54f, 0.38f, 2.3f, 0.7f).feg(II, 0.44f, 0.68f, 0.57f, 1.6f, 0.58f)
               .eg(I, 0.055f, 1.8f, 0.84f, 0.72f).eg(II, 0.11f, 1.5f, 0.67f, 0.60f)
               .touch(II, 0.18f, 0.55f, 0.28f, 0.16f).mix(0.32f).detune(3.5f));
    add("Noir Swell Brass", "Brass", { "brilliance-swell", "aftertouch", "chorus", "sub-mod" },
        "A wide, slow brass bloom with a dark center and a luminous upper rank. Pressure adds brightness and gentle vibrato; the long release supports spacious chords.",
        brass().detune(7.5f).chorus(0.42f, 0.32f).lpf(I, 620.0f, 0.18f).lpf(II, 1180.0f, 0.12f)
               .feg(I, 0.36f, 0.61f, 0.45f, 2.8f, 1.2f).feg(II, 0.48f, 0.50f, 0.72f, 2.1f, 1.0f)
               .eg(I, 0.14f, 2.4f, 0.86f, 1.65f).eg(II, 0.25f, 2.0f, 0.78f, 1.35f)
               .touch(I, 0.12f, 0.46f, 0.46f, 0.32f).touch(II, 0.18f, 0.52f, 0.60f, 0.28f)
               .sub(subSine, 5.0f, 0.0f).subAt(0.06f, 0.105f).bend(0.08f).mix(0.46f)
               .pan(I, -0.24f).pan(II, 0.34f));
    add("Sforzando Brass", "Brass", { "brilliance-swell", "velocity" },
        "A bright, hard-struck accent quickly relaxes into a quieter sustained section. The upper rank's bite decays completely while the lower rank holds the body; velocity controls the emphasis.",
        brass().lpf(I, 820.0f, 0.16f).lpf(II, 1280.0f, 0.24f)
               .feg(I, 0.04f, 0.64f, 0.03f, 0.55f, 0.30f).feg(II, 0.0f, 0.76f, 0.006f, 0.36f, 0.14f)
               .eg(I, 0.014f, 0.58f, 0.42f, 0.38f).eg(II, 0.003f, 0.65f, 0.0f, 0.16f)
               .level(I, 1.0f, 0.38f).level(II, 1.0f, 0.12f)
               .touch(I, 0.25f, 0.72f, 0.20f, 0.20f).touch(II, 0.38f, 0.98f, 0.22f, 0.12f)
               .mix(0.42f).detune(2.8f));
    add("Brass Stab", "Brass", { "velocity", "percussive" },
        "A short, dry brass hit with a rounded low attack and a brighter upper bite. Both ranks decay away even while the key remains held; velocity controls the accent.",
        brass().lpf(I, 1040.0f, 0.16f).lpf(II, 1720.0f, 0.19f)
               .feg(I, 0.0f, 0.56f, 0.012f, 0.58f, 0.12f).feg(II, 0.0f, 0.65f, 0.007f, 0.31f, 0.09f)
               .eg(I, 0.008f, 0.95f, 0.0f, 0.18f).eg(II, 0.005f, 0.58f, 0.0f, 0.12f)
               .level(I, 1.0f, 0.28f).level(II, 1.0f, 0.20f)
               .touch(I, 0.30f, 0.80f, 0.0f, 0.0f).touch(II, 0.40f, 0.95f, 0.0f, 0.0f)
               .subAt(0.0f, 0.0f).mix(0.37f).detune(2.2f));
    add("Horn Section", "Brass", { "dual-rank", "velocity" },
        "A mellow saw body and softer pulse edge form a compact horn section. Slightly staggered attacks avoid a hard synth click; pressure gives a modest brass bloom.",
        brass().wave(I, true, false, 0.07f).wave(II, false, true).pw(II, 0.56f)
               .hpf(I, 32.0f).hpf(II, 62.0f).lpf(I, 610.0f, 0.11f).lpf(II, 1350.0f, 0.13f)
               .feg(I, 0.18f, 0.34f, 0.11f, 1.7f, 0.30f).feg(II, 0.22f, 0.43f, 0.075f, 1.2f, 0.25f)
               .eg(I, 0.065f, 1.7f, 0.87f, 0.43f).eg(II, 0.045f, 1.4f, 0.73f, 0.34f)
               .touch(I, 0.15f, 0.62f, 0.25f, 0.24f).touch(II, 0.22f, 0.65f, 0.34f, 0.18f)
               .mix(0.26f).detune(2.1f).res(0.10f));
    add("Covered Horn", "Brass", { "aftertouch" },
        "Dark, rounded horn phrasing with a small sine foundation and a quiet upper edge. The stable body reduces ensemble beating; pressure shapes level more strongly than brightness. Use the middle and lower registers.",
        brass().wave(I, true, false, 0.12f).wave(II, false, true).pw(II, 0.53f)
               .hpf(I, 25.0f).hpf(II, 220.0f).lpf(I, 470.0f, 0.08f).lpf(II, 920.0f, 0.16f)
               .feg(I, 0.15f, 0.28f, 0.13f, 1.7f, 0.35f).feg(II, 0.22f, 0.35f, 0.16f, 1.2f, 0.30f)
               .eg(I, 0.085f, 1.8f, 0.90f, 0.46f).eg(II, 0.10f, 1.3f, 0.69f, 0.34f)
               .touch(I, 0.10f, 0.52f, 0.18f, 0.42f).touch(II, 0.14f, 0.56f, 0.26f, 0.26f)
               .mix(0.15f).detune(0.0f).res(0.08f).subAt(0.03f, 0.055f)
               .g(P_voiceDispersion, 0.06f).g(P_drift, 0.02f));
    add("Muted Trumpet", "Brass", { "aftertouch", "dual-rank" },
        "A dry, narrow brass body with a focused nasal pulse edge, voiced as a muted-trumpet interpretation. Firm strikes sharpen the attack; pressure gives a small expressive vibrato.",
        brass().wave(I, true, false).wave(II, false, true).pw(II, 0.78f)
               .hpf(I, 280.0f, 0.12f).hpf(II, 680.0f, 0.22f).lpf(I, 1520.0f, 0.22f).lpf(II, 2460.0f, 0.30f)
               .feg(I, 0.10f, 0.26f, 0.034f, 0.9f, 0.20f).feg(II, 0.16f, 0.34f, 0.020f, 0.55f, 0.16f)
               .eg(I, 0.020f, 1.3f, 0.86f, 0.24f).eg(II, 0.012f, 0.8f, 0.62f, 0.18f)
               .level(I, 1.0f, 0.42f).level(II, 0.82f, 0.32f)
               .touch(I, 0.14f, 0.64f, 0.16f, 0.30f).touch(II, 0.22f, 0.74f, 0.22f, 0.20f)
               .mix(0.32f).res(0.20f).detune(0.0f).subAt(0.04f, 0.08f)
               .g(P_voiceDispersion, 0.08f).g(P_drift, 0.025f));
    add("Trombone Glide", "Brass", { "glide", "init-bend" },
        "A low 16' brass voice with a gentle upward lip scoop and portamento for slide-like phrases. A quieter edge rank follows the body without chorus.",
        brass().feet(Both, f16).wave(I, true, false, 0.06f).wave(II, true, false)
               .hpf(I, 24.0f).hpf(II, 45.0f).lpf(I, 690.0f, 0.12f).lpf(II, 1320.0f, 0.16f)
               .feg(I, 0.18f, 0.42f, 0.09f, 1.6f, 0.25f).feg(II, 0.24f, 0.48f, 0.065f, 1.0f, 0.22f)
               .eg(I, 0.045f, 1.6f, 0.88f, 0.32f).eg(II, 0.03f, 1.2f, 0.72f, 0.24f)
               .glide(0.105f).bend(0.14f).mix(0.27f).detune(0.7f).res(0.12f)
               .subAt(0.035f, 0.075f).g(P_drift, 0.035f));
    add("Fanfare Brass", "Brass", { "feet-layer", "velocity" },
        "An assertive 8' section with a shorter 4' crown. The octave brightens the opening accent without dominating the held chord; firm strikes bring out the fanfare bite.",
        brass().feet(II, f4).hpf(I, 46.0f).hpf(II, 150.0f).lpf(I, 1380.0f, 0.15f).lpf(II, 2900.0f, 0.16f)
               .feg(I, 0.06f, 0.44f, 0.043f, 1.3f, 0.25f).feg(II, 0.02f, 0.48f, 0.025f, 0.72f, 0.20f)
               .eg(I, 0.024f, 1.3f, 0.84f, 0.34f).eg(II, 0.016f, 0.9f, 0.53f, 0.24f)
               .touch(I, 0.22f, 0.68f, 0.22f, 0.20f).touch(II, 0.30f, 0.80f, 0.28f, 0.16f)
               .mix(0.28f).detune(1.5f).res(0.14f));
    add("Brass Ensemble 16", "Brass", { "feet-layer", "chorus" },
        "A weighty 16' body under an 8' section, spread gently in stereo beneath restrained chorus. The upper rank enters a little later and stays lighter than the foundation.",
        brass().feet(I, f16).wave(I, true, false, 0.08f)
               .hpf(I, 25.0f).hpf(II, 72.0f).lpf(I, 640.0f, 0.13f).lpf(II, 1480.0f, 0.15f)
               .feg(I, 0.22f, 0.44f, 0.20f, 2.2f, 0.55f).feg(II, 0.30f, 0.50f, 0.29f, 1.7f, 0.48f)
               .eg(I, 0.07f, 2.1f, 0.88f, 0.72f).eg(II, 0.105f, 1.7f, 0.75f, 0.58f)
               .chorus(0.34f, 0.29f).detune(4.5f).mix(0.34f)
               .pan(I, -0.12f).pan(II, 0.38f));
    add("Low Brass", "Brass", { "feet-layer", "dual-rank" },
        "Two 16' ranks combine a rounded saw foundation with a darker pulse edge. A slower low-body attack gives weight without the constant wobble of an ensemble effect.",
        brass().feet(Both, f16).wave(I, true, false, 0.10f).wave(II, false, true).pw(II, 0.58f)
               .hpf(I, 22.0f).hpf(II, 35.0f).lpf(I, 440.0f, 0.10f).lpf(II, 820.0f, 0.17f)
               .feg(I, 0.16f, 0.36f, 0.115f, 1.8f, 0.32f).feg(II, 0.20f, 0.43f, 0.075f, 1.2f, 0.25f)
               .eg(I, 0.065f, 1.9f, 0.91f, 0.45f).eg(II, 0.044f, 1.5f, 0.74f, 0.32f)
               .touch(I, 0.15f, 0.62f, 0.22f, 0.28f).touch(II, 0.22f, 0.68f, 0.31f, 0.20f)
               .mix(0.27f).detune(1.4f).res(0.10f));
    add("Velvet Brass", "Brass", { "chorus", "aftertouch" },
        "A soft brass pad with a rounded sine-supported center and a delayed, dark pulse halo. Gentle pressure adds warmth and level; the chorus softens the edges.",
        brass().wave(I, true, false, 0.10f).wave(II, false, true).pw(II, 0.54f, 0.08f, 0.22f)
               .hpf(I, 30.0f).hpf(II, 62.0f).lpf(I, 510.0f, 0.08f).lpf(II, 840.0f, 0.11f)
               .feg(I, 0.22f, 0.32f, 0.36f, 2.6f, 1.0f).feg(II, 0.30f, 0.38f, 0.58f, 2.0f, 0.85f)
               .eg(I, 0.26f, 2.3f, 0.90f, 1.4f).eg(II, 0.42f, 1.9f, 0.80f, 1.1f)
               .touch(I, 0.08f, 0.42f, 0.22f, 0.34f).touch(II, 0.12f, 0.48f, 0.30f, 0.26f)
               .mix(0.34f).chorus(0.50f, 0.28f).detune(4.2f).res(0.08f));
    add("Bright Brass Poly", "Brass", { "velocity" },
        "A fast, bright brass comping patch. The body sustains while a more percussive upper edge relaxes after each chord; short releases keep rhythmic changes clear.",
        brass().lpf(I, 1680.0f, 0.12f).lpf(II, 2650.0f, 0.17f)
               .feg(I, 0.05f, 0.35f, 0.026f, 1.05f, 0.18f).feg(II, 0.0f, 0.43f, 0.014f, 0.62f, 0.14f)
               .eg(I, 0.012f, 1.1f, 0.76f, 0.25f).eg(II, 0.008f, 0.7f, 0.47f, 0.18f)
               .level(I, 1.0f, 0.38f).level(II, 0.92f, 0.29f)
               .touch(I, 0.22f, 0.70f, 0.18f, 0.16f).touch(II, 0.30f, 0.82f, 0.22f, 0.12f)
               .mix(0.36f).detune(2.6f).res(0.12f));
    add("Brass Pad Swell", "Brass", { "brilliance-swell", "chorus" },
        "A chord pad whose rounded body rises over a second before a brighter rank completes the bloom. Hold notes to hear the full swell and overlapping release.",
        brass().lpf(I, 660.0f, 0.12f).lpf(II, 1190.0f, 0.14f)
               .feg(I, 0.38f, 0.50f, 1.15f, 3.4f, 1.4f).feg(II, 0.50f, 0.58f, 1.55f, 2.8f, 1.2f)
               .eg(I, 0.95f, 3.0f, 0.91f, 2.0f).eg(II, 1.25f, 2.5f, 0.83f, 1.7f)
               .touch(I, 0.06f, 0.38f, 0.32f, 0.32f).touch(II, 0.10f, 0.44f, 0.40f, 0.28f)
               .chorus(0.38f, 0.24f).detune(5.8f).mix(0.44f));
    add("Fifth Brass", "Brass", { "feet-layer" },
        "An 8' brass body with a quieter 5 1/3' rank at a harmonic fifth. The fifth has its own brighter filter and softer sustain, so the played root remains clear.",
        brass().feet(II, f513).hpf(II, 98.0f).lpf(I, 790.0f, 0.15f).lpf(II, 1720.0f, 0.13f)
               .feg(I, 0.21f, 0.46f, 0.13f, 1.9f, 0.42f).feg(II, 0.28f, 0.40f, 0.18f, 1.4f, 0.34f)
               .eg(I, 0.038f, 1.8f, 0.86f, 0.52f).eg(II, 0.065f, 1.4f, 0.64f, 0.40f)
               .mix(0.28f).detune(0.0f).res(0.13f));
    add("Wah Brass", "Brass", { "aftertouch", "sub-mod" },
        "A stable brass body acquires a rhythmic filter wah as pressure feeds the sine SUB into the VCF. The wheel adds a small independent vibrato.",
        brass().hpf(I, 28.0f).hpf(II, 48.0f).lpf(I, 820.0f, 0.26f).lpf(II, 1360.0f, 0.31f)
               .feg(I, 0.12f, 0.25f, 0.09f, 1.4f, 0.35f).feg(II, 0.16f, 0.30f, 0.12f, 1.1f, 0.28f)
               .touch(I, 0.16f, 0.56f, 0.12f, 0.24f).touch(II, 0.20f, 0.62f, 0.16f, 0.20f)
               .sub(subSine, 3.9f, 0.0f, 0.0f).subAt(0.10f, 0.0f, 0.36f)
               .res(0.24f).detune(2.4f).mix(0.40f));
    add("Synth Horns", "Brass", { "pwm", "dual-rank" },
        "A firm saw body beneath a saw-and-pulse upper rank. Slow, shallow pulse-width movement supplies thickness without turning every note into a chorus wash.",
        brass().wave(I, true, false, 0.025f).wave(II, true, true).pw(II, 0.59f, 0.18f, 0.28f)
               .lpf(I, 870.0f, 0.17f).lpf(II, 1510.0f, 0.16f)
               .feg(I, 0.20f, 0.47f, 0.085f, 1.6f, 0.35f).feg(II, 0.26f, 0.40f, 0.13f, 1.2f, 0.30f)
               .eg(I, 0.028f, 1.6f, 0.84f, 0.44f).eg(II, 0.05f, 1.3f, 0.71f, 0.35f)
               .level(II, 0.76f, 0.38f).mix(0.35f).detune(5.2f));
    add("Brass Bite", "Brass", { "velocity", "resonance" },
        "Soft strikes remain rounded; hard strikes raise a focused resonant bite and a stronger upper-rank accent. Pressure adds a smaller bloom after the attack.",
        brass().hpf(I, 28.0f).hpf(II, 45.0f).lpf(I, 630.0f, 0.28f).lpf(II, 1010.0f, 0.40f)
               .feg(I, 0.08f, 0.42f, 0.065f, 1.25f, 0.28f).feg(II, 0.05f, 0.56f, 0.036f, 0.75f, 0.22f)
               .eg(I, 0.026f, 1.4f, 0.78f, 0.36f).eg(II, 0.015f, 0.9f, 0.46f, 0.25f)
               .level(I, 1.0f, 0.27f).level(II, 0.92f, 0.18f)
               .touch(I, 0.42f, 0.84f, 0.20f, 0.18f).touch(II, 0.60f, 0.98f, 0.25f, 0.12f)
               .res(0.30f).mix(0.35f).detune(2.0f));
    add("Distant Brass", "Brass", { "chorus" },
        "A softened brass ensemble with a dark center, slow entrances and a long, diffuse release. Deep chorus supplies distance; the upper edge stays deliberately subdued.",
        brass().wave(I, true, false, 0.06f).lpf(I, 390.0f, 0.07f).lpf(II, 670.0f, 0.09f)
               .hpf(I, 32.0f).hpf(II, 80.0f)
               .feg(I, 0.24f, 0.30f, 0.34f, 2.7f, 1.4f).feg(II, 0.33f, 0.34f, 0.52f, 2.1f, 1.2f)
               .eg(I, 0.23f, 2.6f, 0.87f, 2.2f).eg(II, 0.37f, 2.1f, 0.69f, 1.9f)
               .touch(I, 0.07f, 0.43f, 0.18f, 0.28f).touch(II, 0.10f, 0.48f, 0.23f, 0.22f)
               .chorus(0.65f, 0.25f).mix(0.30f).detune(5.5f).res(0.07f));
    add("Brass Crescendo", "Brass", { "brilliance-swell", "aftertouch" },
        "The body takes 1.6 seconds to reach full amplitude while the upper rank blooms more slowly. Hold the chord through the rise, then use pressure to sustain the intensity.",
        brass().hpf(I, 30.0f).hpf(II, 54.0f).lpf(I, 710.0f, 0.14f).lpf(II, 1290.0f, 0.18f)
               .eg(I, 1.6f, 3.2f, 0.95f, 1.2f).eg(II, 1.95f, 2.6f, 0.85f, 1.0f)
               .feg(I, 0.46f, 0.57f, 1.7f, 3.3f, 0.9f).feg(II, 0.58f, 0.65f, 2.05f, 2.7f, 0.8f)
               .level(I, 1.0f, 0.66f).level(II, 0.96f, 0.58f)
               .touch(I, 0.03f, 0.22f, 0.38f, 0.38f).touch(II, 0.06f, 0.28f, 0.48f, 0.32f)
               .mix(0.42f).detune(3.8f));
    add("Cuivre Brass", "Brass", { "aftertouch", "resonance" },
        "A rounded low-pressure brass body gains an assertive, resonant upper edge as pressure rises. The body keeps its fundamental while the second rank supplies the overblown character.",
        brass().wave(I, true, false, 0.07f).wave(II, false, true).pw(II, 0.71f)
               .hpf(I, 32.0f).hpf(II, 155.0f, 0.12f).lpf(I, 760.0f, 0.18f).lpf(II, 1590.0f, 0.39f)
               .feg(I, 0.12f, 0.33f, 0.085f, 1.6f, 0.32f).feg(II, 0.20f, 0.42f, 0.052f, 1.0f, 0.25f)
               .eg(I, 0.036f, 1.8f, 0.85f, 0.42f).eg(II, 0.025f, 1.2f, 0.68f, 0.30f)
               .level(II, 0.88f, 0.16f).touch(I, 0.16f, 0.58f, 0.24f, 0.28f)
               .touch(II, 0.27f, 0.42f, 0.62f, 0.58f).mix(0.38f).res(0.24f).detune(1.0f));
    add("Tuba", "Brass", { "velocity", "aftertouch" },
        "A single dry 16' rank combines a dark saw with a round sine foundation. A gentle tongue-like onset, stable pitch and short breath release suit low, separated phrases.",
        brass().mix(0.0f).feet(I, f16).wave(I, true, false, 0.22f)
               .hpf(I, 20.0f).lpf(I, 420.0f, 0.10f)
               .feg(I, 0.16f, 0.34f, 0.095f, 1.5f, 0.23f).eg(I, 0.055f, 1.8f, 0.90f, 0.34f)
               .level(I, 1.0f, 0.42f).touch(I, 0.19f, 0.68f, 0.24f, 0.30f)
               .subAt(0.02f, 0.035f).g(P_modWheelSub, 0.035f)
               .g(P_voiceDispersion, 0.06f).g(P_drift, 0.02f).detune(0.0f).res(0.08f));
    add("Brass Octaves", "Brass", { "feet-layer" },
        "Tight 8' and 4' brass ranks with no programmed detune. Matched sustained articulation keeps the octave present throughout the note, with the upper filter opened for its register.",
        brass().feet(II, f4).hpf(I, 40.0f).hpf(II, 100.0f).lpf(I, 920.0f, 0.14f).lpf(II, 2200.0f, 0.12f)
               .feg(I, 0.22f, 0.44f, 0.15f, 1.8f, 0.4f).feg(II, 0.22f, 0.39f, 0.15f, 1.8f, 0.4f)
               .eg(I, 0.04f, 1.7f, 0.86f, 0.48f).eg(II, 0.04f, 1.7f, 0.82f, 0.48f)
               .mix(0.34f).detune(0.0f).g(P_voiceDispersion, 0.08f).g(P_drift, 0.025f));
    add("Swell And Fall", "Brass", { "brilliance-swell" },
        "A warm swell becomes dark immediately after note-off, while the amplitude takes longer to disappear. The faster filter release makes the falling color audible inside the tail.",
        brass().lpf(I, 870.0f, 0.18f).lpf(II, 1530.0f, 0.20f)
               .feg(I, 0.78f, 0.53f, 0.30f, 2.0f, 0.65f).feg(II, 0.90f, 0.58f, 0.43f, 1.6f, 0.48f)
               .eg(I, 0.065f, 1.8f, 0.86f, 2.6f).eg(II, 0.10f, 1.5f, 0.76f, 2.1f)
               .mix(0.40f).detune(4.2f));
    add("Reed Brass", "Brass", { "dual-rank", "aftertouch" },
        "A narrow pulse carries a reedy upper edge above a warmer saw body. Distinct filter shapes and a small pressure vibrato make single-line phrases more vocal.",
        brass().wave(I, true, false, 0.04f).wave(II, false, true).pw(II, 0.69f)
               .hpf(I, 45.0f).hpf(II, 170.0f, 0.08f).lpf(I, 980.0f, 0.18f).lpf(II, 2080.0f, 0.30f)
               .feg(I, 0.12f, 0.34f, 0.060f, 1.5f, 0.25f).feg(II, 0.18f, 0.39f, 0.038f, 0.9f, 0.20f)
               .eg(I, 0.028f, 1.6f, 0.86f, 0.32f).eg(II, 0.018f, 1.1f, 0.70f, 0.25f)
               .touch(I, 0.17f, 0.62f, 0.26f, 0.28f).touch(II, 0.25f, 0.70f, 0.35f, 0.22f)
               .subAt(0.05f, 0.095f).mix(0.36f).detune(0.8f).res(0.18f));
    add("Brass Glide Lead", "Brass", { "glide", "aftertouch" },
        "A focused brass lead with a small upward scoop, smooth portamento and restrained pressure vibrato. Its softer second rank reinforces the line without becoming a wide section.",
        brass().wave(II, false, true).pw(II, 0.60f).lpf(I, 1080.0f, 0.19f).lpf(II, 1770.0f, 0.16f)
               .feg(I, 0.15f, 0.42f, 0.075f, 1.5f, 0.3f).feg(II, 0.22f, 0.36f, 0.11f, 1.1f, 0.24f)
               .eg(I, 0.022f, 1.5f, 0.88f, 0.35f).eg(II, 0.038f, 1.2f, 0.74f, 0.28f)
               .touch(I, 0.18f, 0.62f, 0.38f, 0.30f).touch(II, 0.23f, 0.65f, 0.46f, 0.24f)
               .glide(0.075f).bend(0.075f).subAt(0.045f, 0.10f).mix(0.25f).detune(1.3f));
    add("Tremolo Brass", "Brass", { "tremolo", "sub-mod" },
        "A soft brass pad pulses gently at 4.6 Hz through the original SUB VCA route, with stereo color from the CHORUS/TREMOLO section. The slower second-rank entrance and sustained body keep the motion rounded rather than percussive.",
        brass().lpf(I, 690.0f, 0.11f).lpf(II, 1180.0f, 0.13f)
               .feg(I, 0.23f, 0.38f, 0.25f, 2.1f, 0.65f).feg(II, 0.30f, 0.44f, 0.39f, 1.7f, 0.52f)
               .eg(I, 0.16f, 2.0f, 0.91f, 1.0f).eg(II, 0.25f, 1.6f, 0.82f, 0.82f)
               .tremolo(0.46f, 4.6f).sub(subSine, 4.6f, 0.0f, 0.0f, 0.20f)
               .subAt(0.0f, 0.035f).detune(3.7f).mix(0.39f));
    add("Contour Brass", "Brass", { "kbd-contour" },
        "A broad keyboard brass voice: a modest treble-brilliance contour and low-register level lift help wide voicings retain their shape. The upper rank stays lighter than the root.",
        brass().hpf(I, 32.0f).hpf(II, 62.0f).lpf(I, 700.0f, 0.15f).lpf(II, 1210.0f, 0.16f)
               .feg(I, 0.18f, 0.40f, 0.14f, 1.8f, 0.38f).feg(II, 0.26f, 0.46f, 0.19f, 1.4f, 0.31f)
               .eg(I, 0.045f, 1.8f, 0.87f, 0.5f).eg(II, 0.065f, 1.4f, 0.74f, 0.4f)
               .contour(0.0f, 0.20f, 0.14f, 0.0f).mix(0.34f).detune(2.8f));
    add("Ring Breath Brass", "Brass", { "ring-mod" },
        "A slow, shallow ring-modulator beat adds breathing motion underneath a soft brass chord. The warmer body and slower upper-rank entrance keep the sound smooth.",
        brass().lpf(I, 720.0f, 0.13f).lpf(II, 1280.0f, 0.16f)
               .feg(I, 0.25f, 0.42f, 0.22f, 2.2f, 0.65f).feg(II, 0.34f, 0.48f, 0.34f, 1.7f, 0.55f)
               .eg(I, 0.095f, 2.0f, 0.88f, 0.95f).eg(II, 0.15f, 1.6f, 0.78f, 0.76f)
               .ring(2.4f, 0.0f, 0.45f, 1.7f, 0.10f).subAt(0.02f, 0.045f)
               .mix(0.41f).detune(3.0f));
}
