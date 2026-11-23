// Pads voicings. Included inside namespace curated by CuratedLibrary.inc.
// Existing program names and insertion order are stable host/MIDI identities.
// Body and motion ranks have independent filters, articulation and expression.
// Cutoffs are at MIDI 60; envelope releases are approximately time to -60 dB.

Program pad()
{
    Program x;
    x.wave(I, true, false, 0.06f).wave(II, false, true).pw(II, 0.57f, 0.34f, 0.23f)
     .hpf(I, 48.0f).hpf(II, 145.0f).lpf(I, 1750.0f, 0.10f).lpf(II, 3100.0f, 0.12f)
     .feg(I, 0.20f, 0.23f, 0.85f, 3.4f, 1.7f).feg(II, 0.32f, 0.36f, 1.25f, 2.8f, 2.1f)
     .eg(I, 0.55f, 3.2f, 0.88f, 2.6f).eg(II, 0.95f, 2.7f, 0.79f, 3.1f)
     .level(I, 0.96f, 0.63f).level(II, 0.88f, 0.56f)
     .touch(I, 0.06f, 0.28f, 0.18f, 0.25f).touch(II, 0.10f, 0.30f, 0.28f, 0.28f)
     .chorus(0.43f, 0.29f).detune(6.5f).mix(0.40f).res(0.09f)
     .g(P_voiceDispersion, 0.19f).g(P_drift, 0.09f).g(P_analogNoise, 0.008f)
     .g(P_modWheelSub, 0.0f).volume(0.6f);
    return x;
}

void buildPads(Builder& add)
{
    add("Slow Pad", "Pads", { "pwm", "chorus", "dual-rank" },
        "A warm saw body fades in before a brighter moving pulse rank. Their filter blooms and release lengths overlap gently; pressure brings forward the upper color.",
        pad()
              .lpf(I, 1550.0f, 0.085f).lpf(II, 2850.0f, 0.13f).feg(I, 0.30f, 0.25f, 1.70f, 5.3f, 2.5f)
              .feg(II, 0.46f, 0.34f, 2.45f, 4.1f, 3.0f).eg(I, 0.95f, 4.8f, 0.94f, 4.1f).eg(II, 1.68f, 4.1f, 0.67f, 4.7f)
              .level(I, 0.96f, 0.49f).level(II, 0.88f, 0.39f).touch(I, 0.045f, 0.43f, 0.20f, 0.30f)
              .touch(II, 0.08f, 0.49f, 0.34f, 0.35f).pw(II, 0.545f, 0.14f, 0.137f).chorus(0.33f, 0.24f).detune(4.8f)
              .contour(-0.10f, -0.16f, -0.045f, 0.075f));
    add("Ring Drift Pad", "Pads", { "ring-mod", "dual-rank" },
        "A steady low-mid body and lighter pulse halo move through a slow ring beat. The ring frequency envelope adds a gentle drift while the held pitch remains recognizable.",
        pad()
              .wave(I, true, false, 0.16f).ring(0.55f, 0.07f, 4.0f, 6.0f, 0.12f).detune(1.7f).chorus(0.18f, 0.19f)
              .hpf(II, 220.0f).lpf(I, 1470.0f, 0.075f).lpf(II, 2880.0f, 0.13f).feg(I, 0.17f, 0.21f, 0.62f, 4.9f, 2.1f)
              .feg(II, 0.36f, 0.31f, 2.05f, 5.2f, 2.9f).eg(I, 0.43f, 4.3f, 0.94f, 3.2f).eg(II, 1.32f, 3.8f, 0.63f, 4.2f)
              .pw(II, 0.61f, 0.09f, 0.083f).touch(I, 0.05f, 0.37f, 0.13f, 0.28f).touch(II, 0.07f, 0.34f, 0.26f, 0.39f)
              .mix(0.35f).contour(-0.11f, -0.15f, -0.04f, 0.06f));
    add("Choir Pad", "Pads", { "resonance", "dual-rank" },
        "Two softly entered pulse ranks suggest an open vowel. A low body band supports a brighter nasal band; this is a synthesized choir color.",
        pad()
              .wave(I, false, true, 0.08f).wave(II, false, true).res(0.20f).pw(I, 0.520f, 0.012f, 0.13f)
              .pw(II, 0.540f, 0.020f, 0.19f).hpf(I, 250.0f, 0.17f).hpf(II, 1160.0f, 0.19f).lpf(I, 850.0f, 0.42f)
              .lpf(II, 2280.0f, 0.40f).feg(I, 0.035f, 0.065f, 0.15f, 2.4f, 0.52f).feg(II, 0.065f, 0.085f, 0.24f, 1.7f, 0.63f)
              .eg(I, 0.12f, 3.2f, 0.95f, 1.65f).eg(II, 0.21f, 2.7f, 0.73f, 1.85f).touch(I, 0.015f, 0.38f, 0.055f, 0.41f)
              .touch(II, 0.025f, 0.34f, 0.08f, 0.46f).level(I, 0.96f, 0.53f).level(II, 1.0f, 0.54f).mix(0.34f)
              .chorus(0.21f, 0.23f).detune(1.2f).g(P_drift, 0.025f).g(P_voiceDispersion, 0.10f).g(P_modWheelVcf, 0.08f)
              .contour(0.18f, -0.26f, -0.03f, 0.10f));
    add("Dark Choir", "Pads", { "resonance", "feet-layer" },
        "A covered 16' vowel-like body sits beneath a quiet 8' nasal band. Unequal rank attacks give this low choir color depth without a large detuned wobble.",
        pad()
              .feet(I, f16).wave(I, false, true, 0.13f).wave(II, false, true).res(0.21f).pw(I, 0.510f, 0.014f, 0.12f)
              .hpf(I, 115.0f, 0.15f).hpf(II, 780.0f, 0.19f).lpf(I, 540.0f, 0.39f).lpf(II, 1680.0f, 0.41f)
              .feg(I, 0.08f, 0.07f, 0.34f, 3.2f, 0.85f).feg(II, 0.13f, 0.095f, 0.62f, 2.4f, 1.0f)
              .eg(I, 0.26f, 3.8f, 0.96f, 2.6f).eg(II, 0.53f, 3.1f, 0.65f, 2.15f).touch(I, 0.015f, 0.39f, 0.045f, 0.42f)
              .touch(II, 0.025f, 0.35f, 0.07f, 0.44f).pw(II, 0.52f, 0.016f, 0.137f).mix(0.25f).detune(0.8f)
              .chorus(0.16f, 0.20f).g(P_drift, 0.018f).g(P_voiceDispersion, 0.08f).contour(0.16f, -0.26f, -0.06f, 0.08f));
    add("Sweep Pad", "Pads", { "brilliance-swell", "dual-rank" },
        "A sustained body opens over three seconds, followed by a higher pulse bloom. Hold beyond four seconds to hear both filter crests and their gradual settling.",
        pad()
              .res(0.17f).lpf(I, 750.0f, 0.16f).lpf(II, 1600.0f, 0.22f).feg(I, 0.43f, 0.57f, 2.75f, 6.2f, 2.4f)
              .feg(II, 0.62f, 0.66f, 3.85f, 4.9f, 3.1f).eg(I, 0.50f, 5.2f, 0.94f, 3.7f).eg(II, 0.98f, 4.7f, 0.69f, 4.2f)
              .touch(I, 0.025f, 0.36f, 0.10f, 0.28f).touch(II, 0.045f, 0.39f, 0.21f, 0.35f).pw(II, 0.59f, 0.10f, 0.167f)
              .mix(0.35f).detune(3.7f).chorus(0.26f, 0.22f).contour(-0.08f, -0.20f, -0.035f, 0.065f));
    add("Breathing Pad", "Pads", { "sub-mod", "pwm" },
        "A rounded pad gently swells and recedes at 0.7 Hz. The original SUB moves level by a few decibels while slower PWM keeps the upper rank alive.",
        pad()
              .sub(subSine, 0.7f, 0.0f, 0.035f, 0.38f).subAt(0.0f, 0.0f, 0.0f).wave(I, true, false, 0.14f)
              .pw(II, 0.56f, 0.16f, 0.107f).lpf(I, 1480.0f, 0.065f).lpf(II, 2780.0f, 0.12f).hpf(II, 210.0f)
              .feg(I, 0.10f, 0.17f, 0.29f, 3.2f, 1.7f).feg(II, 0.24f, 0.28f, 0.82f, 2.7f, 2.1f)
              .eg(I, 0.24f, 3.1f, 0.97f, 2.35f).eg(II, 0.64f, 2.5f, 0.75f, 2.75f).touch(I, 0.04f, 0.39f, 0.14f, 0.31f)
              .touch(II, 0.065f, 0.36f, 0.22f, 0.38f).mix(0.35f).detune(2.6f).chorus(0.21f, 0.18f)
              .contour(-0.12f, -0.13f, -0.045f, 0.07f));
    add("Glass Pad", "Pads", { "feet-layer", "sine", "dual-rank" },
        "An 8' soft body supports a late 2' sine and pulse glint. The high rank has its own brighter filter and lower sustain, letting the glass color float above the chord.",
        pad()
              .feet(II, f2).wave(II, false, true, 0.36f).hpf(I, 45.0f).pw(II, 0.57f, 0.045f, 0.27f)
              .wave(I, true, false, 0.12f).hpf(II, 1350.0f).lpf(I, 1460.0f, 0.07f).lpf(II, 8600.0f, 0.035f)
              .level(II, 0.28f, 0.35f).feg(I, 0.14f, 0.19f, 0.56f, 4.3f, 1.9f).feg(II, 0.17f, 0.15f, 1.55f, 5.7f, 2.5f)
              .eg(I, 0.42f, 3.8f, 0.91f, 3.6f).eg(II, 1.05f, 6.5f, 0.32f, 2.7f).touch(I, 0.05f, 0.40f, 0.16f, 0.29f)
              .touch(II, 0.065f, 0.30f, 0.12f, 0.25f).mix(0.21f).detune(0.7f).chorus(0.24f, 0.21f)
              .contour(-0.08f, -0.19f, -0.02f, 0.08f));
    add("Warm Pad", "Pads", { "chorus", "sine" },
        "A filtered saw with a small sine foundation and a quiet soft-pulse companion. Compact stereo motion and modest pressure brightness preserve the warm center.",
        pad()
              .hpf(I, 26.0f).wave(I, true, false, 0.12f).hpf(II, 140.0f).lpf(I, 1420.0f, 0.05f).lpf(II, 1960.0f, 0.075f)
              .pw(II, 0.525f, 0.075f, 0.103f).feg(I, 0.08f, 0.14f, 0.29f, 4.2f, 1.75f)
              .feg(II, 0.20f, 0.23f, 0.84f, 3.1f, 2.1f).eg(I, 0.28f, 4.4f, 0.97f, 3.2f).eg(II, 0.67f, 3.6f, 0.74f, 2.7f)
              .touch(I, 0.035f, 0.40f, 0.16f, 0.32f).touch(II, 0.065f, 0.37f, 0.24f, 0.29f).mix(0.27f).detune(2.2f)
              .chorus(0.20f, 0.19f).contour(-0.06f, -0.10f, -0.05f, 0.07f));
    add("Wide Pad", "Pads", { "chorus", "detune", "dual-rank" },
        "Saw and pulse ranks spread left and right with a broad chorus. Their different attacks and PWM rates create width while each side retains a useful chord body.",
        pad()
              .wave(I, true, true).hpf(I, 38.0f).hpf(II, 165.0f).pw(I, 0.54f, 0.10f, 0.137f).pw(II, 0.61f, 0.21f, 0.223f)
              .lpf(I, 1850.0f, 0.075f).lpf(II, 3450.0f, 0.09f).pan(I, -0.30f).pan(II, 0.34f).eg(I, 0.32f, 4.2f, 0.94f, 3.8f)
              .eg(II, 0.78f, 3.6f, 0.74f, 3.4f).feg(I, 0.13f, 0.22f, 0.53f, 5.1f, 2.35f)
              .feg(II, 0.28f, 0.34f, 1.32f, 3.8f, 2.6f).touch(I, 0.045f, 0.43f, 0.16f, 0.33f)
              .touch(II, 0.075f, 0.46f, 0.25f, 0.30f).mix(0.47f).detune(9.3f).chorus(0.43f, 0.27f).g(P_drift, 0.035f)
              .contour(-0.09f, -0.18f, -0.04f, 0.06f));
    add("Octave Pad", "Pads", { "feet-layer", "dual-rank" },
        "A 16' foundation and an 8' upper rank form one true octave. The lighter upper voice arrives later and fades sooner, leaving a rounded low tail.",
        pad()
              .feet(I, f16).feet(II, f8).wave(I, true, false, 0.11f).hpf(I, 24.0f).pw(II, 0.56f, 0.12f, 0.19f)
              .hpf(II, 220.0f).lpf(I, 1190.0f, 0.065f).lpf(II, 2880.0f, 0.11f).eg(I, 0.52f, 4.2f, 0.97f, 3.9f)
              .eg(II, 0.95f, 3.4f, 0.64f, 2.3f).feg(I, 0.15f, 0.18f, 0.47f, 4.8f, 2.55f)
              .feg(II, 0.28f, 0.31f, 1.45f, 3.8f, 2.4f).touch(I, 0.03f, 0.40f, 0.13f, 0.32f)
              .touch(II, 0.07f, 0.39f, 0.32f, 0.37f).mix(0.27f).detune(1.6f).chorus(0.25f, 0.22f)
              .contour(-0.15f, -0.14f, -0.08f, 0.065f));
    add("Fifth Pad", "Pads", { "feet-layer", "dual-rank" },
        "An 8' root and a quieter 5 1/3' harmonic fifth bloom together. A restrained upper filter crest and little detune keep the open interval clear.",
        pad()
              .feet(II, f513).pw(II, 0.51f, 0.025f, 0.127f).hpf(II, 320.0f).lpf(I, 1580.0f, 0.065f).lpf(II, 3600.0f, 0.06f)
              .feg(I, 0.12f, 0.21f, 0.61f, 4.2f, 2.1f).feg(II, 0.22f, 0.20f, 1.30f, 4.9f, 2.5f)
              .eg(I, 0.48f, 4.1f, 0.96f, 3.1f).eg(II, 1.05f, 3.1f, 0.58f, 2.7f).touch(I, 0.04f, 0.41f, 0.16f, 0.31f)
              .touch(II, 0.045f, 0.33f, 0.14f, 0.26f).mix(0.23f).detune(0.0f).chorus(0.19f, 0.20f).g(P_drift, 0.018f)
              .contour(-0.08f, -0.17f, -0.035f, 0.06f));
    add("Evolving Pad", "Pads", { "sub-mod", "pwm", "dual-rank" },
        "Different rank envelopes evolve into a softly repeating rising filter motion. The 0.7 Hz original SUB supplies the repeating tilt; slow PWM adds an independent movement.",
        pad()
              .sub(subSawUp, 0.7f, 0.0f, 0.17f).wave(I, true, false, 0.11f).lpf(I, 1200.0f, 0.11f).lpf(II, 2600.0f, 0.19f)
              .feg(I, 0.26f, 0.36f, 1.85f, 6.4f, 3.1f).feg(II, 0.52f, 0.55f, 3.75f, 4.9f, 3.5f)
              .eg(I, 0.48f, 5.4f, 0.96f, 4.0f).eg(II, 1.22f, 4.5f, 0.66f, 4.9f).pw(II, 0.59f, 0.18f, 0.093f)
              .touch(I, 0.045f, 0.37f, 0.18f, 0.31f).touch(II, 0.065f, 0.35f, 0.34f, 0.39f).mix(0.35f).detune(3.2f)
              .chorus(0.23f, 0.18f).hpf(II, 230.0f).contour(-0.12f, -0.19f, -0.04f, 0.07f));
    add("Noise Pad", "Pads", { "noise", "dual-rank" },
        "A warm pitched saw sustains beneath a separately enveloped band of air. Pressure emphasizes the noise layer; its longer attack and release frame the airy texture.",
        pad()
              .hpf(I, 50.0f).detune(0.0f).g(P_analogNoise, 0.0f).pan(I, -0.045f).pan(II, 0.18f).wave(I, true, false, 0.10f)
              .wave(II, false, false, 0.0f, 0.52f).hpf(II, 1500.0f, 0.035f).lpf(I, 1600.0f, 0.085f).lpf(II, 6200.0f, 0.055f)
              .level(I, 0.96f, 0.48f).level(II, 1.0f, 0.33f).mix(0.22f).feg(I, 0.16f, 0.19f, 0.49f, 4.2f, 1.85f)
              .feg(II, 0.34f, 0.36f, 2.10f, 5.3f, 2.8f).eg(I, 0.39f, 3.8f, 0.94f, 2.9f).eg(II, 1.60f, 5.0f, 0.51f, 4.2f)
              .touch(I, 0.045f, 0.41f, 0.14f, 0.32f).touch(II, 0.055f, 0.21f, 0.18f, 0.46f).chorus(0.18f, 0.19f)
              .contour(-0.08f, -0.18f, -0.035f, 0.06f));
    add("Pressure Pad", "Pads", { "aftertouch", "dual-rank" },
        "A soft, dark bed gains most of its level and upper glow from pressure. The body stays warm as the later pulse rank opens; easing pressure lets the chord recede.",
        pad()
              .hpf(I, 30.0f).hpf(II, 130.0f).lpf(I, 880.0f, 0.075f).lpf(II, 1620.0f, 0.11f).level(I, 1.0f, 0.16f)
              .level(II, 0.96f, 0.10f).touch(I, 0.015f, 0.065f, 0.24f, 0.90f).touch(II, 0.025f, 0.09f, 0.40f, 0.96f)
              .feg(I, 0.07f, 0.12f, 0.22f, 3.2f, 1.4f).feg(II, 0.18f, 0.22f, 0.63f, 2.6f, 1.9f)
              .eg(I, 0.18f, 3.2f, 0.98f, 2.6f).eg(II, 0.44f, 2.6f, 0.88f, 2.9f).pw(II, 0.55f, 0.10f, 0.137f).mix(0.36f)
              .detune(2.8f).chorus(0.25f, 0.22f).contour(-0.09f, -0.16f, -0.03f, 0.07f));
    add("Sine Pad", "Pads", { "sine", "feet-layer", "aftertouch" },
        "Pure 8' and quieter 4' sines fade in at different speeds. Pressure shapes their level, not their bypassed filters; no chorus or added hiss clouds the rounded octave.",
        pad()
              .feet(II, f4).feg(I, 0.0f, 0.0f, 0.01f, 0.5f, 0.4f).feg(II, 0.0f, 0.0f, 0.01f, 0.5f, 0.4f).detune(0.0f)
              .g(P_chorusOn, 0.0f).g(P_analogNoise, 0.0f).wave(I, false, false, 0.88f).wave(II, false, false, 0.73f)
              .level(I, 0.0f, 0.50f).level(II, 0.0f, 0.38f).mix(0.19f).eg(I, 0.62f, 5.2f, 0.99f, 4.2f)
              .eg(II, 1.45f, 4.6f, 0.55f, 3.6f).touch(I, 0.0f, 0.50f, 0.0f, 0.38f).touch(II, 0.0f, 0.42f, 0.0f, 0.30f)
              .g(P_drift, 0.012f).g(P_voiceDispersion, 0.025f).pan(I, -0.025f).pan(II, 0.12f)
              .contour(0.0f, 0.0f, -0.04f, 0.16f));
    add("Hollow Pad", "Pads", { "pwm", "dual-rank" },
        "Near-square pulses emphasize a hollow odd-harmonic center. Shallow, slow width motion and differently filtered ranks soften the sound without making a thin nasal pulse.",
        pad()
              .wave(I, false, true).wave(II, false, true).hpf(I, 45.0f).pw(I, 0.500f, 0.008f, 0.11f)
              .pw(II, 0.515f, 0.025f, 0.173f).hpf(II, 360.0f, 0.06f).lpf(I, 1800.0f, 0.085f).lpf(II, 2450.0f, 0.16f)
              .feg(I, 0.07f, 0.15f, 0.31f, 3.7f, 1.65f).feg(II, 0.18f, 0.24f, 0.79f, 2.9f, 2.1f)
              .eg(I, 0.31f, 3.7f, 0.95f, 2.9f).eg(II, 0.56f, 3.3f, 0.72f, 2.45f).touch(I, 0.04f, 0.42f, 0.13f, 0.31f)
              .touch(II, 0.07f, 0.38f, 0.23f, 0.30f).mix(0.32f).detune(1.2f).chorus(0.20f, 0.21f)
              .contour(-0.09f, -0.16f, -0.04f, 0.08f));
    add("Resonant Pad", "Pads", { "resonance", "brilliance-swell", "dual-rank" },
        "A resonant pulse crest opens above a smoother saw body. The upper filter peaks later and decays faster, giving a moving color without both ranks whistling together.",
        pad()
              .res(0.34f).lpf(I, 1300.0f, 0.15f).lpf(II, 1100.0f, 0.44f).hpf(II, 340.0f)
              .feg(I, 0.19f, 0.27f, 0.73f, 5.1f, 2.5f).feg(II, 0.46f, 0.65f, 1.85f, 2.9f, 2.1f)
              .eg(I, 0.38f, 4.3f, 0.94f, 3.5f).eg(II, 0.84f, 3.4f, 0.56f, 2.9f).level(II, 0.88f, 0.40f).mix(0.25f)
              .detune(2.9f).chorus(0.18f, 0.20f).touch(I, 0.04f, 0.40f, 0.12f, 0.32f).touch(II, 0.09f, 0.31f, 0.26f, 0.27f)
              .contour(-0.08f, -0.24f, -0.02f, 0.065f));
    add("Pulse Pad", "Pads", { "pwm", "dual-rank" },
        "Two pulse-only ranks use distinct PWM rates and depths. A steady low body anchors the moving upper harmonics, with modest chorus instead of excessive pitch detune.",
        pad()
              .wave(I, false, true).wave(II, false, true).hpf(I, 40.0f).pan(I, -0.11f).pan(II, 0.17f)
              .pw(I, 0.53f, 0.22f, 0.173f).pw(II, 0.61f, 0.31f, 0.283f).lpf(I, 2050.0f, 0.065f).lpf(II, 3600.0f, 0.10f)
              .hpf(II, 195.0f).feg(I, 0.08f, 0.15f, 0.44f, 4.5f, 1.95f).feg(II, 0.18f, 0.28f, 0.83f, 3.4f, 2.6f)
              .eg(I, 0.32f, 3.6f, 0.94f, 2.7f).eg(II, 0.66f, 3.1f, 0.74f, 3.0f).level(I, 0.96f, 0.52f)
              .level(II, 0.88f, 0.44f).touch(I, 0.07f, 0.42f, 0.17f, 0.33f).touch(II, 0.11f, 0.45f, 0.27f, 0.30f).mix(0.39f)
              .detune(1.8f).chorus(0.16f, 0.21f).contour(-0.10f, -0.17f, -0.04f, 0.075f));
    add("Tremolo Pad", "Pads", { "tremolo", "sub-mod", "dual-rank" },
        "A warm body and brighter pulse rank share gentle 3.8 Hz SUB amplitude motion. The circuit tremolo adds stereo movement while the whole chord rises and falls smoothly without chopping to silence.",
        pad()
              .tremolo(0.43f, 3.8f).sub(subSine, 3.8f, 0.0f, 0.0f, 0.24f).wave(I, true, false, 0.14f).hpf(II, 180.0f)
              .lpf(I, 1720.0f, 0.065f).lpf(II, 3250.0f, 0.09f).eg(I, 0.26f, 2.8f, 0.94f, 2.2f)
              .eg(II, 0.51f, 2.1f, 0.80f, 2.55f).feg(I, 0.06f, 0.12f, 0.24f, 2.9f, 1.35f)
              .feg(II, 0.16f, 0.22f, 0.61f, 2.2f, 1.8f).pw(II, 0.52f, 0.024f, 0.137f).touch(I, 0.045f, 0.40f, 0.12f, 0.31f)
              .touch(II, 0.08f, 0.44f, 0.21f, 0.29f).mix(0.35f).detune(1.5f).pan(I, -0.06f).pan(II, 0.13f)
              .g(P_modWheelVcf, 0.08f).contour(-0.09f, -0.15f, -0.035f, 0.07f));
    add("Neo-Noir Pad", "Pads", { "chorus", "ring-mod", "dual-rank" },
        "A dark saw foundation supports a later pulse glow, a broad stereo field, chorus and a faint slow ring beat. Long, unequal releases leave a warm cinematic tail after the upper color recedes.",
        pad()
              .wave(I, true, false, 0.10f).hpf(I, 35.0f).hpf(II, 190.0f).ring(0.3f, 0.06f, 3.0f, 5.0f, 0.08f).mix(0.39f)
              .pan(I, -0.18f).pan(II, 0.40f).lpf(I, 1280.0f, 0.085f).lpf(II, 2800.0f, 0.14f).pw(II, 0.58f, 0.18f, 0.113f)
              .feg(I, 0.26f, 0.35f, 1.55f, 6.3f, 3.5f).feg(II, 0.48f, 0.53f, 2.55f, 4.8f, 3.25f)
              .eg(I, 0.62f, 5.4f, 0.94f, 6.0f).eg(II, 1.17f, 4.5f, 0.76f, 4.1f).touch(I, 0.04f, 0.37f, 0.19f, 0.34f)
              .touch(II, 0.075f, 0.39f, 0.31f, 0.36f).chorus(0.43f, 0.23f).detune(6.6f).g(P_drift, 0.055f)
              .contour(-0.13f, -0.18f, -0.04f, 0.07f));
    add("Rain Pad", "Pads", { "modern", "lfo", "noise" },
        "An 8 Hz sample-and-hold LFO flickers the filters over a soft pulse body and a little band-limited air. The independent upper envelope lets the airy layer emerge after the pitched body.",
        pad()
              .wave(I, false, true, 0.04f).hpf(I, 65.0f).sub(subSine, 5.0f, 0.0f, 0.0f, 0.0f).subAt(0.0f, 0.0f, 0.0f)
              .g(P_lfo1Wave, 5.0f).g(P_analogNoise, 0.0f).pan(I, -0.055f).pan(II, 0.14f).wave(II, false, true, 0.0f, 0.09f)
              .pw(I, 0.535f, 0.075f, 0.173f).pw(II, 0.61f, 0.11f, 0.269f).lfo(0, 8.0f, 0.16f, toFilter)
              .hpf(II, 1200.0f, 0.03f).lpf(I, 1760.0f, 0.09f).lpf(II, 5600.0f, 0.095f)
              .feg(I, 0.08f, 0.17f, 0.29f, 3.7f, 1.55f).feg(II, 0.16f, 0.25f, 0.73f, 2.9f, 2.0f)
              .eg(I, 0.29f, 3.6f, 0.92f, 2.65f).eg(II, 0.96f, 4.2f, 0.55f, 3.7f).touch(I, 0.05f, 0.36f, 0.18f, 0.34f)
              .touch(II, 0.06f, 0.27f, 0.20f, 0.36f).mix(0.29f).detune(2.6f).chorus(0.19f, 0.21f)
              .contour(-0.07f, -0.20f, -0.025f, 0.075f));
    add("Cathedral Pad", "Pads", { "feet-layer", "sine", "dual-rank" },
        "A slow 16' saw foundation carries a quieter 4' sine two octaves above. The clean upper stop enters later; a long dry release suggests space without hidden reverb.",
        pad()
              .feet(I, f16).wave(II, false, false, 0.82f).feet(II, f4).feg(II, 0.0f, 0.0f, 0.01f, 0.5f, 0.4f).detune(0.0f)
              .hpf(I, 26.0f).wave(I, true, false, 0.12f).lpf(I, 1900.0f, 0.065f).feg(I, 0.20f, 0.23f, 1.24f, 5.5f, 3.0f)
              .eg(I, 0.84f, 5.6f, 0.97f, 6.4f).eg(II, 2.10f, 4.8f, 0.57f, 5.2f).level(II, 0.0f, 0.42f).mix(0.19f)
              .touch(I, 0.03f, 0.42f, 0.12f, 0.34f).touch(II, 0.0f, 0.32f, 0.0f, 0.38f).chorus(0.16f, 0.18f)
              .g(P_drift, 0.022f).contour(-0.12f, -0.18f, -0.08f, 0.08f));
    add("Shimmer Pad", "Pads", { "feet-layer", "pwm", "dual-rank" },
        "A soft 8' chord supports a light 2' pulse shimmer. The high rank is deliberately opened for its register and enters late, separating the upper sparkle from the body.",
        pad()
              .feet(II, f2).wave(II, false, true, 0.08f).wave(I, true, false, 0.16f).lpf(I, 1650.0f, 0.065f)
              .lpf(II, 11000.0f, 0.03f).hpf(II, 1600.0f).pw(II, 0.59f, 0.12f, 0.223f).level(II, 0.72f, 0.42f).mix(0.18f)
              .feg(I, 0.18f, 0.25f, 0.79f, 4.2f, 2.1f).feg(II, 0.22f, 0.26f, 1.72f, 5.5f, 2.8f)
              .eg(I, 0.48f, 3.8f, 0.93f, 3.1f).eg(II, 1.46f, 6.3f, 0.37f, 3.4f).touch(I, 0.045f, 0.38f, 0.17f, 0.30f)
              .touch(II, 0.055f, 0.30f, 0.13f, 0.27f).detune(1.2f).chorus(0.29f, 0.26f)
              .contour(-0.07f, -0.22f, -0.02f, 0.06f));
    add("Low Drone", "Pads", { "feet-layer", "dual-rank" },
        "Stable 16' ranks sustain a deep drone: a rounded saw foundation and a quieter hollow pulse. Slow entry and a long low tail support held bass notes without constant pitch wobble.",
        pad()
              .feet(I, f16).feet(II, f16).wave(I, true, false, 0.17f).wave(II, false, true).g(P_chorusOn, 0.0f)
              .g(P_analogNoise, 0.0f).pw(II, 0.505f, 0.022f, 0.08f).hpf(I, 20.0f).hpf(II, 105.0f).lpf(I, 520.0f, 0.075f)
              .lpf(II, 980.0f, 0.14f).feg(I, 0.17f, 0.12f, 1.55f, 6.1f, 3.5f).feg(II, 0.29f, 0.24f, 2.5f, 4.9f, 3.1f)
              .eg(I, 1.30f, 5.0f, 1.0f, 5.2f).eg(II, 1.90f, 4.2f, 0.82f, 3.5f).touch(I, 0.02f, 0.40f, 0.08f, 0.33f)
              .touch(II, 0.035f, 0.33f, 0.17f, 0.38f).mix(0.25f).detune(0.45f).g(P_drift, 0.012f).g(P_voiceDispersion, 0.07f)
              .contour(-0.18f, -0.10f, -0.09f, 0.07f));
    add("Ether Pad", "Pads", { "noise", "sine", "dual-rank" },
        "A pure 8' sine fades in beneath a separate thin veil of filtered noise. The air arrives later and follows pressure more strongly, leaving the pitched center clean and soft.",
        pad()
              .feg(I, 0.0f, 0.0f, 0.01f, 0.5f, 0.4f).level(I, 0.0f, 0.63f).detune(0.0f).g(P_analogNoise, 0.0f)
              .pan(I, -0.035f).pan(II, 0.16f).wave(I, false, false, 0.92f).wave(II, false, false, 0.0f, 0.60f)
              .hpf(II, 2100.0f, 0.035f).lpf(II, 7200.0f, 0.05f).level(II, 1.0f, 0.37f).mix(0.24f)
              .feg(II, 0.27f, 0.23f, 2.05f, 4.9f, 3.0f).eg(I, 0.45f, 5.4f, 0.99f, 3.6f).eg(II, 1.80f, 4.9f, 0.45f, 5.4f)
              .touch(I, 0.0f, 0.44f, 0.0f, 0.38f).touch(II, 0.035f, 0.16f, 0.14f, 0.35f).chorus(0.14f, 0.18f)
              .g(P_drift, 0.018f).contour(0.0f, -0.15f, -0.035f, 0.12f));
    add("Velvet Pad", "Pads", { "chorus", "dual-rank" },
        "Softly filtered saw ranks overlap with shallow, unequal blooms. Modest detune and chorus make a dense, smooth chord; the second rank closes earlier on release.",
        pad()
              .wave(II, true, false).hpf(I, 34.0f).hpf(II, 155.0f).wave(I, true, false, 0.08f).lpf(I, 1200.0f, 0.045f)
              .lpf(II, 1880.0f, 0.065f).feg(I, 0.10f, 0.14f, 0.41f, 4.8f, 2.1f).feg(II, 0.24f, 0.23f, 0.97f, 3.8f, 2.15f)
              .eg(I, 0.46f, 4.5f, 0.96f, 3.4f).eg(II, 0.76f, 3.2f, 0.76f, 2.3f).touch(I, 0.035f, 0.40f, 0.13f, 0.35f)
              .touch(II, 0.065f, 0.35f, 0.21f, 0.29f).mix(0.37f).detune(2.3f).chorus(0.21f, 0.20f)
              .contour(-0.10f, -0.14f, -0.045f, 0.07f));
    add("Analog Haze", "Pads", { "detune", "pwm", "chorus" },
        "A broad saw and moving pulse cloud with deliberately stronger voice variation. Moderate drift and staggered envelopes give a haze while retaining a recognizable chord center.",
        pad()
              .wave(I, true, true).mix(0.44f).g(P_analogNoise, 0.012f).hpf(I, 42.0f).hpf(II, 215.0f)
              .pw(I, 0.52f, 0.12f, 0.11f).pw(II, 0.59f, 0.19f, 0.173f).lpf(I, 1650.0f, 0.085f).lpf(II, 3100.0f, 0.13f)
              .feg(I, 0.19f, 0.26f, 0.89f, 5.1f, 2.3f).feg(II, 0.36f, 0.39f, 1.80f, 5.4f, 3.1f)
              .eg(I, 0.58f, 4.5f, 0.93f, 3.7f).eg(II, 1.12f, 3.8f, 0.73f, 4.1f).level(I, 0.88f, 0.52f)
              .level(II, 0.82f, 0.44f).touch(I, 0.045f, 0.39f, 0.17f, 0.32f).touch(II, 0.07f, 0.40f, 0.25f, 0.36f)
              .detune(9.0f).chorus(0.37f, 0.23f).g(P_drift, 0.10f).g(P_voiceDispersion, 0.21f)
              .contour(-0.08f, -0.18f, -0.04f, 0.065f));
    add("Bowed Glass", "Pads", { "ring-mod", "sine", "feet-layer" },
        "Slowly entered 8' and 4' sines pass through light fixed-frequency ring modulation. Inharmonic sidebands form a bowed-glass color; pressure grows the tone through its amplitude.",
        pad()
              .feet(II, f4).feg(I, 0.0f, 0.0f, 0.01f, 0.5f, 0.4f).feg(II, 0.0f, 0.0f, 0.01f, 0.5f, 0.4f).detune(0.0f)
              .g(P_analogNoise, 0.0f).ring(217.0f, 0.0f, 0.8f, 3.8f, 0.19f).pan(I, -0.05f).pan(II, 0.16f)
              .wave(I, false, false, 0.86f).wave(II, false, false, 0.69f).level(I, 0.0f, 0.50f).level(II, 0.0f, 0.38f)
              .mix(0.23f).eg(I, 0.78f, 5.5f, 0.96f, 3.5f).eg(II, 1.24f, 4.5f, 0.48f, 4.6f).touch(I, 0.0f, 0.40f, 0.0f, 0.52f)
              .touch(II, 0.0f, 0.30f, 0.0f, 0.35f).chorus(0.11f, 0.18f).g(P_drift, 0.012f).g(P_voiceDispersion, 0.045f)
              .contour(0.0f, 0.0f, -0.05f, 0.14f));
    add("Night Pad", "Pads", { "chorus", "dual-rank" },
        "A dark low-mid saw body appears before a dim pulse halo. Shallow filter movement, a quiet upper layer and long releases keep the sustained chord spacious and subdued.",
        pad()
              .hpf(I, 28.0f).hpf(II, 130.0f).wave(I, true, false, 0.15f).pw(II, 0.54f, 0.065f, 0.10f).lpf(I, 680.0f, 0.075f)
              .lpf(II, 1280.0f, 0.115f).feg(I, 0.16f, 0.14f, 1.20f, 6.5f, 2.7f).feg(II, 0.28f, 0.24f, 2.35f, 5.1f, 3.2f)
              .eg(I, 0.84f, 4.8f, 0.96f, 5.1f).eg(II, 1.52f, 4.2f, 0.67f, 3.9f).level(I, 0.96f, 0.48f)
              .level(II, 0.88f, 0.38f).touch(I, 0.025f, 0.36f, 0.12f, 0.34f).touch(II, 0.05f, 0.34f, 0.22f, 0.38f).mix(0.27f)
              .detune(3.3f).chorus(0.30f, 0.19f).contour(-0.12f, -0.11f, -0.045f, 0.08f));
    add("Arctic Pad", "Pads", { "chorus", "feet-layer", "dual-rank" },
        "A thin, bright 8' saw band supports a quiet 4' pulse sheen. Distinct high-pass bands and later upper entry create an icy texture; gently spread ranks supply stereo width under moderate chorus.",
        pad()
              .wave(I, true, false).wave(II, false, true).feet(II, f4).level(II, 0.90f, 0.59f).mix(0.29f).pan(I, -0.12f)
              .pan(II, 0.40f).hpf(I, 300.0f, 0.045f).lpf(I, 4800.0f, 0.065f).hpf(II, 1320.0f, 0.065f).lpf(II, 8600.0f, 0.06f)
              .pw(II, 0.67f, 0.09f, 0.213f).feg(I, 0.13f, 0.19f, 0.53f, 4.4f, 2.0f).feg(II, 0.27f, 0.28f, 1.39f, 3.7f, 2.8f)
              .eg(I, 0.35f, 4.1f, 0.89f, 3.4f).eg(II, 0.92f, 3.7f, 0.72f, 3.2f).touch(I, 0.035f, 0.39f, 0.12f, 0.30f)
              .touch(II, 0.065f, 0.40f, 0.17f, 0.29f).chorus(0.30f, 0.29f).detune(2.1f)
              .contour(-0.08f, -0.22f, -0.025f, 0.08f));
}
