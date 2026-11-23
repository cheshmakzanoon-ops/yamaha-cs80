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
        pad().eg(I, 0.72f, 3.5f, 0.90f, 3.0f).eg(II, 1.15f, 3.0f, 0.82f, 3.7f)
             .feg(I, 0.22f, 0.25f, 1.05f, 3.8f, 2.0f).feg(II, 0.35f, 0.37f, 1.55f, 3.2f, 2.6f));
    add("Ring Drift Pad", "Pads", { "ring-mod", "dual-rank" },
        "A steady low-mid body and lighter pulse halo move through a slow ring beat. The ring frequency envelope adds a gentle drift while the held pitch remains recognizable.",
        pad().lpf(I, 1400.0f, 0.10f).lpf(II, 2600.0f, 0.16f)
             .pw(II, 0.61f, 0.20f, 0.17f).eg(II, 1.25f, 3.0f, 0.72f, 3.4f)
             .ring(0.65f, 0.10f, 3.2f, 5.0f, 0.14f).detune(4.0f).chorus(0.28f, 0.24f));
    add("Choir Pad", "Pads", { "resonance", "dual-rank" },
        "Two softly entered pulse ranks suggest an open vowel. A low body band supports a brighter nasal band; this is a synthesized choir color.",
        pad().wave(I, false, true, 0.08f).wave(II, false, true).pw(I, 0.61f, 0.04f, 0.16f).pw(II, 0.72f, 0.03f, 0.21f)
             .hpf(I, 180.0f, 0.10f).lpf(I, 780.0f, 0.36f).hpf(II, 950.0f, 0.16f).lpf(II, 2050.0f, 0.40f)
             .feg(I, 0.08f, 0.10f, 0.55f, 2.4f, 0.9f).feg(II, 0.12f, 0.16f, 0.85f, 2.0f, 1.0f)
             .eg(I, 0.32f, 2.8f, 0.91f, 1.9f).eg(II, 0.48f, 2.3f, 0.82f, 2.1f)
             .touch(I, 0.04f, 0.23f, 0.08f, 0.25f).touch(II, 0.05f, 0.24f, 0.12f, 0.30f)
             .level(II, 1.0f, 0.65f).mix(0.37f).res(0.20f).detune(2.3f).chorus(0.28f, 0.30f));
    add("Dark Choir", "Pads", { "resonance", "feet-layer" },
        "A covered 16' vowel-like body sits beneath a quiet 8' nasal band. Unequal rank attacks give this low choir color depth without a large detuned wobble.",
        pad().feet(I, f16).wave(I, false, true, 0.13f).wave(II, false, true).pw(I, 0.58f, 0.03f, 0.14f).pw(II, 0.68f, 0.02f, 0.18f)
             .hpf(I, 95.0f, 0.08f).lpf(I, 560.0f, 0.36f).hpf(II, 620.0f, 0.12f).lpf(II, 1360.0f, 0.38f)
             .feg(I, 0.08f, 0.08f, 0.8f, 3.0f, 1.2f).feg(II, 0.15f, 0.12f, 1.15f, 2.5f, 1.4f)
             .eg(I, 0.58f, 3.0f, 0.92f, 2.7f).eg(II, 0.85f, 2.8f, 0.79f, 2.4f)
             .touch(I, 0.02f, 0.22f, 0.06f, 0.30f).touch(II, 0.04f, 0.24f, 0.12f, 0.30f)
             .mix(0.28f).detune(1.6f).res(0.21f).chorus(0.22f, 0.25f));
    add("Sweep Pad", "Pads", { "brilliance-swell", "dual-rank" },
        "A sustained body opens over three seconds, followed by a higher pulse bloom. Hold beyond four seconds to hear both filter crests and their gradual settling.",
        pad().lpf(I, 950.0f, 0.18f).lpf(II, 1750.0f, 0.26f)
             .feg(I, 0.52f, 0.52f, 3.0f, 5.5f, 2.8f).feg(II, 0.68f, 0.60f, 3.9f, 4.6f, 3.2f)
             .eg(I, 0.70f, 4.5f, 0.91f, 3.3f).eg(II, 1.20f, 4.0f, 0.81f, 3.8f)
             .touch(I, 0.02f, 0.24f, 0.12f, 0.20f).touch(II, 0.04f, 0.26f, 0.18f, 0.22f)
             .mix(0.39f).res(0.17f).chorus(0.32f, 0.26f));
    add("Breathing Pad", "Pads", { "sub-mod", "pwm" },
        "A rounded pad gently swells and recedes at 0.7 Hz. The original SUB moves level by a few decibels while slower PWM keeps the upper rank alive.",
        pad().lpf(I, 1500.0f, 0.08f).lpf(II, 2400.0f, 0.10f).pw(II, 0.58f, 0.28f, 0.13f)
             .eg(I, 0.42f, 3.0f, 0.94f, 2.4f).eg(II, 0.72f, 2.8f, 0.84f, 2.8f)
             .sub(subSine, 0.7f, 0.0f, 0.035f, 0.38f).subAt(0.0f, 0.0f, 0.0f).chorus(0.26f, 0.22f));
    add("Glass Pad", "Pads", { "feet-layer", "sine", "dual-rank" },
        "An 8' soft body supports a late 2' sine and pulse glint. The high rank has its own brighter filter and lower sustain, letting the glass color float above the chord.",
        pad().wave(I, true, false, 0.10f).feet(II, f2).wave(II, false, true, 0.36f).pw(II, 0.51f, 0.07f, 0.19f)
             .hpf(I, 45.0f).lpf(I, 1250.0f, 0.08f).hpf(II, 720.0f).lpf(II, 6800.0f, 0.08f)
             .feg(II, 0.18f, 0.22f, 1.15f, 3.2f, 1.8f).eg(II, 0.95f, 4.2f, 0.62f, 3.2f)
             .level(II, 0.38f, 0.52f).touch(II, 0.08f, 0.24f, 0.10f, 0.22f)
             .mix(0.24f).detune(1.8f).chorus(0.33f, 0.26f));
    add("Warm Pad", "Pads", { "chorus", "sine" },
        "A filtered saw with a small sine foundation and a quiet soft-pulse companion. Compact stereo motion and modest pressure brightness preserve the warm center.",
        pad().wave(I, true, false, 0.14f).pw(II, 0.54f, 0.16f, 0.18f)
             .hpf(I, 30.0f).hpf(II, 65.0f).lpf(I, 980.0f, 0.07f).lpf(II, 1450.0f, 0.08f)
             .feg(I, 0.12f, 0.16f, 0.62f, 3.2f, 1.5f).feg(II, 0.18f, 0.20f, 0.9f, 2.7f, 1.8f)
             .eg(I, 0.38f, 3.0f, 0.92f, 2.3f).eg(II, 0.65f, 2.7f, 0.83f, 2.7f)
             .touch(Both, 0.04f, 0.25f, 0.12f, 0.24f).mix(0.31f).detune(3.4f).chorus(0.28f, 0.23f));
    add("Wide Pad", "Pads", { "chorus", "detune", "dual-rank" },
        "Saw and pulse ranks spread left and right with a broad chorus. Their different attacks and PWM rates create width while each side retains a useful chord body.",
        pad().wave(I, true, true).pw(I, 0.55f, 0.16f, 0.17f).pw(II, 0.59f, 0.29f, 0.29f)
             .lpf(I, 2100.0f, 0.08f).lpf(II, 3200.0f, 0.10f)
             .eg(I, 0.60f, 3.1f, 0.86f, 2.9f).eg(II, 0.83f, 3.5f, 0.87f, 3.3f)
             .pan(I, -0.42f).pan(II, 0.42f).mix(0.48f).detune(13.5f).chorus(0.62f, 0.33f));
    add("Octave Pad", "Pads", { "feet-layer", "dual-rank" },
        "A 16' foundation and an 8' upper rank form one true octave. The lighter upper voice arrives later and fades sooner, leaving a rounded low tail.",
        pad().feet(I, f16).feet(II, f8).wave(I, true, false, 0.11f)
             .hpf(I, 25.0f).hpf(II, 110.0f).lpf(I, 1050.0f, 0.08f).lpf(II, 2650.0f, 0.13f)
             .eg(I, 0.62f, 3.5f, 0.93f, 3.4f).eg(II, 1.0f, 2.9f, 0.76f, 2.7f)
             .feg(II, 0.26f, 0.30f, 1.35f, 3.1f, 1.8f).mix(0.32f).detune(3.0f).chorus(0.34f, 0.27f));
    add("Fifth Pad", "Pads", { "feet-layer", "dual-rank" },
        "An 8' root and a quieter 5 1/3' harmonic fifth bloom together. A restrained upper filter crest and little detune keep the open interval clear.",
        pad().feet(II, f513).pw(II, 0.53f, 0.15f, 0.16f)
             .lpf(I, 1450.0f, 0.08f).lpf(II, 2600.0f, 0.11f)
             .feg(II, 0.18f, 0.23f, 0.95f, 3.4f, 1.9f).eg(II, 0.72f, 3.3f, 0.82f, 2.6f)
             .mix(0.28f).detune(0.8f).chorus(0.27f, 0.25f));
    add("Evolving Pad", "Pads", { "sub-mod", "pwm", "dual-rank" },
        "Different rank envelopes evolve into a softly repeating rising filter motion. The 0.7 Hz original SUB supplies the repeating tilt; slow PWM adds an independent movement.",
        pad().pw(II, 0.60f, 0.35f, 0.11f).lpf(I, 1400.0f, 0.13f).lpf(II, 2900.0f, 0.22f)
             .feg(I, 0.18f, 0.26f, 1.4f, 5.5f, 2.0f).feg(II, 0.34f, 0.38f, 2.8f, 4.2f, 2.8f)
             .eg(I, 0.6f, 4.5f, 0.86f, 3.1f).eg(II, 1.5f, 3.8f, 0.74f, 3.7f)
             .sub(subSawUp, 0.7f, 0.0f, 0.17f).mix(0.39f).detune(5.0f).chorus(0.30f, 0.21f));
    add("Noise Pad", "Pads", { "noise", "dual-rank" },
        "A warm pitched saw sustains beneath a separately enveloped band of air. Pressure emphasizes the noise layer; its longer attack and release frame the airy texture.",
        pad().wave(I, true, false, 0.07f).wave(II, false, false, 0.0f, 0.75f)
             .hpf(I, 50.0f).lpf(I, 1900.0f, 0.10f).hpf(II, 700.0f).lpf(II, 4100.0f, 0.16f)
             .feg(II, 0.22f, 0.35f, 1.75f, 3.5f, 2.2f).eg(II, 1.1f, 3.8f, 0.76f, 3.6f)
             .level(II, 1.0f, 0.54f).touch(II, 0.08f, 0.28f, 0.18f, 0.48f)
             .mix(0.23f).detune(0.0f).chorus(0.24f, 0.23f).g(P_analogNoise, 0.0f));
    add("Pressure Pad", "Pads", { "aftertouch", "dual-rank" },
        "A soft, dark bed gains most of its level and upper glow from pressure. The body stays warm as the later pulse rank opens; easing pressure lets the chord recede.",
        pad().hpf(I, 30.0f).hpf(II, 80.0f).lpf(I, 1000.0f, 0.09f).lpf(II, 1750.0f, 0.13f)
             .feg(I, 0.12f, 0.13f, 0.65f, 3.0f, 1.7f).feg(II, 0.20f, 0.18f, 0.95f, 2.6f, 2.1f)
             .level(I, 1.0f, 0.19f).level(II, 0.96f, 0.13f)
             .touch(I, 0.02f, 0.08f, 0.30f, 0.85f).touch(II, 0.02f, 0.10f, 0.48f, 0.94f)
             .eg(I, 0.42f, 3.0f, 0.92f, 2.6f).eg(II, 0.72f, 2.7f, 0.84f, 3.0f)
             .mix(0.39f).detune(4.5f).chorus(0.32f, 0.27f));
    add("Sine Pad", "Pads", { "sine", "feet-layer", "aftertouch" },
        "Pure 8' and quieter 4' sines fade in at different speeds. Pressure shapes their level, not their bypassed filters; no chorus or added hiss clouds the rounded octave.",
        pad().wave(Both, false, false, 0.82f).feet(II, f4)
             .feg(Both, 0.0f, 0.0f, 0.01f, 0.5f, 0.4f)
             .eg(I, 0.62f, 3.2f, 0.95f, 3.0f).eg(II, 1.0f, 3.0f, 0.84f, 3.5f)
             .level(I, 0.0f, 0.64f).level(II, 0.0f, 0.52f)
             .touch(I, 0.0f, 0.24f, 0.0f, 0.30f).touch(II, 0.0f, 0.20f, 0.0f, 0.38f)
             .mix(0.24f).detune(0.0f).g(P_chorusOn, 0.0f).g(P_analogNoise, 0.0f)
             .g(P_drift, 0.025f).g(P_voiceDispersion, 0.06f));
    add("Hollow Pad", "Pads", { "pwm", "dual-rank" },
        "Near-square pulses emphasize a hollow odd-harmonic center. Shallow, slow width motion and differently filtered ranks soften the sound without making a thin nasal pulse.",
        pad().wave(Both, false, true).pw(I, 0.50f, 0.06f, 0.15f).pw(II, 0.52f, 0.08f, 0.21f)
             .hpf(I, 45.0f).hpf(II, 110.0f).lpf(I, 1650.0f, 0.10f).lpf(II, 2600.0f, 0.16f)
             .feg(I, 0.13f, 0.18f, 0.75f, 3.0f, 1.5f).feg(II, 0.20f, 0.24f, 1.1f, 2.7f, 1.8f)
             .eg(I, 0.45f, 3.1f, 0.90f, 2.5f).eg(II, 0.70f, 2.8f, 0.80f, 2.9f)
             .mix(0.38f).detune(3.1f).chorus(0.30f, 0.26f));
    add("Resonant Pad", "Pads", { "resonance", "brilliance-swell", "dual-rank" },
        "A resonant pulse crest opens above a smoother saw body. The upper filter peaks later and decays faster, giving a moving color without both ranks whistling together.",
        pad().lpf(I, 1250.0f, 0.18f).lpf(II, 980.0f, 0.50f).res(0.34f)
             .feg(I, 0.26f, 0.31f, 1.15f, 4.0f, 1.9f).feg(II, 0.48f, 0.58f, 1.75f, 2.7f, 1.5f)
             .eg(I, 0.55f, 3.6f, 0.89f, 2.8f).eg(II, 0.85f, 3.0f, 0.70f, 2.4f)
             .touch(II, 0.05f, 0.26f, 0.16f, 0.22f).mix(0.28f).detune(4.2f).chorus(0.24f, 0.24f));
    add("Pulse Pad", "Pads", { "pwm", "dual-rank" },
        "Two pulse-only ranks use distinct PWM rates and depths. A steady low body anchors the moving upper harmonics, with modest chorus instead of excessive pitch detune.",
        pad().wave(Both, false, true).pw(I, 0.56f, 0.42f, 0.31f).pw(II, 0.63f, 0.55f, 0.47f)
             .hpf(I, 40.0f).hpf(II, 120.0f).lpf(I, 2200.0f, 0.08f).lpf(II, 3700.0f, 0.12f)
             .eg(I, 0.4f, 3.0f, 0.89f, 2.3f).eg(II, 0.7f, 2.6f, 0.81f, 2.9f)
             .mix(0.43f).detune(3.0f).chorus(0.24f, 0.25f));
    add("Tremolo Pad", "Pads", { "tremolo", "sub-mod", "dual-rank" },
        "A warm body and brighter pulse rank share gentle 3.8 Hz SUB amplitude motion. The circuit tremolo adds stereo movement while the whole chord rises and falls smoothly without chopping to silence.",
        pad().lpf(I, 1650.0f, 0.08f).lpf(II, 2900.0f, 0.11f)
             .eg(I, 0.38f, 3.0f, 0.92f, 2.3f).eg(II, 0.65f, 2.8f, 0.85f, 2.7f)
             .tremolo(0.43f, 3.8f).sub(subSine, 3.8f, 0.0f, 0.0f, 0.24f).detune(3.6f).mix(0.39f));
    add("Neo-Noir Pad", "Pads", { "chorus", "ring-mod", "dual-rank" },
        "A dark saw foundation supports a later pulse glow, a broad stereo field, chorus and a faint slow ring beat. Long, unequal releases leave a warm cinematic tail after the upper color recedes.",
        pad().wave(I, true, false, 0.10f).lpf(I, 1100.0f, 0.10f).lpf(II, 2400.0f, 0.17f)
             .hpf(I, 35.0f).hpf(II, 190.0f).pw(II, 0.60f, 0.26f, 0.16f)
             .feg(I, 0.24f, 0.28f, 1.25f, 4.8f, 2.8f).feg(II, 0.40f, 0.42f, 1.8f, 3.8f, 2.4f)
             .eg(I, 0.85f, 4.2f, 0.91f, 5.2f).eg(II, 1.4f, 3.5f, 0.78f, 4.1f)
             .chorus(0.58f, 0.25f).ring(0.3f, 0.06f, 3.0f, 5.0f, 0.08f).detune(8.5f).mix(0.39f)
             .pan(I, -0.18f).pan(II, 0.40f));
    add("Rain Pad", "Pads", { "modern", "lfo", "noise" },
        "An 8 Hz sample-and-hold LFO flickers the filters over a soft pulse body and a little band-limited air. The independent upper envelope lets the airy layer emerge after the pitched body.",
        pad().wave(I, false, true, 0.04f).pw(I, 0.54f, 0.10f, 0.19f)
             .wave(II, false, true, 0.0f, 0.12f).pw(II, 0.62f, 0.16f, 0.25f)
             .hpf(I, 65.0f).lpf(I, 1800.0f, 0.24f).hpf(II, 500.0f).lpf(II, 3900.0f, 0.20f)
             .feg(I, 0.15f, 0.20f, 0.5f, 3.0f, 1.3f).feg(II, 0.20f, 0.26f, 0.8f, 2.5f, 1.7f)
             .eg(I, 0.35f, 3.0f, 0.88f, 2.2f).eg(II, 0.65f, 2.7f, 0.73f, 2.6f)
             .sub(subSine, 5.0f, 0.0f, 0.0f, 0.0f).subAt(0.0f, 0.0f, 0.0f)
             .lfo(0, 8.0f, 0.19f, toFilter).g(P_lfo1Wave, 5.0f)
             .mix(0.34f).detune(4.0f).chorus(0.25f, 0.25f).g(P_analogNoise, 0.0f));
    add("Cathedral Pad", "Pads", { "feet-layer", "sine", "dual-rank" },
        "A slow 16' saw foundation carries a quieter 4' sine two octaves above. The clean upper stop enters later; a long dry release suggests space without hidden reverb.",
        pad().feet(I, f16).wave(I, true, false, 0.10f).wave(II, false, false, 0.82f).feet(II, f4)
             .hpf(I, 25.0f).lpf(I, 1250.0f, 0.09f).feg(I, 0.18f, 0.22f, 1.1f, 4.0f, 2.6f)
             .feg(II, 0.0f, 0.0f, 0.01f, 0.5f, 0.4f)
             .eg(I, 0.85f, 3.8f, 0.95f, 4.7f).eg(II, 1.4f, 3.0f, 0.88f, 5.2f)
             .level(II, 0.0f, 0.54f).touch(II, 0.0f, 0.18f, 0.0f, 0.33f)
             .mix(0.22f).detune(0.0f).chorus(0.20f, 0.21f).g(P_drift, 0.04f));
    add("Shimmer Pad", "Pads", { "feet-layer", "pwm", "dual-rank" },
        "A soft 8' chord supports a light 2' pulse shimmer. The high rank is deliberately opened for its register and enters late, separating the upper sparkle from the body.",
        pad().lpf(I, 1500.0f, 0.08f).feet(II, f2).wave(II, false, true, 0.08f)
             .pw(II, 0.59f, 0.25f, 0.38f).hpf(II, 1050.0f).lpf(II, 8200.0f, 0.08f)
             .feg(II, 0.20f, 0.26f, 1.5f, 3.7f, 2.1f).eg(II, 1.1f, 3.2f, 0.63f, 3.7f)
             .level(II, 0.72f, 0.51f).touch(II, 0.05f, 0.22f, 0.12f, 0.27f)
             .mix(0.20f).detune(2.0f).chorus(0.38f, 0.33f));
    add("Low Drone", "Pads", { "feet-layer", "dual-rank" },
        "Stable 16' ranks sustain a deep drone: a rounded saw foundation and a quieter hollow pulse. Slow entry and a long low tail support held bass notes without constant pitch wobble.",
        pad().feet(Both, f16).wave(I, true, false, 0.17f).wave(II, false, true).pw(II, 0.51f, 0.08f, 0.10f)
             .hpf(I, 20.0f).hpf(II, 30.0f).lpf(I, 520.0f, 0.08f).lpf(II, 880.0f, 0.13f)
             .feg(I, 0.08f, 0.13f, 1.2f, 4.0f, 2.7f).feg(II, 0.15f, 0.18f, 1.8f, 3.6f, 2.2f)
             .eg(I, 1.05f, 4.0f, 1.0f, 4.5f).eg(II, 1.55f, 3.5f, 0.91f, 3.7f)
             .touch(I, 0.02f, 0.24f, 0.08f, 0.28f).touch(II, 0.04f, 0.24f, 0.15f, 0.26f)
             .mix(0.30f).detune(1.1f).g(P_chorusOn, 0.0f).g(P_drift, 0.035f).g(P_analogNoise, 0.0f));
    add("Ether Pad", "Pads", { "noise", "sine", "dual-rank" },
        "A pure 8' sine fades in beneath a separate thin veil of filtered noise. The air arrives later and follows pressure more strongly, leaving the pitched center clean and soft.",
        pad().wave(I, false, false, 0.85f).wave(II, false, false, 0.0f, 0.95f)
             .hpf(II, 1100.0f).lpf(II, 5600.0f, 0.08f)
             .feg(I, 0.0f, 0.0f, 0.01f, 0.5f, 0.4f).feg(II, 0.18f, 0.24f, 1.7f, 3.8f, 2.8f)
             .eg(I, 0.65f, 3.5f, 0.94f, 3.0f).eg(II, 1.25f, 3.0f, 0.68f, 4.0f)
             .level(I, 0.0f, 0.63f).level(II, 1.0f, 0.55f)
             .touch(I, 0.0f, 0.22f, 0.0f, 0.27f).touch(II, 0.04f, 0.20f, 0.12f, 0.44f)
             .mix(0.28f).detune(0.0f).chorus(0.20f, 0.24f).g(P_analogNoise, 0.0f).g(P_drift, 0.035f));
    add("Velvet Pad", "Pads", { "chorus", "dual-rank" },
        "Softly filtered saw ranks overlap with shallow, unequal blooms. Modest detune and chorus make a dense, smooth chord; the second rank closes earlier on release.",
        pad().wave(Both, true, false).lpf(I, 1250.0f, 0.06f).lpf(II, 1950.0f, 0.07f)
             .hpf(I, 38.0f).hpf(II, 95.0f)
             .feg(I, 0.10f, 0.15f, 1.0f, 3.7f, 1.8f).feg(II, 0.16f, 0.21f, 1.4f, 3.0f, 1.4f)
             .eg(I, 0.55f, 3.4f, 0.91f, 2.9f).eg(II, 0.80f, 3.0f, 0.83f, 2.4f)
             .touch(Both, 0.04f, 0.25f, 0.12f, 0.26f).mix(0.43f).detune(3.2f).chorus(0.27f, 0.24f));
    add("Analog Haze", "Pads", { "detune", "pwm", "chorus" },
        "A broad saw and moving pulse cloud with deliberately stronger voice variation. Moderate drift and staggered envelopes give a haze while retaining a recognizable chord center.",
        pad().wave(I, true, true).pw(I, 0.55f, 0.12f, 0.13f).pw(II, 0.62f, 0.32f, 0.21f)
             .lpf(I, 1700.0f, 0.10f).lpf(II, 2600.0f, 0.15f)
             .eg(I, 0.70f, 3.8f, 0.89f, 3.2f).eg(II, 1.05f, 3.1f, 0.80f, 3.8f)
             .g(P_drift, 0.27f).g(P_voiceDispersion, 0.40f).detune(11.0f)
             .mix(0.44f).chorus(0.46f, 0.27f).g(P_analogNoise, 0.012f));
    add("Bowed Glass", "Pads", { "ring-mod", "sine", "feet-layer" },
        "Slowly entered 8' and 4' sines pass through light fixed-frequency ring modulation. Inharmonic sidebands form a bowed-glass color; pressure grows the tone through its amplitude.",
        pad().wave(Both, false, false, 0.82f).feet(II, f4)
             .feg(Both, 0.0f, 0.0f, 0.01f, 0.5f, 0.4f)
             .eg(I, 0.75f, 3.6f, 0.88f, 3.3f).eg(II, 1.15f, 3.0f, 0.72f, 3.9f)
             .level(I, 0.0f, 0.62f).level(II, 0.0f, 0.52f)
             .touch(I, 0.0f, 0.20f, 0.0f, 0.32f).touch(II, 0.0f, 0.24f, 0.0f, 0.38f)
             .ring(217.0f, 0.0f, 0.8f, 3.0f, 0.23f).mix(0.26f).detune(0.0f)
             .chorus(0.18f, 0.22f).g(P_analogNoise, 0.0f).g(P_drift, 0.025f));
    add("Night Pad", "Pads", { "chorus", "dual-rank" },
        "A dark low-mid saw body appears before a dim pulse halo. Shallow filter movement, a quiet upper layer and long releases keep the sustained chord spacious and subdued.",
        pad().wave(I, true, false, 0.13f).hpf(I, 28.0f).hpf(II, 120.0f)
             .lpf(I, 780.0f, 0.09f).lpf(II, 1350.0f, 0.14f).pw(II, 0.58f, 0.19f, 0.14f)
             .feg(I, 0.13f, 0.18f, 1.3f, 4.2f, 2.5f).feg(II, 0.24f, 0.25f, 1.9f, 3.7f, 2.9f)
             .eg(I, 0.9f, 3.8f, 0.92f, 4.0f).eg(II, 1.4f, 3.1f, 0.74f, 4.6f)
             .touch(I, 0.02f, 0.24f, 0.10f, 0.28f).touch(II, 0.04f, 0.26f, 0.18f, 0.28f)
             .mix(0.31f).detune(5.2f).chorus(0.42f, 0.23f));
    add("Arctic Pad", "Pads", { "chorus", "feet-layer", "dual-rank" },
        "A thin, bright 8' saw band supports a quiet 4' pulse sheen. Distinct high-pass bands and later upper entry create an icy texture; gently spread ranks supply stereo width under moderate chorus.",
        pad().wave(I, true, false).wave(II, false, true).feet(II, f4).pw(II, 0.66f, 0.18f, 0.31f)
             .hpf(I, 330.0f, 0.06f).lpf(I, 4300.0f, 0.08f).hpf(II, 1100.0f, 0.08f).lpf(II, 7800.0f, 0.08f)
             .feg(I, 0.10f, 0.15f, 0.75f, 3.2f, 1.6f).feg(II, 0.18f, 0.20f, 1.15f, 2.7f, 2.0f)
             .eg(I, 0.48f, 3.0f, 0.86f, 2.7f).eg(II, 0.85f, 2.6f, 0.72f, 3.3f)
             .touch(I, 0.06f, 0.28f, 0.10f, 0.25f).touch(II, 0.06f, 0.26f, 0.12f, 0.24f)
             .level(II, 0.90f, 0.59f).mix(0.29f).detune(3.5f).chorus(0.40f, 0.34f)
             .pan(I, -0.12f).pan(II, 0.40f));
}
