// Winds voicings. Included inside namespace curated by CuratedLibrary.inc.
// Existing program names and insertion order are stable host/MIDI identities.
// Modeled harmonic bodies and circuit voices share the same rank/VCA path.
// Rank II retains its explicit air, tongue or reed role. The body now follows
// both panel filters, so the fundamental rank stays open and the attack rank is
// genuinely high-passed. Contours balance sounding registers; explicit body
// size distinguishes bore geometry without changing the note's pitch.
// Breath effort owns most of the VCA level: a small fixed floor preserves soft
// attacks without compressing the difference between gentle and firm playing.
// Tongue, air and body releases are voiced separately for each instrument.
// Reed bodies retain a nearly steady held level and individually scaled breath
// pressure, so upper-register effort grows without a louder fixed floor.

Program winds()
{
    Program x;
    x.wave(I, false, true, 0.86f).pw(I, 0.5f)
     .wave(II, false, false, 0.0f, 0.16f).feet(Both, f8)
     .hpf(I, 45.0f).lpf(I, 1800.0f, 0.06f).level(I, 0.12f, 0.30f)
     .hpf(II, 1200.0f).lpf(II, 6200.0f, 0.04f).level(II, 0.80f, 0.18f)
     .feg(I, 0.04f, 0.12f, 0.035f, 0.45f, 0.12f)
     .feg(II, 0.0f, 0.12f, 0.012f, 0.30f, 0.08f)
     .eg(I, 0.045f, 0.65f, 0.92f, 0.18f).eg(II, 0.018f, 0.50f, 0.68f, 0.12f)
     .touch(I, 0.10f, 0.58f, 0.12f, 0.34f).touch(II, 0.04f, 0.42f, 0.08f, 0.24f)
     .sub(subSine, 5.2f, 0.0f).subAt(0.05f, 0.09f).g(P_modWheelSub, 0.05f)
     .mix(0.16f).detune(0.0f).res(0.04f)
     .g(P_voiceDispersion, 0.10f).g(P_drift, 0.035f).g(P_analogNoise, 0.0f).volume(0.6f);
    return x;
}

void buildWinds(Builder& add)
{
    add("Flute", "Winds", { "aftertouch", "sine", "velocity" },
        "A modeled flute harmonic body carries a separately sustained, quiet air stream. Velocity changes the tongued onset and upper-partial balance; pressure adds breath intensity and restrained vibrato without turning the line into a sine whistle.",
        winds().eg(I, 0.040f, 0.70f, 0.94f, 0.17f)
               .feg(I, 0.05f, 0.14f, 0.030f, 0.42f, 0.12f)
               // Keep the independently filtered air subordinate to the pitched body.
               .wave(II, false, false, 0.0f, 0.82f).hpf(II, 950.0f)
               .level(II, 0.95f, 0.32f).eg(II, 0.018f, 0.50f, 0.82f, 0.12f)
               .touch(II, 0.04f, 0.44f, 0.08f, 0.40f).mix(0.22f)
               .sub(subSine, 5.1f, 0.0f).subAt(0.04f, 0.085f)
               .body(I, body::Model::flute, 0.96f, 0.50f, 1.05f)
               .hpf(I, 28.0f).lpf(I, 6400.0f, 0.035f)
               .wave(II, false, false, 0.0f, 0.72f).mix(0.20f)
               .eg(I, 0.026f, 0.80f, 0.95f, 0.11f).eg(II, 0.012f, 0.30f, 0.78f, 0.075f)
               .touch(I, 0.10f, 0.60f, 0.16f, 0.28f)
               .contour(0.10f, -0.14f, 0.03f, -0.04f)
               .level(I, 0.12f, 0.065f).level(II, 0.95f, 0.035f)
               .touch(I, 0.10f, 0.84f, 0.16f, 0.35f).touch(II, 0.04f, 0.72f, 0.08f, 0.46f)
               .eg(I, 0.021f, 0.80f, 0.95f, 0.080f));
    add("Breathy Flute", "Winds", { "noise", "aftertouch", "velocity" },
        "A softer modeled flute body supports a stronger, separately sustained air stream. Its cushioned onset and darker harmonics favor quiet phrases; pressure increases breath and body together.",
        winds().wave(I, false, true, 0.80f).level(I, 0.13f, 0.448f)
               .wave(II, false, false, 0.0f, 0.75f).hpf(II, 800.0f).lpf(II, 7000.0f, 0.04f)
               .level(II, 0.98f, 0.48f)
               .eg(I, 0.065f, 0.85f, 0.91f, 0.24f).eg(II, 0.035f, 0.65f, 0.88f, 0.19f)
               .touch(I, 0.10f, 0.928f, 0.12f, 0.544f).touch(II, 0.08f, 0.88f, 0.10f, 0.80f).mix(0.32f).subAt(0.04f, 0.10f)
               .body(I, body::Model::flute, 0.95f, 0.34f, 1.20f)
               .hpf(I, 25.0f).lpf(I, 4200.0f, 0.04f)
               .wave(II, false, false, 0.0f, 0.70f).mix(0.30f)
               .eg(I, 0.044f, 0.85f, 0.94f, 0.17f).eg(II, 0.026f, 0.50f, 0.84f, 0.145f)
               .contour(0.06f, -0.18f, 0.02f, -0.08f)
               .level(I, 0.13f, 0.090f).level(II, 0.98f, 0.060f)
               .touch(I, 0.10f, 0.88f, 0.12f, 0.48f).touch(II, 0.08f, 0.86f, 0.10f, 0.68f)
               .eg(I, 0.055f, 0.85f, 0.94f, 0.115f).eg(II, 0.021f, 0.50f, 0.84f, 0.095f));
    add("Pan Flute", "Winds", { "noise", "percussive", "velocity" },
        "A short independent noise chiff leads into a modeled pipe-like harmonic body. The air rank dies away while the held pitch remains; a prompt onset and release keep articulated notes clear.",
        winds().wave(I, false, true, 0.92f).level(I, 0.08f, 0.28f)
               .wave(II, false, false, 0.0f, 0.95f).hpf(II, 700.0f).lpf(II, 5200.0f, 0.08f)
               .eg(I, 0.025f, 0.55f, 0.88f, 0.16f).eg(II, 0.004f, 0.32f, 0.0f, 0.06f)
               .feg(II, 0.0f, 0.22f, 0.002f, 0.15f, 0.05f)
               .touch(II, 0.08f, 0.90f, 0.0f, 0.0f).level(II, 0.95f, 0.12f)
               .mix(0.28f).subAt(0.03f, 0.065f)
               .body(I, body::Model::flute, 0.96f, 0.42f, 0.80f)
               .hpf(I, 25.0f).lpf(I, 4600.0f, 0.045f)
               .wave(II, false, false, 0.0f, 0.72f).hpf(II, 1100.0f).level(II, 0.95f, 0.24f).mix(0.27f)
               .eg(I, 0.014f, 0.45f, 0.90f, 0.10f).eg(II, 0.003f, 0.16f, 0.0f, 0.05f)
               .contour(0.12f, -0.10f, 0.02f, -0.06f)
               .level(I, 0.08f, 0.055f).level(II, 0.95f, 0.025f)
               .touch(I, 0.10f, 0.90f, 0.12f, 0.30f).touch(II, 0.08f, 0.98f, 0.0f, 0.0f)
               .eg(I, 0.012f, 0.45f, 0.90f, 0.065f).eg(II, 0.003f, 0.20f, 0.0f, 0.040f));
    add("Recorder", "Winds", { "sine", "velocity" },
        "A clear modeled pipe body is reinforced by quiet odd pulse harmonics and a brief tongue transient. Nearly straight pitch and a short release keep rapid recorder-style phrasing articulate.",
        winds().wave(I, false, true, 0.72f).pw(I, 0.5f).hpf(I, 40.0f).lpf(I, 2100.0f, 0.04f)
               .level(I, 0.22f, 0.32f).eg(I, 0.016f, 0.30f, 0.96f, 0.10f)
               // Give the short tongue a distinct envelope beneath the pitched body.
               .wave(II, false, false, 0.0f, 0.72f).hpf(II, 900.0f).lpf(II, 6000.0f, 0.04f)
               .level(II, 0.95f, 0.27f).eg(II, 0.002f, 0.14f, 0.0f, 0.045f)
               .touch(I, 0.06f, 0.48f, 0.06f, 0.20f).touch(II, 0.04f, 0.75f, 0.0f, 0.0f)
               .mix(0.14f).subAt(0.025f, 0.035f).g(P_modWheelSub, 0.025f)
               .body(I, body::Model::flute, 0.85f, 0.62f, 0.75f).pw(I, 0.50f)
               .hpf(I, 30.0f).lpf(I, 5200.0f, 0.035f)
               .wave(II, false, false, 0.0f, 0.30f).lpf(II, 4500.0f, 0.03f).mix(0.10f)
               .eg(I, 0.009f, 0.30f, 0.97f, 0.058f).subAt(0.0f, 0.0f)
               .touch(I, 0.08f, 0.46f, 0.06f, 0.18f)
               .contour(0.08f, -0.16f, 0.0f, -0.08f)
               .level(I, 0.22f, 0.080f).level(II, 0.95f, 0.025f)
               .touch(I, 0.08f, 0.78f, 0.06f, 0.20f).touch(II, 0.04f, 0.84f, 0.0f, 0.0f)
               .eg(I, 0.007f, 0.30f, 0.97f, 0.037f));
    add("Clarinet", "Winds", { "aftertouch", "velocity" },
        "A modeled clarinet body emphasizes the hollow odd harmonics while allowing a restrained register-dependent upper edge. A separate short tongue and breath rank support the onset; pressure increases reed intensity without broad pitch wobble.",
        winds().wave(I, false, true).pw(I, 0.5f).hpf(I, 50.0f).lpf(I, 1900.0f, 0.12f)
               .level(I, 0.95f, 0.27f).feg(I, 0.08f, 0.22f, 0.015f, 0.28f, 0.10f)
               .eg(I, 0.022f, 0.55f, 0.94f, 0.15f)
               // Keep the reed body dominant while making its tongued onset present.
               .wave(II, false, false, 0.0f, 0.65f).hpf(II, 850.0f).lpf(II, 5800.0f, 0.04f)
               .level(II, 0.95f, 0.35f).eg(II, 0.003f, 0.18f, 0.12f, 0.06f)
               .feg(II, 0.0f, 0.16f, 0.003f, 0.16f, 0.06f)
               .touch(I, 0.12f, 0.812f, 0.16f, 0.504f).touch(II, 0.04f, 1.0f, 0.04f, 0.252f)
               .mix(0.14f).subAt(0.04f, 0.07f)
               .body(I, body::Model::clarinet, 0.98f, 0.55f, 1.00f).bodySize(I, 1.0f)
               .hpf(I, 25.0f).lpf(I, 6200.0f, 0.08f).level(I, 0.95f, 0.434f)
               .wave(II, false, false, 0.0f, 0.20f).hpf(II, 900.0f).mix(0.08f)
               .eg(I, 0.014f, 0.55f, 0.96f, 0.095f)
               .contour(0.04f, -0.05f, 0.02f, -0.07f)
               .level(I, 0.95f, 0.065f).level(II, 0.95f, 0.025f)
               .touch(I, 0.12f, 1.0f, 0.16f, 0.65f).touch(II, 0.04f, 0.92f, 0.04f, 0.40f)
               .eg(I, 0.011f, 0.55f, 0.98f, 0.066f));
    add("Bass Clarinet", "Winds", { "aftertouch", "velocity", "dual-rank" },
        "A 16' clarinet body uses a larger bore geometry and a lower register transition, retaining a woody fundamental beneath a quieter upper reed band. The softer breath onset and restrained pressure vibrato keep low-register phrases clear.",
        winds().wave(I, false, true, 0.08f).wave(II, false, true, 0.0f, 0.018f)
               .feet(Both, f16).pw(Both, 0.5f)
               .hpf(I, 35.0f).lpf(I, 1250.0f, 0.10f).level(I, 0.95f, 0.434f)
               .hpf(II, 440.0f).lpf(II, 2800.0f, 0.08f).level(II, 0.42f, 0.372f)
               .eg(I, 0.042f, 0.80f, 0.94f, 0.22f).eg(II, 0.030f, 0.60f, 0.82f, 0.16f)
               .touch(I, 0.10f, 0.56f, 0.12f, 0.34f).touch(II, 0.12f, 0.682f, 0.18f, 0.341f)
               .mix(0.10f).sub(subSine, 4.8f, 0.0f).subAt(0.03f, 0.065f)
               .body(I, body::Model::clarinet, 0.98f, 0.42f, 1.25f)
               .body(II, body::Model::clarinet, 0.40f, 0.58f, 1.10f).bodySize(Both, 2.0f)
               .hpf(I, 22.0f).lpf(I, 4300.0f, 0.075f).hpf(II, 260.0f).lpf(II, 5600.0f, 0.06f)
               .eg(I, 0.030f, 0.75f, 0.96f, 0.14f).eg(II, 0.023f, 0.55f, 0.80f, 0.10f)
               .touch(I, 0.11f, 0.992f, 0.20f, 0.465f)
               .contour(0.12f, 0.02f, 0.04f, -0.08f)
               .level(I, 0.95f, 0.075f).level(II, 0.42f, 0.045f)
               .touch(I, 0.11f, 1.0f, 0.20f, 0.72f).touch(II, 0.12f, 0.78f, 0.18f, 0.45f)
               .eg(I, 0.033f, 0.75f, 0.98f, 0.10f).eg(II, 0.023f, 0.55f, 0.80f, 0.075f));
    add("Oboe", "Winds", { "resonance", "aftertouch", "velocity" },
        "A focused modeled double-reed spectrum combines a strong nasal center with a smaller upper edge. The ranks speak at slightly different times; velocity sharpens the tongue while pressure changes harmonic intensity during the hold.",
        winds().wave(I, false, true).pw(I, 0.72f).hpf(I, 140.0f, 0.06f).lpf(I, 2500.0f, 0.25f)
               .wave(II, true, false).hpf(II, 850.0f).lpf(II, 2800.0f, 0.20f)
               .level(I, 0.95f, 0.56f).level(II, 0.40f, 0.455f)
               .feg(I, 0.05f, 0.20f, 0.028f, 0.40f, 0.15f).feg(II, 0.08f, 0.12f, 0.045f, 0.90f, 0.12f)
               .eg(I, 0.025f, 0.65f, 0.90f, 0.18f).eg(II, 0.040f, 0.80f, 0.88f, 0.15f)
               .touch(I, 0.10f, 0.91f, 0.14f, 0.49f).touch(II, 0.10f, 0.4375f, 0.12f, 0.385f)
               .mix(0.16f).subAt(0.05f, 0.095f).res(0.08f)
               .body(I, body::Model::oboe, 0.98f, 0.68f, 0.95f).bodySize(I, 1.0f)
               .body(II, body::Model::oboe, 0.36f, 0.81f, 0.80f).bodySize(II, 0.98f)
               .hpf(I, 70.0f, 0.03f).lpf(I, 7600.0f, 0.12f)
               .hpf(II, 1000.0f).lpf(II, 7200.0f, 0.09f).level(II, 0.40f, 0.35f).mix(0.13f)
               .eg(I, 0.013f, 0.65f, 0.94f, 0.105f).touch(I, 0.10f, 0.91f, 0.20f, 0.36f)
               .subAt(0.04f, 0.07f).contour(0.04f, -0.10f, 0.02f, -0.08f)
               .level(I, 0.95f, 0.055f).level(II, 0.40f, 0.030f)
               .touch(I, 0.10f, 1.0f, 0.20f, 0.85f).touch(II, 0.10f, 0.72f, 0.12f, 0.52f)
               .eg(I, 0.010f, 0.65f, 0.98f, 0.070f).eg(II, 0.018f, 0.40f, 0.84f, 0.065f));
    add("English Horn", "Winds", { "resonance", "aftertouch", "velocity" },
        "A larger double-reed body shifts the resonant bands downward while keeping the played pitch intact. Its rounded fundamental, broader articulation and gentler nasal edge distinguish the voice from Oboe even at the same sounding note.",
        winds().wave(I, false, true, 0.18f, 0.012f).pw(I, 0.68f).wave(II, false, true).pw(II, 0.74f)
               .hpf(I, 105.0f).lpf(I, 1650.0f, 0.22f).level(I, 0.85f, 0.525f)
               .hpf(II, 650.0f).lpf(II, 2150.0f, 0.18f).level(II, 0.44f, 0.4025f)
               .feg(I, 0.07f, 0.16f, 0.045f, 0.70f, 0.20f)
               .eg(I, 0.050f, 0.90f, 0.92f, 0.22f).eg(II, 0.065f, 0.75f, 0.86f, 0.18f)
               .touch(I, 0.08f, 0.98f, 0.12f, 0.56f).touch(II, 0.10f, 0.595f, 0.14f, 0.385f)
               .mix(0.18f).sub(subSine, 4.9f, 0.0f).subAt(0.04f, 0.085f).res(0.06f)
               .body(I, body::Model::oboe, 0.97f, 0.45f, 1.20f).bodySize(I, 1.35f)
               .body(II, body::Model::oboe, 0.40f, 0.58f, 1.15f).bodySize(II, 1.30f)
               .hpf(I, 45.0f).lpf(I, 5000.0f, 0.11f).hpf(II, 680.0f).lpf(II, 6100.0f, 0.08f)
               .eg(I, 0.032f, 0.85f, 0.95f, 0.15f).touch(I, 0.08f, 0.80f, 0.17f, 0.40f)
               .contour(0.10f, -0.13f, 0.03f, -0.09f)
               .level(I, 0.85f, 0.080f).level(II, 0.44f, 0.040f)
               .touch(I, 0.08f, 1.0f, 0.17f, 0.70f).touch(II, 0.10f, 0.72f, 0.14f, 0.48f)
               .eg(I, 0.030f, 0.85f, 0.98f, 0.11f).eg(II, 0.043f, 0.65f, 0.83f, 0.09f));
    add("Bassoon", "Winds", { "aftertouch", "velocity", "dual-rank" },
        "A 16' modeled bassoon spectrum pairs a woody low body with a smaller nasal overtone band. The definite tongue and compact release keep low lines separated, while pressure adds body and controlled reed brightness.",
        winds().wave(I, false, true, 0.06f).pw(I, 0.65f)
               .wave(II, false, true, 0.0f, 0.012f).pw(II, 0.78f).feet(Both, f16)
               .hpf(I, 40.0f).lpf(I, 1200.0f, 0.16f).level(I, 0.95f, 0.51f)
               .hpf(II, 400.0f).lpf(II, 1800.0f, 0.17f).level(II, 0.46f, 0.374f)
               .feg(I, 0.06f, 0.24f, 0.018f, 0.32f, 0.12f)
               .eg(I, 0.035f, 0.70f, 0.90f, 0.16f).eg(II, 0.020f, 0.45f, 0.78f, 0.12f)
               .touch(I, 0.12f, 0.986f, 0.12f, 0.442f).touch(II, 0.12f, 0.595f, 0.15f, 0.34f)
               .mix(0.18f).sub(subSine, 4.7f, 0.0f).subAt(0.03f, 0.075f).res(0.06f)
               .body(I, body::Model::bassoon, 0.98f, 0.49f, 1.20f).bodySize(Both, 1.0f)
               .body(II, body::Model::bassoon, 0.45f, 0.64f, 1.00f)
               .hpf(I, 22.0f).lpf(I, 4800.0f, 0.10f).hpf(II, 330.0f).lpf(II, 5400.0f, 0.08f)
               .eg(I, 0.020f, 0.70f, 0.94f, 0.095f).mix(0.14f)
               .contour(0.12f, 0.04f, 0.04f, -0.10f)
               .level(I, 0.95f, 0.070f).level(II, 0.46f, 0.040f)
               .touch(I, 0.12f, 1.0f, 0.12f, 0.72f).touch(II, 0.12f, 0.78f, 0.15f, 0.45f)
               .eg(I, 0.018f, 0.70f, 0.98f, 0.065f).eg(II, 0.014f, 0.45f, 0.65f, 0.050f));
    add("Soprano Reed", "Winds", { "aftertouch", "velocity", "init-bend" },
        "A bright modeled reed body supports a lighter narrow-pulse upper band. A restrained onset scoop and stable tuning keep upper-register lines focused; pressure supplies harmonic brightness and a small vibrato.",
        winds().wave(I, true, false).hpf(I, 125.0f).lpf(I, 2400.0f, 0.23f)
               .wave(II, false, true).pw(II, 0.72f).hpf(II, 700.0f).lpf(II, 3900.0f, 0.12f)
               .level(I, 0.95f, 0.466667f).level(II, 0.32f, 0.55f)
               .eg(I, 0.018f, 0.65f, 0.92f, 0.16f).eg(II, 0.035f, 0.45f, 0.84f, 0.12f)
               .touch(I, 0.16f, 1.0f, 0.24f, 0.533333f).touch(II, 0.12f, 1.0f, 0.15f, 0.60f)
               .mix(0.18f).bend(0.06f).subAt(0.06f, 0.11f).res(0.08f)
               .body(I, body::Model::oboe, 1.0f, 0.80f, 0.85f).bodySize(I, 0.82f)
               .hpf(I, 75.0f).lpf(I, 9800.0f, 0.14f).hpf(II, 1100.0f).lpf(II, 6200.0f, 0.10f)
               .eg(I, 0.012f, 0.65f, 0.95f, 0.10f).eg(II, 0.043f, 0.52f, 0.76f, 0.10f)
               .mix(0.10f).bend(0.022f).detune(0.0f).subAt(0.04f, 0.075f)
               .contour(0.04f, -0.20f, 0.0f, -0.10f)
               .level(I, 0.95f, 0.055f).level(II, 0.32f, 0.030f)
               .touch(I, 0.16f, 1.0f, 0.24f, 0.92f).touch(II, 0.12f, 0.80f, 0.15f, 0.60f)
               .eg(I, 0.010f, 0.65f, 0.99f, 0.070f).eg(II, 0.030f, 0.42f, 0.65f, 0.070f));
    add("Ocarina", "Winds", { "sine", "aftertouch", "velocity" },
        "A soft 4' vessel-like body keeps the fundamental dominant with restrained modeled harmonics. A trace of separate air and a rounded onset keep the line alive, with no lower pitched rank mixed in.",
        winds().wave(I, false, false, 1.0f).wave(II, false, false, 0.0f, 0.08f)
               .feet(Both, f4).level(I, 0.0f, 0.30f).touch(I, 0.0f, 0.62f, 0.0f, 0.26f)
               .hpf(II, 1800.0f).lpf(II, 6200.0f, 0.03f)
               .eg(I, 0.045f, 0.55f, 0.97f, 0.18f).eg(II, 0.025f, 0.45f, 0.82f, 0.12f)
               .mix(0.045f).sub(subSine, 5.0f, 0.0f).subAt(0.025f, 0.05f).g(P_modWheelSub, 0.025f)
               // One coherent pitched body avoids cancellation against a free-running sine.
               .body(I, body::Model::flute, 1.0f, 0.10f, 1.35f)
               .hpf(I, 22.0f).lpf(I, 3500.0f, 0.025f)
               .wave(II, false, false, 0.0f, 0.05f).mix(0.032f)
               .eg(I, 0.024f, 0.55f, 0.985f, 0.09f)
               .contour(0.04f, -0.22f, 0.0f, -0.07f)
               .level(I, 0.0f, 0.090f).level(II, 0.80f, 0.025f)
               .touch(I, 0.0f, 0.80f, 0.0f, 0.28f).touch(II, 0.04f, 0.62f, 0.08f, 0.20f)
               .eg(I, 0.031f, 0.55f, 0.985f, 0.070f));
    add("Piccolo", "Winds", { "sine", "velocity" },
        "An agile 2' modeled flute body carries a bright, controlled harmonic edge and a short independent air onset. Both ranks remain in the high register; quick articulation keeps the piccolo-like line clear.",
        winds().wave(I, false, true, 0.88f).pw(I, 0.5f).level(I, 0.10f, 0.28f)
               .wave(II, false, false, 0.0f, 0.16f).feet(Both, f2)
               .hpf(I, 35.0f).lpf(I, 11500.0f, 0.02f).hpf(II, 2400.0f).lpf(II, 10000.0f, 0.03f)
               .eg(I, 0.012f, 0.45f, 0.92f, 0.08f).eg(II, 0.004f, 0.18f, 0.08f, 0.05f)
               .touch(I, 0.04f, 0.60f, 0.06f, 0.25f).touch(II, 0.0f, 0.50f, 0.0f, 0.10f)
               .mix(0.08f).sub(subSine, 5.6f, 0.0f).subAt(0.03f, 0.065f).g(P_modWheelSub, 0.035f)
               .body(I, body::Model::flute, 0.94f, 0.74f, 0.65f)
               .hpf(I, 25.0f).lpf(I, 14500.0f, 0.02f)
               .wave(II, false, false, 0.0f, 0.12f).mix(0.06f)
               .eg(I, 0.006f, 0.45f, 0.96f, 0.043f)
               .contour(0.14f, -0.26f, 0.02f, -0.13f)
               .level(I, 0.10f, 0.055f).level(II, 0.80f, 0.020f)
               .touch(I, 0.04f, 0.88f, 0.06f, 0.28f).touch(II, 0.0f, 0.72f, 0.0f, 0.10f)
               .eg(I, 0.0055f, 0.45f, 0.96f, 0.029f));
    add("Whistle", "Winds", { "init-bend", "glide", "sine" },
        "A nearly pure 4' whistle gains a trace of moving pipe harmonics beneath a gentle onset rise. The second rank stays out of the mix; a short glide, responsive level and restrained vibrato keep the line focused.",
        winds().wave(Both, false, false, 1.0f).feet(Both, f4).mix(0.0f)
               .level(I, 0.0f, 0.33f).touch(I, 0.0f, 0.60f, 0.0f, 0.32f)
               .eg(I, 0.022f, 0.50f, 0.98f, 0.13f).bend(0.08f).glide(0.035f)
               .sub(subSine, 5.3f, 0.0f).subAt(0.03f, 0.075f).g(P_modWheelSub, 0.05f)
               .body(I, body::Model::flute, 0.28f, 0.07f, 1.10f)
               .hpf(I, 20.0f).lpf(I, 3100.0f, 0.025f)
               .eg(I, 0.013f, 0.50f, 0.99f, 0.075f).bend(0.04f).glide(0.021f)
               .contour(0.0f, -0.22f, 0.0f, -0.08f)
               .level(I, 0.0f, 0.080f).touch(I, 0.0f, 0.82f, 0.0f, 0.34f)
               .eg(I, 0.017f, 0.50f, 0.99f, 0.055f));
    add("Shakuhachi", "Winds", { "noise", "init-bend", "aftertouch", "velocity" },
        "A modeled flute body follows an aspirated attack that settles quickly to quiet breath. A controlled upward onset bend and pressure-driven harmonic intensity distinguish the phrase from a static noise-and-sine blend.",
        winds().wave(I, false, true, 0.78f).pw(I, 0.58f)
               .hpf(I, 85.0f).lpf(I, 1700.0f, 0.08f).level(I, 0.16f, 0.30f)
               .wave(II, false, false, 0.0f, 0.70f).hpf(II, 500.0f).lpf(II, 4200.0f, 0.12f)
               .eg(I, 0.055f, 0.90f, 0.82f, 0.25f).eg(II, 0.008f, 0.55f, 0.14f, 0.15f)
               .feg(II, 0.0f, 0.28f, 0.006f, 0.40f, 0.12f)
               .touch(I, 0.08f, 0.50f, 0.12f, 0.38f).touch(II, 0.12f, 0.65f, 0.14f, 0.32f)
               .mix(0.24f).bend(0.18f).sub(subSine, 4.8f, 0.0f).subAt(0.09f, 0.12f)
               .body(I, body::Model::flute, 0.95f, 0.44f, 1.25f)
               .hpf(I, 40.0f).lpf(I, 5600.0f, 0.055f)
               .wave(II, false, false, 0.0f, 0.74f).hpf(II, 700.0f).lpf(II, 5600.0f, 0.08f)
               .level(II, 0.80f, 0.18f).mix(0.20f).bend(0.085f)
               .eg(I, 0.030f, 0.90f, 0.90f, 0.17f).eg(II, 0.005f, 0.31f, 0.10f, 0.09f)
               .touch(I, 0.08f, 0.56f, 0.22f, 0.38f)
               .contour(0.12f, -0.16f, 0.01f, -0.08f)
               .level(I, 0.16f, 0.070f).level(II, 0.80f, 0.035f)
               .touch(I, 0.08f, 0.86f, 0.22f, 0.48f).touch(II, 0.12f, 0.92f, 0.14f, 0.40f)
               .eg(I, 0.034f, 0.90f, 0.90f, 0.125f).eg(II, 0.005f, 0.40f, 0.07f, 0.075f));
}
