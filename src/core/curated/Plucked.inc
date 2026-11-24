// Plucked voicings. Included inside namespace curated by CuratedLibrary.inc.
// Existing program names and insertion order are stable host/MIDI identities.
// Plucked bodies use instrument-specific excitation spectra and partial losses.
// Independent pick ranks retain their circuit transients and touch dynamics.
// Body spectra now pass through the real rank filters. Broad main bandwidths
// preserve the string modes; separate pick filters remove their low duplicate.
// Octave courses keep their pitched root. String resonators have distinct
// sizes; all voices have their own contour, damping and contact/body balance.
// These are authored modal interpretations, without sampled source audio.
// Touch-sensitive voices leave most level to initial touch. Individual
// fixed floors keep soft notes usable without flattening the player's
// dynamics. Pure decaying bodies retain a small aftersound pedestal;
// mixed circuit plucks still reach zero.
Program plucked()
{
    Program x;
    x.wave(I, true, false, 0.16f).wave(II, false, true, 0.0f, 0.012f).pw(II, 0.72f)
     .feet(Both, f8).hpf(I, 45.0f).hpf(II, 850.0f)
     .lpf(I, 1150.0f, 0.10f).lpf(II, 4200.0f, 0.10f)
     .feg(I, 0.0f, 0.36f, 0.0025f, 1.15f, 0.24f)
     .feg(II, 0.0f, 0.26f, 0.0012f, 0.20f, 0.07f)
     .eg(I, 0.0025f, 3.7f, 0.0f, 0.65f).eg(II, 0.0012f, 0.32f, 0.0f, 0.09f)
     .level(I, 0.88f, 0.35f).level(II, 0.88f, 0.12f)
     .touch(I, 0.18f, 0.72f, 0.0f, 0.0f).touch(II, 0.28f, 0.94f, 0.0f, 0.0f)
     .sub(subSine, 5.1f, 0.0f).subAt(0.0f, 0.0f)
     .mix(0.22f).detune(0.0f).res(0.06f)
     .g(P_voiceDispersion, 0.08f).g(P_drift, 0.025f).g(P_analogNoise, 0.0f).volume(0.6f);
    return x;
}

void buildPlucked(Builder& add)
{
    add("Guitar I", "Plucked", { "velocity", "dual-rank" },
        "A nylon-like plucked-string spectrum has a warm body and a brief finger/pick edge. The excitation notch and faster high-partial losses let the clear lower string outlast the attack; harder playing brightens the strike.",
        plucked().level(I, 0.88f, 0.075f)
              .hpf(I, 32.0f).hpf(II, 980.0f).lpf(I, 5800.0f, 0.035f).lpf(II, 7400.0f, 0.045f)
              .feg(I, 0.0f, 0.10f, 0.0027f, 0.70f, 0.18f).feg(II, 0.0f, 0.13f, 0.001f, 0.085f, 0.035f)
              .eg(I, 0.0034f, 5.5f, 0.045f, 0.17f).eg(II, 0.001f, 0.12f, 0.0f, 0.035f).wave(II, false, true, 0.0f, 0.008f)
              .level(II, 0.88f, 0.025f).touch(I, 0.06f, 0.86f, 0.0f, 0.0f).touch(II, 0.08f, 0.98f, 0.0f, 0.0f)
              .body(I, body::Model::nylon, 1.0f, 0.48f, 1.18f).body(II, body::Model::nylon, 0.42f, 0.66f, 0.27f)
              .bodySize(I, 1.04f).bodySize(II, 0.94f).mix(0.21f).contour(-0.06f, -0.16f, -0.015f, 0.085f));
    add("Guitar II", "Plucked", { "velocity", "dual-rank" },
        "A brighter steel-string body carries a firmer pick and longer upper shimmer than Guitar I. Independently damping partials soften the attack naturally above the ringing lower string.",
        plucked().level(I, 0.88f, 0.065f)
              .wave(I, true, false, 0.12f).pw(II, 0.76f).hpf(I, 35.0f).hpf(II, 1350.0f).lpf(I, 8400.0f, 0.04f)
              .lpf(II, 10500.0f, 0.035f).feg(I, 0.0f, 0.16f, 0.002f, 0.95f, 0.20f).feg(II, 0.0f, 0.18f, 0.001f, 0.12f, 0.045f)
              .eg(I, 0.0022f, 6.5f, 0.06f, 0.21f).eg(II, 0.001f, 0.18f, 0.0f, 0.045f).wave(II, false, true, 0.0f, 0.013f)
              .level(II, 0.88f, 0.022f).touch(I, 0.09f, 0.88f, 0.0f, 0.0f).touch(II, 0.14f, 0.96f, 0.0f, 0.0f)
              .body(I, body::Model::steel, 1.0f, 0.69f, 1.38f).body(II, body::Model::steel, 0.57f, 0.88f, 0.32f)
              .bodySize(I, 0.91f).bodySize(II, 0.88f).detune(0.0f).mix(0.22f).contour(-0.08f, -0.20f, -0.025f, 0.10f));
    add("Funky Pluck I", "Plucked", { "resonance", "velocity", "percussive" },
        "A compact tangent-string snap joins a fast resonant filter attack. Independent upper losses clarify repeated notes while the circuit pulse retains the dry, rhythmic wah character.",
        plucked()
              .wave(I, false, true, 0.10f).pw(I, 0.64f).wave(II, true, false).level(I, 1.0f, 0.07f).level(II, 0.96f, 0.02f)
              .hpf(I, 34.0f).hpf(II, 460.0f).lpf(I, 800.0f, 0.30f).lpf(II, 1850.0f, 0.36f)
              .feg(I, 0.0f, 0.62f, 0.0015f, 0.43f, 0.075f).feg(II, 0.0f, 0.63f, 0.001f, 0.17f, 0.045f)
              .eg(I, 0.0018f, 1.25f, 0.0f, 0.075f).eg(II, 0.001f, 0.24f, 0.0f, 0.055f)
              .body(I, body::Model::clav, 0.72f, 0.72f, 0.38f).body(II, body::Model::clav, 0.31f, 0.91f, 0.25f)
              .bodySize(I, 0.88f).bodySize(II, 0.76f).touch(I, 0.22f, 0.92f, 0.0f, 0.0f).touch(II, 0.29f, 1.0f, 0.0f, 0.0f)
              .mix(0.23f).res(0.24f).contour(0.04f, -0.14f, -0.02f, 0.08f));
    add("Funky Pluck II", "Plucked", { "resonance", "velocity" },
        "A more open plucked-string body sits inside the slower wah-shaped circuit contour. The brief upper snap marks the strike, then independently damping string partials leave a rounded, expressive tail.",
        plucked()
              .wave(I, false, true, 0.08f).pw(I, 0.69f).wave(II, true, false).level(I, 1.0f, 0.085f).level(II, 0.84f, 0.03f)
              .hpf(I, 36.0f).hpf(II, 700.0f).lpf(I, 700.0f, 0.31f).lpf(II, 3400.0f, 0.23f)
              .feg(I, 0.0f, 0.65f, 0.006f, 1.35f, 0.15f).feg(II, 0.0f, 0.43f, 0.0015f, 0.28f, 0.065f)
              .eg(I, 0.0032f, 2.55f, 0.0f, 0.18f).eg(II, 0.001f, 0.39f, 0.0f, 0.075f)
              .body(I, body::Model::clav, 0.63f, 0.55f, 0.67f).body(II, body::Model::steel, 0.34f, 0.75f, 0.30f)
              .bodySize(I, 1.12f).bodySize(II, 0.95f).touch(I, 0.22f, 0.88f, 0.0f, 0.0f).touch(II, 0.21f, 0.98f, 0.0f, 0.0f)
              .mix(0.23f).res(0.22f).contour(-0.03f, -0.17f, -0.025f, 0.075f));
    add("Harp", "Plucked", { "sine", "velocity", "dual-rank" },
        "A finger-plucked harp string excites a rounded soundboard response with a broad, softly notched upper spectrum. The short contact rank shares the string's pitch, then yields to a long lower aftersound. Soft arpeggios retain their gentler attack and can ring across key releases.",
        plucked()
              .wave(Both, false, false, 1.0f).feet(Both, f8).res(0.018f).detune(0.0f)
              .hpf(I, 22.0f).hpf(II, 360.0f).lpf(I, 11500.0f, 0.01f).lpf(II, 13800.0f, 0.015f)
              .feg(I, 0.0f, 0.015f, 0.003f, 0.90f, 0.50f).feg(II, 0.0f, 0.025f, 0.0018f, 0.21f, 0.16f)
              .eg(I, 0.0039f, 11.2f, 0.065f, 2.9f).eg(II, 0.0018f, 0.42f, 0.0f, 0.28f)
              .level(I, 0.0f, 0.065f).level(II, 0.0f, 0.020f)
              .touch(I, 0.02f, 0.88f, 0.0f, 0.0f).touch(II, 0.025f, 0.88f, 0.0f, 0.0f)
              .body(I, body::Model::nylon, 1.0f, 0.56f, 2.05f).body(II, body::Model::nylon, 1.0f, 0.81f, 0.46f)
              .bodyMaterial(Both, body::harp).bodySize(I, 1.0f).bodySize(II, 1.0f)
              .mix(0.13f).g(P_voiceDispersion, 0.025f).g(P_drift, 0.004f)
              .contour(-0.055f, -0.14f, -0.035f, 0.10f));
    add("Koto", "Plucked", { "init-bend", "velocity", "percussive" },
        "A near-bridge plectrum opens a bright notched string spectrum over the koto's long-board response. A quiet short contact rank shares the played pitch; upper string modes soften before the ringing body. Restrained initial settling leaves the ribbon available for deliberate bends.",
        plucked()
              .wave(Both, false, false, 1.0f).feet(Both, f8).level(I, 0.0f, 0.060f).level(II, 0.0f, 0.018f)
              .res(0.025f).hpf(I, 24.0f).hpf(II, 1100.0f).lpf(I, 13200.0f, 0.015f).lpf(II, 15000.0f, 0.02f)
              .feg(I, 0.0f, 0.045f, 0.0015f, 0.64f, 0.28f).feg(II, 0.0f, 0.06f, 0.001f, 0.085f, 0.035f)
              .eg(I, 0.0018f, 6.4f, 0.045f, 0.92f).eg(II, 0.001f, 0.13f, 0.0f, 0.06f)
              .body(I, body::Model::steel, 1.0f, 0.76f, 1.35f).body(II, body::Model::steel, 1.0f, 0.96f, 0.28f)
              .bodyMaterial(Both, body::koto).bodySize(I, 1.0f).bodySize(II, 1.0f)
              .touch(I, 0.045f, 0.88f, 0.0f, 0.0f).touch(II, 0.065f, 0.94f, 0.0f, 0.0f)
              .bend(0.006f).mix(0.12f).detune(0.0f).g(P_voiceDispersion, 0.025f).g(P_drift, 0.004f)
              .contour(-0.035f, -0.19f, -0.02f, 0.095f));
    add("Banjo", "Plucked", { "percussive", "velocity", "dual-rank" },
        "A sharp string pluck excites the banjo's membrane-and-bridge response, with a strong low-mid body and a dry bright edge. Fast losses around the head resonance let the sound clear between picked notes; the brief contact rank adds no unrelated clavichord or held oscillator buzz.",
        plucked()
              .wave(Both, false, false, 1.0f).feet(Both, f8).level(I, 0.0f, 0.065f).level(II, 0.0f, 0.018f)
              .hpf(I, 55.0f, 0.0f).hpf(II, 1350.0f).lpf(I, 12800.0f, 0.015f).lpf(II, 15000.0f, 0.02f)
              .feg(I, 0.0f, 0.025f, 0.0012f, 0.23f, 0.07f).feg(II, 0.0f, 0.035f, 0.001f, 0.06f, 0.025f)
              .eg(I, 0.0013f, 2.6f, 0.018f, 0.095f).eg(II, 0.001f, 0.075f, 0.0f, 0.035f)
              .body(I, body::Model::steel, 1.0f, 0.86f, 0.95f).body(II, body::Model::steel, 1.0f, 0.98f, 0.25f)
              .bodyMaterial(Both, body::banjo).bodySize(I, 1.0f).bodySize(II, 1.0f)
              .touch(I, 0.035f, 0.91f, 0.0f, 0.0f).touch(II, 0.05f, 0.95f, 0.0f, 0.0f)
              .mix(0.10f).res(0.025f).detune(0.0f).g(P_voiceDispersion, 0.02f).g(P_drift, 0.003f)
              .contour(-0.025f, -0.16f, -0.015f, 0.09f));
    add("Mandolin", "Plucked", { "detune", "velocity", "dual-rank" },
        "Two close steel-string courses ring with independent partial losses and a restrained pitch offset. Their bright paired attacks stay compact; repeated strikes create tremolo picking while the paired courses retain gentle beating between strokes.",
        plucked()
              .wave(I, true, false, 0.08f).wave(II, false, true, 0.06f).pw(II, 0.72f).level(I, 0.92f, 0.075f)
              .level(II, 0.78f, 0.06f).hpf(I, 95.0f).hpf(II, 110.0f).lpf(I, 9300.0f, 0.045f).lpf(II, 10100.0f, 0.04f)
              .feg(I, 0.0f, 0.12f, 0.0016f, 0.50f, 0.10f).feg(II, 0.0f, 0.16f, 0.001f, 0.36f, 0.08f)
              .eg(I, 0.0018f, 4.2f, 0.02f, 0.12f).eg(II, 0.0023f, 3.6f, 0.018f, 0.11f)
              .body(I, body::Model::steel, 1.0f, 0.79f, 0.68f).body(II, body::Model::steel, 1.0f, 0.71f, 0.59f)
              .bodySize(I, 0.67f).bodySize(II, 0.69f).touch(I, 0.08f, 0.92f, 0.0f, 0.0f).touch(II, 0.10f, 0.96f, 0.0f, 0.0f)
              .sub(subSine, 12.0f, 0.0f, 0.0f, 0.0f).detune(1.8f).mix(0.47f).contour(-0.08f, -0.18f, 0.02f, 0.12f));
    add("Drone Pluck", "Plucked", { "ring-mod", "resonance", "aftertouch" },
        "A bright string strike recedes into a quiet resonant drone with faint ring color. Independently damping plucked partials animate the opening; pressure raises the remaining circuit bed after the upper pick disappears.",
        plucked().level(II, 0.88f, 0.03f)
              .wave(I, false, true, 0.22f).pw(I, 0.64f).wave(II, true, false).level(I, 0.90f, 0.095f).res(0.20f).hpf(I, 35.0f)
              .hpf(II, 1100.0f).lpf(I, 2800.0f, 0.23f).lpf(II, 8000.0f, 0.08f).feg(I, 0.0f, 0.30f, 0.003f, 1.7f, 0.60f)
              .feg(II, 0.0f, 0.21f, 0.001f, 0.13f, 0.045f).eg(I, 0.003f, 4.5f, 0.16f, 1.3f).eg(II, 0.001f, 0.18f, 0.0f, 0.055f)
              .body(I, body::Model::steel, 0.55f, 0.58f, 1.76f).body(II, body::Model::steel, 0.61f, 0.88f, 0.27f)
              .bodySize(I, 1.32f).bodySize(II, 0.87f).touch(I, 0.09f, 0.82f, 0.22f, 0.55f)
              .ring(110.0f, 0.0f, 0.001f, 1.4f, 0.075f).mix(0.14f).contour(-0.10f, -0.19f, -0.06f, 0.07f));
    add("Nylon Pluck", "Plucked", { "sine", "velocity", "noise" },
        "A warm plucked nylon-string spectrum carries a soft finger attack and clear lower body. The excitation notch and fast upper-partial damping keep the sound round as it decays, without chorus or detuning.",
        plucked()
              .wave(I, true, false, 0.35f).pw(II, 0.55f).level(I, 0.65f, 0.085f).res(0.025f).detune(0.0f).hpf(I, 26.0f)
              .hpf(II, 1100.0f).lpf(I, 4200.0f, 0.025f).lpf(II, 6000.0f, 0.04f).feg(I, 0.0f, 0.09f, 0.0032f, 0.80f, 0.24f)
              .feg(II, 0.0f, 0.08f, 0.001f, 0.055f, 0.025f).eg(I, 0.0037f, 5.9f, 0.045f, 0.21f)
              .eg(II, 0.001f, 0.08f, 0.0f, 0.025f).wave(II, false, true, 0.0f, 0.032f).level(II, 0.82f, 0.025f)
              .body(I, body::Model::nylon, 1.0f, 0.40f, 1.05f).body(II, body::Model::nylon, 0.24f, 0.66f, 0.25f)
              .bodySize(I, 1.15f).bodySize(II, 1.05f).touch(I, 0.035f, 0.92f, 0.0f, 0.0f).touch(II, 0.02f, 1.0f, 0.0f, 0.0f)
              .mix(0.22f).contour(-0.05f, -0.14f, -0.02f, 0.085f));
    add("Steel Pluck", "Plucked", { "feet-layer", "velocity", "dual-rank" },
        "Paired 8-foot and 4-foot steel-string courses open with bright, independently fading harmonics. The octave course recedes before the full lower body, with a tiny tuning offset adding restrained beating.",
        plucked()
              .wave(I, true, true, 0.10f).pw(I, 0.58f).wave(II, false, true, 0.12f).pw(II, 0.73f).feet(II, f4)
              .level(I, 0.70f, 0.07f).level(II, 0.61f, 0.035f).hpf(I, 38.0f).hpf(II, 240.0f).lpf(I, 9000.0f, 0.035f)
              .lpf(II, 11000.0f, 0.045f).feg(I, 0.0f, 0.14f, 0.0024f, 1.0f, 0.28f).feg(II, 0.0f, 0.18f, 0.0012f, 0.43f, 0.16f)
              .eg(I, 0.0024f, 7.2f, 0.065f, 0.32f).eg(II, 0.0014f, 2.15f, 0.0f, 0.35f)
              .body(I, body::Model::steel, 1.0f, 0.73f, 1.50f).body(II, body::Model::steel, 0.96f, 0.79f, 0.88f)
              .bodySize(I, 0.96f).bodySize(II, 0.79f).touch(I, 0.09f, 0.88f, 0.0f, 0.0f).touch(II, 0.12f, 0.94f, 0.0f, 0.0f)
              .detune(0.6f).mix(0.21f).contour(-0.075f, -0.24f, -0.03f, 0.11f));
    add("Muted Pluck", "Plucked", { "percussive", "velocity", "noise" },
        "A palm-muted string spectrum carries a brief pick click and a rounded low note. Fast partial losses and short body damping make repeated strokes distinct even while the keys stay held.",
        plucked()
              .wave(I, false, true, 0.12f).pw(I, 0.58f).pw(II, 0.80f).level(I, 0.91f, 0.06f).level(II, 0.82f, 0.015f).res(0.08f)
              .hpf(I, 28.0f).hpf(II, 1700.0f).lpf(I, 3500.0f, 0.07f).lpf(II, 7500.0f, 0.035f)
              .feg(I, 0.0f, 0.16f, 0.0015f, 0.18f, 0.045f).feg(II, 0.0f, 0.12f, 0.001f, 0.045f, 0.025f)
              .eg(I, 0.0016f, 0.5f, 0.0f, 0.045f).eg(II, 0.001f, 0.055f, 0.0f, 0.018f).wave(II, false, true, 0.0f, 0.028f)
              .body(I, body::Model::steel, 0.99f, 0.28f, 0.25f).body(II, body::Model::clav, 0.31f, 0.82f, 0.25f)
              .bodySize(I, 1.10f).bodySize(II, 0.85f).touch(I, 0.06f, 0.94f, 0.0f, 0.0f).touch(II, 0.10f, 1.0f, 0.0f, 0.0f)
              .mix(0.18f).contour(-0.035f, -0.13f, -0.025f, 0.08f));
    add("Kalimba", "Plucked", { "sine", "percussive", "velocity", "dual-rank" },
        "A thumb-plucked supported tine gives a rounded fundamental and a brief nonharmonic ping, distinct from an electric-piano pickup. Both ranks share the played pitch; the brighter contact fades first while the lower tine continues ringing after key release.",
        plucked()
              .wave(Both, false, false, 1.0f).feet(Both, f8).res(0.018f).detune(0.0f)
              .level(I, 0.0f, 0.035f).level(II, 0.0f, 0.014f).touch(I, 0.0f, 0.90f, 0.0f, 0.0f)
              .touch(II, 0.015f, 0.94f, 0.0f, 0.0f).hpf(I, 35.0f).hpf(II, 1050.0f)
              .lpf(I, 13200.0f, 0.01f).lpf(II, 16000.0f, 0.015f)
              .feg(I, 0.0f, 0.0f, 0.001f, 0.4f, 0.4f).feg(II, 0.0f, 0.015f, 0.001f, 0.09f, 0.07f)
              .eg(I, 0.0022f, 4.5f, 0.045f, 1.35f).eg(II, 0.001f, 0.15f, 0.0f, 0.085f)
              .body(I, body::Model::tine, 1.0f, 0.72f, 1.15f).body(II, body::Model::tine, 1.0f, 0.96f, 0.30f)
              .bodyMaterial(Both, body::kalimba).bodySize(I, 1.0f).bodySize(II, 1.0f)
              .mix(0.11f).g(P_voiceDispersion, 0.02f).g(P_drift, 0.002f)
              .contour(-0.025f, -0.17f, -0.015f, 0.09f));
    add("Zither", "Plucked", { "chorus", "velocity", "dual-rank" },
        "Two bright steel-string courses have long unequal body decays and independent upper-partial losses. A restrained pitch offset and shallow chorus add a gentle ensemble shimmer after the clearly plucked onset.",
        plucked()
              .wave(I, true, false, 0.12f).wave(II, false, true, 0.06f).pw(II, 0.76f).level(I, 0.82f, 0.075f)
              .level(II, 0.70f, 0.045f).hpf(I, 42.0f).hpf(II, 160.0f).lpf(I, 9200.0f, 0.035f).lpf(II, 10800.0f, 0.045f)
              .feg(I, 0.0f, 0.12f, 0.0018f, 1.05f, 0.50f).feg(II, 0.0f, 0.16f, 0.001f, 0.50f, 0.32f)
              .eg(I, 0.002f, 8.6f, 0.05f, 2.6f).eg(II, 0.0015f, 4.5f, 0.02f, 1.4f)
              .body(I, body::Model::steel, 1.0f, 0.81f, 2.05f).body(II, body::Model::steel, 1.0f, 0.73f, 1.27f)
              .bodySize(I, 1.22f).bodySize(II, 1.19f).touch(I, 0.08f, 0.86f, 0.0f, 0.0f).touch(II, 0.12f, 0.94f, 0.0f, 0.0f)
              .chorus(0.10f, 0.23f).detune(1.2f).mix(0.29f).contour(-0.08f, -0.22f, -0.04f, 0.11f));
    add("Dulcimer Pluck", "Plucked", { "feet-layer", "percussive", "velocity" },
        "A firm steel-string pluck has a bright short octave course and a longer lower body. Upper modes lose energy independently, leaving a clear ringing string after the plectrum edge has faded.",
        plucked()
              .wave(I, true, false, 0.15f).wave(II, false, true, 0.04f).feet(II, f4).pw(II, 0.79f).level(I, 0.82f, 0.065f)
              .level(II, 0.78f, 0.025f).hpf(I, 45.0f).hpf(II, 700.0f).lpf(I, 9400.0f, 0.045f).lpf(II, 12500.0f, 0.055f)
              .feg(I, 0.0f, 0.15f, 0.0018f, 0.72f, 0.22f).feg(II, 0.0f, 0.19f, 0.001f, 0.16f, 0.065f)
              .eg(I, 0.0023f, 6.2f, 0.045f, 1.05f).eg(II, 0.001f, 0.28f, 0.0f, 0.085f)
              .body(I, body::Model::steel, 1.0f, 0.79f, 1.31f).body(II, body::Model::steel, 0.91f, 0.92f, 0.38f)
              .bodySize(I, 1.35f).bodySize(II, 1.05f).touch(I, 0.10f, 0.90f, 0.0f, 0.0f).touch(II, 0.10f, 0.99f, 0.0f, 0.0f)
              .detune(0.10f).mix(0.24f).contour(-0.04f, -0.24f, -0.025f, 0.12f));
    add("Pressure Pluck", "Plucked", { "aftertouch", "velocity", "dual-rank" },
        "A natural-style picked string onset settles over a quiet held circuit bed. The high string modes fade independently; pressure swells and colors the remaining body after the short pick has disappeared.",
        plucked()
              .wave(I, true, false, 0.24f).wave(II, false, true).pw(II, 0.70f).level(I, 0.82f, 0.08f).level(II, 0.86f, 0.025f)
              .res(0.09f).hpf(I, 26.0f).hpf(II, 1200.0f).lpf(I, 4200.0f, 0.10f).lpf(II, 9300.0f, 0.055f)
              .feg(I, 0.0f, 0.18f, 0.003f, 1.50f, 0.45f).feg(II, 0.0f, 0.17f, 0.001f, 0.115f, 0.045f)
              .eg(I, 0.003f, 4.2f, 0.12f, 0.90f).eg(II, 0.001f, 0.16f, 0.0f, 0.05f)
              .body(I, body::Model::nylon, 0.60f, 0.55f, 1.48f).body(II, body::Model::steel, 0.62f, 0.84f, 0.28f)
              .bodySize(I, 1.13f).bodySize(II, 0.90f).touch(I, 0.05f, 0.82f, 0.24f, 0.75f).touch(II, 0.08f, 1.0f, 0.0f, 0.0f)
              .mix(0.21f).contour(-0.06f, -0.18f, -0.035f, 0.08f));
}
