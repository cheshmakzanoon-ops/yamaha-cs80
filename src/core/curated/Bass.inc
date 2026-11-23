// Bass voicings. Included inside namespace curated by CuratedLibrary.inc.
// Existing program names and insertion order are stable host/MIDI identities.
// Rank I normally carries the 16' foundation; rank II has an explicitly
// balanced attack or harmonic role. Decay times are approximately -60 dB times.

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
        bass().feg(I, 0.0f, 0.44f, 0.005f, 0.78f, 0.12f)
              .eg(II, 0.002f, 0.58f, 0.22f, 0.09f).mix(0.27f)
              .lpf(I, 690.0f, 0.13f).lpf(II, 2050.0f, 0.10f)
              .feg(I, 0.0f, 0.48f, 0.004f, 0.95f, 0.10f).eg(I, 0.0032f, 1.8f, 0.68f, 0.13f)
              .eg(II, 0.0017f, 0.46f, 0.15f, 0.065f).touch(II, 0.31f, 0.81f, 0.13f, 0.06f).mix(0.22f));
    add("Resonance Bass", "Bass", { "resonance", "velocity" },
        "A fast resonant upper sweep settles above a firm sine-supported low note. The body opens more slowly than the edge, keeping the attack lively while the low sustain remains focused.",
        bass().wave(I, true, false, 0.16f).lpf(I, 430.0f, 0.40f).lpf(II, 1020.0f, 0.52f)
              .feg(I, 0.0f, 0.56f, 0.004f, 0.64f, 0.11f).feg(II, 0.0f, 0.70f, 0.002f, 0.32f, 0.08f)
              .eg(II, 0.002f, 0.45f, 0.12f, 0.09f).touch(II, 0.32f, 0.78f, 0.10f, 0.05f)
              .res(0.38f).mix(0.23f)
              .lpf(I, 350.0f, 0.34f).lpf(II, 1180.0f, 0.56f)
              .feg(I, 0.0f, 0.61f, 0.005f, 0.81f, 0.09f).feg(II, 0.0f, 0.74f, 0.0015f, 0.24f, 0.06f)
              .eg(I, 0.0035f, 1.65f, 0.64f, 0.12f).eg(II, 0.0015f, 0.34f, 0.08f, 0.065f).mix(0.19f));
    add("Sine Bass", "Bass", { "sine", "percussive", "velocity" },
        "A rounded 16-foot sine takes over from a very brief, velocity-sensitive upper click. A slower low-frequency onset and quicker click release reduce mechanical edges while the held fundamental stays clean and steady.",
        bass().wave(I, false, false, 1.0f).wave(II, false, true).pw(II, 0.70f)
              .hpf(II, 450.0f).lpf(II, 4300.0f, 0.06f).feg(II, 0.0f, 0.25f, 0.001f, 0.09f, 0.04f)
              .eg(I, 0.005f, 0.6f, 1.0f, 0.18f).eg(II, 0.001f, 0.14f, 0.0f, 0.045f)
              .touch(I, 0.0f, 0.64f, 0.0f, 0.0f).touch(II, 0.10f, 0.75f, 0.0f, 0.0f)
              .level(I, 0.0f, 0.32f).level(II, 0.95f, 0.08f).mix(0.21f).res(0.04f)
              .eg(I, 0.0075f, 0.6f, 1.0f, 0.145f).eg(II, 0.0012f, 0.105f, 0.0f, 0.03f)
              .hpf(II, 650.0f).lpf(II, 3600.0f, 0.035f).touch(I, 0.0f, 0.76f, 0.0f, 0.0f)
              .touch(II, 0.13f, 0.82f, 0.0f, 0.0f).mix(0.27f).g(P_drift, 0.0f));
    add("Sub Bass", "Bass", { "sine", "velocity" },
        "A centered 16-foot sine has a smooth low-frequency onset, useful touch dynamics and a tightly controlled ending. Keyboard level contour gently restrains the lowest notes while the pure sustaining body remains stable.",
        bass().wave(I, false, false, 1.0f).wave(II, false, false)
              .feet(Both, f16).eg(I, 0.008f, 0.8f, 1.0f, 0.20f)
              .touch(I, 0.0f, 0.70f, 0.0f, 0.0f).touch(II, 0.0f, 0.0f, 0.0f, 0.0f)
              .level(I, 0.0f, 0.30f).level(II, 0.0f, 0.0f)
              .mix(0.0f).g(P_voiceDispersion, 0.035f).g(P_drift, 0.0f)
              .eg(I, 0.012f, 0.8f, 1.0f, 0.16f).touch(I, 0.0f, 0.82f, 0.0f, 0.0f)
              .level(I, 0.0f, 0.24f).contour(0.0f, 0.0f, -0.12f, 0.0f).g(P_voiceDispersion, 0.0f));
    add("Pluck Bass", "Bass", { "percussive", "velocity" },
        "A plucked-string spectrum joins the compact resonant low body. The upper pick decays first and high string partials soften independently, leaving a clearly pitched low tail that fully dies on a held key.",
        bass().lpf(I, 670.0f, 0.18f).lpf(II, 2600.0f, 0.10f)
              .feg(I, 0.0f, 0.52f, 0.003f, 0.85f, 0.12f).feg(II, 0.0f, 0.52f, 0.0015f, 0.21f, 0.07f)
              .eg(I, 0.003f, 2.4f, 0.0f, 0.22f).eg(II, 0.0015f, 0.28f, 0.0f, 0.07f)
              .touch(I, 0.18f, 0.78f, 0.0f, 0.0f).touch(II, 0.30f, 0.92f, 0.0f, 0.0f)
              .level(I, 1.0f, 0.22f).level(II, 0.88f, 0.12f).mix(0.20f)
              .body(I, body::Model::bass, 0.84f, 0.51f, 0.75f).body(II, body::Model::steel, 0.38f, 0.83f, 0.29f)
              .eg(I, 0.0028f, 2.85f, 0.0f, 0.17f).eg(II, 0.0012f, 0.22f, 0.0f, 0.055f).mix(0.16f));
    add("Fretless Glide", "Bass", { "glide", "aftertouch", "percussive" },
        "A soft fingered string spectrum sings through a long low decay. Independent partial losses soften the note after the onset; short portamento joins phrases and pressure adds restrained vibrato and a singing upper edge.",
        bass().wave(I, false, true, 0.48f).pw(I, 0.54f).wave(II, true, false, 0.0f, 0.018f).feet(Both, f16)
              .hpf(I, 22.0f).hpf(II, 210.0f).lpf(I, 850.0f, 0.10f).lpf(II, 2300.0f, 0.17f)
              .feg(I, 0.04f, 0.22f, 0.016f, 1.25f, 0.20f).feg(II, 0.0f, 0.34f, 0.005f, 0.40f, 0.12f)
              .eg(I, 0.009f, 4.8f, 0.0f, 0.28f).eg(II, 0.004f, 1.15f, 0.0f, 0.14f)
              .touch(I, 0.08f, 0.65f, 0.12f, 0.24f).touch(II, 0.16f, 0.62f, 0.22f, 0.14f)
              .glide(0.065f).subAt(0.035f, 0.075f).g(P_modWheelSub, 0.035f).mix(0.15f).res(0.08f)
              .body(I, body::Model::bass, 0.96f, 0.36f, 1.55f).body(II, body::Model::bass, 0.57f, 0.67f, 0.48f)
              .eg(I, 0.007f, 5.6f, 0.0f, 0.23f).eg(II, 0.003f, 0.90f, 0.0f, 0.10f).mix(0.12f).glide(0.052f));
    add("Funk Bass", "Bass", { "velocity", "resonance", "percussive" },
        "A dry fingered-string snap sits inside a tight resonant filter pluck. Hard strikes expose the brief upper edge; independently damping string partials keep the low body defined between rhythmic notes.",
        bass().lpf(I, 510.0f, 0.25f).lpf(II, 1550.0f, 0.40f).pw(II, 0.67f)
              .feg(I, 0.0f, 0.48f, 0.003f, 0.52f, 0.08f).feg(II, 0.0f, 0.68f, 0.0015f, 0.20f, 0.06f)
              .eg(I, 0.0025f, 1.75f, 0.0f, 0.13f).eg(II, 0.0015f, 0.33f, 0.0f, 0.07f)
              .level(I, 1.0f, 0.289474f).level(II, 0.90f, 0.133333f)
              .touch(I, 0.25f, 1.0f, 0.0f, 0.0f).touch(II, 0.44f, 1.0f, 0.0f, 0.0f).res(0.28f).mix(0.26f)
              .body(I, body::Model::bass, 0.59f, 0.69f, 0.42f).body(II, body::Model::clav, 0.32f, 0.90f, 0.25f)
              .eg(I, 0.002f, 1.95f, 0.0f, 0.10f).eg(II, 0.0012f, 0.26f, 0.0f, 0.05f).mix(0.21f));
    add("Brass Bass", "Bass", { "brilliance-swell", "aftertouch" },
        "A solid 16-foot saw body opens into an 8-foot brass spectrum. Unequal attack and filter blooms separate the low support from the lip-like upper accent; pressure opens the held brass color without loosening the foundation.",
        bass().wave(II, true, false).lpf(I, 580.0f, 0.15f).lpf(II, 1250.0f, 0.18f)
              .feg(I, 0.19f, 0.44f, 0.085f, 1.25f, 0.22f).feg(II, 0.28f, 0.48f, 0.14f, 1.05f, 0.19f)
              .eg(I, 0.022f, 1.4f, 0.84f, 0.23f).eg(II, 0.045f, 1.1f, 0.65f, 0.18f)
              .touch(I, 0.12f, 0.58f, 0.18f, 0.18f).touch(II, 0.19f, 0.62f, 0.30f, 0.20f).mix(0.28f)
              .body(II, body::Model::brass, 0.43f, 0.55f, 0.91f)
              .feg(I, 0.16f, 0.40f, 0.065f, 1.45f, 0.18f).feg(II, 0.23f, 0.52f, 0.105f, 1.20f, 0.16f)
              .eg(I, 0.015f, 1.65f, 0.81f, 0.18f).eg(II, 0.032f, 1.25f, 0.58f, 0.14f).mix(0.23f));
    add("Octave Bass", "Bass", { "feet-layer", "velocity" },
        "A firm lower saw and a separately shaped octave retain two audible registers. The upper attack relaxes into a controlled sustain while the low rank settles more slowly, adding movement without detuning the bass.",
        bass().wave(I, true, false, 0.12f).wave(II, true, false)
              .lpf(I, 950.0f, 0.12f).lpf(II, 2100.0f, 0.10f)
              .feg(I, 0.0f, 0.32f, 0.005f, 0.85f, 0.13f).feg(II, 0.0f, 0.28f, 0.007f, 0.65f, 0.12f)
              .eg(I, 0.004f, 1.5f, 0.82f, 0.17f).eg(II, 0.006f, 1.2f, 0.70f, 0.14f)
              .level(II, 0.90f, 0.30f).mix(0.32f)
              .lpf(I, 830.0f, 0.09f).lpf(II, 2450.0f, 0.075f)
              .eg(I, 0.0045f, 1.85f, 0.78f, 0.14f).eg(II, 0.0035f, 0.92f, 0.62f, 0.10f)
              .feg(II, 0.0f, 0.34f, 0.004f, 0.48f, 0.085f).touch(II, 0.22f, 0.78f, 0.10f, 0.06f).mix(0.28f));
    add("Pulse Bass", "Bass", { "pwm", "velocity" },
        "Two low pulses move with independently paced shallow PWM above a stable sine foundation. A compact upper onset and calmer held harmonic balance keep the width motion clear without softening the bass center.",
        bass().wave(I, false, true, 0.20f).wave(II, false, true).feet(Both, f16)
              .pw(I, 0.54f, 0.12f, 0.22f).pw(II, 0.62f, 0.20f, 0.31f)
              .hpf(II, 70.0f).lpf(I, 920.0f, 0.12f).lpf(II, 1700.0f, 0.10f)
              .eg(I, 0.004f, 1.7f, 0.84f, 0.17f).eg(II, 0.007f, 1.2f, 0.66f, 0.13f)
              .feg(II, 0.0f, 0.30f, 0.006f, 0.65f, 0.11f).detune(0.6f).mix(0.24f)
              .pw(I, 0.52f, 0.09f, 0.17f).pw(II, 0.64f, 0.17f, 0.29f)
              .lpf(I, 810.0f, 0.085f).lpf(II, 1950.0f, 0.075f)
              .eg(I, 0.0045f, 1.95f, 0.80f, 0.14f).eg(II, 0.004f, 0.93f, 0.55f, 0.10f).detune(0.25f).mix(0.20f));
    add("Round Bass", "Bass", { "sine", "velocity" },
        "A soft fingered-string component adds a gentle falling articulation to the warm sustaining sine-and-saw body. A quiet same-register edge supplies definition while low resonance keeps the bass rounded.",
        bass().wave(I, true, false, 0.50f).wave(II, false, true).feet(Both, f16).pw(II, 0.52f)
              .lpf(I, 420.0f, 0.03f).lpf(II, 1000.0f, 0.04f).level(I, 0.55f, 0.38f).level(II, 0.42f, 0.24f)
              .feg(I, 0.0f, 0.15f, 0.012f, 0.80f, 0.15f).feg(II, 0.0f, 0.20f, 0.010f, 0.42f, 0.10f)
              .eg(I, 0.010f, 1.4f, 0.86f, 0.20f).eg(II, 0.008f, 1.0f, 0.48f, 0.13f)
              .touch(I, 0.04f, 0.60f, 0.0f, 0.08f).touch(II, 0.08f, 0.55f, 0.0f, 0.0f).res(0.03f).mix(0.16f)
              .body(I, body::Model::bass, 0.42f, 0.23f, 1.24f)
              .lpf(I, 360.0f, 0.025f).lpf(II, 1150.0f, 0.03f)
              .eg(I, 0.008f, 1.8f, 0.80f, 0.17f).eg(II, 0.005f, 0.78f, 0.36f, 0.10f).mix(0.13f));
    add("Velocity Bass", "Bass", { "velocity" },
        "Soft strikes expose a covered low body while hard strikes bring out a much brighter, shorter upper attack. The two filter and amplitude contours settle at different rates, keeping the struck dynamic distinct in a phrase.",
        bass().level(I, 1.0f, 0.14f).level(II, 0.92f, 0.06f)
              .touch(I, 0.34f, 0.88f, 0.0f, 0.0f).touch(II, 0.54f, 0.96f, 0.0f, 0.0f)
              .lpf(I, 580.0f, 0.12f).lpf(II, 1450.0f, 0.18f)
              .feg(II, 0.0f, 0.44f, 0.002f, 0.36f, 0.09f).eg(II, 0.002f, 0.78f, 0.18f, 0.09f).mix(0.24f)
              .lpf(I, 490.0f, 0.09f).lpf(II, 1800.0f, 0.15f)
              .feg(I, 0.0f, 0.45f, 0.004f, 0.96f, 0.10f).feg(II, 0.0f, 0.54f, 0.0015f, 0.27f, 0.065f)
              .eg(I, 0.0032f, 1.8f, 0.66f, 0.13f).eg(II, 0.0015f, 0.60f, 0.10f, 0.065f).mix(0.20f));
    add("Rubber Bass", "Bass", { "init-bend", "percussive" },
        "A brief upward pitch scoop lands on a rounded resonant pluck. A small string-like component strengthens the initial note center while both ranks decay completely, avoiding a permanent sustain or ongoing pitch wobble.",
        bass().bend(0.32f).wave(I, false, true, 0.28f).pw(I, 0.56f)
              .lpf(I, 580.0f, 0.28f).lpf(II, 1650.0f, 0.22f)
              .feg(I, 0.0f, 0.46f, 0.004f, 0.58f, 0.12f).feg(II, 0.0f, 0.55f, 0.002f, 0.25f, 0.08f)
              .eg(I, 0.004f, 2.0f, 0.0f, 0.18f).eg(II, 0.002f, 0.30f, 0.0f, 0.07f)
              .touch(Both, 0.18f, 0.68f, 0.0f, 0.0f).res(0.21f).mix(0.20f)
              .body(I, body::Model::bass, 0.36f, 0.39f, 0.53f)
              .feg(I, 0.0f, 0.49f, 0.003f, 0.66f, 0.10f).feg(II, 0.0f, 0.61f, 0.0015f, 0.19f, 0.06f)
              .eg(II, 0.0015f, 0.24f, 0.0f, 0.05f).mix(0.17f).bend(0.25f));
    add("Dark Bass", "Bass", { "sine" },
        "A deep sine-supported body has a covered same-register saw shadow and a softly falling string component. The restrained upper spectrum preserves pitch clarity while a smooth onset and short release keep the dark bass controlled.",
        bass().wave(I, true, false, 0.58f).wave(II, true, false).feet(Both, f16)
              .lpf(I, 270.0f, 0.06f).lpf(II, 540.0f, 0.08f).level(I, 0.65f, 0.40f).level(II, 0.50f, 0.26f)
              .feg(I, 0.0f, 0.12f, 0.014f, 0.72f, 0.15f).feg(II, 0.0f, 0.18f, 0.010f, 0.46f, 0.11f)
              .eg(I, 0.009f, 1.5f, 0.91f, 0.22f).eg(II, 0.012f, 1.1f, 0.66f, 0.15f)
              .touch(Both, 0.04f, 0.58f, 0.05f, 0.08f).res(0.05f).mix(0.14f)
              .body(I, body::Model::bass, 0.29f, 0.14f, 1.48f)
              .lpf(I, 235.0f, 0.035f).lpf(II, 470.0f, 0.055f)
              .eg(I, 0.010f, 1.85f, 0.88f, 0.18f).eg(II, 0.007f, 0.88f, 0.53f, 0.11f).mix(0.115f));
    add("Bright Bass", "Bass", { "velocity", "feet-layer" },
        "A full low saw body supports a clear, velocity-sensitive upper saw edge. The brighter rank decays into a restrained sustain, leaving a firm foundation beneath the articulate attack.",
        bass().wave(II, true, false).lpf(I, 1600.0f, 0.11f).lpf(II, 4200.0f, 0.13f).hpf(II, 150.0f)
              .feg(I, 0.0f, 0.22f, 0.003f, 0.62f, 0.11f).feg(II, 0.0f, 0.32f, 0.002f, 0.35f, 0.08f)
              .eg(II, 0.002f, 0.85f, 0.34f, 0.10f).level(II, 0.82f, 0.24f)
              .touch(II, 0.24f, 0.72f, 0.12f, 0.05f).mix(0.29f).res(0.09f)
              .lpf(I, 1350.0f, 0.075f).lpf(II, 4700.0f, 0.09f).hpf(II, 190.0f)
              .feg(I, 0.0f, 0.29f, 0.0035f, 0.84f, 0.09f).feg(II, 0.0f, 0.39f, 0.0015f, 0.26f, 0.06f)
              .eg(I, 0.0035f, 1.8f, 0.70f, 0.13f).eg(II, 0.0015f, 0.67f, 0.23f, 0.075f).mix(0.24f));
    add("Picked Bass", "Bass", { "velocity", "percussive" },
        "A short, firm pick excites a low string spectrum with independent harmonic losses. The upper pick vanishes quickly while the fundamental and lower modes continue ringing, then damp promptly on key release.",
        bass().wave(I, true, false, 0.30f).wave(II, false, true, 0.0f, 0.045f).pw(II, 0.74f)
              .hpf(II, 520.0f).lpf(I, 1050.0f, 0.14f).lpf(II, 4300.0f, 0.09f)
              .feg(I, 0.0f, 0.32f, 0.004f, 0.9f, 0.15f).feg(II, 0.0f, 0.55f, 0.001f, 0.13f, 0.055f)
              .eg(I, 0.0035f, 5.4f, 0.0f, 0.23f).eg(II, 0.001f, 0.19f, 0.0f, 0.055f)
              .level(I, 1.0f, 0.25f).level(II, 0.90f, 0.08f)
              .touch(I, 0.14f, 0.72f, 0.0f, 0.0f).touch(II, 0.28f, 0.94f, 0.0f, 0.0f).mix(0.20f)
              .body(I, body::Model::bass, 1.0f, 0.71f, 1.42f).body(II, body::Model::steel, 0.57f, 0.94f, 0.26f)
              .eg(I, 0.0025f, 5.9f, 0.0f, 0.18f).eg(II, 0.001f, 0.145f, 0.0f, 0.04f).mix(0.16f));
    add("Upright Pluck", "Bass", { "sine", "percussive", "velocity" },
        "A rounded low string spectrum has a woody finger onset and a gently falling body. Independently damping upper partials leave a clear fundamental, while a small circuit-noise transient supplies a brief finger-and-string texture.",
        bass().wave(I, true, false, 0.70f).wave(II, false, true, 0.0f, 0.16f).feet(Both, f16).pw(II, 0.55f)
              .hpf(I, 25.0f).hpf(II, 300.0f).lpf(I, 600.0f, 0.12f).lpf(II, 2300.0f, 0.22f)
              .level(I, 0.42f, 0.24f).level(II, 0.95f, 0.08f)
              .feg(I, 0.0f, 0.20f, 0.009f, 1.2f, 0.20f).feg(II, 0.0f, 0.32f, 0.003f, 0.31f, 0.08f)
              .eg(I, 0.010f, 3.8f, 0.0f, 0.30f).eg(II, 0.003f, 0.70f, 0.0f, 0.10f)
              .touch(I, 0.06f, 0.76f, 0.0f, 0.0f).touch(II, 0.14f, 0.88f, 0.0f, 0.0f)
              .mix(0.19f).res(0.08f)
              .body(I, body::Model::bass, 1.0f, 0.25f, 0.93f).body(II, body::Model::nylon, 0.58f, 0.57f, 0.34f)
              .eg(I, 0.007f, 4.45f, 0.0f, 0.24f).eg(II, 0.0025f, 0.47f, 0.0f, 0.075f)
              .wave(II, false, true, 0.0f, 0.085f).mix(0.15f));
    add("Fifth Bass", "Bass", { "feet-layer" },
        "A stable low foundation supports a quieter upper twelfth. Its upper rank speaks briefly at the strike, then settles into a lean harmonic support, making the fifth clear without masking the bass fundamental.",
        bass().feet(II, f513).wave(II, true, false).hpf(II, 160.0f).lpf(II, 3100.0f, 0.10f)
              .feg(II, 0.0f, 0.25f, 0.004f, 0.48f, 0.10f).eg(II, 0.006f, 1.0f, 0.48f, 0.13f)
              .touch(II, 0.16f, 0.58f, 0.12f, 0.05f).level(II, 0.78f, 0.26f).mix(0.24f)
              .lpf(I, 740.0f, 0.11f).lpf(II, 2800.0f, 0.075f)
              .feg(II, 0.0f, 0.33f, 0.0025f, 0.34f, 0.075f).eg(I, 0.004f, 1.85f, 0.73f, 0.14f)
              .eg(II, 0.0035f, 0.78f, 0.37f, 0.10f).mix(0.19f));
    add("Pedal Bass", "Bass", { "sine", "feet-layer" },
        "Steady low and octave sine stops use rounded keying and closely controlled releases. A gentle low-key level contour restrains boom; velocity remains neutral so held organ-style bass lines stay even.",
        bass().wave(Both, false, false, 1.0f).eg(I, 0.010f, 0.3f, 1.0f, 0.20f).eg(II, 0.008f, 0.3f, 1.0f, 0.16f)
              .feg(Both, 0.0f, 0.0f, 0.01f, 0.3f, 0.1f).touch(Both, 0.0f, 0.0f, 0.0f, 0.0f)
              .level(I, 0.0f, 0.86f).level(II, 0.0f, 0.66f).mix(0.24f)
              .g(P_voiceDispersion, 0.025f).g(P_drift, 0.0f)
              .eg(I, 0.014f, 0.3f, 1.0f, 0.17f).eg(II, 0.006f, 0.3f, 1.0f, 0.12f)
              .level(I, 0.0f, 0.91f).level(II, 0.0f, 0.58f).mix(0.20f)
              .contour(0.0f, 0.0f, -0.10f, -0.025f).g(P_voiceDispersion, 0.0f));
    add("Squelch Bass", "Bass", { "resonance", "velocity" },
        "A sharply resonant upper contour sweeps into a compact sine-supported bass. The upper accent closes rapidly while the lower filter settles more gradually, preserving a firm note center under the squelch.",
        bass().wave(I, true, false, 0.22f).lpf(I, 390.0f, 0.38f).lpf(II, 870.0f, 0.62f)
              .feg(I, 0.0f, 0.52f, 0.003f, 0.52f, 0.09f).feg(II, 0.0f, 0.82f, 0.0015f, 0.24f, 0.065f)
              .eg(I, 0.003f, 1.25f, 0.52f, 0.13f).eg(II, 0.002f, 0.45f, 0.10f, 0.08f)
              .touch(I, 0.18f, 0.66f, 0.0f, 0.0f).touch(II, 0.36f, 0.78f, 0.0f, 0.0f)
              .level(II, 0.78f, 0.22f).res(0.46f).mix(0.24f)
              .lpf(I, 335.0f, 0.34f).lpf(II, 1030.0f, 0.66f)
              .feg(I, 0.0f, 0.58f, 0.0035f, 0.66f, 0.075f).feg(II, 0.0f, 0.87f, 0.001f, 0.18f, 0.05f)
              .eg(I, 0.0028f, 1.5f, 0.46f, 0.105f).eg(II, 0.0015f, 0.34f, 0.055f, 0.06f).mix(0.20f));
    add("Ring Bass", "Bass", { "ring-mod" },
        "A centered low body feeds a light 35 Hz ring growl beneath a narrower upper attack. Unequal filter settling and restrained upper sustain keep the moving sidebands textured while the dry bass remains clear.",
        bass().wave(I, true, false, 0.30f).pw(II, 0.65f).lpf(I, 680.0f, 0.12f).lpf(II, 1800.0f, 0.16f)
              .eg(II, 0.004f, 1.1f, 0.42f, 0.12f).mix(0.20f)
              .ring(35.0f, 0.0f, 0.01f, 1.0f, 0.18f).res(0.10f)
              .lpf(I, 590.0f, 0.085f).lpf(II, 2050.0f, 0.12f)
              .feg(I, 0.0f, 0.35f, 0.006f, 1.1f, 0.10f).feg(II, 0.0f, 0.48f, 0.002f, 0.34f, 0.075f)
              .eg(II, 0.0025f, 0.83f, 0.31f, 0.09f).ring(35.0f, 0.0f, 0.01f, 1.0f, 0.145f).mix(0.17f));
    add("Sub Wobble Bass", "Bass", { "sub-mod", "sine" },
        "A 3 Hz filter movement animates the upper pulse layer while a stable bypass sine supports the bass. A more compact onset and separated rank contours keep the rhythmic motion clear without pitch wobble.",
        bass().wave(I, true, false, 0.52f).wave(II, false, true).pw(II, 0.64f)
              .lpf(I, 560.0f, 0.20f).lpf(II, 1450.0f, 0.38f)
              .feg(Both, 0.0f, 0.12f, 0.01f, 0.6f, 0.13f)
              .eg(I, 0.007f, 1.2f, 0.90f, 0.20f).eg(II, 0.010f, 1.0f, 0.76f, 0.16f)
              .level(I, 0.70f, 0.36f).level(II, 0.92f, 0.26f)
              .sub(subSine, 3.0f, 0.0f, 0.32f).res(0.26f).mix(0.28f)
              .lpf(I, 460.0f, 0.15f).lpf(II, 1700.0f, 0.34f).pw(II, 0.61f)
              .eg(I, 0.006f, 1.55f, 0.87f, 0.17f).eg(II, 0.0065f, 0.78f, 0.69f, 0.12f)
              .feg(II, 0.0f, 0.20f, 0.006f, 0.42f, 0.10f).sub(subSine, 3.0f, 0.0f, 0.28f).mix(0.24f));
    add("Pressure Bass", "Bass", { "aftertouch", "sine" },
        "A firm sine-supported note speaks immediately, with a soft falling string detail at the onset. Pressure then raises the brighter upper circuit rank and opens its spectrum, keeping the low center steady as the phrase grows.",
        bass().wave(I, true, false, 0.35f).lpf(I, 470.0f, 0.12f).lpf(II, 980.0f, 0.23f)
              .hpf(I, 20.0f).hpf(II, 55.0f).feg(Both, 0.0f, 0.18f, 0.012f, 0.8f, 0.18f)
              .eg(I, 0.008f, 1.5f, 0.88f, 0.22f).eg(II, 0.010f, 1.2f, 0.78f, 0.16f)
              .level(I, 1.0f, 0.34f).level(II, 0.90f, 0.12f)
              .touch(I, 0.08f, 0.48f, 0.18f, 0.30f).touch(II, 0.12f, 0.38f, 0.58f, 0.62f)
              .mix(0.28f).res(0.14f)
              .body(I, body::Model::bass, 0.34f, 0.32f, 1.24f)
              .eg(I, 0.006f, 1.8f, 0.85f, 0.18f).eg(II, 0.006f, 0.92f, 0.69f, 0.12f)
              .touch(I, 0.09f, 0.53f, 0.15f, 0.26f).touch(II, 0.15f, 0.34f, 0.63f, 0.69f).mix(0.23f));
    add("Contour Bass", "Bass", { "kbd-contour", "velocity" },
        "A dry saw-and-pulse bass has a restrained low-key level and a clearer upper-register attack. Different rank decays and keyboard brightness contour keep fast runs articulated without adding pitch instability.",
        bass().lpf(I, 880.0f, 0.12f).lpf(II, 1850.0f, 0.13f)
              .eg(I, 0.005f, 1.6f, 0.76f, 0.16f).eg(II, 0.003f, 0.72f, 0.28f, 0.11f)
              .touch(I, 0.12f, 0.68f, 0.08f, 0.10f).mix(0.23f)
              .contour(-0.08f, 0.20f, -0.24f, -0.03f)
              .lpf(I, 760.0f, 0.085f).lpf(II, 2200.0f, 0.095f)
              .feg(I, 0.0f, 0.44f, 0.004f, 0.91f, 0.10f).feg(II, 0.0f, 0.54f, 0.002f, 0.23f, 0.07f)
              .eg(I, 0.004f, 1.95f, 0.71f, 0.13f).eg(II, 0.002f, 0.55f, 0.18f, 0.08f)
              .contour(-0.05f, 0.25f, -0.21f, -0.015f).mix(0.19f));
}
