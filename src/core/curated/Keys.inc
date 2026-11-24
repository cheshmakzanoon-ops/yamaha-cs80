// Keys voicings. Included inside namespace curated by CuratedLibrary.inc.
// Existing program names and insertion order are stable host/MIDI identities.
// Authored struck bodies have independent partial losses; circuit ranks retain
// separate amplitude envelopes, filters and touch. Body filters now honor
// the high-pass strike roles: wide main-rank low-pass settings retain the
// authored modes, while the shorter upper rank loses its low fundamentals.
// Individual keyboard contours balance bass detail and treble definition.
// The constant VCA floor is deliberately low on touch-sensitive keyboards.
// Initial touch supplies the played level; this preserves soft/firm contrast
// after the modal normalizer stops compensating quiet strikes. Quill keyboards
// keep their fixed registration and deliberately near-flat velocity response.
// A small held VCA pedestal on fully decaying main bodies lets their own
// partial losses retain aftersound. Mixed circuit ranks still decay to zero;
// short release times keep key damping distinct from pedal-held resonance.
// Modal profiles are designed
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
        "A warm tine body balances a rounded second harmonic with velocity-sensitive upper bark. The pickup overtones soften with the tine's motion, while a short strike falls away into the ringing fundamental; key release damps the note.",
        // Tine size is pickup alignment, not pitch. The main rank favors a
        // rounder second harmonic; the brief upper strike has a sharper edge.
        keys()
              .level(I, 0.46f, 0.055f).hpf(I, 25.0f).lpf(I, 6400.0f, 0.025f).hpf(II, 700.0f)
              .lpf(II, 8400.0f, 0.04f).body(I, body::Model::tine, 1.0f, 0.86f, 1.36f)
              .body(II, body::Model::tine, 0.94f, 0.84f, 0.35f).bodySize(I, 2.00f).bodySize(II, 0.85f)
              .feg(I, 0.0f, 0.08f, 0.0018f, 0.70f, 0.16f)
              .feg(II, 0.0f, 0.18f, 0.001f, 0.22f, 0.065f).eg(I, 0.0018f, 7.8f, 0.055f, 0.19f)
              .eg(II, 0.001f, 0.68f, 0.0f, 0.055f).touch(I, 0.08f, 1.0f, 0.0f, 0.0f)
              .touch(II, 0.16f, 1.0f, 0.0f, 0.0f).contour(0.10f, -0.08f, 0.015f, 0.035f).mix(0.20f).detune(0.0f));
    add("Electric Piano II", "Keys", { "velocity", "chorus", "feet-layer" },
        "A clean soft tine body opens into a broader pickup bark under firmer fingers. A lighter 4-foot strike fades above the lingering body, while shallow chorus colors the long decay.",
        // The lower pickup alignment opens even and odd overtones under firm
        // touch, distinct from I's rounder quiet body. The original chorus
        // keeps its register-dependent color and the 4-foot strike stays light.
        keys()
              .wave(I, false, true, 0.58f).wave(II, false, true, 0.72f).feet(II, f4).pw(II, 0.70f)
              .level(II, 0.35f, 0.025f).level(I, 0.40f, 0.055f).hpf(I, 28.0f).lpf(I, 5200.0f, 0.02f)
              .hpf(II, 720.0f).lpf(II, 9600.0f, 0.03f).body(I, body::Model::tine, 1.0f, 0.82f, 1.72f)
              .body(II, body::Model::tine, 0.94f, 0.74f, 0.56f).bodySize(I, 0.50f).bodySize(II, 1.55f)
              .feg(I, 0.0f, 0.055f, 0.0024f, 0.90f, 0.20f)
              .feg(II, 0.0f, 0.14f, 0.0013f, 0.34f, 0.08f).eg(I, 0.0025f, 8.3f, 0.045f, 0.27f)
              .eg(II, 0.0016f, 1.05f, 0.0f, 0.075f).touch(I, 0.055f, 1.0f, 0.0f, 0.0f)
              .touch(II, 0.12f, 1.0f, 0.0f, 0.0f).contour(0.075f, -0.12f, 0.015f, 0.04f).mix(0.22f)
              .detune(0.18f).chorus(0.10f, 0.34f));
    add("Tine Piano", "Keys", { "feet-layer", "velocity", "sine" },
        "A concentrated tine resonance follows a brief third-harmonic strike. Gentle notes stay rounded; firm strikes bring out the pickup overtones and a bell-like onset before the long body softens.",
        keys()
              .wave(I, false, true, 0.84f).level(I, 0.13f, 0.080f).wave(II, false, false, 0.38f).feet(II, f223)
              .level(II, 0.0f, 0.01f).detune(0.0f).hpf(I, 24.0f).lpf(I, 7500.0f, 0.02f)
              .body(I, body::Model::tine, 1.0f, 0.94f, 1.05f).bodySize(I, 0.65f)
              .feg(I, 0.0f, 0.04f, 0.001f, 0.45f, 0.14f)
              .eg(I, 0.0015f, 8.4f, 0.05f, 0.22f).eg(II, 0.001f, 0.23f, 0.0f, 0.060f)
              .touch(I, 0.04f, 0.95f, 0.0f, 0.0f).touch(II, 0.0f, 1.0f, 0.0f, 0.0f)
              .contour(0.12f, -0.05f, 0.01f, 0.025f).mix(0.20f).g(P_voiceDispersion, 0.025f));
    add("Clavichord I", "Keys", { "velocity", "percussive" },
        "A dry tangent-excited string spectrum with a short upper snap. Its high partials damp independently, leaving a small clear body; velocity changes the edge as well as the level.",
        keys()
              .wave(I, false, true).wave(II, false, true, 0.0f, 0.008f).pw(I, 0.76f).pw(II, 0.84f)
              .level(I, 1.0f, 0.090f).level(II, 0.94f, 0.035f).detune(0.0f).hpf(I, 42.0f).lpf(I, 6600.0f, 0.045f)
              .hpf(II, 980.0f).lpf(II, 8800.0f, 0.04f).body(I, body::Model::clav, 1.0f, 0.62f, 0.80f)
              .body(II, body::Model::clav, 0.86f, 0.89f, 0.30f).bodySize(I, 0.92f).bodySize(II, 0.90f)
              .feg(I, 0.0f, 0.16f, 0.001f, 0.42f, 0.075f).feg(II, 0.0f, 0.24f, 0.001f, 0.12f, 0.04f)
              .eg(I, 0.0012f, 4.0f, 0.04f, 0.085f).eg(II, 0.001f, 0.30f, 0.0f, 0.035f)
              .touch(I, 0.16f, 0.90f, 0.0f, 0.0f).touch(II, 0.28f, 1.0f, 0.0f, 0.0f)
              .contour(0.12f, -0.16f, 0.02f, 0.025f).mix(0.18f));
    add("Clavichord II", "Keys", { "velocity", "resonance", "percussive" },
        "A darker tangent string with a shorter, nasal decay. A restrained resonant pulse remains in the attack, while independent upper-partial damping keeps the held tail from becoming an electronic buzz.",
        keys()
              .wave(I, false, true).wave(II, false, true).pw(I, 0.70f).pw(II, 0.81f).level(I, 1.0f, 0.090f)
              .level(II, 0.94f, 0.03f).detune(0.0f).hpf(I, 46.0f).lpf(I, 3900.0f, 0.12f).hpf(II, 1100.0f)
              .lpf(II, 5900.0f, 0.09f).body(I, body::Model::clav, 1.0f, 0.35f, 0.53f)
              .body(II, body::Model::clav, 0.73f, 0.72f, 0.27f).bodySize(I, 1.08f).bodySize(II, 1.05f)
              .feg(I, 0.0f, 0.24f, 0.0015f, 0.48f, 0.07f).feg(II, 0.0f, 0.35f, 0.001f, 0.18f, 0.045f)
              .eg(I, 0.0015f, 3.4f, 0.025f, 0.075f).eg(II, 0.001f, 0.38f, 0.0f, 0.04f)
              .touch(I, 0.19f, 0.86f, 0.0f, 0.0f).touch(II, 0.30f, 1.0f, 0.0f, 0.0f)
              .contour(0.08f, -0.18f, 0.025f, 0.02f).mix(0.21f).res(0.10f));
    add("Funky Clavichord", "Keys", { "velocity", "resonance", "percussive" },
        "A tangent-string snap and compact resonant wah share the attack. The dry string body adds definition between the filter accents; hard playing brings out the upper rank without leaving a held pad.",
        keys()
              .wave(I, false, true).pw(I, 0.66f).wave(II, true, false).level(I, 1.0f, 0.075f)
              .level(II, 0.95f, 0.025f).detune(0.0f).hpf(I, 42.0f).lpf(I, 1180.0f, 0.31f).hpf(II, 370.0f)
              .lpf(II, 1850.0f, 0.37f).body(I, body::Model::clav, 0.72f, 0.78f, 0.44f)
              .body(II, body::Model::clav, 0.31f, 0.91f, 0.27f).feg(I, 0.0f, 0.58f, 0.0015f, 0.39f, 0.06f)
              .feg(II, 0.0f, 0.67f, 0.001f, 0.18f, 0.04f).eg(I, 0.0012f, 2.35f, 0.0f, 0.070f)
              .eg(II, 0.001f, 0.34f, 0.0f, 0.035f).touch(I, 0.30f, 0.94f, 0.0f, 0.0f)
              .touch(II, 0.48f, 1.0f, 0.0f, 0.0f).contour(0.04f, -0.14f, 0.015f, 0.025f).mix(0.27f).res(0.20f));
    add("Harpsichord I", "Keys", { "percussive", "dual-rank" },
        "A bright quill-plucked string has a notched harmonic spectrum and individually fading upper partials. The short second rank marks the quill release; nearly even strike dynamics and prompt key damping keep the register articulate.",
        keys()
              .wave(I, true, true).pw(I, 0.68f).wave(II, false, true, 0.0f, 0.008f).pw(II, 0.82f)
              .level(I, 0.86f, 0.96f).level(II, 0.78f, 0.82f).detune(0.0f).res(0.03f).hpf(I, 35.0f)
              .lpf(I, 10800.0f, 0.025f).hpf(II, 1250.0f).lpf(II, 12500.0f, 0.025f)
              .body(I, body::Model::harpsichord, 1.0f, 0.71f, 1.16f)
              .body(II, body::Model::harpsichord, 1.0f, 0.92f, 0.30f).bodySize(I, 1.12f).bodySize(II, 1.12f)
              .feg(I, 0.0f, 0.06f, 0.001f, 0.66f, 0.065f).feg(II, 0.0f, 0.08f, 0.001f, 0.10f, 0.03f)
              .eg(I, 0.001f, 5.8f, 0.025f, 0.070f).eg(II, 0.001f, 0.19f, 0.0f, 0.025f)
              .touch(Both, 0.0f, 0.015f, 0.0f, 0.0f).contour(0.08f, -0.17f, 0.015f, 0.035f).mix(0.11f));
    add("Harpsichord II", "Keys", { "percussive", "feet-layer" },
        "Coupled quill-plucked 8-foot and 4-foot strings with independent harmonic losses. The octave course speaks brightly then recedes, while the lower course rings; the two registers keep nearly even strike dynamics.",
        keys()
              .wave(I, true, true).wave(II, false, true).pw(I, 0.69f).pw(II, 0.76f).feet(II, f4)
              .level(I, 0.88f, 0.94f).level(II, 0.70f, 0.86f).detune(0.0f).res(0.035f).hpf(I, 38.0f)
              .lpf(I, 9200.0f, 0.025f).hpf(II, 300.0f).lpf(II, 11600.0f, 0.025f)
              .body(I, body::Model::harpsichord, 1.0f, 0.61f, 1.38f)
              .body(II, body::Model::harpsichord, 1.0f, 0.80f, 0.81f).bodySize(I, 1.22f).bodySize(II, 0.92f)
              .feg(I, 0.0f, 0.07f, 0.001f, 0.90f, 0.08f).feg(II, 0.0f, 0.10f, 0.001f, 0.38f, 0.055f)
              .eg(I, 0.001f, 6.4f, 0.02f, 0.085f).eg(II, 0.001f, 2.8f, 0.0f, 0.065f)
              .touch(Both, 0.0f, 0.02f, 0.0f, 0.0f).contour(0.055f, -0.20f, 0.01f, 0.035f).mix(0.25f));
    add("Lute Harpsichord", "Keys", { "percussive" },
        "A covered buff-stop string with rapidly damped upper partials and a restrained quill edge. The low body keeps a definite plucked onset, then settles into a short muted decay with little velocity variation.",
        keys()
              .wave(I, true, false, 0.12f).wave(II, false, true).pw(II, 0.72f).level(I, 0.80f, 0.93f)
              .level(II, 0.65f, 0.70f).detune(0.0f).res(0.025f).hpf(I, 32.0f).lpf(I, 2900.0f, 0.02f)
              .hpf(II, 650.0f).lpf(II, 5200.0f, 0.03f).body(I, body::Model::harpsichord, 1.0f, 0.19f, 0.42f)
              .body(II, body::Model::harpsichord, 0.94f, 0.43f, 0.25f).bodySize(Both, 1.18f)
              .feg(I, 0.0f, 0.06f, 0.0013f, 0.35f, 0.055f).feg(II, 0.0f, 0.08f, 0.001f, 0.12f, 0.03f)
              .eg(I, 0.0013f, 3.1f, 0.015f, 0.060f).eg(II, 0.001f, 0.17f, 0.0f, 0.025f)
              .touch(Both, 0.0f, 0.015f, 0.0f, 0.0f).contour(0.10f, -0.11f, 0.02f, 0.025f).mix(0.09f));
    add("Toy Piano", "Keys", { "feet-layer", "percussive", "velocity" },
        "Small high-register metal modes produce a slightly hollow struck body and brittle upper ping. The bright modes disappear at different rates, leaving a simple ringing center that damps on key release.",
        keys()
              .wave(I, false, false, 0.90f).feet(I, f4).level(I, 0.0f, 0.085f).wave(II, false, true, 0.62f)
              .feet(II, f2).pw(II, 0.82f).level(II, 0.38f, 0.025f).detune(0.0f).hpf(I, 65.0f)
              .lpf(I, 11700.0f, 0.025f).hpf(II, 2100.0f).lpf(II, 14500.0f, 0.04f)
              .body(I, body::Model::celesta, 1.0f, 0.88f, 0.64f)
              .body(II, body::Model::bell, 0.82f, 0.92f, 0.32f).feg(I, 0.0f, 0.05f, 0.001f, 0.25f, 0.17f)
              .feg(II, 0.0f, 0.12f, 0.001f, 0.12f, 0.045f).eg(I, 0.001f, 4.5f, 0.035f, 0.44f)
              .eg(II, 0.001f, 0.35f, 0.0f, 0.055f).touch(I, 0.03f, 0.85f, 0.0f, 0.0f)
              .touch(II, 0.12f, 1.0f, 0.0f, 0.0f).contour(0.02f, -0.26f, 0.005f, 0.02f).mix(0.15f)
              .g(P_voiceDispersion, 0.025f));
    add("Honky Keys", "Keys", { "detune", "velocity", "dual-rank" },
        "Two struck-string bodies have controlled upright-like beating and slightly stretched upper partials. Harder notes reveal a firmer hammer spectrum; the detuned ranks decay independently into a warm, uneven shimmer.",
        keys()
              .wave(I, true, false, 0.30f).wave(II, false, true, 0.22f).pw(II, 0.56f).level(I, 0.60f, 0.095f)
              .level(II, 0.66f, 0.080f).g(P_voiceDispersion, 0.035f).hpf(Both, 28.0f).lpf(I, 8400.0f, 0.035f)
              .lpf(II, 6700.0f, 0.03f).body(I, body::Model::piano, 1.0f, 0.70f, 1.17f)
              .body(II, body::Model::piano, 1.0f, 0.52f, 0.88f).bodySize(I, 0.91f).bodySize(II, 0.96f)
              .feg(I, 0.0f, 0.09f, 0.0012f, 0.62f, 0.12f).feg(II, 0.0f, 0.06f, 0.0016f, 0.54f, 0.10f)
              .eg(I, 0.0013f, 7.2f, 0.13f, 0.16f).eg(II, 0.0019f, 5.8f, 0.0f, 0.14f)
              .touch(I, 0.10f, 0.91f, 0.0f, 0.0f).touch(II, 0.12f, 0.95f, 0.0f, 0.0f)
              .contour(0.14f, -0.10f, 0.02f, 0.045f).detune(5.2f).mix(0.44f).g(P_drift, 0.008f));
    add("Vibes Keys", "Keys", { "sub-mod", "sine", "percussive", "velocity" },
        "A metal-bar body carries independently decaying higher modes, with a separately voiced fourth-harmonic strike. Gentle 5 Hz amplitude motion shades the ringing tail; release damps the body and firm strikes brighten the onset.",
        keys()
              .wave(I, false, false, 0.98f).wave(II, false, false, 0.58f).feet(II, f2).level(I, 0.0f, 0.080f)
              .level(II, 0.0f, 0.025f).detune(0.0f).hpf(I, 24.0f).lpf(I, 10500.0f, 0.02f)
              .body(I, body::Model::vibes, 1.0f, 0.43f, 1.82f).feg(I, 0.0f, 0.035f, 0.0013f, 0.50f, 0.25f)
              .eg(I, 0.0018f, 9.2f, 0.065f, 0.45f).eg(II, 0.001f, 0.95f, 0.0f, 0.12f)
              .touch(I, 0.02f, 0.91f, 0.0f, 0.0f).touch(II, 0.0f, 1.0f, 0.0f, 0.0f)
              .contour(0.05f, -0.14f, 0.015f, 0.05f).mix(0.15f).sub(subSine, 5.0f, 0.0f, 0.0f, 0.13f));
    add("Celesta", "Keys", { "sine", "feet-layer", "percussive", "velocity" },
        "A soft hammer excites a clear high-register metal-bar body. Sparse upper modes lose energy independently above the fundamental; the lighter second rank supplies a brief glint without a permanent octave buzz.",
        keys()
              .wave(I, false, false, 0.95f).feet(I, f4).wave(II, false, true, 0.45f).feet(II, f2).pw(II, 0.5f)
              .level(I, 0.0f, 0.075f).level(II, 0.28f, 0.03f).detune(0.0f).hpf(I, 30.0f).lpf(I, 11000.0f, 0.02f)
              .hpf(II, 2300.0f).lpf(II, 13900.0f, 0.025f).body(I, body::Model::celesta, 1.0f, 0.50f, 1.35f)
              .body(II, body::Model::celesta, 0.94f, 0.80f, 0.43f).feg(I, 0.0f, 0.04f, 0.0013f, 0.55f, 0.23f)
              .feg(II, 0.0f, 0.08f, 0.001f, 0.19f, 0.07f).eg(I, 0.0014f, 5.5f, 0.05f, 0.29f)
              .eg(II, 0.001f, 0.66f, 0.0f, 0.065f).touch(I, 0.035f, 0.92f, 0.0f, 0.0f)
              .touch(II, 0.10f, 1.0f, 0.0f, 0.0f).contour(0.04f, -0.18f, 0.01f, 0.035f).mix(0.13f));
    add("Saw Pulse Piano", "Keys", { "velocity", "dual-rank" },
        "A struck-string body joins the deliberately synthetic saw-and-pulse attack. Independent partial losses add a falling piano-like tone beneath the circuit filter motion, retaining the bright electronic character of the name.",
        keys()
              .wave(I, true, false, 0.18f).wave(II, false, true).pw(II, 0.63f).level(I, 0.80f, 0.100f)
              .level(II, 0.98f, 0.03f).detune(0.6f).hpf(I, 32.0f).lpf(I, 3300.0f, 0.075f).hpf(II, 460.0f)
              .lpf(II, 5900.0f, 0.10f).body(I, body::Model::piano, 0.72f, 0.73f, 1.02f)
              .body(II, body::Model::piano, 0.34f, 0.90f, 0.32f).bodySize(I, 1.0f).bodySize(II, 0.90f)
              .feg(I, 0.0f, 0.24f, 0.0016f, 0.95f, 0.15f).feg(II, 0.0f, 0.38f, 0.001f, 0.31f, 0.065f)
              .eg(I, 0.0015f, 7.5f, 0.0f, 0.17f).eg(II, 0.001f, 0.72f, 0.0f, 0.055f)
              .touch(I, 0.22f, 0.86f, 0.0f, 0.0f).touch(II, 0.36f, 1.0f, 0.0f, 0.0f)
              .contour(0.11f, -0.15f, 0.015f, 0.035f).mix(0.27f));
    add("Soft Keys", "Keys", { "velocity", "sine" },
        "A felt-like hammer spectrum excites a mellow struck-string body. Upper partials soften quickly while the low body rings, so soft chords remain warm and clear; firmer strikes still open the harmonic color.",
        keys()
              .wave(I, false, true, 0.94f).level(I, 0.10f, 0.120f).wave(II, false, true, 0.08f).pw(II, 0.53f)
              .level(II, 0.42f, 0.02f).detune(0.0f).hpf(I, 24.0f).lpf(I, 4200.0f, 0.015f).hpf(II, 830.0f)
              .lpf(II, 6400.0f, 0.025f).body(I, body::Model::piano, 1.0f, 0.16f, 1.04f)
              .body(II, body::Model::piano, 0.98f, 0.35f, 0.40f).bodySize(Both, 1.20f)
              .feg(I, 0.0f, 0.045f, 0.0028f, 0.80f, 0.18f).feg(II, 0.0f, 0.085f, 0.0014f, 0.23f, 0.055f)
              .eg(I, 0.0031f, 7.8f, 0.13f, 0.23f).eg(II, 0.0017f, 0.54f, 0.0f, 0.050f)
              .touch(I, 0.055f, 0.84f, 0.0f, 0.0f).touch(II, 0.10f, 0.96f, 0.0f, 0.0f)
              .contour(0.14f, -0.12f, 0.02f, 0.05f).mix(0.11f).g(P_voiceDispersion, 0.025f));
    add("Chorus Keys", "Keys", { "chorus", "velocity" },
        "Firm notes add odd-harmonic tine bite above a smooth electric body. The pickup overtones soften with the motion while shallow chorus shades the long decay and the short strike keeps chord changes clear.",
        keys()
              .wave(I, false, true, 0.70f).level(I, 0.38f, 0.085f).wave(II, true, false, 0.12f).hpf(I, 27.0f)
              .lpf(I, 7600.0f, 0.025f).hpf(II, 1350.0f).lpf(II, 11000.0f, 0.035f)
              .body(I, body::Model::tine, 1.0f, 0.71f, 1.42f).body(II, body::Model::tine, 0.91f, 0.89f, 0.34f)
              .bodySize(I, 1.25f).bodySize(II, 0.70f)
              .feg(I, 0.0f, 0.07f, 0.0016f, 0.65f, 0.17f).feg(II, 0.0f, 0.17f, 0.001f, 0.18f, 0.06f)
              .eg(I, 0.0016f, 7.8f, 0.05f, 0.23f).eg(II, 0.001f, 0.62f, 0.0f, 0.055f)
              .touch(I, 0.10f, 0.94f, 0.0f, 0.0f).touch(II, 0.18f, 1.0f, 0.0f, 0.0f)
              .contour(0.08f, -0.15f, 0.01f, 0.04f).mix(0.18f).detune(0.0f).chorus(0.14f, 0.39f));
    add("Stage Keys", "Keys", { "velocity", "dual-rank" },
        "A firm hammer excites a full struck-string spectrum with slight stiffness and controlled unison beating. The bright partials fall away at different rates above a long rounded body; key release damps the strings promptly.",
        keys()
              .wave(I, true, false, 0.48f).level(I, 0.48f, 0.085f).wave(II, false, true, 0.18f).pw(II, 0.62f)
              .level(II, 0.96f, 0.025f).detune(0.0f).g(P_voiceDispersion, 0.035f).hpf(I, 22.0f)
              .lpf(I, 9800.0f, 0.02f).hpf(II, 720.0f).lpf(II, 12800.0f, 0.025f)
              .body(I, body::Model::piano, 1.0f, 0.66f, 1.44f).body(II, body::Model::piano, 1.0f, 0.88f, 0.40f)
              .bodySize(I, 1.10f).bodySize(II, 1.08f).feg(I, 0.0f, 0.06f, 0.0012f, 0.76f, 0.15f)
              .feg(II, 0.0f, 0.14f, 0.001f, 0.24f, 0.055f).eg(I, 0.0014f, 8.0f, 0.16f, 0.17f)
              .eg(II, 0.001f, 0.82f, 0.0f, 0.055f).touch(I, 0.08f, 0.95f, 0.0f, 0.0f)
              .touch(II, 0.20f, 1.0f, 0.0f, 0.0f).contour(0.16f, -0.10f, 0.025f, 0.06f).mix(0.17f)
              .g(P_drift, 0.008f));
    add("Velocity Keys", "Keys", { "velocity", "dual-rank" },
        "Wide hammer dynamics reveal a soft struck-string body at low velocity and a firm, bright upper attack on hard strikes. Partial-specific losses let the brilliant opening settle naturally instead of leaving a fixed bright tail.",
        keys()
              .wave(I, false, true, 0.66f).level(I, 0.35f, 0.045f).wave(II, true, true, 0.06f).pw(II, 0.61f)
              .level(II, 0.94f, 0.025f).detune(0.0f).hpf(I, 24.0f).lpf(I, 6700.0f, 0.025f).hpf(II, 850.0f)
              .lpf(II, 10800.0f, 0.04f).body(I, body::Model::piano, 1.0f, 0.41f, 1.18f)
              .body(II, body::Model::piano, 1.0f, 0.94f, 0.38f).bodySize(I, 1.05f).bodySize(II, 0.98f)
              .feg(I, 0.0f, 0.10f, 0.0015f, 0.78f, 0.17f).feg(II, 0.0f, 0.23f, 0.001f, 0.26f, 0.055f)
              .eg(I, 0.0016f, 7.4f, 0.12f, 0.19f).eg(II, 0.001f, 0.70f, 0.0f, 0.05f)
              .touch(I, 0.18f, 1.0f, 0.0f, 0.0f).touch(II, 0.48f, 1.0f, 0.0f, 0.0f)
              .contour(0.12f, -0.12f, 0.01f, 0.025f).mix(0.24f).g(P_voiceDispersion, 0.03f));
    add("Dulcimer", "Keys", { "feet-layer", "percussive", "velocity" },
        "A bright hammered steel-string spectrum rings in two courses. Rapidly fading high partials and small pitch beating give way to a clearer lower body; the shorter upper course emphasizes hard mallet strokes.",
        keys()
              .wave(I, true, false, 0.24f).wave(II, false, true, 0.08f).feet(II, f4).pw(II, 0.78f)
              .level(I, 0.68f, 0.090f).level(II, 0.82f, 0.025f).hpf(I, 36.0f).lpf(I, 11200.0f, 0.025f)
              .hpf(II, 1000.0f).lpf(II, 14200.0f, 0.035f).body(I, body::Model::steel, 1.0f, 0.83f, 1.78f)
              .body(II, body::Model::steel, 0.98f, 0.94f, 0.62f).bodySize(I, 1.28f).bodySize(II, 1.08f)
              .feg(I, 0.0f, 0.06f, 0.001f, 0.82f, 0.60f).feg(II, 0.0f, 0.12f, 0.001f, 0.25f, 0.14f)
              .eg(I, 0.001f, 8.2f, 0.05f, 1.15f).eg(II, 0.001f, 0.94f, 0.0f, 0.26f)
              .touch(I, 0.10f, 0.94f, 0.0f, 0.0f).touch(II, 0.22f, 1.0f, 0.0f, 0.0f)
              .contour(0.13f, -0.20f, 0.015f, 0.04f).mix(0.20f).detune(0.8f).g(P_drift, 0.008f));
    add("Reed Piano", "Keys", { "velocity", "dual-rank" },
        "A vibrating reed and capacitive-style pickup give hard notes a broad harmonic bark, distinct from a tine bell. The short upper bark falls away as the reed motion softens, leaving a warmer rounded body with prompt key-off damping.",
        keys()
              .wave(I, false, true, 0.20f).pw(I, 0.59f).wave(II, false, true).pw(II, 0.69f)
              .level(I, 0.66f, 0.080f).level(II, 0.96f, 0.02f).detune(0.0f).hpf(I, 32.0f).lpf(I, 5400.0f, 0.055f)
              .hpf(II, 780.0f).lpf(II, 7900.0f, 0.08f).body(I, body::Model::reed, 1.0f, 0.61f, 1.28f)
              .body(II, body::Model::reed, 0.96f, 0.94f, 0.36f).feg(I, 0.0f, 0.12f, 0.0015f, 0.72f, 0.13f)
              .feg(II, 0.0f, 0.26f, 0.001f, 0.24f, 0.05f).eg(I, 0.0017f, 7.4f, 0.10f, 0.14f)
              .eg(II, 0.001f, 0.64f, 0.0f, 0.045f).touch(I, 0.14f, 0.94f, 0.0f, 0.0f)
              .touch(II, 0.30f, 1.0f, 0.0f, 0.0f).contour(0.13f, -0.18f, 0.02f, 0.04f).mix(0.22f).res(0.055f)
              .g(P_voiceDispersion, 0.035f));
    add("Glass Keys", "Keys", { "sine", "chorus", "percussive", "feet-layer" },
        "A clear metal-mode body and brief upper glint create a glassy keyboard texture. Different mode decays evolve inside the shallow chorus, leaving a high, simple ringing tail without held sustain.",
        keys()
              .wave(I, false, false, 0.94f).wave(II, false, false, 0.62f).feet(I, f4).feet(II, f2)
              .level(I, 0.0f, 0.090f).level(II, 0.0f, 0.025f).hpf(I, 38.0f).lpf(I, 13000.0f, 0.02f)
              .hpf(II, 1900.0f).lpf(II, 15500.0f, 0.025f).body(I, body::Model::celesta, 1.0f, 0.74f, 1.72f)
              .body(II, body::Model::bell, 0.92f, 0.90f, 0.47f).feg(I, 0.0f, 0.03f, 0.0014f, 0.70f, 0.38f)
              .feg(II, 0.0f, 0.08f, 0.001f, 0.18f, 0.08f).eg(I, 0.0015f, 6.3f, 0.06f, 0.67f)
              .eg(II, 0.001f, 0.86f, 0.0f, 0.13f).touch(I, 0.035f, 0.88f, 0.0f, 0.0f)
              .touch(II, 0.06f, 1.0f, 0.0f, 0.0f).contour(0.06f, -0.23f, 0.01f, 0.03f).mix(0.17f)
              .chorus(0.15f, 0.27f).detune(0.22f));
    add("Tremolo Keys", "Keys", { "tremolo", "velocity", "sub-mod" },
        "A rounded second-harmonic tine body and crisp upper strike carry gentle 4.8 Hz full-body tremolo. The pickup overtones soften as the body rings, with restrained stereo color from the chorus/tremolo circuit.",
        keys()
              .wave(I, false, true, 0.76f).level(I, 0.33f, 0.085f).wave(II, false, true, 0.12f).pw(II, 0.65f)
              .detune(0.0f).hpf(I, 25.0f).lpf(I, 6200.0f, 0.025f).hpf(II, 900.0f).lpf(II, 8700.0f, 0.035f)
              .body(I, body::Model::tine, 1.0f, 0.46f, 1.69f).body(II, body::Model::tine, 0.95f, 0.80f, 0.38f)
              .bodySize(I, 2.15f).bodySize(II, 1.10f)
              .feg(I, 0.0f, 0.06f, 0.0018f, 0.74f, 0.23f).feg(II, 0.0f, 0.16f, 0.001f, 0.23f, 0.055f)
              .eg(I, 0.0019f, 7.9f, 0.055f, 0.31f).eg(II, 0.001f, 0.72f, 0.0f, 0.06f)
              .touch(I, 0.08f, 0.92f, 0.0f, 0.0f).touch(II, 0.16f, 1.0f, 0.0f, 0.0f)
              .contour(0.08f, -0.11f, 0.015f, 0.035f).mix(0.19f).tremolo(0.20f, 4.8f)
              .sub(subSine, 4.8f, 0.0f, 0.0f, 0.16f));
    add("Contour Keys", "Keys", { "kbd-contour", "velocity" },
        "A tine-based electric keyboard with a fuller bass and a more present reed-tinted treble strike. Firm playing opens both even and odd pickup overtones; keyboard contour adds level and circuit-brightness contrast.",
        keys()
              .wave(I, false, true, 0.75f).level(I, 0.34f, 0.095f).wave(II, true, false, 0.08f).detune(0.0f)
              .hpf(I, 24.0f).lpf(I, 6500.0f, 0.025f).hpf(II, 1150.0f).lpf(II, 9200.0f, 0.045f)
              .body(I, body::Model::tine, 1.0f, 0.48f, 1.42f).body(II, body::Model::reed, 0.80f, 0.80f, 0.36f)
              .bodySize(I, 0.65f).bodySize(II, 1.50f)
              .feg(I, 0.0f, 0.07f, 0.002f, 0.80f, 0.19f).feg(II, 0.0f, 0.19f, 0.001f, 0.24f, 0.06f)
              .eg(I, 0.002f, 7.5f, 0.05f, 0.26f).eg(II, 0.001f, 0.72f, 0.0f, 0.06f)
              .touch(I, 0.08f, 0.93f, 0.0f, 0.0f).touch(II, 0.18f, 1.0f, 0.0f, 0.0f)
              .contour(-0.025f, 0.16f, 0.055f, -0.015f).mix(0.18f));
    add("Pressure Keys", "Keys", { "aftertouch", "velocity", "dual-rank" },
        "A struck-string attack settles into a quiet sustaining circuit body. Pressing into the note raises and colors that held bed after the short hammer has faded; the natural-style struck partials provide the onset and initial decay.",
        keys()
              .wave(I, false, true, 0.62f).level(I, 0.42f, 0.120f).wave(II, false, true, 0.06f).pw(II, 0.68f)
              .level(II, 0.94f, 0.03f).hpf(I, 28.0f).lpf(I, 5200.0f, 0.06f).hpf(II, 760.0f)
              .lpf(II, 10400.0f, 0.035f).body(I, body::Model::piano, 0.76f, 0.46f, 1.62f)
              .body(II, body::Model::piano, 0.95f, 0.88f, 0.37f).bodySize(I, 1.15f).bodySize(II, 1.08f)
              .feg(I, 0.0f, 0.13f, 0.0018f, 0.94f, 0.24f).feg(II, 0.0f, 0.21f, 0.001f, 0.25f, 0.06f)
              .eg(I, 0.0018f, 5.9f, 0.14f, 0.34f).eg(II, 0.001f, 0.72f, 0.0f, 0.060f)
              .touch(I, 0.12f, 0.73f, 0.23f, 0.66f).touch(II, 0.25f, 1.0f, 0.0f, 0.0f)
              .contour(0.10f, -0.15f, 0.015f, 0.035f).mix(0.21f).detune(0.12f));
}
