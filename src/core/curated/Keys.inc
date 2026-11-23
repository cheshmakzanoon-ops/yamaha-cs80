// Keys voicings. Included inside namespace curated by CuratedLibrary.inc.
// Existing program names and insertion order are stable host/MIDI identities.
// Authored struck bodies have independent partial losses; circuit ranks retain
// separate amplitude envelopes, filters and touch. Modal profiles are designed
// interpretations, not sampled instruments or calibrated physical replicas.
Program keys()
{
    Program x;
    x.wave(I, false, true, 0.58f).pw(I, 0.57f).feet(I, f8)
     .wave(II, false, true, 0.12f).pw(II, 0.71f).feet(II, f8)
     .hpf(I, 30.0f).lpf(I, 1450.0f, 0.055f).level(I, 0.46f, 0.24f)
     .hpf(II, 230.0f).lpf(II, 4300.0f, 0.14f).level(II, 0.96f, 0.035f)
     .feg(I, 0.0f, 0.20f, 0.0025f, 1.65f, 0.20f)
     .feg(II, 0.0f, 0.46f, 0.0015f, 0.65f, 0.10f)
     .eg(I, 0.0025f, 6.2f, 0.0f, 0.34f).eg(II, 0.0015f, 1.25f, 0.0f, 0.10f)
     .touch(I, 0.20f, 0.74f, 0.0f, 0.0f).touch(II, 0.48f, 1.0f, 0.0f, 0.0f)
     .sub(subSine, 5.0f, 0.0f).subAt(0.0f, 0.0f)
     .mix(0.37f).detune(0.35f).res(0.05f)
     .g(P_voiceDispersion, 0.08f).g(P_drift, 0.025f).g(P_analogNoise, 0.0f).volume(0.6f);
    return x;
}

void buildKeys(Builder& add)
{
    add("Electric Piano I", "Keys", { "velocity", "sine", "dual-rank" },
        "A warm tine body with individually fading overtones and a velocity-sensitive upper bark. The short strike falls away into a rounded ringing fundamental; key release damps the note.",
        keys()
              .body(I, body::Model::tine, 0.98f, 0.46f, 1.45f).body(II, body::Model::tine, 0.72f, 0.80f, 0.42f)
              .level(I, 0.46f, 0.10f).touch(I, 0.20f, 1.0f, 0.0f, 0.0f)
              .eg(I, 0.0028f, 6.8f, 0.0f, 0.26f).eg(II, 0.0012f, 0.95f, 0.0f, 0.075f).mix(0.25f));
    add("Electric Piano II", "Keys", { "velocity", "chorus", "feet-layer" },
        "A covered tine body supports a lighter 4-foot strike with a longer, softer overtone decay than Electric Piano I. Shallow chorus opens the stereo image while the dry attack remains defined.",
        keys().wave(I, false, true, 0.58f).level(I, 0.40f, 0.25f)
              .wave(II, false, true, 0.72f).feet(II, f4).pw(II, 0.70f)
              .hpf(II, 480.0f).lpf(II, 5200.0f, 0.08f).level(II, 0.35f, 0.025f)
              .eg(I, 0.0035f, 6.6f, 0.0f, 0.38f).eg(II, 0.0018f, 1.65f, 0.0f, 0.12f)
              .feg(II, 0.0f, 0.28f, 0.0015f, 0.45f, 0.12f)
              .touch(II, 0.25f, 1.0f, 0.0f, 0.0f).mix(0.30f).detune(0.65f).chorus(0.15f, 0.38f)
              .body(I, body::Model::tine, 0.97f, 0.49f, 1.85f).body(II, body::Model::tine, 0.76f, 0.65f, 0.70f)
              .level(I, 0.40f, 0.10f).touch(I, 0.20f, 1.0f, 0.0f, 0.0f)
              .eg(I, 0.003f, 7.2f, 0.0f, 0.34f).touch(II, 0.18f, 0.92f, 0.0f, 0.0f).mix(0.25f));
    add("Tine Piano", "Keys", { "feet-layer", "velocity", "sine" },
        "A concentrated tine resonance beneath a separately controlled third-harmonic strike. The struck partials soften at their own rates; the upper rank follows the keyboard and gives firm notes a clear bell-like attack.",
        keys().wave(I, false, true, 0.84f).level(I, 0.13f, 0.22f).lpf(I, 1200.0f, 0.04f)
              .wave(II, false, false, 0.97f).feet(II, f223).level(II, 0.0f, 0.01f)
              .eg(I, 0.0025f, 6.9f, 0.0f, 0.35f).eg(II, 0.0012f, 2.1f, 0.0f, 0.10f)
              .touch(I, 0.08f, 0.72f, 0.0f, 0.0f).touch(II, 0.0f, 1.0f, 0.0f, 0.0f)
              .mix(0.31f).detune(0.0f)
              .body(I, body::Model::tine, 0.98f, 0.36f, 1.65f)
              .eg(I, 0.002f, 7.4f, 0.0f, 0.29f).eg(II, 0.001f, 1.85f, 0.0f, 0.085f)
              .touch(I, 0.09f, 0.78f, 0.0f, 0.0f).mix(0.28f));
    add("Clavichord I", "Keys", { "velocity", "percussive" },
        "A dry tangent-excited string spectrum with a short upper snap. Its high partials damp independently, leaving a small clear body; velocity changes the edge as well as the level.",
        keys().wave(I, false, true).wave(II, false, true, 0.0f, 0.008f)
              .pw(I, 0.76f).pw(II, 0.84f).hpf(I, 110.0f).hpf(II, 460.0f)
              .lpf(I, 2200.0f, 0.12f).lpf(II, 4600.0f, 0.08f)
              .level(I, 1.0f, 0.33f).level(II, 0.94f, 0.035f)
              .feg(I, 0.0f, 0.40f, 0.0015f, 0.55f, 0.10f).feg(II, 0.0f, 0.24f, 0.001f, 0.14f, 0.06f)
              .eg(I, 0.0018f, 2.4f, 0.0f, 0.16f).eg(II, 0.001f, 0.40f, 0.0f, 0.055f)
              .touch(I, 0.25f, 0.78f, 0.0f, 0.0f).touch(II, 0.32f, 0.92f, 0.0f, 0.0f)
              .mix(0.31f).detune(0.0f)
              .body(I, body::Model::clav, 0.97f, 0.58f, 0.68f).body(II, body::Model::clav, 0.68f, 0.86f, 0.32f)
              .eg(I, 0.0015f, 2.7f, 0.0f, 0.12f).mix(0.23f));
    add("Clavichord II", "Keys", { "velocity", "resonance", "percussive" },
        "A darker tangent string with a shorter, nasal decay. A restrained resonant pulse remains in the attack, while independent upper-partial damping keeps the held tail from becoming an electronic buzz.",
        keys().wave(I, false, true).wave(II, false, true).pw(I, 0.70f).pw(II, 0.81f)
              .hpf(I, 100.0f).hpf(II, 520.0f).lpf(I, 1250.0f, 0.34f).lpf(II, 2800.0f, 0.22f)
              .level(I, 1.0f, 0.34f).level(II, 0.94f, 0.03f).res(0.22f)
              .feg(I, 0.0f, 0.58f, 0.002f, 0.72f, 0.11f).feg(II, 0.0f, 0.42f, 0.001f, 0.26f, 0.07f)
              .eg(I, 0.002f, 2.0f, 0.0f, 0.14f).eg(II, 0.0012f, 0.52f, 0.0f, 0.06f)
              .touch(I, 0.24f, 0.78f, 0.0f, 0.0f).touch(II, 0.38f, 0.92f, 0.0f, 0.0f)
              .mix(0.32f).detune(0.25f)
              .body(I, body::Model::clav, 0.92f, 0.34f, 0.46f).body(II, body::Model::clav, 0.52f, 0.69f, 0.30f)
              .eg(I, 0.0017f, 2.3f, 0.0f, 0.105f).mix(0.25f).detune(0.0f));
    add("Funky Clavichord", "Keys", { "velocity", "resonance", "percussive" },
        "A tangent-string snap and compact resonant wah share the attack. The dry string body adds definition between the filter accents; hard playing brings out the upper rank without leaving a held pad.",
        keys().wave(I, false, true).pw(I, 0.66f).wave(II, true, false)
              .hpf(I, 65.0f).hpf(II, 210.0f).lpf(I, 730.0f, 0.35f).lpf(II, 1180.0f, 0.42f)
              .feg(I, 0.0f, 0.61f, 0.002f, 0.62f, 0.08f).feg(II, 0.0f, 0.74f, 0.001f, 0.28f, 0.06f)
              .eg(I, 0.0018f, 1.65f, 0.0f, 0.12f).eg(II, 0.001f, 0.52f, 0.0f, 0.06f)
              .level(I, 1.0f, 0.24f).level(II, 0.95f, 0.025f)
              .touch(I, 0.38f, 0.84f, 0.0f, 0.0f).touch(II, 0.54f, 1.0f, 0.0f, 0.0f)
              .mix(0.39f).res(0.27f).detune(0.0f)
              .body(I, body::Model::clav, 0.66f, 0.67f, 0.38f).body(II, body::Model::clav, 0.25f, 0.86f, 0.28f)
              .eg(I, 0.0015f, 1.85f, 0.0f, 0.095f).feg(II, 0.0f, 0.70f, 0.001f, 0.23f, 0.05f).mix(0.32f));
    add("Harpsichord I", "Keys", { "percussive", "dual-rank" },
        "A bright quill-plucked string has a notched harmonic spectrum and individually fading upper partials. The short second rank marks the quill release; nearly even strike dynamics and prompt key damping keep the register articulate.",
        keys().wave(I, true, true).pw(I, 0.68f).wave(II, false, true, 0.0f, 0.008f).pw(II, 0.82f)
              .hpf(I, 85.0f).hpf(II, 850.0f).lpf(I, 4900.0f, 0.06f).lpf(II, 8000.0f, 0.04f)
              .feg(I, 0.0f, 0.12f, 0.001f, 0.9f, 0.08f).feg(II, 0.0f, 0.16f, 0.001f, 0.12f, 0.04f)
              .eg(I, 0.0012f, 3.2f, 0.0f, 0.13f).eg(II, 0.001f, 0.25f, 0.0f, 0.045f)
              .level(I, 0.86f, 0.96f).level(II, 0.78f, 0.82f)
              .touch(Both, 0.02f, 0.035f, 0.0f, 0.0f).mix(0.21f).detune(0.0f).res(0.03f)
              .body(I, body::Model::harpsichord, 1.0f, 0.67f, 1.05f).body(II, body::Model::harpsichord, 0.90f, 0.89f, 0.32f)
              .eg(I, 0.001f, 3.65f, 0.0f, 0.095f).eg(II, 0.001f, 0.21f, 0.0f, 0.035f).mix(0.16f));
    add("Harpsichord II", "Keys", { "percussive", "feet-layer" },
        "Coupled quill-plucked 8-foot and 4-foot strings with independent harmonic losses. The octave course speaks brightly then recedes, while the lower course rings; the two registers keep nearly even strike dynamics.",
        keys().wave(I, true, true).wave(II, false, true).pw(I, 0.69f).pw(II, 0.76f).feet(II, f4)
              .hpf(I, 75.0f).hpf(II, 180.0f).lpf(I, 4300.0f, 0.05f).lpf(II, 7200.0f, 0.05f)
              .feg(I, 0.0f, 0.14f, 0.001f, 1.1f, 0.10f).feg(II, 0.0f, 0.20f, 0.001f, 0.50f, 0.07f)
              .eg(I, 0.0013f, 3.5f, 0.0f, 0.16f).eg(II, 0.001f, 2.1f, 0.0f, 0.12f)
              .level(I, 0.88f, 0.94f).level(II, 0.70f, 0.86f)
              .touch(Both, 0.025f, 0.045f, 0.0f, 0.0f).mix(0.32f).detune(0.0f).res(0.035f)
              .body(I, body::Model::harpsichord, 1.0f, 0.58f, 1.25f).body(II, body::Model::harpsichord, 0.98f, 0.77f, 0.72f)
              .eg(I, 0.0012f, 4.1f, 0.0f, 0.12f).eg(II, 0.001f, 2.35f, 0.0f, 0.09f).mix(0.28f));
    add("Lute Harpsichord", "Keys", { "percussive" },
        "A covered buff-stop string with rapidly damped upper partials and a restrained quill edge. The low body keeps a definite plucked onset, then settles into a short muted decay with little velocity variation.",
        keys().wave(I, true, false, 0.12f).wave(II, false, true).pw(II, 0.72f)
              .hpf(I, 45.0f).hpf(II, 420.0f).lpf(I, 1250.0f, 0.05f).lpf(II, 2500.0f, 0.08f)
              .feg(I, 0.0f, 0.20f, 0.002f, 0.60f, 0.09f).feg(II, 0.0f, 0.16f, 0.001f, 0.18f, 0.05f)
              .eg(I, 0.0018f, 1.95f, 0.0f, 0.12f).eg(II, 0.001f, 0.28f, 0.0f, 0.045f)
              .level(I, 0.80f, 0.93f).level(II, 0.65f, 0.70f)
              .touch(Both, 0.02f, 0.04f, 0.0f, 0.0f).mix(0.19f).detune(0.0f).res(0.025f)
              .body(I, body::Model::harpsichord, 0.97f, 0.17f, 0.36f).body(II, body::Model::harpsichord, 0.72f, 0.40f, 0.25f)
              .eg(I, 0.0015f, 2.25f, 0.0f, 0.085f).eg(II, 0.001f, 0.20f, 0.0f, 0.035f).mix(0.14f));
    add("Toy Piano", "Keys", { "feet-layer", "percussive", "velocity" },
        "Small high-register metal modes produce a slightly hollow struck body and brittle upper ping. The bright modes disappear at different rates, leaving a simple ringing center that damps on key release.",
        keys().wave(I, false, false, 0.90f).feet(I, f4).level(I, 0.0f, 0.30f)
              .wave(II, false, true, 0.62f).feet(II, f2).pw(II, 0.82f)
              .hpf(II, 1500.0f).lpf(II, 9000.0f, 0.10f).level(II, 0.38f, 0.025f)
              .eg(I, 0.0018f, 2.65f, 0.0f, 0.58f).eg(II, 0.001f, 0.48f, 0.0f, 0.065f)
              .touch(I, 0.0f, 0.74f, 0.0f, 0.0f).touch(II, 0.16f, 0.90f, 0.0f, 0.0f)
              .mix(0.28f).detune(0.0f)
              .body(I, body::Model::celesta, 0.94f, 0.82f, 0.56f).body(II, body::Model::bell, 0.55f, 0.88f, 0.29f)
              .eg(I, 0.0014f, 2.8f, 0.0f, 0.38f).mix(0.19f));
    add("Honky Keys", "Keys", { "detune", "velocity", "dual-rank" },
        "Two struck-string bodies have controlled upright-like beating and slightly stretched upper partials. Harder notes reveal a firmer hammer spectrum; the detuned ranks decay independently into a warm, uneven shimmer.",
        keys().wave(I, true, false, 0.30f).wave(II, false, true, 0.22f).pw(II, 0.56f)
              .hpf(Both, 45.0f).lpf(I, 2000.0f, 0.06f).lpf(II, 2700.0f, 0.05f)
              .level(I, 0.60f, 0.34f).level(II, 0.66f, 0.30f)
              .feg(I, 0.0f, 0.32f, 0.002f, 1.1f, 0.20f).feg(II, 0.0f, 0.27f, 0.003f, 0.8f, 0.16f)
              .eg(I, 0.0025f, 5.5f, 0.0f, 0.26f).eg(II, 0.0035f, 4.7f, 0.0f, 0.23f)
              .touch(I, 0.25f, 0.74f, 0.0f, 0.0f).touch(II, 0.30f, 0.78f, 0.0f, 0.0f)
              .mix(0.43f).detune(7.5f).g(P_drift, 0.015f)
              .body(I, body::Model::piano, 0.98f, 0.65f, 1.10f).body(II, body::Model::piano, 0.98f, 0.49f, 0.78f)
              .eg(I, 0.0018f, 6.1f, 0.0f, 0.19f).eg(II, 0.0024f, 5.5f, 0.0f, 0.17f)
              .detune(5.7f).mix(0.46f).g(P_voiceDispersion, 0.035f));
    add("Vibes Keys", "Keys", { "sub-mod", "sine", "percussive", "velocity" },
        "A metal-bar body carries independently decaying higher modes, with a separately voiced fourth-harmonic strike. Gentle 5 Hz amplitude motion shades the ringing tail; release damps the body and firm strikes brighten the onset.",
        keys().wave(I, false, false, 0.98f).wave(II, false, false, 0.58f).feet(II, f2)
              .level(I, 0.0f, 0.32f).level(II, 0.0f, 0.025f)
              .eg(I, 0.003f, 6.5f, 0.0f, 0.75f).eg(II, 0.0015f, 1.55f, 0.0f, 0.25f)
              .touch(I, 0.0f, 0.75f, 0.0f, 0.0f).touch(II, 0.0f, 0.90f, 0.0f, 0.0f)
              .sub(subSine, 5.0f, 0.0f, 0.0f, 0.16f).mix(0.27f).detune(0.0f)
              .body(I, body::Model::vibes, 0.95f, 0.38f, 1.65f)
              .eg(I, 0.0025f, 7.0f, 0.0f, 0.57f).eg(II, 0.0012f, 1.40f, 0.0f, 0.19f).mix(0.22f));
    add("Celesta", "Keys", { "sine", "feet-layer", "percussive", "velocity" },
        "A soft hammer excites a clear high-register metal-bar body. Sparse upper modes lose energy independently above the fundamental; the lighter second rank supplies a brief glint without a permanent octave buzz.",
        keys().wave(I, false, false, 0.95f).feet(I, f4)
              .wave(II, false, true, 0.45f).feet(II, f2).pw(II, 0.5f)
              .hpf(II, 1800.0f).lpf(II, 7600.0f, 0.04f)
              .level(I, 0.0f, 0.29f).level(II, 0.28f, 0.03f)
              .eg(I, 0.002f, 3.7f, 0.0f, 0.48f).eg(II, 0.0012f, 0.95f, 0.0f, 0.10f)
              .touch(I, 0.0f, 0.78f, 0.0f, 0.0f).touch(II, 0.12f, 0.86f, 0.0f, 0.0f)
              .mix(0.30f).detune(0.0f)
              .body(I, body::Model::celesta, 1.0f, 0.47f, 1.24f).body(II, body::Model::celesta, 0.68f, 0.74f, 0.48f)
              .eg(I, 0.0018f, 4.3f, 0.0f, 0.39f).mix(0.19f));
    add("Saw Pulse Piano", "Keys", { "velocity", "dual-rank" },
        "A struck-string body joins the deliberately synthetic saw-and-pulse attack. Independent partial losses add a falling piano-like tone beneath the circuit filter motion, retaining the bright electronic character of the name.",
        keys().wave(I, true, false, 0.18f).wave(II, false, true).pw(II, 0.63f)
              .hpf(I, 45.0f).hpf(II, 270.0f).lpf(I, 1800.0f, 0.08f).lpf(II, 3400.0f, 0.12f)
              .level(I, 0.80f, 0.32f).level(II, 0.98f, 0.03f)
              .feg(I, 0.0f, 0.37f, 0.003f, 1.5f, 0.20f).feg(II, 0.0f, 0.48f, 0.0015f, 0.50f, 0.12f)
              .eg(I, 0.003f, 5.2f, 0.0f, 0.28f).eg(II, 0.0015f, 1.25f, 0.0f, 0.11f)
              .touch(I, 0.28f, 0.76f, 0.0f, 0.0f).touch(II, 0.38f, 0.92f, 0.0f, 0.0f)
              .mix(0.40f).detune(0.6f)
              .body(I, body::Model::piano, 0.67f, 0.70f, 0.93f).body(II, body::Model::piano, 0.29f, 0.86f, 0.35f)
              .eg(I, 0.002f, 5.9f, 0.0f, 0.22f).eg(II, 0.0012f, 0.95f, 0.0f, 0.085f).mix(0.32f));
    add("Soft Keys", "Keys", { "velocity", "sine" },
        "A felt-like hammer spectrum excites a mellow struck-string body. Upper partials soften quickly while the low body rings, so soft chords remain warm and clear; firmer strikes still open the harmonic color.",
        keys().wave(I, false, true, 0.94f).level(I, 0.10f, 0.32f).lpf(I, 900.0f, 0.025f)
              .wave(II, false, true, 0.08f).pw(II, 0.53f).lpf(II, 1900.0f, 0.04f)
              .level(II, 0.42f, 0.02f).eg(I, 0.009f, 6.1f, 0.0f, 0.34f).eg(II, 0.004f, 0.8f, 0.0f, 0.10f)
              .feg(I, 0.0f, 0.10f, 0.007f, 1.0f, 0.20f).feg(II, 0.0f, 0.16f, 0.003f, 0.30f, 0.10f)
              .touch(I, 0.08f, 0.70f, 0.0f, 0.0f).touch(II, 0.16f, 0.76f, 0.0f, 0.0f)
              .mix(0.23f).detune(0.0f)
              .body(I, body::Model::piano, 0.99f, 0.19f, 0.82f).body(II, body::Model::piano, 0.80f, 0.37f, 0.36f)
              .eg(I, 0.0045f, 6.8f, 0.0f, 0.28f).eg(II, 0.002f, 0.72f, 0.0f, 0.075f).mix(0.16f));
    add("Chorus Keys", "Keys", { "chorus", "velocity" },
        "A bright tine attack resolves into a smooth electric body with independently softening overtones. Shallow chorus motion shades the long decay while the dry strike keeps chord changes clear.",
        keys().wave(I, false, true, 0.70f).lpf(I, 2400.0f, 0.05f).level(I, 0.38f, 0.34f)
              .wave(II, true, false, 0.12f).lpf(II, 6200.0f, 0.06f).hpf(II, 900.0f)
              .eg(I, 0.003f, 5.8f, 0.0f, 0.32f).eg(II, 0.0015f, 1.0f, 0.0f, 0.10f)
              .feg(II, 0.0f, 0.30f, 0.001f, 0.26f, 0.10f)
              .touch(II, 0.24f, 0.90f, 0.0f, 0.0f).mix(0.33f).detune(0.45f).chorus(0.19f, 0.52f)
              .body(I, body::Model::tine, 0.97f, 0.65f, 1.33f).body(II, body::Model::tine, 0.63f, 0.88f, 0.39f)
              .eg(I, 0.002f, 6.4f, 0.0f, 0.28f).mix(0.24f).chorus(0.16f, 0.48f));
    add("Stage Keys", "Keys", { "velocity", "dual-rank" },
        "A firm hammer excites a full struck-string spectrum with slight stiffness and controlled unison beating. The bright partials fall away at different rates above a long rounded body; key release damps the strings promptly.",
        keys().wave(I, true, false, 0.48f).lpf(I, 1650.0f, 0.07f).level(I, 0.48f, 0.35f)
              .wave(II, false, true, 0.18f).pw(II, 0.62f).lpf(II, 4200.0f, 0.10f)
              .hpf(II, 240.0f).level(II, 0.96f, 0.025f)
              .feg(I, 0.0f, 0.27f, 0.0025f, 1.6f, 0.24f).feg(II, 0.0f, 0.44f, 0.001f, 0.42f, 0.10f)
              .eg(I, 0.0025f, 5.8f, 0.0f, 0.35f).eg(II, 0.0013f, 1.55f, 0.0f, 0.10f)
              .touch(I, 0.20f, 0.78f, 0.0f, 0.0f).touch(II, 0.36f, 0.95f, 0.0f, 0.0f)
              .mix(0.40f).detune(0.3f)
              .body(I, body::Model::piano, 1.0f, 0.63f, 1.28f).body(II, body::Model::piano, 0.89f, 0.83f, 0.43f)
              .eg(I, 0.0018f, 6.6f, 0.0f, 0.22f).eg(II, 0.0012f, 1.25f, 0.0f, 0.075f)
              .mix(0.22f).detune(0.0f).g(P_voiceDispersion, 0.035f));
    add("Velocity Keys", "Keys", { "velocity", "dual-rank" },
        "Wide hammer dynamics reveal a soft struck-string body at low velocity and a firm, bright upper attack on hard strikes. Partial-specific losses let the brilliant opening settle naturally instead of leaving a fixed bright tail.",
        keys().wave(I, false, true, 0.66f).level(I, 0.35f, 0.09f)
              .wave(II, true, true, 0.06f).pw(II, 0.61f).hpf(II, 330.0f).lpf(II, 2200.0f, 0.14f)
              .level(II, 0.94f, 0.025f).feg(II, 0.0f, 0.38f, 0.001f, 0.48f, 0.12f)
              .eg(I, 0.003f, 4.6f, 0.0f, 0.38f).eg(II, 0.001f, 1.25f, 0.0f, 0.10f)
              .touch(I, 0.20f, 0.96f, 0.0f, 0.0f).touch(II, 0.66f, 1.0f, 0.0f, 0.0f)
              .mix(0.44f).detune(0.0f)
              .body(I, body::Model::piano, 0.98f, 0.48f, 1.03f).body(II, body::Model::piano, 0.88f, 0.92f, 0.36f)
              .eg(I, 0.0022f, 5.5f, 0.0f, 0.25f).eg(II, 0.001f, 0.95f, 0.0f, 0.075f).mix(0.29f));
    add("Dulcimer", "Keys", { "feet-layer", "percussive", "velocity" },
        "A bright hammered steel-string spectrum rings in two courses. Rapidly fading high partials and small pitch beating give way to a clearer lower body; the shorter upper course emphasizes hard mallet strokes.",
        keys().wave(I, true, false, 0.24f).wave(II, false, true, 0.08f).feet(II, f4).pw(II, 0.78f)
              .hpf(I, 70.0f).hpf(II, 550.0f).lpf(I, 2400.0f, 0.08f).lpf(II, 6800.0f, 0.12f)
              .level(I, 0.68f, 0.30f).level(II, 0.82f, 0.025f)
              .feg(I, 0.0f, 0.22f, 0.0015f, 1.3f, 0.4f).feg(II, 0.0f, 0.32f, 0.001f, 0.35f, 0.16f)
              .eg(I, 0.0015f, 5.1f, 0.0f, 1.1f).eg(II, 0.001f, 1.3f, 0.0f, 0.28f)
              .touch(I, 0.18f, 0.78f, 0.0f, 0.0f).touch(II, 0.26f, 0.94f, 0.0f, 0.0f)
              .mix(0.35f).detune(1.0f)
              .body(I, body::Model::steel, 0.98f, 0.78f, 1.62f).body(II, body::Model::steel, 0.89f, 0.88f, 0.56f)
              .eg(I, 0.0013f, 5.7f, 0.0f, 0.94f).eg(II, 0.001f, 1.15f, 0.0f, 0.22f).mix(0.26f).detune(0.6f));
    add("Reed Piano", "Keys", { "velocity", "dual-rank" },
        "A vibrating-reed spectrum gives hard notes a broad harmonic bark, distinct from a tine bell. The short upper bark falls away into a warmer rounded reed body with partial-specific losses and prompt key-off damping.",
        keys().wave(I, false, true, 0.20f).pw(I, 0.59f).wave(II, false, true).pw(II, 0.69f)
              .hpf(I, 45.0f).hpf(II, 350.0f).lpf(I, 1350.0f, 0.16f).lpf(II, 2300.0f, 0.25f)
              .level(I, 0.66f, 0.32f).level(II, 0.96f, 0.02f)
              .feg(I, 0.0f, 0.24f, 0.0025f, 1.25f, 0.2f).feg(II, 0.0f, 0.48f, 0.001f, 0.40f, 0.10f)
              .eg(I, 0.003f, 5.4f, 0.0f, 0.25f).eg(II, 0.0013f, 1.05f, 0.0f, 0.09f)
              .touch(I, 0.25f, 0.77f, 0.0f, 0.0f).touch(II, 0.45f, 0.98f, 0.0f, 0.0f)
              .mix(0.40f).res(0.12f).detune(0.35f)
              .body(I, body::Model::reed, 0.98f, 0.56f, 1.07f).body(II, body::Model::reed, 0.78f, 0.88f, 0.33f)
              .eg(I, 0.0022f, 5.9f, 0.0f, 0.19f).eg(II, 0.0011f, 0.84f, 0.0f, 0.065f).mix(0.29f).detune(0.0f));
    add("Glass Keys", "Keys", { "sine", "chorus", "percussive", "feet-layer" },
        "A clear metal-mode body and brief upper glint create a glassy keyboard texture. Different mode decays evolve inside the shallow chorus, leaving a high, simple ringing tail without held sustain.",
        keys().wave(I, false, false, 0.94f).wave(II, false, false, 0.62f).feet(I, f4).feet(II, f2)
              .level(I, 0.0f, 0.32f).level(II, 0.0f, 0.025f)
              .eg(I, 0.003f, 4.4f, 0.0f, 0.95f).eg(II, 0.0015f, 1.25f, 0.0f, 0.18f)
              .touch(I, 0.0f, 0.73f, 0.0f, 0.0f).touch(II, 0.0f, 0.88f, 0.0f, 0.0f)
              .mix(0.30f).detune(0.5f).chorus(0.24f, 0.34f)
              .body(I, body::Model::celesta, 0.96f, 0.69f, 1.55f).body(II, body::Model::bell, 0.62f, 0.83f, 0.42f)
              .eg(I, 0.002f, 4.9f, 0.0f, 0.76f).mix(0.22f).chorus(0.19f, 0.32f));
    add("Tremolo Keys", "Keys", { "tremolo", "velocity", "sub-mod" },
        "A rounded tine body with a crisp upper strike carries gentle 4.8 Hz full-body tremolo. The upper modes soften independently as the body rings, with restrained stereo color from the chorus/tremolo circuit.",
        keys().wave(I, false, true, 0.76f).lpf(I, 1750.0f, 0.06f).level(I, 0.33f, 0.35f)
              .wave(II, false, true, 0.12f).pw(II, 0.65f).lpf(II, 3200.0f, 0.09f)
              .eg(I, 0.003f, 5.2f, 0.0f, 0.50f).eg(II, 0.0015f, 1.2f, 0.0f, 0.10f)
              .feg(II, 0.0f, 0.35f, 0.0015f, 0.40f, 0.12f)
              .mix(0.34f).detune(0.35f).tremolo(0.27f, 4.8f)
              .sub(subSine, 4.8f, 0.0f, 0.0f, 0.20f)
              .body(I, body::Model::tine, 0.98f, 0.43f, 1.55f).body(II, body::Model::tine, 0.70f, 0.76f, 0.42f)
              .eg(I, 0.0025f, 6.1f, 0.0f, 0.39f).mix(0.25f).detune(0.0f));
    add("Contour Keys", "Keys", { "kbd-contour", "velocity" },
        "A tine-based electric keyboard with a fuller bass and a more present treble strike. Independently damping modes preserve the evolving body across registers; keyboard contour adds level and circuit-brightness contrast.",
        keys().wave(I, false, true, 0.75f).level(I, 0.34f, 0.33f).lpf(I, 1400.0f, 0.06f)
              .wave(II, true, false, 0.08f).lpf(II, 3700.0f, 0.08f).hpf(II, 520.0f)
              .eg(I, 0.0035f, 4.8f, 0.0f, 0.42f).eg(II, 0.0015f, 1.05f, 0.0f, 0.10f)
              .feg(I, 0.0f, 0.22f, 0.003f, 1.45f, 0.20f)
              .contour(-0.08f, 0.26f, 0.12f, -0.04f).mix(0.35f).detune(0.4f)
              .body(I, body::Model::tine, 0.97f, 0.52f, 1.28f).body(II, body::Model::reed, 0.52f, 0.73f, 0.40f)
              .eg(I, 0.0027f, 5.7f, 0.0f, 0.33f).mix(0.24f).detune(0.0f));
    add("Pressure Keys", "Keys", { "aftertouch", "velocity", "dual-rank" },
        "A struck-string attack settles into a quiet sustaining circuit body. Pressing into the note raises and colors that held bed after the short hammer has faded; the natural-style struck partials provide the onset and initial decay.",
        keys().wave(I, false, true, 0.62f).lpf(I, 1450.0f, 0.12f).level(I, 0.42f, 0.22f)
              .wave(II, false, true, 0.06f).pw(II, 0.68f).level(II, 0.94f, 0.03f)
              .eg(I, 0.003f, 4.4f, 0.16f, 0.55f).eg(II, 0.0015f, 1.2f, 0.0f, 0.11f)
              .feg(I, 0.0f, 0.26f, 0.003f, 1.3f, 0.3f)
              .touch(I, 0.20f, 0.60f, 0.32f, 0.72f).touch(II, 0.30f, 0.90f, 0.0f, 0.0f)
              .mix(0.34f).detune(0.45f)
              .body(I, body::Model::piano, 0.71f, 0.47f, 1.46f).body(II, body::Model::piano, 0.82f, 0.82f, 0.36f)
              .eg(I, 0.0024f, 5.1f, 0.16f, 0.43f).eg(II, 0.0011f, 0.95f, 0.0f, 0.085f).mix(0.26f));
}
