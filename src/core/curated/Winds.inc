// Winds voicings. Included inside namespace curated by CuratedLibrary.inc.
// Existing program names and insertion order are stable host/MIDI identities.
// Subtractive interpretations: rank I carries the pitched body; rank II has
// an explicit air, tongue or reed role. Sine bypasses both filters, so any
// brightness motion must come from a separately balanced filtered component.

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
        "Rounded 8' body with quiet upper harmonics and a separate sustained air rank. Velocity shapes the attack; pressure adds breath and gentle vibrato.",
        winds().eg(I, 0.040f, 0.70f, 0.94f, 0.17f)
               .feg(I, 0.05f, 0.14f, 0.030f, 0.42f, 0.12f)
               // Balance the filtered air against the bypass sine in the final mix.
               .wave(II, false, false, 0.0f, 0.82f).hpf(II, 950.0f)
               .level(II, 0.95f, 0.32f).eg(II, 0.018f, 0.50f, 0.82f, 0.12f)
               .touch(II, 0.04f, 0.44f, 0.08f, 0.40f).mix(0.22f)
               .sub(subSine, 5.1f, 0.0f).subAt(0.04f, 0.085f));
    add("Breathy Flute", "Winds", { "noise", "aftertouch", "velocity" },
        "Soft pitched body with a separately sustained, band-limited air stream. Pressure increases the breath as well as the body; use a gentle attack.",
        winds().wave(I, false, true, 0.80f).level(I, 0.13f, 0.28f)
               .wave(II, false, false, 0.0f, 0.75f).hpf(II, 800.0f).lpf(II, 7000.0f, 0.04f)
               .level(II, 0.98f, 0.30f)
               .eg(I, 0.065f, 0.85f, 0.91f, 0.24f).eg(II, 0.035f, 0.65f, 0.88f, 0.19f)
               .touch(II, 0.08f, 0.55f, 0.10f, 0.50f).mix(0.32f).subAt(0.04f, 0.10f));
    add("Pan Flute", "Winds", { "noise", "percussive", "velocity" },
        "A short, independent noise chiff precedes the sustained 8' body. The air rank decays to silence while the held pitch remains; velocity controls the tongued onset.",
        winds().wave(I, false, true, 0.92f).level(I, 0.08f, 0.28f)
               .wave(II, false, false, 0.0f, 0.95f).hpf(II, 700.0f).lpf(II, 5200.0f, 0.08f)
               .eg(I, 0.025f, 0.55f, 0.88f, 0.16f).eg(II, 0.004f, 0.32f, 0.0f, 0.06f)
               .feg(II, 0.0f, 0.22f, 0.002f, 0.15f, 0.05f)
               .touch(II, 0.08f, 0.90f, 0.0f, 0.0f).level(II, 0.95f, 0.12f)
               .mix(0.28f).subAt(0.03f, 0.065f));
    add("Recorder", "Winds", { "sine", "velocity" },
        "An 8' sine reinforced by quiet odd harmonics, with a brief tongue transient. Clear, nearly straight tone and a quick release suit articulated lines.",
        winds().wave(I, false, true, 0.72f).pw(I, 0.5f).hpf(I, 40.0f).lpf(I, 2100.0f, 0.04f)
               .level(I, 0.22f, 0.32f).eg(I, 0.016f, 0.30f, 0.96f, 0.10f)
               .wave(II, false, false, 0.0f, 0.18f).eg(II, 0.003f, 0.20f, 0.0f, 0.06f)
               .touch(I, 0.06f, 0.48f, 0.06f, 0.20f).touch(II, 0.0f, 0.5f, 0.0f, 0.0f)
               .mix(0.09f).subAt(0.025f, 0.035f).g(P_modWheelSub, 0.025f));
    add("Clarinet", "Winds", { "aftertouch", "velocity" },
        "A centered 8' square supplies the hollow odd-harmonic body. A short tongue and restrained breath rank support the onset; pressure opens the reed without a broad pitch wobble.",
        winds().wave(I, false, true).pw(I, 0.5f).hpf(I, 50.0f).lpf(I, 1900.0f, 0.12f)
               .level(I, 0.95f, 0.27f).feg(I, 0.08f, 0.22f, 0.015f, 0.28f, 0.10f)
               .eg(I, 0.022f, 0.55f, 0.94f, 0.15f)
               .wave(II, false, false, 0.0f, 0.22f).eg(II, 0.004f, 0.22f, 0.12f, 0.07f)
               .touch(I, 0.12f, 0.58f, 0.16f, 0.36f).mix(0.12f).subAt(0.04f, 0.07f));
    add("Bass Clarinet", "Winds", { "aftertouch", "velocity", "dual-rank" },
        "Both ranks follow the same 16' fundamental: a hollow square body and a quiet upper reed band. Slow breath onset and restrained pressure vibrato keep the low register clear.",
        winds().wave(I, false, true, 0.08f).wave(II, false, true, 0.0f, 0.018f)
               .feet(Both, f16).pw(Both, 0.5f)
               .hpf(I, 35.0f).lpf(I, 1250.0f, 0.10f).level(I, 0.95f, 0.28f)
               .hpf(II, 440.0f).lpf(II, 2800.0f, 0.08f).level(II, 0.42f, 0.24f)
               .eg(I, 0.042f, 0.80f, 0.94f, 0.22f).eg(II, 0.030f, 0.60f, 0.82f, 0.16f)
               .touch(I, 0.10f, 0.56f, 0.12f, 0.34f).touch(II, 0.12f, 0.44f, 0.18f, 0.22f)
               .mix(0.10f).sub(subSine, 4.8f, 0.0f).subAt(0.03f, 0.065f));
    add("Oboe", "Winds", { "resonance", "aftertouch", "velocity" },
        "A narrow 8' pulse gives the focused double-reed body; a quieter saw rank adds a nasal upper band. Distinct rank attacks soften the tongue while pressure adds controlled intensity.",
        winds().wave(I, false, true).pw(I, 0.72f).hpf(I, 140.0f, 0.06f).lpf(I, 2500.0f, 0.25f)
               .wave(II, true, false).hpf(II, 850.0f).lpf(II, 2800.0f, 0.20f)
               .level(I, 0.95f, 0.32f).level(II, 0.40f, 0.26f)
               .feg(I, 0.05f, 0.20f, 0.028f, 0.40f, 0.15f).feg(II, 0.08f, 0.12f, 0.045f, 0.90f, 0.12f)
               .eg(I, 0.025f, 0.65f, 0.90f, 0.18f).eg(II, 0.040f, 0.80f, 0.88f, 0.15f)
               .touch(I, 0.10f, 0.52f, 0.14f, 0.28f).touch(II, 0.10f, 0.25f, 0.12f, 0.22f)
               .mix(0.16f).subAt(0.05f, 0.095f).res(0.08f));
    add("English Horn", "Winds", { "resonance", "aftertouch", "velocity" },
        "A darker double-reed interpretation for the lower middle register. The rounded fundamental and quieter nasal rank share 8' tuning, with a broader breath onset than Oboe.",
        winds().wave(I, false, true, 0.18f, 0.012f).pw(I, 0.68f).wave(II, false, true).pw(II, 0.74f)
               .hpf(I, 105.0f).lpf(I, 1650.0f, 0.22f).level(I, 0.85f, 0.30f)
               .hpf(II, 650.0f).lpf(II, 2150.0f, 0.18f).level(II, 0.44f, 0.23f)
               .feg(I, 0.07f, 0.16f, 0.045f, 0.70f, 0.20f)
               .eg(I, 0.050f, 0.90f, 0.92f, 0.22f).eg(II, 0.065f, 0.75f, 0.86f, 0.18f)
               .touch(I, 0.08f, 0.56f, 0.12f, 0.32f).touch(II, 0.10f, 0.34f, 0.14f, 0.22f)
               .mix(0.18f).sub(subSine, 4.9f, 0.0f).subAt(0.04f, 0.085f).res(0.06f));
    add("Bassoon", "Winds", { "aftertouch", "velocity", "dual-rank" },
        "Two 16' reed ranks combine a woody low body with a smaller, nasal overtone band. Shorter release and a definite tongue keep low-register phrases articulate.",
        winds().wave(I, false, true, 0.06f).pw(I, 0.65f)
               .wave(II, false, true, 0.0f, 0.012f).pw(II, 0.78f).feet(Both, f16)
               .hpf(I, 40.0f).lpf(I, 1200.0f, 0.16f).level(I, 0.95f, 0.30f)
               .hpf(II, 400.0f).lpf(II, 1800.0f, 0.17f).level(II, 0.46f, 0.22f)
               .feg(I, 0.06f, 0.24f, 0.018f, 0.32f, 0.12f)
               .eg(I, 0.035f, 0.70f, 0.90f, 0.16f).eg(II, 0.020f, 0.45f, 0.78f, 0.12f)
               .touch(I, 0.12f, 0.58f, 0.12f, 0.26f).touch(II, 0.12f, 0.35f, 0.15f, 0.20f)
               .mix(0.18f).sub(subSine, 4.7f, 0.0f).subAt(0.03f, 0.075f).res(0.06f));
    add("Soprano Reed", "Winds", { "aftertouch", "velocity", "init-bend" },
        "A bright 8' saw body and narrow-pulse reed band for upper-register lines. Pressure adds brightness and a small vibrato; a subtle upward onset scoop keeps hard attacks expressive.",
        winds().wave(I, true, false).hpf(I, 125.0f).lpf(I, 2400.0f, 0.23f)
               .wave(II, false, true).pw(II, 0.72f).hpf(II, 700.0f).lpf(II, 3900.0f, 0.12f)
               .level(I, 0.95f, 0.28f).level(II, 0.32f, 0.22f)
               .eg(I, 0.018f, 0.65f, 0.92f, 0.16f).eg(II, 0.035f, 0.45f, 0.84f, 0.12f)
               .touch(I, 0.16f, 0.60f, 0.24f, 0.32f).touch(II, 0.12f, 0.40f, 0.15f, 0.24f)
               .mix(0.18f).bend(0.06f).subAt(0.06f, 0.11f).res(0.08f));
    add("Ocarina", "Winds", { "sine", "aftertouch", "velocity" },
        "A soft 4' vessel-flute sine with a trace of separately filtered air. No lower pitched rank is mixed in; pressure changes level and adds restrained vibrato.",
        winds().wave(I, false, false, 1.0f).wave(II, false, false, 0.0f, 0.08f)
               .feet(Both, f4).level(I, 0.0f, 0.30f).touch(I, 0.0f, 0.62f, 0.0f, 0.26f)
               .hpf(II, 1800.0f).lpf(II, 6200.0f, 0.03f)
               .eg(I, 0.045f, 0.55f, 0.97f, 0.18f).eg(II, 0.025f, 0.45f, 0.82f, 0.12f)
               .mix(0.045f).sub(subSine, 5.0f, 0.0f).subAt(0.025f, 0.05f).g(P_modWheelSub, 0.025f));
    add("Piccolo", "Winds", { "sine", "velocity" },
        "An agile 2' body with a small filtered harmonic component and a short air onset. Both ranks are registered at 2'; there is no unintended lower-octave sine.",
        winds().wave(I, false, true, 0.88f).pw(I, 0.5f).level(I, 0.10f, 0.28f)
               .wave(II, false, false, 0.0f, 0.16f).feet(Both, f2)
               .hpf(I, 35.0f).lpf(I, 11500.0f, 0.02f).hpf(II, 2400.0f).lpf(II, 10000.0f, 0.03f)
               .eg(I, 0.012f, 0.45f, 0.92f, 0.08f).eg(II, 0.004f, 0.18f, 0.08f, 0.05f)
               .touch(I, 0.04f, 0.60f, 0.06f, 0.25f).touch(II, 0.0f, 0.50f, 0.0f, 0.10f)
               .mix(0.08f).sub(subSine, 5.6f, 0.0f).subAt(0.03f, 0.065f).g(P_modWheelSub, 0.035f));
    add("Whistle", "Winds", { "init-bend", "glide", "sine" },
        "One clear 4' sine rises gently into pitch with a short glide. The second rank is silent in the mix; velocity and pressure shape level, with optional gentle vibrato.",
        winds().wave(Both, false, false, 1.0f).feet(Both, f4).mix(0.0f)
               .level(I, 0.0f, 0.33f).touch(I, 0.0f, 0.60f, 0.0f, 0.32f)
               .eg(I, 0.022f, 0.50f, 0.98f, 0.13f).bend(0.08f).glide(0.035f)
               .sub(subSine, 5.3f, 0.0f).subAt(0.03f, 0.075f).g(P_modWheelSub, 0.05f));
    add("Shakuhachi", "Winds", { "noise", "init-bend", "aftertouch", "velocity" },
        "A rounded 8' body follows an aspirated noise attack that settles to quiet breath. Initial bend rises into the note; pressure adds controlled vibrato and breath intensity.",
        winds().wave(I, false, true, 0.78f).pw(I, 0.58f)
               .hpf(I, 85.0f).lpf(I, 1700.0f, 0.08f).level(I, 0.16f, 0.30f)
               .wave(II, false, false, 0.0f, 0.70f).hpf(II, 500.0f).lpf(II, 4200.0f, 0.12f)
               .eg(I, 0.055f, 0.90f, 0.82f, 0.25f).eg(II, 0.008f, 0.55f, 0.14f, 0.15f)
               .feg(II, 0.0f, 0.28f, 0.006f, 0.40f, 0.12f)
               .touch(I, 0.08f, 0.50f, 0.12f, 0.38f).touch(II, 0.12f, 0.65f, 0.14f, 0.32f)
               .mix(0.24f).bend(0.18f).sub(subSine, 4.8f, 0.0f).subAt(0.09f, 0.12f));
}
