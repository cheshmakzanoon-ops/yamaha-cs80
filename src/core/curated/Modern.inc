// Modern performance voicings. Included inside namespace curated by CuratedLibrary.inc.
// Existing program names and insertion order are stable host/MIDI identities.
// Each dry voicing is articulated for its modulation/effect, including the arp's
// fixed 58% gate (45% in random mode). Rates are free-running, not host-synced.
// Model 4 echo time is 0.06 * 20^A seconds; model 3 plate decay is 0.8 * 10^A
// seconds before brightness-dependent damping. Conservative wet levels retain
// the onset and register balance. The software layer remains available in the
// single circuit instrument; individual routes and effects stay preset-specific.
// Played effort is balanced against each fixed VCA floor near the previous
// firm-note level. Short dry releases leave space for the authored echoes;
// full struck main bodies retain a little independent modal aftersound.

void buildModern(Builder& add)
{
    add("Delay Brass", "Brass", { "modern", "brilliance-swell", "aftertouch", "velocity" },
        "Play clipped midrange chords; a defined brass body and shorter lip accent leave gaps for 375 ms echoes. Pressure develops the held spectrum while the short dry releases keep phrases clear.",
        brass()
               .wave(I, true, false, 0.025f).wave(II, true, false).hpf(I, 29.0f).hpf(II, 190.0f).lpf(I, 3500.0f, 0.11f)
               .lpf(II, 6400.0f, 0.09f).level(I, 0.96f, 0.13f).level(II, 0.90f, 0.08f).touch(I, 0.16f, 0.96f, 0.31f, 0.29f)
               .touch(II, 0.28f, 0.99f, 0.45f, 0.23f).subAt(0.025f, 0.042f).mix(0.29f).detune(1.3f).fx(0, fxDelay, 0.19f)
               .g(P_fx1Model, 4.0f).g(P_fx1ParamA, 0.612f).g(P_fx1ParamB, 0.50f).fx(1, fxPlate, 0.06f).g(P_fx2Model, 3.0f)
               .g(P_fx2ParamA, 0.22f).g(P_fx2ParamB, 0.42f).body(I, body::Model::brass, 0.84f, 0.50f, 1.05f)
               .body(II, body::Model::brass, 0.54f, 0.74f, 0.78f).feg(I, 0.16f, 0.28f, 0.039f, 1.25f, 0.16f)
               .feg(II, 0.23f, 0.39f, 0.064f, 0.57f, 0.105f).eg(I, 0.024f, 1.8f, 0.88f, 0.105f)
               .eg(II, 0.041f, 0.63f, 0.31f, 0.062f).contour(-0.13f, -0.24f, -0.04f, 0.08f).bodySize(I, 1.12f).bodySize(II, 0.94f));

    add("Plate Pad", "Pads", { "modern", "feet-layer", "pwm", "aftertouch" },
        "Hold open chords around C3-C5; a soft lower pulse blooms beneath an octave sheen. Release between changes to hear the warm plate tail.",
        pad()
               .wave(I, false, true, 0.12f).wave(II, true, false).feet(II, f4).hpf(I, 42.0f).lpf(I, 1910.0f, 0.055f)
               .lpf(II, 5300.0f, 0.04f).level(I, 0.92f, 0.13f).level(II, 0.82f, 0.09f).touch(I, 0.045f, 0.80f, 0.17f, 0.28f)
               .pan(I, -0.055f).pan(II, 0.17f).mix(0.195f).detune(1.25f).g(P_chorusOn, 0.0f).fx(0, fxPlate, 0.225f)
               .g(P_fx1Model, 3.0f).g(P_fx1ParamA, 0.48f).g(P_fx1ParamB, 0.35f).pw(I, 0.54f, 0.052f, 0.125f).hpf(II, 510.0f)
               .feg(I, 0.22f, 0.17f, 0.93f, 4.3f, 1.4f).feg(II, 0.39f, 0.27f, 1.74f, 3.4f, 1.9f).eg(I, 0.49f, 4.1f, 0.94f, 1.35f)
               .eg(II, 1.19f, 3.4f, 0.46f, 1.9f).touch(II, 0.11f, 0.65f, 0.31f, 0.34f).contour(-0.10f, -0.20f, -0.045f, 0.08f));

    add("Phaser Strings", "Strings", { "modern", "dual-rank", "pwm", "aftertouch" },
        "Bow connected chords in C3-C5 and hold phrase endings. A clear bowed body and quieter, later pulse edge expose the phaser notches while keeping a steady pitch center.",
        strings()
               .wave(I, true, false).wave(II, false, true).pw(II, 0.61f, 0.045f, 0.23f).hpf(I, 76.0f).hpf(II, 410.0f)
               .lpf(I, 5700.0f, 0.05f).lpf(II, 7800.0f, 0.035f).level(I, 0.93f, 0.075f).level(II, 0.83f, 0.065f)
               .touch(I, 0.085f, 1.0f, 0.16f, 0.24f).pan(I, -0.045f).pan(II, 0.12f).mix(0.285f).detune(1.9f).g(P_chorusOn, 0.0f)
               .fx(0, fxPhaser, 0.29f).body(I, body::Model::bowed, 0.72f, 0.64f, 1.18f)
               .body(II, body::Model::bowed, 0.31f, 0.78f, 1.05f).eg(I, 0.089f, 1.7f, 0.95f, 0.43f)
               .eg(II, 0.22f, 2.1f, 0.62f, 0.65f).feg(I, 0.08f, 0.12f, 0.17f, 2.0f, 0.46f)
               .feg(II, 0.17f, 0.10f, 0.37f, 2.5f, 0.61f).touch(II, 0.13f, 0.84f, 0.25f, 0.27f)
               .contour(-0.12f, -0.24f, -0.045f, 0.075f).bodySize(I, 1.08f).bodySize(II, 0.96f));

    add("Drive Lead", "Leads", { "modern", "aftertouch", "velocity", "dual-rank" },
        "Play firm lines in C4-C6: a pulse body carries a shorter saw edge into warm drive. Pressure adds bite and restrained vibrato.",
        lead()
               .wave(I, false, true, 0.05f).wave(II, true, false).hpf(I, 33.0f).lpf(I, 1800.0f, 0.12f).lpf(II, 3600.0f, 0.08f)
               .level(I, 0.96f, 0.11f).level(II, 0.88f, 0.075f).touch(I, 0.19f, 0.95f, 0.22f, 0.33f).subAt(0.035f, 0.09f).bend(0.0f)
               .glide(0.018f).mix(0.19f).detune(0.22f).fx(0, fxDrive, 0.36f).g(P_fx1Model, 5.0f).g(P_fx1ParamA, 0.38f)
               .g(P_fx1ParamB, 0.31f).g(P_fx1Oversampling, 2.0f).pw(I, 0.55f).hpf(II, 370.0f)
               .feg(I, 0.04f, 0.22f, 0.008f, 1.18f, 0.17f).feg(II, 0.0f, 0.32f, 0.003f, 0.18f, 0.052f)
               .eg(I, 0.005f, 1.65f, 0.97f, 0.105f).eg(II, 0.011f, 0.32f, 0.24f, 0.047f).touch(II, 0.34f, 1.0f, 0.39f, 0.26f)
               .contour(-0.09f, -0.27f, -0.03f, 0.075f));

    add("Arp Poly", "Sequences", { "modern", "lfo", "velocity", "percussive", "feet-layer" },
        "Hold two to four notes around C3-C4 for eight notes per second across two octaves. A short octave accent and one-second filter cycle articulate the ascent.",
        poly()
               .wave(I, false, true, 0.04f).wave(II, true, false).feet(II, f4).hpf(I, 31.0f).lpf(I, 1310.0f, 0.13f)
               .lpf(II, 3400.0f, 0.09f).level(I, 0.94f, 0.14f).level(II, 0.86f, 0.025f).touch(I, 0.18f, 1.0f, 0.09f, 0.08f)
               .mix(0.18f).detune(0.35f).g(P_chorusOn, 0.0f).arp(arpUp, 8.0f, 2).lfo(0, 1.0f, 0.12f, toFilter).g(P_lfo1Wave, 1.0f)
               .pw(I, 0.58f).hpf(II, 460.0f).feg(I, 0.0f, 0.47f, 0.002f, 0.255f, 0.036f)
               .feg(II, 0.0f, 0.57f, 0.001f, 0.073f, 0.019f).eg(I, 0.002f, 0.60f, 0.015f, 0.032f)
               .eg(II, 0.001f, 0.125f, 0.0f, 0.017f).touch(II, 0.33f, 1.0f, 0.0f, 0.0f).contour(-0.13f, -0.19f, -0.045f, 0.08f));

    add("Arp Bells", "Sequences", { "modern", "ring-mod", "velocity", "percussive", "feet-layer" },
        "Hold a small chord around C4 for four notes per second, rising and falling over two octaves. A short third-harmonic strike leaves space for 375 ms echoes.",
        bell()
               .wave(I, false, false, 1.0f).feet(I, f8).wave(II, false, false, 0.72f).feet(II, f223).hpf(I, 24.0f).hpf(II, 750.0f)
               .lpf(I, 9800.0f, 0.015f).lpf(II, 13500.0f, 0.015f).feg(I, 0.0f, 0.0f, 0.001f, 0.3f, 0.10f)
               .feg(II, 0.0f, 0.0f, 0.001f, 0.3f, 0.10f).level(I, 0.0f, 0.080f).level(II, 0.0f, 0.025f)
               .touch(I, 0.0f, 1.0f, 0.0f, 0.0f).touch(II, 0.0f, 1.0f, 0.0f, 0.0f).sub(subSine, 5.0f, 0.0f).subAt(0.0f, 0.0f)
               .pan(I, 0.0f).pan(II, 0.0f).mix(0.13f).detune(0.0f).res(0.02f).g(P_chorusOn, 0.0f).g(P_modWheelSub, 0.0f)
               .g(P_voiceDispersion, 0.04f).g(P_drift, 0.015f).g(P_analogNoise, 0.0f).arp(arpUpDown, 4.0f, 2)
               .fx(0, fxDelay, 0.145f).g(P_fx1Model, 4.0f).g(P_fx1ParamA, 0.612f).g(P_fx1ParamB, 0.43f)
               .body(I, body::Model::bell, 1.0f, 0.46f, 0.58f).body(II, body::Model::bell, 0.77f, 0.74f, 0.25f)
               .eg(I, 0.002f, 3.8f, 0.015f, 0.14f).eg(II, 0.001f, 0.21f, 0.0f, 0.035f).ring(211.0f, 0.0f, 0.001f, 0.20f, 0.022f)
               .contour(0.06f, -0.17f, -0.065f, 0.14f));

    add("Arp Bass", "Sequences", { "modern", "lfo", "velocity", "percussive", "feet-layer" },
        "Hold a root and fifth around C3 for a six-note-per-second bass pattern. A compact 16' body and short 8' snap follow a gentle four-step filter cycle.",
        bass()
               .wave(II, false, true).hpf(I, 20.0f).lpf(I, 650.0f, 0.16f).lpf(II, 2050.0f, 0.11f).level(I, 1.0f, 0.14f)
               .level(II, 0.78f, 0.035f).touch(I, 0.13f, 1.0f, 0.07f, 0.05f).touch(II, 0.26f, 1.0f, 0.0f, 0.0f).mix(0.16f)
               .detune(0.0f).res(0.13f).arp(arpUp, 6.0f, 1).lfo(0, 1.5f, 0.11f, toFilter).g(P_lfo1Wave, 1.0f)
               .wave(I, true, false, 0.37f).pw(II, 0.66f).hpf(II, 175.0f).feg(I, 0.0f, 0.48f, 0.002f, 0.265f, 0.034f)
               .feg(II, 0.0f, 0.59f, 0.001f, 0.063f, 0.018f).eg(I, 0.002f, 0.75f, 0.040f, 0.034f)
               .eg(II, 0.001f, 0.11f, 0.0f, 0.017f).contour(-0.18f, -0.17f, -0.08f, 0.06f));

    add("LFO Sweep Pad", "Pads", { "modern", "lfo", "pwm", "chorus", "dual-rank" },
        "Hold spacious chords for at least ten seconds. A broad triangle filter sweep moves over a warm low body, with slower irregular stereo drift.",
        pad()
               .wave(I, true, false, 0.15f).wave(II, false, true).hpf(I, 25.0f).lpf(I, 1240.0f, 0.19f).lpf(II, 2800.0f, 0.22f)
               .level(I, 0.92f, 0.13f).level(II, 0.86f, 0.11f).touch(I, 0.035f, 0.84f, 0.16f, 0.22f).pan(I, -0.065f).pan(II, 0.15f)
               .mix(0.31f).detune(2.4f).res(0.15f).chorus(0.14f, 0.19f).lfo(0, 0.10f, 0.40f, toFilter).g(P_lfo1Wave, 1.0f)
               .lfo(1, 0.07f, 0.14f, toPan).g(P_lfo2Wave, 6.0f).pw(II, 0.61f, 0.035f, 0.135f).hpf(II, 270.0f)
               .feg(I, 0.13f, 0.10f, 0.61f, 3.6f, 1.25f).feg(II, 0.29f, 0.22f, 1.24f, 4.1f, 1.65f).eg(I, 0.40f, 4.2f, 0.97f, 1.65f)
               .eg(II, 1.02f, 4.6f, 0.54f, 2.20f).touch(II, 0.10f, 0.78f, 0.29f, 0.33f).contour(-0.11f, -0.23f, -0.04f, 0.08f));

    add("LFO Pan Keys", "Keys", { "modern", "lfo", "velocity", "sine", "feet-layer" },
        "Play relaxed C3-C6 chords and let them ring. A tine body changes color with velocity and rocks left to right once per second; its separate high-passed octave strike fades first.",
        keys()
               .wave(I, false, true, 0.81f).pw(I, 0.50f).wave(II, false, true, 0.25f).pw(II, 0.66f).feet(II, f4).hpf(I, 27.0f)
               .hpf(II, 1150.0f).lpf(I, 6200.0f, 0.025f).lpf(II, 10600.0f, 0.035f).feg(I, 0.0f, 0.09f, 0.003f, 1.95f, 0.22f)
               .level(I, 0.16f, 0.070f).level(II, 0.39f, 0.025f).pan(I, 0.0f).pan(II, 0.0f).mix(0.15f).detune(0.0f)
               .lfo(0, 1.0f, 0.72f, toPan).g(P_lfo1Wave, 1.0f).fx(0, fxPlate, 0.065f).g(P_fx1Model, 3.0f).g(P_fx1ParamA, 0.12f)
               .g(P_fx1ParamB, 0.38f).body(I, body::Model::tine, 1.0f, 0.56f, 1.55f)
               .body(II, body::Model::tine, 0.78f, 0.75f, 0.32f).eg(I, 0.0028f, 9.0f, 0.055f, 0.23f)
               .eg(II, 0.0012f, 0.52f, 0.0f, 0.068f).feg(II, 0.0f, 0.20f, 0.001f, 0.17f, 0.061f).touch(I, 0.19f, 1.0f, 0.0f, 0.0f)
               .touch(II, 0.36f, 1.0f, 0.0f, 0.0f).contour(-0.14f, -0.23f, -0.065f, 0.15f));

    add("PWM LFO Strings", "Strings", { "modern", "lfo", "pwm", "dual-rank", "aftertouch" },
        "Sustain midrange chords: two differently biased pulse ranks share a slow triangle width sweep. The later, brighter rank adds a gentle shimmer.",
        strings()
               .wave(I, false, true).wave(II, false, true).hpf(I, 65.0f).lpf(I, 3650.0f, 0.055f).lpf(II, 5800.0f, 0.04f)
               .level(I, 0.94f, 0.090f).level(II, 0.82f, 0.075f).touch(I, 0.07f, 0.91f, 0.17f, 0.23f).mix(0.32f).detune(0.85f)
               .g(P_chorusOn, 0.0f).lfo(0, 0.22f, 0.56f, toPwm).g(P_lfo1Wave, 1.0f).pw(I, 0.53f, 0.0f).pw(II, 0.60f, 0.0f)
               .hpf(II, 450.0f).feg(I, 0.12f, 0.14f, 0.12f, 2.1f, 0.67f).feg(II, 0.28f, 0.13f, 0.37f, 2.7f, 0.92f)
               .eg(I, 0.11f, 1.9f, 0.96f, 0.62f).eg(II, 0.28f, 2.4f, 0.57f, 0.90f).touch(II, 0.11f, 0.82f, 0.23f, 0.28f)
               .contour(-0.11f, -0.22f, -0.045f, 0.08f));

    add("Stereo Delay Pluck", "Plucked", { "modern", "velocity", "percussive", "feet-layer" },
        "Play sparse picked figures around C3-C5. A resonant steel-string body and short high-passed octave pick feed 250 ms stereo echoes; lift early for clean gaps.",
        plucked()
               .wave(I, false, true, 0.20f).pw(I, 0.57f).wave(II, true, false).feet(II, f4).hpf(I, 29.0f).hpf(II, 1250.0f)
               .lpf(I, 5900.0f, 0.045f).lpf(II, 10800.0f, 0.035f).feg(I, 0.0f, 0.12f, 0.002f, 1.65f, 0.17f).level(I, 0.80f, 0.075f)
               .level(II, 0.78f, 0.025f).pan(I, -0.06f).pan(II, 0.19f).mix(0.15f).detune(0.0f).fx(0, fxDelay, 0.16f)
               .g(P_fx1Model, 4.0f).g(P_fx1ParamA, 0.4764f).g(P_fx1ParamB, 0.38f).body(I, body::Model::steel, 1.0f, 0.54f, 1.08f)
               .body(II, body::Model::steel, 0.68f, 0.78f, 0.28f).eg(I, 0.002f, 6.2f, 0.055f, 0.15f)
               .eg(II, 0.001f, 0.12f, 0.0f, 0.029f).feg(II, 0.0f, 0.21f, 0.001f, 0.072f, 0.024f).touch(I, 0.23f, 1.0f, 0.0f, 0.0f)
               .touch(II, 0.38f, 1.0f, 0.0f, 0.0f).contour(-0.12f, -0.24f, -0.055f, 0.13f).bodySize(I, 1.12f).bodySize(II, 0.92f));

    add("Ambient Bells", "Bells", { "modern", "ring-mod", "sine", "feet-layer", "velocity" },
        "Strike widely spaced notes around C4-C6 and allow the decay to clear. A rounded fundamental outlasts the upper fifth; quiet 560 ms echoes open into a dark plate.",
        bell()
               .wave(I, false, false, 1.0f).feet(I, f8).wave(II, false, false, 0.48f).feet(II, f513).hpf(I, 24.0f).hpf(II, 470.0f)
               .lpf(I, 11200.0f, 0.01f).lpf(II, 14500.0f, 0.015f).feg(I, 0.0f, 0.0f, 0.001f, 0.3f, 0.10f)
               .feg(II, 0.0f, 0.0f, 0.001f, 0.3f, 0.10f).level(I, 0.0f, 0.060f).level(II, 0.0f, 0.025f).sub(subSine, 5.0f, 0.0f)
               .subAt(0.0f, 0.0f).pan(I, -0.035f).pan(II, 0.12f).mix(0.19f).detune(0.0f).res(0.02f).g(P_chorusOn, 0.0f)
               .g(P_modWheelSub, 0.0f).g(P_voiceDispersion, 0.04f).g(P_drift, 0.01f).g(P_analogNoise, 0.0f).fx(0, fxDelay, 0.14f)
               .g(P_fx1Model, 4.0f).g(P_fx1ParamA, 0.7456f).g(P_fx1ParamB, 0.61f).fx(1, fxPlate, 0.20f).g(P_fx2Model, 3.0f)
               .g(P_fx2ParamA, 0.58f).g(P_fx2ParamB, 0.30f).body(I, body::Model::bell, 1.0f, 0.38f, 2.15f)
               .body(II, body::Model::bell, 0.91f, 0.59f, 0.53f).eg(I, 0.0025f, 10.4f, 0.060f, 2.85f)
               .eg(II, 0.0015f, 1.65f, 0.0f, 0.34f).ring(137.0f, 0.0f, 0.001f, 3.2f, 0.018f).touch(I, 0.065f, 1.0f, 0.0f, 0.0f)
               .touch(II, 0.17f, 1.0f, 0.0f, 0.0f).contour(0.045f, -0.20f, -0.075f, 0.15f));

    add("Ensemble Plus", "Strings", { "modern", "chorus", "pwm", "dual-rank", "aftertouch" },
        "Use open midrange voicings and sustain through the staggered bow onset. Differently sized string bodies, restrained original chorus and a shallow three-tap ensemble widen the sound while keeping its center clear.",
        strings()
               .wave(I, true, false).wave(II, false, true).pw(II, 0.59f, 0.045f, 0.22f).hpf(I, 70.0f).hpf(II, 310.0f)
               .lpf(I, 5200.0f, 0.055f).lpf(II, 7500.0f, 0.04f).level(I, 0.95f, 0.12f).level(II, 0.86f, 0.075f)
               .touch(I, 0.08f, 1.0f, 0.18f, 0.34f).mix(0.39f).detune(1.1f).chorus(0.125f, 0.19f).fx(0, fxChorus, 0.17f)
               .g(P_fx1Model, 1.0f).g(P_fx1ParamA, 0.27f).g(P_fx1ParamB, 0.25f).body(I, body::Model::bowed, 0.76f, 0.53f, 1.28f)
               .body(II, body::Model::bowed, 0.42f, 0.72f, 1.08f).eg(I, 0.10f, 2.0f, 0.95f, 0.78f)
               .eg(II, 0.26f, 2.4f, 0.63f, 1.05f).feg(I, 0.08f, 0.13f, 0.19f, 2.3f, 0.67f)
               .feg(II, 0.18f, 0.12f, 0.43f, 2.9f, 0.98f).touch(II, 0.12f, 0.98f, 0.25f, 0.45f)
               .contour(-0.12f, -0.23f, -0.045f, 0.08f).bodySize(I, 1.15f).bodySize(II, 0.96f));

    add("Space Lead", "Leads", { "modern", "glide", "aftertouch", "dual-rank" },
        "Connect single lines in C4-C6 with short slides and leave room for 400 ms echoes. Pressure introduces gentle vibrato; the dry release keeps phrase endings clear.",
        lead()
               .wave(I, true, false, 0.16f).wave(II, false, true).hpf(I, 31.0f).lpf(I, 1510.0f, 0.09f).lpf(II, 3200.0f, 0.075f)
               .level(I, 0.90f, 0.12f).level(II, 0.80f, 0.085f).touch(I, 0.13f, 0.97f, 0.22f, 0.27f).subAt(0.03f, 0.08f)
               .g(P_modWheelSub, 0.05f).bend(0.0f).glide(0.09f).mix(0.20f).detune(0.25f).fx(0, fxDelay, 0.19f).g(P_fx1Model, 4.0f)
               .g(P_fx1ParamA, 0.6333f).g(P_fx1ParamB, 0.49f).fx(1, fxPlate, 0.075f).g(P_fx2Model, 3.0f).g(P_fx2ParamA, 0.33f)
               .g(P_fx2ParamB, 0.35f).pw(II, 0.63f, 0.025f, 0.165f).hpf(II, 340.0f).feg(I, 0.08f, 0.19f, 0.041f, 1.85f, 0.23f)
               .feg(II, 0.15f, 0.29f, 0.078f, 0.71f, 0.16f).eg(I, 0.018f, 2.4f, 0.98f, 0.135f).eg(II, 0.043f, 0.91f, 0.34f, 0.085f)
               .touch(II, 0.27f, 1.0f, 0.39f, 0.30f).contour(-0.11f, -0.21f, -0.03f, 0.095f));

    add("Random Arp", "Sequences", { "modern", "lfo", "velocity", "percussive", "feet-layer" },
        "Hold a small chord near C3 for six random notes per second over two octaves. Sample-and-hold brightness changes at half the note rate; short releases separate the notes.",
        poly()
               .wave(I, false, true, 0.06f).wave(II, true, false).feet(II, f4).hpf(I, 43.0f).lpf(I, 1740.0f, 0.18f)
               .lpf(II, 4300.0f, 0.11f).level(I, 0.91f, 0.13f).level(II, 0.83f, 0.025f).touch(I, 0.16f, 1.0f, 0.09f, 0.065f)
               .mix(0.14f).detune(0.0f).g(P_chorusOn, 0.0f).arp(arpRandom, 6.0f, 2).lfo(0, 3.0f, 0.13f, toFilter)
               .g(P_lfo1Wave, 5.0f).pw(I, 0.67f).hpf(II, 570.0f).feg(I, 0.0f, 0.39f, 0.002f, 0.185f, 0.028f)
               .feg(II, 0.0f, 0.49f, 0.001f, 0.063f, 0.018f).eg(I, 0.002f, 0.48f, 0.0f, 0.024f)
               .eg(II, 0.001f, 0.095f, 0.0f, 0.015f).touch(II, 0.36f, 1.0f, 0.0f, 0.0f).contour(-0.10f, -0.24f, -0.045f, 0.085f));

    add("Drive Bass", "Bass", { "modern", "velocity", "feet-layer", "sine" },
        "Play short bass lines around C3. A firm 16' pulse-and-sine body anchors a brief 8' rasp; firmer strikes add level and bite, while parallel drive gently smooths the edge.",
        bass()
               .wave(II, true, false).hpf(I, 20.0f).lpf(I, 810.0f, 0.105f).lpf(II, 2850.0f, 0.08f).level(I, 1.0f, 0.14f)
               .level(II, 0.84f, 0.035f).touch(I, 0.11f, 1.0f, 0.055f, 0.05f).mix(0.16f).detune(0.0f).res(0.075f)
               .g(P_voiceDispersion, 0.05f).g(P_drift, 0.015f).fx(0, fxDrive, 0.245f).g(P_fx1Model, 5.0f).g(P_fx1ParamA, 0.30f)
               .g(P_fx1ParamB, 0.20f).g(P_fx1Oversampling, 2.0f).wave(I, false, true, 0.46f).pw(I, 0.53f).hpf(II, 340.0f)
               .feg(I, 0.0f, 0.27f, 0.004f, 1.2f, 0.09f).feg(II, 0.0f, 0.55f, 0.001f, 0.115f, 0.032f)
               .eg(I, 0.003f, 2.1f, 0.91f, 0.058f).eg(II, 0.001f, 0.17f, 0.025f, 0.025f).touch(II, 0.32f, 1.0f, 0.17f, 0.065f)
               .contour(-0.18f, -0.20f, -0.09f, 0.06f));

    add("Phased Polysynth", "Polysynth", { "modern", "velocity", "dual-rank", "percussive" },
        "Use compact rhythmic chords in C3-C5. A broad saw body follows a sharper pulse accent; the phaser animates the held tail, with a short plate after release.",
        poly()
               .wave(I, true, false, 0.035f).wave(II, false, true).hpf(I, 51.0f).lpf(I, 2700.0f, 0.075f).lpf(II, 5300.0f, 0.055f)
               .level(I, 0.94f, 0.13f).level(II, 0.86f, 0.060f).touch(I, 0.18f, 0.99f, 0.17f, 0.20f).mix(0.27f).detune(1.3f)
               .g(P_chorusOn, 0.0f).fx(0, fxPhaser, 0.32f).fx(1, fxPlate, 0.07f).g(P_fx2Model, 3.0f).g(P_fx2ParamA, 0.16f)
               .g(P_fx2ParamB, 0.44f).pw(II, 0.65f, 0.024f, 0.25f).hpf(II, 350.0f).feg(I, 0.0f, 0.25f, 0.005f, 1.32f, 0.145f)
               .feg(II, 0.0f, 0.36f, 0.002f, 0.17f, 0.052f).eg(I, 0.005f, 2.7f, 0.65f, 0.145f).eg(II, 0.003f, 0.24f, 0.03f, 0.045f)
               .touch(II, 0.33f, 1.0f, 0.27f, 0.19f).contour(-0.11f, -0.23f, -0.045f, 0.085f));

    add("Tremolo LFO Organ", "Organ", { "modern", "lfo", "tremolo", "sine", "feet-layer", "aftertouch" },
        "Hold chords around C3-C5 for a steady four-pulse-per-second tremolo. The 8' body has a little reed edge; pressure brings forward the sustained 4' stop.",
        organ()
               .wave(I, false, true, 0.83f).pw(I, 0.50f).feet(I, f8).wave(II, false, false, 0.56f).feet(II, f4)
               .level(I, 0.15f, 0.55f).level(II, 0.0f, 0.33f).feg(I, 0.0f, 0.0f, 0.01f, 0.3f, 0.06f)
               .feg(II, 0.0f, 0.0f, 0.01f, 0.3f, 0.06f).touch(I, 0.0f, 0.0f, 0.0f, 0.0f).touch(II, 0.0f, 0.0f, 0.0f, 0.22f)
               .pan(I, 0.0f).pan(II, 0.0f).mix(0.22f).detune(0.0f).lfo(0, 4.0f, 0.80f, toAmp).g(P_lfo1Wave, 1.0f)
               .body(I, body::Model::organ, 0.96f, 0.37f, 1.0f).body(II, body::Model::organ, 0.87f, 0.61f, 1.0f)
               .eg(I, 0.012f, 0.38f, 1.0f, 0.028f).eg(II, 0.009f, 0.47f, 1.0f, 0.022f).hpf(I, 24.0f).lpf(I, 4300.0f, 0.02f)
               .contour(-0.11f, -0.18f, -0.055f, 0.10f).hpf(II, 95.0f).lpf(II, 11000.0f, 0.015f));
}
