// Strings voicings. Included inside namespace curated by CuratedLibrary.inc.
// Existing program names and insertion order are stable host/MIDI identities.
// These are synthesized string articulations, not sampled acoustic instruments.

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
        "A bowed saw body arrives before a quieter PWM section. Gentle chorus joins the ranks; pressure adds bow weight and a restrained vibrato.",
        strings());
    add("Slow Strings", "Strings", { "chorus", "brilliance-swell" },
        "The body rises over 0.85 seconds and the upper section follows over 1.15 seconds. Hold chords through the opening; the two releases overlap softly.",
        strings().eg(I, 0.85f, 2.0f, 0.90f, 1.7f).eg(II, 1.15f, 2.4f, 0.84f, 2.1f)
            .feg(I, 0.24f, 0.18f, 0.9f, 2.4f, 1.6f).feg(II, 0.32f, 0.12f, 1.25f, 2.8f, 1.9f)
            .lpf(I, 2800.0f, 0.08f).lpf(II, 4200.0f, 0.06f).chorus(0.40f, 0.22f));
    add("Violins", "Strings", { "feet-layer", "velocity" },
        "An 8' bowed section with a quieter 4' upper sheen. A quick bow onset and restrained chorus keep articulated lines clear.",
        strings().feet(II, f4).mix(0.20f).hpf(I, 160.0f, 0.04f).hpf(II, 650.0f, 0.02f)
            .lpf(I, 5000.0f, 0.10f).lpf(II, 8500.0f, 0.04f)
            .eg(I, 0.09f, 0.7f, 0.86f, 0.55f).eg(II, 0.18f, 1.0f, 0.70f, 0.7f)
            .level(II, 0.82f, 0.42f).touch(I, 0.14f, 0.38f, 0.18f, 0.18f)
            .chorus(0.28f, 0.32f).detune(4.0f));
    add("Cellos", "Strings", { "feet-layer", "aftertouch" },
        "A full 16' bowed body with a lighter 8' bow edge. The low filter preserves weight while pressure opens the upper harmonics.",
        strings().feet(I, f16).mix(0.25f).hpf(I, 28.0f).hpf(II, 115.0f, 0.03f)
            .lpf(I, 1600.0f, 0.12f).lpf(II, 2900.0f, 0.08f)
            .eg(I, 0.17f, 1.1f, 0.92f, 0.85f).eg(II, 0.10f, 0.7f, 0.48f, 0.55f)
            .feg(II, 0.06f, 0.24f, 0.09f, 0.65f, 0.4f)
            .touch(I, 0.08f, 0.32f, 0.16f, 0.24f).sub(subSine, 4.9f, 0.0f).subAt(0.03f, 0.075f)
            .chorus(0.24f, 0.24f).detune(3.0f));
    add("Octave Strings", "Strings", { "feet-layer", "chorus" },
        "Balanced 8' and 4' sections, with the octave entering a little later. Their separate filter ranges keep the upper section present without hollowing the body.",
        strings().feet(II, f4).mix(0.43f).hpf(I, 90.0f).hpf(II, 230.0f)
            .lpf(I, 3600.0f, 0.08f).lpf(II, 7000.0f, 0.05f)
            .eg(I, 0.16f, 1.0f, 0.88f, 0.9f).eg(II, 0.30f, 1.3f, 0.78f, 1.1f)
            .chorus(0.34f, 0.30f).detune(3.5f));
    add("PWM Strings", "Strings", { "pwm", "chorus" },
        "Two pulse sections breathe at different slow PWM rates. Moderate pulse motion and a light chorus give shimmer while keeping the pitch centre steady.",
        strings().wave(Both, false, true).pw(I, 0.53f, 0.32f, 0.29f).pw(II, 0.61f, 0.25f, 0.41f)
            .hpf(I, 100.0f).hpf(II, 180.0f).lpf(I, 3800.0f, 0.07f).lpf(II, 5400.0f, 0.05f)
            .feg(I, 0.10f, 0.08f, 0.20f, 1.5f, 0.9f).feg(II, 0.16f, 0.12f, 0.34f, 1.7f, 1.0f)
            .chorus(0.28f, 0.32f).detune(4.0f));
    add("Thin Strings", "Strings", {},
        "High-passed saw sections leave a narrow, distant upper-string texture. The second rank is thinner and slower, with little pressure-driven thinning.",
        strings().wave(Both, true, false).hpf(I, 700.0f, 0.06f).hpf(II, 1350.0f, 0.04f)
            .lpf(I, 5000.0f, 0.05f).lpf(II, 7000.0f, 0.04f)
            .level(I, 1.0f, 0.68f).level(II, 1.0f, 0.58f)
            .touch(I, 0.05f, 0.32f, 0.08f, 0.18f).touch(II, 0.04f, 0.26f, 0.06f, 0.15f)
            .eg(I, 0.22f, 1.0f, 0.82f, 1.0f).eg(II, 0.38f, 1.4f, 0.74f, 1.3f)
            .chorus(0.30f, 0.24f).detune(3.8f));
    add("Dark Strings", "Strings", {},
        "Covered saw body and soft pulse overtones, with the low end retained. A slow, shallow filter opening gives a sombre held texture.",
        strings().wave(I, true, false, 0.05f).hpf(I, 45.0f).hpf(II, 85.0f)
            .lpf(I, 1100.0f, 0.12f).lpf(II, 1700.0f, 0.08f).brill(-0.15f)
            .feg(I, 0.16f, 0.10f, 0.40f, 1.7f, 1.2f).feg(II, 0.22f, 0.08f, 0.55f, 2.0f, 1.5f)
            .eg(I, 0.36f, 1.4f, 0.91f, 1.4f).eg(II, 0.50f, 1.8f, 0.84f, 1.7f)
            .touch(Both, 0.04f, 0.25f, 0.12f, 0.20f).chorus(0.28f, 0.18f));
    add("Tremolo Strings", "Strings", { "sub-mod" },
        "A 6.5 Hz square SUB alternates the bow level without silencing the sustain. Quick attacks and light chorus retain the repeated-stroke texture.",
        strings().sub(subSquare, 6.5f, 0.0f, 0.025f, 0.50f).subAt(0.0f, 0.0f)
            .g(P_modWheelSub, 0.0f).eg(I, 0.08f, 0.8f, 0.90f, 0.5f).eg(II, 0.13f, 1.0f, 0.82f, 0.65f)
            .lpf(I, 3100.0f, 0.08f).lpf(II, 4600.0f, 0.05f).chorus(0.16f, 0.35f).detune(3.0f));
    add("Solo Violin", "Strings", { "aftertouch", "sub-mod", "init-bend", "velocity" },
        "A single dry 8' saw rank with a small bow-noise component. Velocity shapes the onset; pressure introduces about 16 cents of vibrato. The wheel provides a separate gentle vibrato gesture.",
        strings().mix(0.0f).wave(I, true, false, 0.025f, 0.006f).wave(II, false, false)
            .feet(I, f8).g(P_chorusOn, 0.0f).detune(0.0f).pan(Both, 0.0f)
            .hpf(I, 150.0f, 0.06f).lpf(I, 3800.0f, 0.12f)
            .feg(I, 0.08f, 0.16f, 0.10f, 0.55f, 0.28f).eg(I, 0.045f, 0.65f, 0.85f, 0.28f)
            .level(I, 0.92f, 0.32f).level(II, 0.0f, 0.0f).touch(I, 0.18f, 0.52f, 0.18f, 0.24f)
            .sub(subSine, 5.7f, 0.0f).subAt(0.03f, 0.105f).g(P_modWheelSub, 0.065f).bend(0.045f)
            .contour(0.0f, -0.10f, 0.0f, -0.05f)
            .g(P_voiceDispersion, 0.08f).g(P_drift, 0.025f).g(P_analogNoise, 0.003f));
    add("Solo Cello", "Strings", { "aftertouch", "sub-mod", "velocity" },
        "A single dry 16' bowed rank with a warm fundamental and a slower bow onset. Pressure adds weight and about 12 cents of vibrato; no octave layer or ensemble chorus is present.",
        strings().mix(0.0f).wave(I, true, false, 0.07f, 0.004f).wave(II, false, false)
            .feet(I, f16).g(P_chorusOn, 0.0f).detune(0.0f).pan(Both, 0.0f)
            .hpf(I, 28.0f, 0.03f).lpf(I, 1500.0f, 0.14f)
            .feg(I, 0.12f, 0.18f, 0.12f, 0.8f, 0.4f).eg(I, 0.075f, 0.85f, 0.90f, 0.42f)
            .level(I, 0.95f, 0.34f).level(II, 0.0f, 0.0f).touch(I, 0.14f, 0.48f, 0.16f, 0.28f)
            .sub(subSine, 4.9f, 0.0f).subAt(0.03f, 0.08f).g(P_modWheelSub, 0.055f)
            .contour(0.05f, -0.12f, 0.0f, -0.04f)
            .g(P_voiceDispersion, 0.07f).g(P_drift, 0.025f).g(P_analogNoise, 0.003f));
    add("Pizzicato Strings", "Strings", { "percussive", "velocity", "dual-rank" },
        "A short upper pluck sits above an 8' resonant body. Both ranks decay to silence while the key is held; velocity controls the pluck strength and brightness.",
        strings().wave(I, true, false, 0.12f).wave(II, false, true).pw(II, 0.76f)
            .feet(II, f4).mix(0.22f).hpf(I, 55.0f).hpf(II, 680.0f)
            .lpf(I, 1800.0f, 0.18f).lpf(II, 6200.0f, 0.08f)
            .feg(I, 0.0f, 0.35f, 0.002f, 0.34f, 0.25f).feg(II, 0.0f, 0.22f, 0.001f, 0.07f, 0.06f)
            .eg(I, 0.0015f, 1.7f, 0.0f, 0.38f).eg(II, 0.001f, 0.17f, 0.0f, 0.09f)
            .level(I, 0.95f, 0.13f).level(II, 0.85f, 0.10f)
            .touch(I, 0.22f, 0.88f, 0.0f, 0.0f).touch(II, 0.28f, 0.55f, 0.0f, 0.0f)
            .g(P_chorusOn, 0.0f).subAt(0.0f, 0.0f).g(P_modWheelSub, 0.0f)
            .pan(Both, 0.0f).detune(0.8f).res(0.14f).g(P_drift, 0.02f).g(P_analogNoise, 0.002f));
    add("Swell Strings", "Strings", { "aftertouch" },
        "A quiet bowed bed gains level and harmonic weight under pressure. Release pressure to return to the soft sustained texture without retriggering the bow.",
        strings().level(I, 0.95f, 0.22f).level(II, 0.88f, 0.16f)
            .touch(I, 0.0f, 0.0f, 0.30f, 0.65f).touch(II, 0.0f, 0.0f, 0.20f, 0.55f)
            .hpf(I, 70.0f).hpf(II, 120.0f).lpf(I, 2300.0f, 0.10f).lpf(II, 3500.0f, 0.07f)
            .eg(I, 0.28f, 1.3f, 0.90f, 1.2f).eg(II, 0.42f, 1.7f, 0.84f, 1.5f)
            .chorus(0.30f, 0.26f));
    add("Chamber Strings", "Strings", { "dual-rank" },
        "Two closely tuned bow sections with no chorus. Their modest pulse motion and slightly different attacks suggest a smaller group and preserve clear note changes.",
        strings().g(P_chorusOn, 0.0f).detune(2.2f).mix(0.30f).pw(II, 0.59f, 0.06f, 0.27f)
            .hpf(I, 100.0f, 0.03f).hpf(II, 180.0f, 0.02f).lpf(I, 3200.0f, 0.10f).lpf(II, 4200.0f, 0.06f)
            .eg(I, 0.08f, 0.7f, 0.88f, 0.5f).eg(II, 0.14f, 0.9f, 0.82f, 0.65f)
            .level(II, 0.88f, 0.40f).touch(II, 0.08f, 0.24f, 0.12f, 0.18f)
            .pan(I, -0.04f).pan(II, 0.04f).subAt(0.04f, 0.05f)
            .g(P_voiceDispersion, 0.10f).g(P_drift, 0.04f));
    add("Wide Strings", "Strings", { "chorus", "pwm" },
        "Broad stereo sections with slower upper-rank motion and a deeper chorus. The detune stays moderate so sustained chords retain a clear pitch centre.",
        strings().mix(0.48f).detune(10.0f).chorus(0.60f, 0.28f).pan(I, -0.36f).pan(II, 0.36f)
            .pw(II, 0.58f, 0.25f, 0.21f).lpf(I, 3600.0f, 0.08f).lpf(II, 5100.0f, 0.05f)
            .eg(I, 0.25f, 1.3f, 0.88f, 1.25f).eg(II, 0.40f, 1.7f, 0.82f, 1.6f));
    add("Crystal Strings", "Strings", { "feet-layer", "pwm" },
        "An 8' bowed body supports a quiet 2' pulse-and-sine sheen. The upper rank opens later and has its own high cutoff for a delicate glassy edge.",
        strings().feet(II, f2).wave(II, false, true, 0.18f).pw(II, 0.63f, 0.10f, 0.28f)
            .hpf(I, 100.0f).hpf(II, 700.0f, 0.02f).lpf(I, 3500.0f, 0.08f).lpf(II, 10500.0f, 0.04f)
            .mix(0.19f).level(II, 0.65f, 0.55f).touch(II, 0.04f, 0.22f, 0.12f, 0.12f)
            .eg(II, 0.45f, 1.6f, 0.45f, 1.4f).chorus(0.22f, 0.32f).detune(2.5f));
    add("Fifth Strings", "Strings", { "feet-layer" },
        "A softly entering 5 1/3' upper section adds an open fifth above the 8' body. Play sparse voicings to hear the harmonic interval clearly.",
        strings().feet(II, f513).mix(0.28f).hpf(II, 200.0f).lpf(II, 5000.0f, 0.05f)
            .level(II, 0.85f, 0.48f).eg(II, 0.30f, 1.3f, 0.78f, 1.1f)
            .feg(II, 0.15f, 0.08f, 0.35f, 1.6f, 1.0f).chorus(0.30f, 0.25f).detune(1.2f).res(0.08f));
    add("Analog Orchestra", "Strings", { "feet-layer", "chorus" },
        "A warm 16' body and lighter 4' upper section span two octaves. Separate filter ranges and gently staggered bows keep the low section from becoming thin.",
        strings().feet(I, f16).feet(II, f4).mix(0.32f)
            .hpf(I, 40.0f).hpf(II, 200.0f).lpf(I, 2000.0f, 0.10f).lpf(II, 7000.0f, 0.06f)
            .eg(I, 0.25f, 1.4f, 0.90f, 1.2f).eg(II, 0.38f, 1.8f, 0.80f, 1.5f)
            .touch(I, 0.12f, 0.26f, 0.21f, 0.16f).touch(II, 0.07f, 0.22f, 0.16f, 0.12f)
            .chorus(0.48f, 0.25f).detune(4.0f).pan(I, -0.15f).pan(II, 0.15f));
    add("Spiccato Strings", "Strings", { "percussive", "velocity", "dual-rank" },
        "Short rebounding bow strokes: a quick body and an even shorter upper bite, both returning to silence on a held key. Play detached notes for repeated strokes.",
        strings().wave(I, true, false, 0.04f).wave(II, false, true).pw(II, 0.68f)
            .hpf(I, 110.0f).hpf(II, 420.0f).lpf(I, 2800.0f, 0.12f).lpf(II, 5200.0f, 0.06f)
            .feg(I, 0.0f, 0.30f, 0.012f, 0.14f, 0.1f).feg(II, 0.0f, 0.35f, 0.006f, 0.12f, 0.08f)
            .eg(I, 0.012f, 0.48f, 0.0f, 0.14f).eg(II, 0.006f, 0.18f, 0.0f, 0.08f)
            .level(I, 0.95f, 0.15f).level(II, 0.88f, 0.10f)
            .touch(I, 0.22f, 0.85f, 0.0f, 0.0f).touch(II, 0.28f, 0.65f, 0.0f, 0.0f)
            .mix(0.28f).g(P_chorusOn, 0.0f).subAt(0.0f, 0.0f).g(P_modWheelSub, 0.0f)
            .pan(Both, 0.0f).detune(1.0f).g(P_drift, 0.02f).g(P_analogNoise, 0.002f));
    add("Bowed Swell Strings", "Strings", { "brilliance-swell", "aftertouch" },
        "The bow level rises first, then the two filters bloom over 1.1 and 1.5 seconds. Hold the chord to let the brighter section emerge from the covered body.",
        strings().hpf(I, 65.0f).hpf(II, 150.0f).lpf(I, 2100.0f, 0.12f).lpf(II, 3600.0f, 0.08f)
            .feg(I, 0.45f, 0.20f, 1.1f, 2.2f, 1.4f).feg(II, 0.55f, 0.18f, 1.5f, 2.6f, 1.8f)
            .eg(I, 0.55f, 1.8f, 0.90f, 1.6f).eg(II, 0.80f, 2.2f, 0.82f, 2.0f)
            .touch(I, 0.06f, 0.22f, 0.25f, 0.22f).touch(II, 0.04f, 0.18f, 0.20f, 0.18f)
            .chorus(0.33f, 0.23f).detune(4.5f));
    add("Synth Strings Saw", "Strings", { "chorus" },
        "Two saw sections with a covered upper spectrum and pronounced chorus. Their shallow filter motion and long overlapping releases make a classic sustained synth-string texture.",
        strings().wave(Both, true, false).hpf(I, 110.0f).hpf(II, 150.0f)
            .lpf(I, 2600.0f, 0.08f).lpf(II, 3700.0f, 0.06f)
            .feg(I, 0.08f, 0.08f, 0.20f, 1.8f, 1.2f).feg(II, 0.12f, 0.06f, 0.28f, 2.0f, 1.4f)
            .eg(I, 0.18f, 1.3f, 0.91f, 1.3f).eg(II, 0.23f, 1.6f, 0.88f, 1.5f)
            .chorus(0.58f, 0.35f).detune(7.2f).pan(I, -0.12f).pan(II, 0.12f));
    add("String Machine", "Strings", { "chorus" },
        "Bright saw sections with a fast ensemble chorus, nearly steady sustain and little touch dependence. Short attacks keep chord changes immediate.",
        strings().wave(Both, true, false).hpf(I, 190.0f, 0.03f).hpf(II, 280.0f, 0.02f)
            .lpf(I, 5600.0f, 0.04f).lpf(II, 7200.0f, 0.03f).feg(Both, 0.0f, 0.0f, 0.01f, 0.5f, 0.4f)
            .eg(I, 0.07f, 0.7f, 0.96f, 0.75f).eg(II, 0.11f, 0.9f, 0.96f, 0.85f)
            .level(I, 0.92f, 0.72f).level(II, 0.88f, 0.66f).touch(Both, 0.0f, 0.0f, 0.08f, 0.10f)
            .subAt(0.0f, 0.0f).g(P_modWheelSub, 0.0f).chorus(0.80f, 0.60f).detune(3.0f).res(0.05f));
    add("Contour Strings", "Strings", { "kbd-contour", "aftertouch" },
        "The keyboard contour softens the bass and adds upper-register brilliance with a small level reduction. A low-pass body and brighter second rank remain balanced across the keyboard.",
        strings().contour(-0.15f, 0.20f, -0.10f, -0.08f)
            .hpf(I, 65.0f).hpf(II, 150.0f).lpf(I, 2700.0f, 0.10f).lpf(II, 4800.0f, 0.06f)
            .eg(I, 0.18f, 1.1f, 0.88f, 0.95f).eg(II, 0.30f, 1.5f, 0.80f, 1.2f)
            .mix(0.36f).detune(4.2f));
    add("Ring Strings", "Strings", { "ring-mod" },
        "A quiet 0.8 Hz ring component adds a slow level shimmer behind the sustained ensemble. It stays subtle enough for held chords and does not impose a metallic pitch.",
        strings().ring(0.8f, 0.0f, 1.2f, 2.8f, 0.08f)
            .eg(I, 0.28f, 1.3f, 0.90f, 1.2f).eg(II, 0.45f, 1.7f, 0.84f, 1.5f)
            .lpf(I, 3000.0f, 0.08f).lpf(II, 4600.0f, 0.06f).chorus(0.24f, 0.24f).detune(3.0f));
    add("Velvet Strings", "Strings", { "chorus" },
        "Soft saw body, low pulse overtones and a very light chorus. The two long releases make a smooth tail without adding a bright upper layer.",
        strings().wave(I, true, false, 0.06f).pw(II, 0.54f, 0.16f, 0.24f)
            .hpf(I, 40.0f).hpf(II, 95.0f).lpf(I, 1350.0f, 0.08f).lpf(II, 2100.0f, 0.05f)
            .feg(I, 0.12f, 0.08f, 0.40f, 1.7f, 1.6f).feg(II, 0.16f, 0.06f, 0.55f, 2.0f, 1.9f)
            .eg(I, 0.35f, 1.5f, 0.91f, 2.2f).eg(II, 0.50f, 1.9f, 0.86f, 2.65f)
            .level(II, 0.85f, 0.40f).touch(Both, 0.05f, 0.24f, 0.12f, 0.18f)
            .chorus(0.25f, 0.20f).detune(3.2f));
    add("Tutti Strings", "Strings", { "feet-layer", "pwm", "velocity" },
        "A strong 16' saw body supports an 8' PWM section. Faster bows and a moderate chorus give full, articulated ensemble chords with velocity-dependent attack weight.",
        strings().feet(I, f16).wave(I, true, false, 0.035f).wave(II, false, true).pw(II, 0.60f, 0.22f, 0.38f)
            .hpf(I, 35.0f).hpf(II, 120.0f).lpf(I, 2500.0f, 0.10f).lpf(II, 5000.0f, 0.07f)
            .eg(I, 0.095f, 0.9f, 0.90f, 0.8f).eg(II, 0.14f, 1.2f, 0.86f, 1.0f)
            .level(I, 0.95f, 0.75f).level(II, 0.90f, 0.62f)
            .touch(I, 0.14f, 0.25f, 0.16f, 0.10f).touch(II, 0.12f, 0.30f, 0.12f, 0.10f)
            .mix(0.44f).chorus(0.42f, 0.34f).detune(5.2f));
    add("Marcato Strings", "Strings", { "velocity", "dual-rank" },
        "An accented upper bow burst settles into a sustained body. Velocity controls both the bite and its weight; the quieter sustained section remains after the accent.",
        strings().wave(II, true, false, 0.0f, 0.01f).hpf(I, 160.0f).hpf(II, 650.0f)
            .lpf(I, 3200.0f, 0.12f).lpf(II, 7000.0f, 0.08f)
            .feg(I, 0.06f, 0.22f, 0.026f, 0.32f, 0.24f).feg(II, 0.05f, 0.30f, 0.008f, 0.18f, 0.10f)
            .eg(I, 0.026f, 0.45f, 0.72f, 0.30f).eg(II, 0.008f, 0.30f, 0.12f, 0.14f)
            .level(I, 0.95f, 0.23f).level(II, 0.85f, 0.14f)
            .touch(I, 0.22f, 0.75f, 0.12f, 0.12f).touch(II, 0.32f, 0.70f, 0.06f, 0.06f)
            .mix(0.30f).chorus(0.14f, 0.35f).detune(2.0f).subAt(0.02f, 0.03f));
    add("Air Strings", "Strings", { "noise", "dual-rank", "aftertouch" },
        "Quiet filtered bow air fades after the onset behind a sustained saw body. The separate noise envelope leaves only a soft breath during the hold; pressure supports the body.",
        strings().wave(I, true, false).wave(II, false, false, 0.0f, 0.55f)
            .hpf(I, 100.0f).hpf(II, 1700.0f).lpf(I, 3000.0f, 0.10f).lpf(II, 6500.0f, 0.04f)
            .feg(II, 0.0f, 0.12f, 0.025f, 0.32f, 0.20f)
            .eg(I, 0.12f, 1.0f, 0.88f, 0.8f).eg(II, 0.045f, 0.60f, 0.20f, 0.35f)
            .level(II, 0.50f, 0.22f).touch(II, 0.06f, 0.35f, 0.0f, 0.12f)
            .mix(0.14f).chorus(0.18f, 0.30f).detune(0.0f).g(P_drift, 0.04f).g(P_analogNoise, 0.004f));
}
