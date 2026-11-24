// Bass voicings. Included inside namespace curated by CuratedLibrary.inc.
// Existing program names and insertion order are stable host/MIDI identities.
// Rank I normally carries the 16' foundation; rank II has an explicitly
// balanced attack or harmonic role. Decay times are approximately -60 dB times.

// Touch-sensitive voices leave most level to initial touch. Individual
// fixed floors keep soft notes usable without flattening the player's
// dynamics. Pure decaying bodies retain a small aftersound pedestal;
// mixed circuit plucks still reach zero, and Pedal Bass stays level-neutral.
Program bass()
{
    Program x;
    x.wave(I, true, false, 0.18f).wave(II, false, true).feet(I, f16).feet(II, f8).pw(II, 0.57f)
     .hpf(I, 22.0f).hpf(II, 75.0f).lpf(I, 780.0f, 0.17f).lpf(II, 1800.0f, 0.12f)
     .feg(I, 0.0f, 0.42f, 0.006f, 0.72f, 0.12f).feg(II, 0.0f, 0.50f, 0.003f, 0.28f, 0.08f)
     .eg(I, 0.004f, 1.5f, 0.72f, 0.16f).eg(II, 0.0025f, 0.65f, 0.24f, 0.10f)
     .touch(I, 0.15f, 0.62f, 0.10f, 0.12f).touch(II, 0.28f, 0.72f, 0.16f, 0.08f)
     .level(I, 1.0f, 0.38f).level(II, 0.82f, 0.26f)
     .sub(subSine, 4.8f, 0.0f).subAt(0.0f, 0.0f).g(P_modWheelSub, 0.0f)
     .g(P_voiceDispersion, 0.08f).g(P_drift, 0.025f).g(P_analogNoise, 0.0f)
     .mix(0.25f).detune(0.0f).res(0.12f).volume(0.6f);
    return x;
}

void buildBass(Builder& add)
{
    add("Analog Bass", "Bass", { "feet-layer", "velocity" },
        "A firm 16-foot saw-and-sine body carries a brisk, separately decaying 8-foot pulse edge. A cleaner low sustain and more gradual filter settling give repeated notes weight without leaving an abrasive upper buzz.",
        bass().level(II, 0.82f, 0.04f).level(I, 1.0f, 0.085f).feg(I, 0.0f, 0.39f, 0.005f, 1.10f, 0.085f).eg(II, 0.0018f, 0.39f, 0.09f, 0.050f).mix(0.19f)
              .lpf(I, 820.0f, 0.10f).lpf(II, 2300.0f, 0.075f).eg(I, 0.004f, 2.05f, 0.74f, 0.11f)
              .touch(II, 0.27f, 0.87f, 0.09f, 0.045f).hpf(I, 20.0f).hpf(II, 105.0f)
              .feg(II, 0.0f, 0.57f, 0.002f, 0.19f, 0.055f).touch(I, 0.12f, 0.71f, 0.075f, 0.10f).res(0.085f)
              .contour(-0.04f, -0.14f, -0.09f, 0.04f));
    add("Resonance Bass", "Bass", { "resonance", "velocity" },
        "A fast resonant upper sweep settles above a firm sine-supported low note. The body opens more slowly than the edge, keeping the attack lively while the low sustain remains focused.",
        bass().level(II, 0.82f, 0.035f).level(I, 1.0f, 0.075f).wave(I, true, false, 0.23f).lpf(I, 410.0f, 0.27f).lpf(II, 1320.0f, 0.53f)
              .feg(I, 0.0f, 0.54f, 0.006f, 0.94f, 0.075f).feg(II, 0.0f, 0.82f, 0.002f, 0.20f, 0.045f)
              .eg(II, 0.0018f, 0.29f, 0.055f, 0.050f).touch(II, 0.32f, 0.78f, 0.10f, 0.05f).res(0.32f).mix(0.17f)
              .eg(I, 0.0045f, 1.9f, 0.70f, 0.10f).hpf(II, 115.0f).touch(I, 0.18f, 0.75f, 0.08f, 0.10f)
              .contour(-0.03f, -0.20f, -0.075f, 0.035f));
    add("Sine Bass", "Bass", { "sine", "percussive", "velocity" },
        "A rounded 16-foot sine takes over from a very brief, velocity-sensitive upper click. A slower low-frequency onset and quicker click release reduce mechanical edges while the held fundamental stays clean and steady.",
        bass().wave(I, false, false, 1.0f).wave(II, false, true).pw(II, 0.66f).hpf(II, 720.0f)
              .lpf(II, 4100.0f, 0.025f).feg(II, 0.0f, 0.18f, 0.0014f, 0.065f, 0.025f)
              .eg(I, 0.009f, 0.75f, 1.0f, 0.115f).eg(II, 0.0014f, 0.115f, 0.0f, 0.022f)
              .touch(I, 0.0f, 0.84f, 0.0f, 0.0f).touch(II, 0.10f, 0.93f, 0.0f, 0.0f).level(I, 0.0f, 0.055f)
              .level(II, 0.95f, 0.018f).mix(0.29f).res(0.04f).g(P_drift, 0.0f).contour(0.0f, -0.10f, -0.10f, 0.035f)
              .g(P_voiceDispersion, 0.0f));
    add("Sub Bass", "Bass", { "sine", "velocity" },
        "A centered 16-foot sine has a smooth low-frequency onset, useful touch dynamics and a tightly controlled ending. Keyboard level contour gently restrains the lowest notes while the pure sustaining body remains stable.",
        bass().wave(I, false, false, 1.0f).wave(II, false, false).feet(I, f16).feet(II, f16)
              .eg(I, 0.015f, 1.0f, 1.0f, 0.125f).touch(I, 0.0f, 0.94f, 0.0f, 0.0f).touch(II, 0.0f, 0.0f, 0.0f, 0.0f)
              .level(I, 0.0f, 0.055f).level(II, 0.0f, 0.0f).mix(0.0f).g(P_voiceDispersion, 0.0f).g(P_drift, 0.0f)
              .contour(0.0f, 0.0f, -0.16f, 0.045f));
    add("Pluck Bass", "Bass", { "percussive", "velocity" },
        "A plucked-string spectrum joins the compact resonant low body. The upper pick decays first and high string partials soften independently, leaving a clearly pitched low tail that fully dies on a held key.",
        bass().lpf(I, 3200.0f, 0.07f).lpf(II, 5600.0f, 0.045f).feg(I, 0.0f, 0.19f, 0.004f, 1.05f, 0.09f)
              .feg(II, 0.0f, 0.27f, 0.0015f, 0.16f, 0.045f).eg(I, 0.0042f, 4.2f, 0.0f, 0.115f)
              .eg(II, 0.0018f, 0.18f, 0.0f, 0.038f).touch(I, 0.10f, 0.94f, 0.0f, 0.0f)
              .touch(II, 0.18f, 1.0f, 0.0f, 0.0f).level(I, 1.0f, 0.065f).level(II, 0.88f, 0.025f).mix(0.18f)
              .body(I, body::Model::bass, 0.95f, 0.46f, 0.90f).body(II, body::Model::steel, 0.48f, 0.76f, 0.25f)
              .hpf(I, 21.0f).hpf(II, 420.0f).bodySize(I, 1.20f).bodySize(II, 1.35f).res(0.065f)
              .contour(-0.07f, -0.16f, -0.08f, 0.045f));
    add("Fretless Glide", "Bass", { "glide", "aftertouch", "percussive" },
        "A soft fingered string spectrum sings through a long low decay. Independent partial losses soften the note after the onset; short portamento joins phrases and pressure adds restrained vibrato and a singing upper edge.",
        bass().level(II, 0.82f, 0.035f).level(I, 1.0f, 0.075f).wave(I, false, true, 0.48f).pw(I, 0.54f).wave(II, true, false, 0.0f, 0.010f).feet(I, f16).feet(II, f16)
              .hpf(I, 21.0f).hpf(II, 285.0f).lpf(I, 2850.0f, 0.085f).lpf(II, 3900.0f, 0.11f)
              .feg(I, 0.035f, 0.15f, 0.020f, 1.55f, 0.15f).feg(II, 0.0f, 0.23f, 0.007f, 0.29f, 0.08f)
              .eg(I, 0.01f, 6.8f, 0.07f, 0.125f).eg(II, 0.0045f, 0.72f, 0.0f, 0.075f)
              .touch(I, 0.05f, 0.85f, 0.095f, 0.21f).touch(II, 0.10f, 0.75f, 0.17f, 0.12f).glide(0.046f)
              .subAt(0.025f, 0.060f).g(P_modWheelSub, 0.035f).mix(0.14f).res(0.08f)
              .body(I, body::Model::bass, 1.0f, 0.40f, 1.65f).body(II, body::Model::bass, 0.60f, 0.59f, 0.39f)
              .bodySize(I, 1.25f).bodySize(II, 1.14f).contour(-0.055f, -0.18f, -0.075f, 0.055f));
    add("Funk Bass", "Bass", { "velocity", "resonance", "percussive" },
        "A dry fingered-string snap sits inside a tight resonant filter pluck. Hard strikes expose the brief upper edge; independently damping string partials keep the low body defined between rhythmic notes.",
        bass().wave(I, true, false, 0.30f).hpf(I, 20.0f).lpf(I, 1450.0f, 0.20f).lpf(II, 2450.0f, 0.34f).pw(II, 0.67f)
              .feg(I, 0.0f, 0.35f, 0.0038f, 0.46f, 0.065f).feg(II, 0.0f, 0.63f, 0.0016f, 0.15f, 0.04f)
              .eg(I, 0.0035f, 2.15f, 0.0f, 0.080f).eg(II, 0.0015f, 0.20f, 0.0f, 0.035f).level(I, 1.0f, 0.090f)
              .level(II, 0.90f, 0.03f).touch(I, 0.18f, 1.0f, 0.0f, 0.0f).touch(II, 0.35f, 1.0f, 0.0f, 0.0f)
              .res(0.24f).mix(0.10f).body(I, body::Model::bass, 0.63f, 0.62f, 0.52f)
              .body(II, body::Model::clav, 0.36f, 0.83f, 0.25f).hpf(II, 380.0f).bodySize(I, 1.08f).bodySize(II, 1.30f)
              .contour(-0.02f, -0.17f, -0.065f, 0.04f));
    add("Brass Bass", "Bass", { "brilliance-swell", "aftertouch" },
        "A solid 16-foot saw body opens into an 8-foot brass spectrum. Unequal attack and filter blooms separate the low support from the lip-like upper accent; pressure opens the held brass color without loosening the foundation.",
        bass().level(II, 0.82f, 0.055f).level(I, 1.0f, 0.105f).wave(II, true, false).lpf(I, 660.0f, 0.105f).lpf(II, 3300.0f, 0.095f)
              .feg(I, 0.12f, 0.33f, 0.055f, 1.65f, 0.145f).feg(II, 0.17f, 0.38f, 0.082f, 0.91f, 0.12f)
              .eg(I, 0.019f, 1.9f, 0.86f, 0.155f).eg(II, 0.040f, 0.98f, 0.50f, 0.11f)
              .touch(I, 0.10f, 0.68f, 0.13f, 0.14f).touch(II, 0.16f, 0.76f, 0.26f, 0.18f).mix(0.26f)
              .body(II, body::Model::brass, 0.62f, 0.47f, 1.05f).hpf(I, 21.0f).hpf(II, 110.0f).bodySize(II, 1.55f)
              .contour(-0.06f, -0.14f, -0.08f, 0.025f));
    add("Octave Bass", "Bass", { "feet-layer", "velocity" },
        "A firm lower saw and a separately shaped octave retain two audible registers. The upper attack relaxes into a controlled sustain while the low rank settles more slowly, adding movement without detuning the bass.",
        bass().level(I, 1.0f, 0.085f).wave(I, true, false, 0.12f).wave(II, true, false).lpf(I, 960.0f, 0.065f).lpf(II, 2800.0f, 0.055f)
              .feg(I, 0.0f, 0.27f, 0.006f, 1.08f, 0.10f).feg(II, 0.0f, 0.30f, 0.005f, 0.36f, 0.065f)
              .eg(I, 0.0055f, 2.1f, 0.83f, 0.12f).eg(II, 0.004f, 0.75f, 0.53f, 0.08f).level(II, 0.90f, 0.06f)
              .mix(0.25f).touch(II, 0.18f, 0.84f, 0.08f, 0.045f).hpf(I, 21.0f).hpf(II, 95.0f)
              .touch(I, 0.12f, 0.72f, 0.075f, 0.10f).contour(-0.04f, -0.15f, -0.07f, 0.035f));
    add("Pulse Bass", "Bass", { "pwm", "velocity" },
        "Two low pulses move with independently paced shallow PWM above a stable sine foundation. A compact upper onset and calmer held harmonic balance keep the width motion clear without softening the bass center.",
        bass().level(II, 0.82f, 0.045f).level(I, 1.0f, 0.085f).wave(I, false, true, 0.20f).wave(II, false, true).feet(I, f16).feet(II, f16)
              .pw(I, 0.515f, 0.065f, 0.14f).pw(II, 0.61f, 0.12f, 0.23f).hpf(II, 110.0f).lpf(I, 990.0f, 0.065f)
              .lpf(II, 2200.0f, 0.055f).eg(I, 0.006f, 2.2f, 0.85f, 0.12f).eg(II, 0.0065f, 0.79f, 0.45f, 0.08f)
              .feg(II, 0.0f, 0.25f, 0.005f, 0.43f, 0.075f).detune(0.0f).mix(0.18f)
              .feg(I, 0.0f, 0.30f, 0.007f, 1.18f, 0.10f).touch(I, 0.10f, 0.72f, 0.08f, 0.10f)
              .contour(-0.045f, -0.18f, -0.065f, 0.025f));
    add("Round Bass", "Bass", { "sine", "velocity" },
        "A soft fingered-string component adds a gentle falling articulation to the warm sustaining sine-and-saw body. A quiet same-register edge supplies definition while low resonance keeps the bass rounded.",
        bass().wave(I, true, false, 0.50f).wave(II, false, true).feet(I, f16).feet(II, f16).pw(II, 0.52f)
              .lpf(I, 1100.0f, 0.025f).lpf(II, 1500.0f, 0.025f).level(I, 0.55f, 0.11f).level(II, 0.42f, 0.055f)
              .feg(I, 0.0f, 0.10f, 0.014f, 1.05f, 0.12f).feg(II, 0.0f, 0.15f, 0.008f, 0.30f, 0.075f)
              .eg(I, 0.011f, 2.15f, 0.85f, 0.145f).eg(II, 0.006f, 0.62f, 0.25f, 0.075f)
              .touch(I, 0.025f, 0.76f, 0.0f, 0.07f).touch(II, 0.055f, 0.68f, 0.0f, 0.0f).res(0.03f).mix(0.15f)
              .body(I, body::Model::bass, 0.50f, 0.19f, 1.40f).hpf(II, 105.0f).bodySize(I, 1.35f)
              .contour(-0.06f, -0.12f, -0.10f, 0.04f));
    add("Velocity Bass", "Bass", { "velocity" },
        "Soft strikes expose a covered low body while hard strikes bring out a much brighter, shorter upper attack. The two filter and amplitude contours settle at different rates, keeping the struck dynamic distinct in a phrase.",
        bass().level(I, 1.0f, 0.045f).level(II, 0.92f, 0.018f).touch(I, 0.28f, 0.97f, 0.0f, 0.0f)
              .touch(II, 0.47f, 1.0f, 0.0f, 0.0f).lpf(I, 610.0f, 0.075f).lpf(II, 2250.0f, 0.12f)
              .feg(II, 0.0f, 0.61f, 0.0018f, 0.21f, 0.05f).eg(II, 0.0018f, 0.46f, 0.065f, 0.05f).mix(0.22f)
              .feg(I, 0.0f, 0.38f, 0.005f, 1.10f, 0.08f).eg(I, 0.0045f, 2.05f, 0.72f, 0.11f).hpf(I, 21.0f)
              .hpf(II, 130.0f).contour(-0.025f, -0.18f, -0.055f, 0.035f));
    add("Rubber Bass", "Bass", { "init-bend", "percussive" },
        "A brief upward pitch scoop lands on a rounded resonant pluck. A small string-like component strengthens the initial note center while both ranks decay completely, avoiding a permanent sustain or ongoing pitch wobble.",
        bass().level(II, 0.82f, 0.035f).level(I, 1.0f, 0.09f).bend(0.25f).wave(I, false, true, 0.28f).pw(I, 0.56f).lpf(I, 670.0f, 0.24f).lpf(II, 1900.0f, 0.17f)
              .feg(I, 0.0f, 0.43f, 0.0045f, 0.56f, 0.075f).feg(II, 0.0f, 0.55f, 0.002f, 0.145f, 0.04f)
              .eg(I, 0.0055f, 1.82f, 0.0f, 0.14f).eg(II, 0.0018f, 0.19f, 0.0f, 0.035f)
              .touch(I, 0.14f, 0.80f, 0.0f, 0.0f).touch(II, 0.24f, 0.88f, 0.0f, 0.0f).res(0.18f).mix(0.19f)
              .body(I, body::Model::bass, 0.28f, 0.46f, 0.45f).hpf(II, 135.0f).bodySize(I, 1.12f)
              .contour(-0.025f, -0.13f, -0.07f, 0.035f));
    add("Dark Bass", "Bass", { "sine" },
        "A deep sine-supported body has a covered same-register saw shadow and a softly falling string component. The restrained upper spectrum preserves pitch clarity while a smooth onset and short release keep the dark bass controlled.",
        bass().wave(I, true, false, 0.58f).wave(II, true, false).feet(I, f16).feet(II, f16).lpf(I, 420.0f, 0.025f)
              .lpf(II, 620.0f, 0.035f).level(I, 0.65f, 0.13f).level(II, 0.50f, 0.06f)
              .feg(I, 0.0f, 0.085f, 0.018f, 1.05f, 0.12f).feg(II, 0.0f, 0.13f, 0.009f, 0.35f, 0.08f)
              .eg(I, 0.014f, 2.2f, 0.91f, 0.15f).eg(II, 0.008f, 0.71f, 0.40f, 0.08f)
              .touch(I, 0.025f, 0.70f, 0.035f, 0.065f).touch(II, 0.04f, 0.66f, 0.045f, 0.055f).res(0.05f).mix(0.13f)
              .body(I, body::Model::bass, 0.37f, 0.10f, 1.60f).hpf(II, 85.0f).bodySize(I, 1.55f)
              .contour(-0.07f, -0.09f, -0.13f, 0.035f));
    add("Bright Bass", "Bass", { "velocity", "feet-layer" },
        "A full low saw body supports a clear, velocity-sensitive upper saw edge. The brighter rank decays into a restrained sustain, leaving a firm foundation beneath the articulate attack.",
        bass().level(I, 1.0f, 0.075f).wave(II, true, false).lpf(I, 1550.0f, 0.055f).lpf(II, 5200.0f, 0.065f).hpf(II, 235.0f)
              .feg(I, 0.0f, 0.23f, 0.0045f, 1.04f, 0.075f).feg(II, 0.0f, 0.34f, 0.0018f, 0.19f, 0.045f)
              .eg(II, 0.0018f, 0.52f, 0.16f, 0.055f).level(II, 0.82f, 0.04f).touch(II, 0.20f, 0.85f, 0.09f, 0.04f)
              .mix(0.21f).res(0.065f).eg(I, 0.0045f, 2.0f, 0.76f, 0.11f).touch(I, 0.12f, 0.74f, 0.07f, 0.09f)
              .contour(-0.02f, -0.23f, -0.065f, 0.02f));
    add("Picked Bass", "Bass", { "velocity", "percussive" },
        "A short, firm pick excites a low string spectrum with independent harmonic losses. The upper pick vanishes quickly while the fundamental and lower modes continue ringing, then damp promptly on key release.",
        bass().wave(I, true, false, 0.30f).wave(II, false, true, 0.0f, 0.018f).pw(II, 0.74f).hpf(II, 580.0f)
              .lpf(I, 4100.0f, 0.06f).lpf(II, 6400.0f, 0.035f).feg(I, 0.0f, 0.13f, 0.005f, 1.15f, 0.11f)
              .feg(II, 0.0f, 0.27f, 0.0014f, 0.095f, 0.035f).eg(I, 0.004f, 6.4f, 0.0f, 0.1f)
              .eg(II, 0.0014f, 0.115f, 0.0f, 0.026f).level(I, 1.0f, 0.06f).level(II, 0.90f, 0.025f)
              .touch(I, 0.075f, 0.90f, 0.0f, 0.0f).touch(II, 0.19f, 1.0f, 0.0f, 0.0f).mix(0.20f)
              .body(I, body::Model::bass, 1.0f, 0.68f, 1.55f).body(II, body::Model::steel, 0.68f, 0.87f, 0.25f)
              .hpf(I, 21.0f).bodySize(I, 1.12f).bodySize(II, 1.35f).res(0.065f)
              .contour(-0.045f, -0.18f, -0.08f, 0.05f));
    add("Upright Pluck", "Bass", { "sine", "percussive", "velocity" },
        "A rounded low string spectrum has a woody finger onset and a gently falling body. Independently damping upper partials leave a clear fundamental, while a small circuit-noise transient supplies a brief finger-and-string texture.",
        bass().wave(I, true, false, 0.70f).wave(II, false, true, 0.0f, 0.048f).feet(I, f16).feet(II, f16)
              .pw(II, 0.55f).hpf(I, 24.0f).hpf(II, 360.0f).lpf(I, 2800.0f, 0.055f).lpf(II, 3900.0f, 0.12f)
              .level(I, 0.42f, 0.07f).level(II, 0.95f, 0.025f).feg(I, 0.0f, 0.12f, 0.011f, 1.55f, 0.14f)
              .feg(II, 0.0f, 0.24f, 0.0025f, 0.22f, 0.055f).eg(I, 0.009f, 5.8f, 0.035f, 0.14f)
              .eg(II, 0.0018f, 0.35f, 0.0f, 0.055f).touch(I, 0.04f, 0.94f, 0.0f, 0.0f)
              .touch(II, 0.10f, 1.0f, 0.0f, 0.0f).mix(0.19f).res(0.055f)
              .body(I, body::Model::bass, 1.0f, 0.30f, 0.86f).body(II, body::Model::nylon, 0.52f, 0.65f, 0.28f)
              .bodySize(I, 1.85f).bodySize(II, 2.10f).contour(-0.10f, -0.19f, -0.12f, 0.07f));
    add("Fifth Bass", "Bass", { "feet-layer" },
        "A stable low foundation supports a quieter upper twelfth. Its upper rank speaks briefly at the strike, then settles into a lean harmonic support, making the fifth clear without masking the bass fundamental.",
        bass().level(I, 1.0f, 0.09f).feet(II, f513).wave(II, true, false).hpf(II, 210.0f).lpf(II, 3200.0f, 0.055f)
              .feg(II, 0.0f, 0.27f, 0.0035f, 0.26f, 0.055f).eg(II, 0.005f, 0.64f, 0.30f, 0.075f)
              .touch(II, 0.12f, 0.70f, 0.08f, 0.04f).level(II, 0.78f, 0.055f).mix(0.17f).lpf(I, 880.0f, 0.075f)
              .eg(I, 0.0055f, 2.1f, 0.79f, 0.12f).feg(I, 0.0f, 0.32f, 0.006f, 1.12f, 0.10f)
              .touch(I, 0.12f, 0.71f, 0.07f, 0.10f).contour(-0.045f, -0.18f, -0.08f, 0.03f));
    add("Pedal Bass", "Bass", { "sine", "feet-layer" },
        "Steady low and octave sine stops use rounded keying and closely controlled releases. A gentle low-key level contour restrains boom; velocity remains neutral so held organ-style bass lines stay even.",
        bass().wave(I, false, false, 1.0f).wave(II, false, false, 1.0f).eg(I, 0.024f, 0.4f, 1.0f, 0.095f)
              .eg(II, 0.012f, 0.4f, 1.0f, 0.06f).feg(I, 0.0f, 0.0f, 0.01f, 0.3f, 0.1f)
              .feg(II, 0.0f, 0.0f, 0.01f, 0.3f, 0.1f).touch(I, 0.0f, 0.0f, 0.0f, 0.0f)
              .touch(II, 0.0f, 0.0f, 0.0f, 0.0f).level(I, 0.0f, 0.88f).level(II, 0.0f, 0.54f).mix(0.18f)
              .g(P_voiceDispersion, 0.0f).g(P_drift, 0.0f).contour(0.0f, 0.0f, -0.14f, 0.015f));
    add("Squelch Bass", "Bass", { "resonance", "velocity" },
        "A sharply resonant upper contour sweeps into a compact sine-supported bass. The upper accent closes rapidly while the lower filter settles more gradually, preserving a firm note center under the squelch.",
        bass().level(I, 1.0f, 0.07f).wave(I, true, false, 0.28f).lpf(I, 390.0f, 0.29f).lpf(II, 1200.0f, 0.62f)
              .feg(I, 0.0f, 0.50f, 0.0048f, 0.79f, 0.06f).feg(II, 0.0f, 0.92f, 0.0014f, 0.145f, 0.035f)
              .eg(I, 0.0038f, 1.72f, 0.54f, 0.085f).eg(II, 0.0018f, 0.27f, 0.035f, 0.040f)
              .touch(I, 0.15f, 0.79f, 0.0f, 0.0f).touch(II, 0.30f, 0.91f, 0.0f, 0.0f).level(II, 0.78f, 0.03f)
              .res(0.40f).mix(0.18f).hpf(II, 135.0f).contour(-0.02f, -0.23f, -0.065f, 0.025f));
    add("Ring Bass", "Bass", { "ring-mod" },
        "A centered low body feeds a light 35 Hz ring growl beneath a narrower upper attack. Unequal filter settling and restrained upper sustain keep the moving sidebands textured while the dry bass remains clear.",
        bass().level(II, 0.82f, 0.045f).level(I, 1.0f, 0.09f).wave(I, true, false, 0.30f).pw(II, 0.65f).lpf(I, 740.0f, 0.065f).lpf(II, 2350.0f, 0.085f)
              .eg(II, 0.0035f, 0.68f, 0.23f, 0.07f).mix(0.15f).ring(35.0f, 0.0f, 0.015f, 1.15f, 0.13f).res(0.10f)
              .feg(I, 0.0f, 0.28f, 0.008f, 1.30f, 0.085f).feg(II, 0.0f, 0.40f, 0.003f, 0.26f, 0.055f).hpf(II, 135.0f)
              .eg(I, 0.006f, 2.05f, 0.79f, 0.13f).touch(I, 0.12f, 0.74f, 0.075f, 0.10f)
              .contour(-0.04f, -0.17f, -0.09f, 0.035f));
    add("Sub Wobble Bass", "Bass", { "sub-mod", "sine" },
        "A 3 Hz filter movement animates the upper pulse layer while a stable bypass sine supports the bass. A more compact onset and separated rank contours keep the rhythmic motion clear without pitch wobble.",
        bass().wave(I, true, false, 0.52f).wave(II, false, true).pw(II, 0.58f).lpf(I, 530.0f, 0.12f)
              .lpf(II, 2100.0f, 0.30f).feg(I, 0.0f, 0.09f, 0.012f, 0.92f, 0.10f)
              .feg(II, 0.0f, 0.15f, 0.008f, 0.31f, 0.075f).eg(I, 0.008f, 1.85f, 0.92f, 0.14f)
              .eg(II, 0.009f, 0.63f, 0.61f, 0.095f).level(I, 0.70f, 0.105f).level(II, 0.92f, 0.045f)
              .sub(subSine, 3.0f, 0.0f, 0.28f).res(0.22f).mix(0.21f).hpf(II, 135.0f)
              .touch(I, 0.08f, 0.72f, 0.05f, 0.10f).touch(II, 0.17f, 0.82f, 0.11f, 0.07f)
              .contour(-0.05f, -0.17f, -0.08f, 0.035f));
    add("Pressure Bass", "Bass", { "aftertouch", "sine" },
        "A firm sine-supported note speaks immediately, with a soft falling string detail at the onset. Pressure then raises the brighter upper circuit rank and opens its spectrum, keeping the low center steady as the phrase grows.",
        bass().wave(I, true, false, 0.35f).lpf(I, 780.0f, 0.075f).lpf(II, 1750.0f, 0.18f).hpf(I, 20.0f)
              .hpf(II, 105.0f).feg(I, 0.0f, 0.13f, 0.015f, 1.10f, 0.14f).feg(II, 0.03f, 0.22f, 0.018f, 0.58f, 0.095f)
              .eg(I, 0.008f, 2.15f, 0.90f, 0.15f).eg(II, 0.011f, 0.78f, 0.61f, 0.10f).level(I, 1.0f, 0.12f)
              .level(II, 0.90f, 0.035f).touch(I, 0.06f, 0.62f, 0.11f, 0.25f).touch(II, 0.11f, 0.40f, 0.56f, 0.76f)
              .mix(0.25f).res(0.10f).body(I, body::Model::bass, 0.28f, 0.27f, 1.40f).bodySize(I, 1.35f)
              .contour(-0.06f, -0.14f, -0.09f, 0.03f));
    add("Contour Bass", "Bass", { "kbd-contour", "velocity" },
        "A dry saw-and-pulse bass has a restrained low-key level and a clearer upper-register attack. Different rank decays and keyboard brightness contour keep fast runs articulated without adding pitch instability.",
        bass().level(II, 0.82f, 0.03f).level(I, 1.0f, 0.07f).lpf(I, 920.0f, 0.065f).lpf(II, 2650.0f, 0.075f).eg(I, 0.0055f, 2.2f, 0.78f, 0.11f)
              .eg(II, 0.0025f, 0.43f, 0.12f, 0.06f).touch(I, 0.09f, 0.81f, 0.06f, 0.08f).mix(0.17f)
              .contour(-0.09f, 0.18f, -0.17f, 0.025f).feg(I, 0.0f, 0.36f, 0.0055f, 1.13f, 0.085f)
              .feg(II, 0.0f, 0.47f, 0.0025f, 0.18f, 0.05f).hpf(II, 125.0f).touch(II, 0.22f, 0.87f, 0.10f, 0.06f));
}
