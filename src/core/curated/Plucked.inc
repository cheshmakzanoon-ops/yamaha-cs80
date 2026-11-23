// Plucked voicings. Included inside namespace curated by CuratedLibrary.inc.
// Existing program names and insertion order are stable host/MIDI identities.
// Plucked bodies use instrument-specific excitation spectra and partial losses.
// Independent pick ranks retain their circuit transients and touch dynamics.
// These are authored modal interpretations, without sampled source audio.
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
        plucked()
                 .body(I, body::Model::nylon, 0.98f, 0.43f, 1.12f).body(II, body::Model::nylon, 0.52f, 0.72f, 0.34f)
                 .eg(I, 0.0023f, 4.25f, 0.0f, 0.52f).eg(II, 0.001f, 0.25f, 0.0f, 0.065f).mix(0.17f));
    add("Guitar II", "Plucked", { "velocity", "dual-rank" },
        "A brighter steel-string body carries a firmer pick and longer upper shimmer than Guitar I. Independently damping partials soften the attack naturally above the ringing lower string.",
        plucked().wave(I, true, false, 0.12f).lpf(I, 1750.0f, 0.11f)
                 .wave(II, false, true, 0.0f, 0.018f).pw(II, 0.76f).lpf(II, 6200.0f, 0.08f)
                 .feg(I, 0.0f, 0.32f, 0.002f, 1.45f, 0.32f).feg(II, 0.0f, 0.30f, 0.001f, 0.28f, 0.09f)
                 .eg(I, 0.002f, 4.8f, 0.0f, 0.85f).eg(II, 0.001f, 0.50f, 0.0f, 0.12f)
                 .touch(I, 0.20f, 0.73f, 0.0f, 0.0f).touch(II, 0.30f, 0.96f, 0.0f, 0.0f)
                 .mix(0.25f).detune(0.4f)
                 .body(I, body::Model::steel, 0.98f, 0.62f, 1.26f).body(II, body::Model::steel, 0.63f, 0.87f, 0.44f)
                 .eg(I, 0.0017f, 5.4f, 0.0f, 0.68f).eg(II, 0.001f, 0.37f, 0.0f, 0.09f).mix(0.20f).detune(0.15f));
    add("Funky Pluck I", "Plucked", { "resonance", "velocity", "percussive" },
        "A compact tangent-string snap joins a fast resonant filter attack. Independent upper losses clarify repeated notes while the circuit pulse retains the dry, rhythmic wah character.",
        plucked().wave(I, false, true, 0.10f).pw(I, 0.64f).wave(II, true, false)
                 .hpf(I, 38.0f).hpf(II, 220.0f).lpf(I, 680.0f, 0.34f).lpf(II, 1080.0f, 0.45f)
                 .feg(I, 0.0f, 0.67f, 0.002f, 0.62f, 0.10f).feg(II, 0.0f, 0.78f, 0.001f, 0.24f, 0.06f)
                 .eg(I, 0.0018f, 1.45f, 0.0f, 0.14f).eg(II, 0.001f, 0.30f, 0.0f, 0.07f)
                 .level(I, 1.0f, 0.30f).level(II, 0.96f, 0.10f)
                 .touch(I, 0.28f, 0.78f, 0.0f, 0.0f).touch(II, 0.42f, 0.98f, 0.0f, 0.0f)
                 .res(0.28f).mix(0.31f)
                 .body(I, body::Model::clav, 0.61f, 0.64f, 0.41f).body(II, body::Model::clav, 0.24f, 0.88f, 0.25f)
                 .feg(II, 0.0f, 0.73f, 0.001f, 0.19f, 0.05f).eg(I, 0.0015f, 1.7f, 0.0f, 0.10f).mix(0.25f));
    add("Funky Pluck II", "Plucked", { "resonance", "velocity" },
        "A more open plucked-string body sits inside the slower wah-shaped circuit contour. The brief upper snap marks the strike, then independently damping string partials leave a rounded, expressive tail.",
        plucked().wave(I, false, true, 0.08f).pw(I, 0.69f).wave(II, true, false)
                 .hpf(I, 42.0f).hpf(II, 320.0f).lpf(I, 560.0f, 0.37f).lpf(II, 1550.0f, 0.28f)
                 .feg(I, 0.0f, 0.72f, 0.008f, 1.65f, 0.18f).feg(II, 0.0f, 0.52f, 0.002f, 0.47f, 0.09f)
                 .eg(I, 0.003f, 2.9f, 0.0f, 0.30f).eg(II, 0.0015f, 0.62f, 0.0f, 0.12f)
                 .level(I, 1.0f, 0.34f).level(II, 0.84f, 0.14f)
                 .touch(I, 0.24f, 0.74f, 0.0f, 0.0f).touch(II, 0.30f, 0.90f, 0.0f, 0.0f)
                 .mix(0.25f).res(0.25f)
                 .body(I, body::Model::clav, 0.54f, 0.49f, 0.77f).body(II, body::Model::steel, 0.31f, 0.78f, 0.36f)
                 .eg(I, 0.0024f, 3.4f, 0.0f, 0.23f).feg(II, 0.0f, 0.49f, 0.0015f, 0.38f, 0.075f).mix(0.21f));
    add("Harp", "Plucked", { "sine", "feet-layer", "velocity" },
        "A long finger-plucked string spectrum carries a rounded fundamental and a clear octave glint. Upper partials damp individually above the open body, allowing soft arpeggios to ring without a static buzzy tail.",
        plucked().wave(I, true, false, 0.78f).wave(II, false, true, 0.46f).feet(II, f4).pw(II, 0.55f)
                 .hpf(I, 30.0f).hpf(II, 480.0f).lpf(I, 1700.0f, 0.025f).lpf(II, 4200.0f, 0.035f)
                 .level(I, 0.18f, 0.32f).level(II, 0.34f, 0.10f)
                 .feg(I, 0.0f, 0.12f, 0.003f, 2.1f, 0.65f).feg(II, 0.0f, 0.24f, 0.0015f, 0.65f, 0.25f)
                 .eg(I, 0.003f, 6.8f, 0.0f, 2.3f).eg(II, 0.0015f, 1.95f, 0.0f, 0.65f)
                 .touch(I, 0.08f, 0.76f, 0.0f, 0.0f).touch(II, 0.18f, 0.92f, 0.0f, 0.0f)
                 .mix(0.24f).res(0.025f).detune(0.0f)
                 .body(I, body::Model::nylon, 1.0f, 0.37f, 1.92f).body(II, body::Model::nylon, 0.91f, 0.66f, 0.79f)
                 .eg(I, 0.0025f, 7.8f, 0.0f, 2.5f).eg(II, 0.0012f, 1.65f, 0.0f, 0.54f).mix(0.17f));
    add("Koto", "Plucked", { "init-bend", "velocity", "percussive" },
        "A bright steel-string body has a sharp plectrum edge and a slight initial pitch settling. A notched excitation spectrum and unequal upper-partial damping separate the short pick from the longer ringing string.",
        plucked().wave(I, false, true, 0.07f).pw(I, 0.78f)
                 .wave(II, false, true, 0.0f, 0.025f).pw(II, 0.88f)
                 .hpf(I, 100.0f).hpf(II, 1050.0f).lpf(I, 2350.0f, 0.17f).lpf(II, 6400.0f, 0.12f)
                 .feg(I, 0.0f, 0.40f, 0.0015f, 0.95f, 0.25f).feg(II, 0.0f, 0.26f, 0.001f, 0.14f, 0.05f)
                 .eg(I, 0.0018f, 3.3f, 0.0f, 0.62f).eg(II, 0.001f, 0.19f, 0.0f, 0.06f)
                 .level(I, 0.96f, 0.32f).level(II, 0.82f, 0.10f)
                 .touch(I, 0.20f, 0.76f, 0.0f, 0.0f).touch(II, 0.28f, 0.94f, 0.0f, 0.0f)
                 .mix(0.23f).bend(0.08f).res(0.10f)
                 .body(I, body::Model::steel, 0.91f, 0.82f, 0.91f).body(II, body::Model::clav, 0.48f, 0.94f, 0.28f)
                 .eg(I, 0.0015f, 3.8f, 0.0f, 0.52f).eg(II, 0.001f, 0.15f, 0.0f, 0.045f).mix(0.18f).bend(0.045f));
    add("Banjo", "Plucked", { "percussive", "velocity", "noise" },
        "A bright, rapidly damping plucked spectrum joins a short nasal circuit body. The tiny noisy pick edge marks each strike while upper-partial losses keep the following decay dry and compact.",
        plucked().wave(I, false, true, 0.035f).pw(I, 0.84f)
                 .wave(II, false, true, 0.0f, 0.055f).pw(II, 0.90f)
                 .hpf(I, 180.0f, 0.06f).hpf(II, 1250.0f).lpf(I, 2850.0f, 0.22f).lpf(II, 6500.0f, 0.08f)
                 .feg(I, 0.0f, 0.31f, 0.0015f, 0.42f, 0.10f).feg(II, 0.0f, 0.24f, 0.001f, 0.10f, 0.04f)
                 .eg(I, 0.0015f, 1.85f, 0.0f, 0.18f).eg(II, 0.001f, 0.14f, 0.0f, 0.045f)
                 .level(I, 1.0f, 0.34f).level(II, 0.82f, 0.10f)
                 .touch(I, 0.20f, 0.74f, 0.0f, 0.0f).touch(II, 0.26f, 0.94f, 0.0f, 0.0f)
                 .mix(0.19f).res(0.12f)
                 .body(I, body::Model::steel, 0.73f, 0.91f, 0.32f).body(II, body::Model::clav, 0.36f, 0.97f, 0.25f)
                 .eg(I, 0.0012f, 2.0f, 0.0f, 0.13f).eg(II, 0.001f, 0.115f, 0.0f, 0.035f).mix(0.16f));
    add("Mandolin", "Plucked", { "sub-mod", "detune", "velocity", "dual-rank" },
        "Two close steel-string courses ring with independent partial losses and a restrained pitch offset. Their bright paired attacks stay compact; repeated strikes create tremolo picking while only a faint level flutter continues between strokes.",
        plucked().wave(I, true, false, 0.08f).wave(II, false, true, 0.06f).pw(II, 0.72f)
                 .hpf(I, 100.0f).hpf(II, 140.0f).lpf(I, 2450.0f, 0.10f).lpf(II, 3200.0f, 0.12f)
                 .level(I, 0.92f, 0.34f).level(II, 0.78f, 0.28f)
                 .feg(I, 0.0f, 0.30f, 0.0018f, 0.76f, 0.12f).feg(II, 0.0f, 0.35f, 0.001f, 0.52f, 0.10f)
                 .eg(I, 0.0018f, 3.2f, 0.0f, 0.28f).eg(II, 0.0025f, 2.65f, 0.0f, 0.22f)
                 .touch(I, 0.20f, 0.72f, 0.0f, 0.0f).touch(II, 0.24f, 0.78f, 0.0f, 0.0f)
                 .sub(subSine, 12.0f, 0.0f, 0.0f, 0.025f).mix(0.43f).detune(3.4f)
                 .body(I, body::Model::steel, 0.97f, 0.77f, 0.73f).body(II, body::Model::steel, 0.97f, 0.66f, 0.56f)
                 .eg(I, 0.0014f, 3.5f, 0.0f, 0.21f).eg(II, 0.0018f, 2.95f, 0.0f, 0.18f).detune(2.6f).mix(0.46f));
    add("Drone Pluck", "Plucked", { "ring-mod", "resonance", "aftertouch" },
        "A bright string strike recedes into a quiet resonant drone with faint ring color. Independently damping plucked partials animate the opening; pressure raises the remaining circuit bed after the upper pick disappears.",
        plucked().wave(I, false, true, 0.22f).pw(I, 0.64f).wave(II, true, false)
                 .hpf(I, 40.0f).hpf(II, 420.0f).lpf(I, 980.0f, 0.26f).lpf(II, 2600.0f, 0.18f)
                 .feg(I, 0.0f, 0.48f, 0.003f, 1.4f, 0.50f).feg(II, 0.0f, 0.40f, 0.001f, 0.28f, 0.08f)
                 .eg(I, 0.0025f, 3.0f, 0.18f, 0.90f).eg(II, 0.001f, 0.40f, 0.0f, 0.10f)
                 .level(I, 0.90f, 0.30f).touch(I, 0.16f, 0.68f, 0.20f, 0.36f)
                 .ring(110.0f, 0.0f, 0.001f, 1.0f, 0.09f).mix(0.21f).res(0.20f)
                 .body(I, body::Model::steel, 0.68f, 0.53f, 1.48f).body(II, body::Model::steel, 0.50f, 0.84f, 0.32f)
                 .eg(I, 0.002f, 3.6f, 0.18f, 0.95f).eg(II, 0.001f, 0.31f, 0.0f, 0.075f).mix(0.17f));
    add("Nylon Pluck", "Plucked", { "sine", "velocity", "noise" },
        "A warm plucked nylon-string spectrum carries a soft finger attack and clear lower body. The excitation notch and fast upper-partial damping keep the sound round as it decays, without chorus or detuning.",
        plucked().wave(I, true, false, 0.35f).wave(II, false, true, 0.0f, 0.06f).pw(II, 0.55f)
                 .hpf(I, 35.0f).hpf(II, 350.0f).lpf(I, 1200.0f, 0.035f).lpf(II, 2800.0f, 0.06f)
                 .level(I, 0.65f, 0.34f).level(II, 0.82f, 0.10f)
                 .feg(I, 0.0f, 0.21f, 0.0035f, 1.3f, 0.3f).feg(II, 0.0f, 0.12f, 0.001f, 0.12f, 0.04f)
                 .eg(I, 0.0035f, 4.1f, 0.0f, 0.75f).eg(II, 0.001f, 0.18f, 0.0f, 0.05f)
                 .touch(I, 0.12f, 0.72f, 0.0f, 0.0f).touch(II, 0.08f, 0.90f, 0.0f, 0.0f)
                 .mix(0.20f).res(0.025f).detune(0.0f)
                 .body(I, body::Model::nylon, 1.0f, 0.38f, 0.92f).body(II, body::Model::nylon, 0.48f, 0.55f, 0.29f)
                 .eg(I, 0.0028f, 4.6f, 0.0f, 0.61f).eg(II, 0.001f, 0.14f, 0.0f, 0.04f).mix(0.15f));
    add("Steel Pluck", "Plucked", { "feet-layer", "velocity", "dual-rank" },
        "Paired 8-foot and 4-foot steel-string courses open with bright, independently fading harmonics. The octave course recedes before the full lower body, with a tiny tuning offset adding restrained beating.",
        plucked().wave(I, true, true, 0.10f).pw(I, 0.58f)
                 .wave(II, false, true, 0.12f).pw(II, 0.73f).feet(II, f4)
                 .hpf(I, 55.0f).hpf(II, 190.0f).lpf(I, 2150.0f, 0.08f).lpf(II, 6100.0f, 0.10f)
                 .level(I, 0.70f, 0.34f).level(II, 0.61f, 0.20f)
                 .feg(I, 0.0f, 0.31f, 0.002f, 1.4f, 0.34f).feg(II, 0.0f, 0.26f, 0.0012f, 0.65f, 0.25f)
                 .eg(I, 0.002f, 4.9f, 0.0f, 0.85f).eg(II, 0.0012f, 2.8f, 0.0f, 0.55f)
                 .touch(I, 0.20f, 0.74f, 0.0f, 0.0f).touch(II, 0.28f, 0.86f, 0.0f, 0.0f)
                 .mix(0.29f).detune(1.5f)
                 .body(I, body::Model::steel, 1.0f, 0.70f, 1.37f).body(II, body::Model::steel, 0.97f, 0.84f, 0.76f)
                 .eg(I, 0.0017f, 5.6f, 0.0f, 0.74f).eg(II, 0.0011f, 2.55f, 0.0f, 0.43f).mix(0.24f).detune(0.8f));
    add("Muted Pluck", "Plucked", { "percussive", "velocity", "noise" },
        "A palm-muted string spectrum carries a brief pick click and a rounded low note. Fast partial losses and short body damping make repeated strokes distinct even while the keys stay held.",
        plucked().wave(I, false, true, 0.12f).pw(I, 0.58f)
                 .wave(II, false, true, 0.0f, 0.045f).pw(II, 0.80f)
                 .hpf(I, 35.0f).hpf(II, 1200.0f).lpf(I, 760.0f, 0.12f).lpf(II, 4500.0f, 0.08f)
                 .feg(I, 0.0f, 0.34f, 0.0018f, 0.26f, 0.06f).feg(II, 0.0f, 0.20f, 0.001f, 0.075f, 0.035f)
                 .eg(I, 0.0018f, 0.62f, 0.0f, 0.09f).eg(II, 0.001f, 0.11f, 0.0f, 0.04f)
                 .level(I, 0.91f, 0.30f).level(II, 0.82f, 0.10f)
                 .touch(I, 0.19f, 0.76f, 0.0f, 0.0f).touch(II, 0.25f, 0.94f, 0.0f, 0.0f)
                 .mix(0.19f).res(0.08f)
                 .body(I, body::Model::steel, 0.96f, 0.31f, 0.25f).body(II, body::Model::clav, 0.43f, 0.77f, 0.25f)
                 .eg(I, 0.0015f, 0.72f, 0.0f, 0.065f).eg(II, 0.001f, 0.085f, 0.0f, 0.03f).mix(0.15f));
    add("Kalimba", "Plucked", { "sine", "feet-layer", "percussive", "velocity" },
        "A plucked high-register tine produces a rounded pitched body and a short metallic ping. Sparse upper modes fall away independently after the attack; stronger strikes bring them forward without a held buzz.",
        plucked().wave(I, false, false, 0.96f).feet(I, f4)
                 .wave(II, false, true, 0.54f).pw(II, 0.5f).feet(II, f2)
                 .hpf(II, 1700.0f).lpf(II, 7800.0f, 0.06f)
                 .level(I, 0.0f, 0.30f).level(II, 0.20f, 0.08f)
                 .eg(I, 0.002f, 3.25f, 0.0f, 0.70f).eg(II, 0.001f, 0.38f, 0.0f, 0.12f)
                 .feg(II, 0.0f, 0.17f, 0.001f, 0.19f, 0.075f)
                 .touch(I, 0.0f, 0.78f, 0.0f, 0.0f).touch(II, 0.14f, 0.94f, 0.0f, 0.0f)
                 .mix(0.23f).res(0.025f).detune(0.0f)
                 .body(I, body::Model::tine, 1.0f, 0.59f, 0.84f).body(II, body::Model::celesta, 0.79f, 0.80f, 0.32f)
                 .level(I, 0.0f, 0.10f).touch(I, 0.0f, 1.0f, 0.0f, 0.0f)
                 .eg(I, 0.0016f, 3.7f, 0.0f, 0.59f).eg(II, 0.001f, 0.28f, 0.0f, 0.09f).mix(0.17f));
    add("Zither", "Plucked", { "chorus", "velocity", "dual-rank" },
        "Two bright steel-string courses have long unequal body decays and independent upper-partial losses. A restrained pitch offset and shallow chorus add a gentle ensemble shimmer after the clearly plucked onset.",
        plucked().wave(I, true, false, 0.12f).wave(II, false, true, 0.06f).pw(II, 0.76f)
                 .hpf(I, 75.0f).hpf(II, 140.0f).lpf(I, 2600.0f, 0.08f).lpf(II, 4400.0f, 0.10f)
                 .level(I, 0.82f, 0.33f).level(II, 0.70f, 0.23f)
                 .feg(I, 0.0f, 0.25f, 0.0018f, 1.4f, 0.65f).feg(II, 0.0f, 0.31f, 0.001f, 0.65f, 0.42f)
                 .eg(I, 0.0018f, 5.6f, 0.0f, 1.8f).eg(II, 0.001f, 2.7f, 0.0f, 1.15f)
                 .touch(I, 0.18f, 0.73f, 0.0f, 0.0f).touch(II, 0.24f, 0.84f, 0.0f, 0.0f)
                 .mix(0.34f).detune(2.2f).chorus(0.16f, 0.28f)
                 .body(I, body::Model::steel, 0.98f, 0.78f, 1.71f).body(II, body::Model::steel, 0.95f, 0.67f, 1.03f)
                 .eg(I, 0.0015f, 6.3f, 0.0f, 1.9f).eg(II, 0.0012f, 3.1f, 0.0f, 1.1f).mix(0.31f).detune(1.5f));
    add("Dulcimer Pluck", "Plucked", { "feet-layer", "percussive", "velocity" },
        "A firm steel-string pluck has a bright short octave course and a longer lower body. Upper modes lose energy independently, leaving a clear ringing string after the plectrum edge has faded.",
        plucked().wave(I, true, false, 0.15f).wave(II, false, true, 0.04f).feet(II, f4).pw(II, 0.79f)
                 .hpf(I, 65.0f).hpf(II, 520.0f).lpf(I, 1900.0f, 0.10f).lpf(II, 5800.0f, 0.14f)
                 .level(I, 0.82f, 0.33f).level(II, 0.78f, 0.11f)
                 .feg(I, 0.0f, 0.31f, 0.002f, 1.0f, 0.28f).feg(II, 0.0f, 0.38f, 0.001f, 0.25f, 0.10f)
                 .eg(I, 0.002f, 3.45f, 0.0f, 0.75f).eg(II, 0.001f, 0.58f, 0.0f, 0.16f)
                 .touch(I, 0.20f, 0.75f, 0.0f, 0.0f).touch(II, 0.29f, 0.96f, 0.0f, 0.0f)
                 .mix(0.27f).detune(0.6f)
                 .body(I, body::Model::steel, 0.98f, 0.70f, 1.12f).body(II, body::Model::steel, 0.85f, 0.89f, 0.42f)
                 .eg(I, 0.0017f, 3.95f, 0.0f, 0.62f).eg(II, 0.001f, 0.44f, 0.0f, 0.12f).mix(0.20f).detune(0.2f));
    add("Pressure Pluck", "Plucked", { "aftertouch", "velocity", "dual-rank" },
        "A natural-style picked string onset settles over a quiet held circuit bed. The high string modes fade independently; pressure swells and colors the remaining body after the short pick has disappeared.",
        plucked().wave(I, true, false, 0.24f).wave(II, false, true).pw(II, 0.70f)
                 .hpf(I, 35.0f).hpf(II, 650.0f).lpf(I, 1050.0f, 0.14f).lpf(II, 3400.0f, 0.12f)
                 .level(I, 0.82f, 0.23f).level(II, 0.86f, 0.11f)
                 .feg(I, 0.0f, 0.36f, 0.003f, 1.2f, 0.35f).feg(II, 0.0f, 0.31f, 0.001f, 0.26f, 0.08f)
                 .eg(I, 0.0025f, 3.0f, 0.14f, 0.68f).eg(II, 0.0012f, 0.42f, 0.0f, 0.10f)
                 .touch(I, 0.16f, 0.62f, 0.32f, 0.68f).touch(II, 0.26f, 0.94f, 0.0f, 0.0f)
                 .mix(0.22f).res(0.09f)
                 .body(I, body::Model::nylon, 0.70f, 0.49f, 1.26f).body(II, body::Model::steel, 0.67f, 0.79f, 0.36f)
                 .eg(I, 0.002f, 3.6f, 0.14f, 0.72f).eg(II, 0.001f, 0.33f, 0.0f, 0.075f).mix(0.17f));
}
