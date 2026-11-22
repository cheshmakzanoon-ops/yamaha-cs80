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
        "A stable 16' saw-and-sine foundation with a shorter 8' pulse attack. Velocity adds bite and level; the dry body remains clear under held notes.",
        bass().feg(I, 0.0f, 0.44f, 0.005f, 0.78f, 0.12f)
              .eg(II, 0.002f, 0.58f, 0.22f, 0.09f).mix(0.27f));
    add("Resonance Bass", "Bass", { "resonance", "velocity" },
        "A resonant upper attack sweeps into a firm low body. A small sine foundation preserves weight while the filter peak provides the character.",
        bass().wave(I, true, false, 0.16f).lpf(I, 430.0f, 0.40f).lpf(II, 1020.0f, 0.52f)
              .feg(I, 0.0f, 0.56f, 0.004f, 0.64f, 0.11f).feg(II, 0.0f, 0.70f, 0.002f, 0.32f, 0.08f)
              .eg(II, 0.002f, 0.45f, 0.12f, 0.09f).touch(II, 0.32f, 0.78f, 0.10f, 0.05f)
              .res(0.38f).mix(0.23f));
    add("Sine Bass", "Bass", { "sine", "percussive", "velocity" },
        "A sustained 16' sine with a separate, very short 8' pulse click. The upper rank disappears after the onset, leaving a clean low fundamental.",
        bass().wave(I, false, false, 1.0f).wave(II, false, true).pw(II, 0.70f)
              .hpf(II, 650.0f).lpf(II, 4300.0f, 0.06f).feg(II, 0.0f, 0.25f, 0.001f, 0.09f, 0.04f)
              .eg(I, 0.005f, 0.6f, 1.0f, 0.18f).eg(II, 0.001f, 0.14f, 0.0f, 0.045f)
              .touch(I, 0.0f, 0.64f, 0.0f, 0.0f).touch(II, 0.10f, 0.75f, 0.0f, 0.0f)
              .level(I, 0.0f, 0.32f).level(II, 0.78f, 0.08f).mix(0.14f).res(0.04f));
    add("Sub Bass", "Bass", { "sine", "velocity" },
        "One centered 16' sine, with a short rounded onset and controlled release. Velocity controls level; there is no detuned upper layer, filter sweep or vibrato.",
        bass().wave(I, false, false, 1.0f).wave(II, false, false)
              .feet(Both, f16).eg(I, 0.008f, 0.8f, 1.0f, 0.20f)
              .touch(I, 0.0f, 0.70f, 0.0f, 0.0f).touch(II, 0.0f, 0.0f, 0.0f, 0.0f)
              .level(I, 0.0f, 0.30f).level(II, 0.0f, 0.0f)
              .mix(0.0f).g(P_voiceDispersion, 0.035f).g(P_drift, 0.0f));
    add("Pluck Bass", "Bass", { "percussive", "velocity" },
        "A resonant low pluck with a separately decaying upper attack. Both ranks fall to silence on a held key; the body outlasts the bright initial edge.",
        bass().lpf(I, 670.0f, 0.18f).lpf(II, 2600.0f, 0.10f)
              .feg(I, 0.0f, 0.52f, 0.003f, 0.85f, 0.12f).feg(II, 0.0f, 0.52f, 0.0015f, 0.21f, 0.07f)
              .eg(I, 0.003f, 2.4f, 0.0f, 0.22f).eg(II, 0.0015f, 0.28f, 0.0f, 0.07f)
              .touch(I, 0.18f, 0.78f, 0.0f, 0.0f).touch(II, 0.30f, 0.92f, 0.0f, 0.0f)
              .level(I, 1.0f, 0.22f).level(II, 0.88f, 0.12f).mix(0.20f));
    add("Fretless Glide", "Bass", { "glide", "aftertouch", "percussive" },
        "A soft fingered attack and a long, gently decaying 16' body. Short portamento joins phrases; pressure adds a small pitch vibrato and a warmer singing edge.",
        bass().wave(I, false, true, 0.48f).pw(I, 0.54f).wave(II, true, false, 0.0f, 0.018f).feet(Both, f16)
              .hpf(I, 22.0f).hpf(II, 210.0f).lpf(I, 850.0f, 0.10f).lpf(II, 2300.0f, 0.17f)
              .feg(I, 0.04f, 0.22f, 0.016f, 1.25f, 0.20f).feg(II, 0.0f, 0.34f, 0.005f, 0.40f, 0.12f)
              .eg(I, 0.009f, 4.8f, 0.0f, 0.28f).eg(II, 0.004f, 1.15f, 0.0f, 0.14f)
              .touch(I, 0.08f, 0.65f, 0.12f, 0.24f).touch(II, 0.16f, 0.62f, 0.22f, 0.14f)
              .glide(0.065f).subAt(0.035f, 0.075f).g(P_modWheelSub, 0.035f).mix(0.15f).res(0.08f));
    add("Funk Bass", "Bass", { "velocity", "resonance", "percussive" },
        "A tight, velocity-sensitive filter pluck over a rounded low body. Harder strikes emphasize the upper snap; both ranks decay for clearly separated rhythmic notes.",
        bass().lpf(I, 510.0f, 0.25f).lpf(II, 1550.0f, 0.40f).pw(II, 0.67f)
              .feg(I, 0.0f, 0.48f, 0.003f, 0.52f, 0.08f).feg(II, 0.0f, 0.68f, 0.0015f, 0.20f, 0.06f)
              .eg(I, 0.0025f, 1.75f, 0.0f, 0.13f).eg(II, 0.0015f, 0.33f, 0.0f, 0.07f)
              .level(I, 1.0f, 0.22f).level(II, 0.90f, 0.12f)
              .touch(I, 0.25f, 0.76f, 0.0f, 0.0f).touch(II, 0.44f, 0.90f, 0.0f, 0.0f).res(0.28f).mix(0.26f));
    add("Brass Bass", "Bass", { "brilliance-swell", "aftertouch" },
        "A 16' saw body grows into an 8' brass edge, with a compact filter bloom. Pressure opens the held sound without washing out the low foundation.",
        bass().wave(II, true, false).lpf(I, 580.0f, 0.15f).lpf(II, 1250.0f, 0.18f)
              .feg(I, 0.19f, 0.44f, 0.085f, 1.25f, 0.22f).feg(II, 0.28f, 0.48f, 0.14f, 1.05f, 0.19f)
              .eg(I, 0.022f, 1.4f, 0.84f, 0.23f).eg(II, 0.045f, 1.1f, 0.65f, 0.18f)
              .touch(I, 0.12f, 0.58f, 0.18f, 0.18f).touch(II, 0.19f, 0.62f, 0.30f, 0.20f).mix(0.28f));
    add("Octave Bass", "Bass", { "feet-layer", "velocity" },
        "Stable 16' and 8' saws with separate low-body and upper-octave envelopes. The octave remains present in the sustain, rather than becoming only a click.",
        bass().wave(I, true, false, 0.12f).wave(II, true, false)
              .lpf(I, 950.0f, 0.12f).lpf(II, 2100.0f, 0.10f)
              .feg(I, 0.0f, 0.32f, 0.005f, 0.85f, 0.13f).feg(II, 0.0f, 0.28f, 0.007f, 0.65f, 0.12f)
              .eg(I, 0.004f, 1.5f, 0.82f, 0.17f).eg(II, 0.006f, 1.2f, 0.70f, 0.14f)
              .level(II, 0.90f, 0.30f).mix(0.32f));
    add("Pulse Bass", "Bass", { "pwm", "velocity" },
        "Two 16' pulses with restrained, differently paced PWM. A small sine foundation keeps the low end steady as the harmonic width moves.",
        bass().wave(I, false, true, 0.20f).wave(II, false, true).feet(Both, f16)
              .pw(I, 0.54f, 0.12f, 0.22f).pw(II, 0.62f, 0.20f, 0.31f)
              .hpf(II, 70.0f).lpf(I, 920.0f, 0.12f).lpf(II, 1700.0f, 0.10f)
              .eg(I, 0.004f, 1.7f, 0.84f, 0.17f).eg(II, 0.007f, 1.2f, 0.66f, 0.13f)
              .feg(II, 0.0f, 0.30f, 0.006f, 0.65f, 0.11f).detune(0.6f).mix(0.24f));
    add("Round Bass", "Bass", { "sine", "velocity" },
        "A smooth 16' sine-and-soft-saw body, with a quiet same-register harmonic edge. Little resonance and a rounded onset suit warm, even bass lines.",
        bass().wave(I, true, false, 0.50f).wave(II, false, true).feet(Both, f16).pw(II, 0.52f)
              .lpf(I, 420.0f, 0.03f).lpf(II, 1000.0f, 0.04f).level(I, 0.55f, 0.38f).level(II, 0.42f, 0.24f)
              .feg(I, 0.0f, 0.15f, 0.012f, 0.80f, 0.15f).feg(II, 0.0f, 0.20f, 0.010f, 0.42f, 0.10f)
              .eg(I, 0.010f, 1.4f, 0.86f, 0.20f).eg(II, 0.008f, 1.0f, 0.48f, 0.13f)
              .touch(I, 0.04f, 0.60f, 0.0f, 0.08f).touch(II, 0.08f, 0.55f, 0.0f, 0.0f).res(0.03f).mix(0.16f));
    add("Velocity Bass", "Bass", { "velocity" },
        "Soft strikes give a rounded low body; hard strikes bring forward a brighter upper attack and much stronger level. Pressure does not override the struck dynamic.",
        bass().level(I, 1.0f, 0.14f).level(II, 0.92f, 0.06f)
              .touch(I, 0.34f, 0.88f, 0.0f, 0.0f).touch(II, 0.54f, 0.96f, 0.0f, 0.0f)
              .lpf(I, 580.0f, 0.12f).lpf(II, 1450.0f, 0.18f)
              .feg(II, 0.0f, 0.44f, 0.002f, 0.36f, 0.09f).eg(II, 0.002f, 0.78f, 0.18f, 0.09f).mix(0.24f));
    add("Rubber Bass", "Bass", { "init-bend", "percussive" },
        "A small upward pitch scoop rises into each note, followed by a rounded resonant pluck. The short upper attack makes the springy onset clear without an ongoing pitch wobble.",
        bass().bend(0.32f).wave(I, false, true, 0.28f).pw(I, 0.56f)
              .lpf(I, 580.0f, 0.28f).lpf(II, 1650.0f, 0.22f)
              .feg(I, 0.0f, 0.46f, 0.004f, 0.58f, 0.12f).feg(II, 0.0f, 0.55f, 0.002f, 0.25f, 0.08f)
              .eg(I, 0.004f, 2.0f, 0.08f, 0.18f).eg(II, 0.002f, 0.30f, 0.0f, 0.07f)
              .touch(Both, 0.18f, 0.68f, 0.0f, 0.0f).res(0.21f).mix(0.20f));
    add("Dark Bass", "Bass", { "sine" },
        "A deep 16' foundation with a deliberately dark, same-register saw shadow. The bypass sine preserves the fundamental while the filtered harmonics stay restrained.",
        bass().wave(I, true, false, 0.58f).wave(II, true, false).feet(Both, f16)
              .lpf(I, 270.0f, 0.06f).lpf(II, 540.0f, 0.08f).level(I, 0.65f, 0.40f).level(II, 0.50f, 0.26f)
              .feg(I, 0.0f, 0.12f, 0.014f, 0.72f, 0.15f).feg(II, 0.0f, 0.18f, 0.010f, 0.46f, 0.11f)
              .eg(I, 0.009f, 1.5f, 0.91f, 0.22f).eg(II, 0.012f, 1.1f, 0.66f, 0.15f)
              .touch(Both, 0.04f, 0.58f, 0.05f, 0.08f).res(0.05f).mix(0.14f));
    add("Bright Bass", "Bass", { "velocity", "feet-layer" },
        "A full 16' body with a cutting 8' saw edge. The upper rank settles after the attack, keeping the held bass bright without overwhelming its foundation.",
        bass().wave(II, true, false).lpf(I, 1600.0f, 0.11f).lpf(II, 4200.0f, 0.13f).hpf(II, 150.0f)
              .feg(I, 0.0f, 0.22f, 0.003f, 0.62f, 0.11f).feg(II, 0.0f, 0.32f, 0.002f, 0.35f, 0.08f)
              .eg(II, 0.002f, 0.85f, 0.34f, 0.10f).level(II, 0.82f, 0.24f)
              .touch(II, 0.24f, 0.72f, 0.12f, 0.05f).mix(0.29f).res(0.09f));
    add("Picked Bass", "Bass", { "velocity", "percussive" },
        "A short pulse-and-noise pick transient sits above a longer decaying 16' string body. Hard strikes sharpen the pick; the upper rank does not persist through the held note.",
        bass().wave(I, true, false, 0.30f).wave(II, false, true, 0.0f, 0.045f).pw(II, 0.74f)
              .hpf(II, 520.0f).lpf(I, 1050.0f, 0.14f).lpf(II, 4300.0f, 0.09f)
              .feg(I, 0.0f, 0.32f, 0.004f, 0.9f, 0.15f).feg(II, 0.0f, 0.55f, 0.001f, 0.13f, 0.055f)
              .eg(I, 0.0035f, 3.1f, 0.0f, 0.23f).eg(II, 0.001f, 0.19f, 0.0f, 0.055f)
              .level(I, 1.0f, 0.25f).level(II, 0.90f, 0.08f)
              .touch(I, 0.14f, 0.72f, 0.0f, 0.0f).touch(II, 0.28f, 0.94f, 0.0f, 0.0f).mix(0.20f));
    add("Upright Pluck", "Bass", { "sine", "percussive", "velocity" },
        "A rounded decaying 16' body with a separate woody finger-and-string transient. This synthesized upright interpretation favors lower notes and short, connected phrases.",
        bass().wave(I, true, false, 0.70f).wave(II, false, true, 0.0f, 0.16f).feet(Both, f16).pw(II, 0.55f)
              .hpf(I, 25.0f).hpf(II, 450.0f).lpf(I, 600.0f, 0.12f).lpf(II, 2300.0f, 0.22f)
              .level(I, 0.42f, 0.24f).level(II, 0.74f, 0.08f)
              .feg(I, 0.0f, 0.20f, 0.009f, 1.2f, 0.20f).feg(II, 0.0f, 0.32f, 0.003f, 0.31f, 0.08f)
              .eg(I, 0.010f, 3.8f, 0.0f, 0.30f).eg(II, 0.003f, 0.70f, 0.0f, 0.10f)
              .touch(I, 0.06f, 0.76f, 0.0f, 0.0f).touch(II, 0.14f, 0.88f, 0.0f, 0.0f)
              .mix(0.14f).res(0.08f));
    add("Fifth Bass", "Bass", { "feet-layer" },
        "A 16' foundation supports a quieter 5 1/3' upper twelfth: the fifth above its octave. The upper harmonic has a shorter attack accent and a controlled sustain.",
        bass().feet(II, f513).wave(II, true, false).hpf(II, 160.0f).lpf(II, 3100.0f, 0.10f)
              .feg(II, 0.0f, 0.25f, 0.004f, 0.48f, 0.10f).eg(II, 0.006f, 1.0f, 0.48f, 0.13f)
              .touch(II, 0.16f, 0.58f, 0.12f, 0.05f).level(II, 0.78f, 0.26f).mix(0.24f));
    add("Pedal Bass", "Bass", { "sine", "feet-layer" },
        "Steady 16' and quieter 8' sine stops, with rounded keying and a short release. Like an organ pedal registration, its level is independent of strike velocity.",
        bass().wave(Both, false, false, 1.0f).eg(I, 0.010f, 0.3f, 1.0f, 0.20f).eg(II, 0.008f, 0.3f, 1.0f, 0.16f)
              .feg(Both, 0.0f, 0.0f, 0.01f, 0.3f, 0.1f).touch(Both, 0.0f, 0.0f, 0.0f, 0.0f)
              .level(I, 0.0f, 0.86f).level(II, 0.0f, 0.66f).mix(0.24f)
              .g(P_voiceDispersion, 0.025f).g(P_drift, 0.0f));
    add("Squelch Bass", "Bass", { "resonance", "velocity" },
        "A quick, strongly resonant upper sweep over a compact low body. Velocity opens the squelch; the low sine component prevents the resonance from hollowing out the bass.",
        bass().wave(I, true, false, 0.22f).lpf(I, 390.0f, 0.38f).lpf(II, 870.0f, 0.62f)
              .feg(I, 0.0f, 0.52f, 0.003f, 0.52f, 0.09f).feg(II, 0.0f, 0.82f, 0.0015f, 0.24f, 0.065f)
              .eg(I, 0.003f, 1.25f, 0.52f, 0.13f).eg(II, 0.002f, 0.45f, 0.10f, 0.08f)
              .touch(I, 0.18f, 0.66f, 0.0f, 0.0f).touch(II, 0.36f, 0.78f, 0.0f, 0.0f)
              .level(II, 0.78f, 0.22f).res(0.46f).mix(0.24f));
    add("Ring Bass", "Bass", { "ring-mod" },
        "A stable low body and narrower upper pulse feed a light 35 Hz ring blend. The audible low-rate sidebands add a rough growl while the dry component keeps the note centered.",
        bass().wave(I, true, false, 0.30f).pw(II, 0.65f).lpf(I, 680.0f, 0.12f).lpf(II, 1800.0f, 0.16f)
              .eg(II, 0.004f, 1.1f, 0.42f, 0.12f).mix(0.20f)
              .ring(35.0f, 0.0f, 0.01f, 1.0f, 0.18f).res(0.10f));
    add("Sub Wobble Bass", "Bass", { "sub-mod", "sine" },
        "A 3 Hz SUB sine moves the filtered harmonic layer while a bypass sine keeps the low foundation steady. Hold notes for the rhythmic filter motion; there is no pitch wobble.",
        bass().wave(I, true, false, 0.52f).wave(II, false, true).pw(II, 0.64f)
              .lpf(I, 560.0f, 0.20f).lpf(II, 1450.0f, 0.38f)
              .feg(Both, 0.0f, 0.12f, 0.01f, 0.6f, 0.13f)
              .eg(I, 0.007f, 1.2f, 0.90f, 0.20f).eg(II, 0.010f, 1.0f, 0.76f, 0.16f)
              .level(I, 0.70f, 0.36f).level(II, 0.92f, 0.26f)
              .sub(subSine, 3.0f, 0.0f, 0.32f).res(0.26f).mix(0.28f));
    add("Pressure Bass", "Bass", { "aftertouch", "sine" },
        "A playable low body is present at the strike. Pressure brings forward a brighter upper rank and opens the filters while the sine-supported fundamental stays firm.",
        bass().wave(I, true, false, 0.35f).lpf(I, 470.0f, 0.12f).lpf(II, 980.0f, 0.23f)
              .hpf(I, 20.0f).hpf(II, 55.0f).feg(Both, 0.0f, 0.18f, 0.012f, 0.8f, 0.18f)
              .eg(I, 0.008f, 1.5f, 0.88f, 0.22f).eg(II, 0.010f, 1.2f, 0.78f, 0.16f)
              .level(I, 1.0f, 0.34f).level(II, 0.90f, 0.12f)
              .touch(I, 0.08f, 0.48f, 0.18f, 0.30f).touch(II, 0.12f, 0.38f, 0.58f, 0.62f)
              .mix(0.28f).res(0.14f));
    add("Contour Bass", "Bass", { "kbd-contour", "velocity" },
        "A dry saw-and-pulse bass whose low-key level contour restrains boom. Upper notes gain a little brightness, keeping runs clearer across the keyboard.",
        bass().lpf(I, 880.0f, 0.12f).lpf(II, 1850.0f, 0.13f)
              .eg(I, 0.005f, 1.6f, 0.76f, 0.16f).eg(II, 0.003f, 0.72f, 0.28f, 0.11f)
              .touch(I, 0.12f, 0.68f, 0.08f, 0.10f).mix(0.23f)
              .contour(-0.08f, 0.20f, -0.24f, -0.03f));
}
