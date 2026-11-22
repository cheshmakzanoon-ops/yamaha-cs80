// Modern performance voicings. Included inside namespace curated by CuratedLibrary.inc.
// Existing program names and insertion order are stable host/MIDI identities.
// Each dry voicing is articulated for its modulation/effect, including the arp's
// fixed 58% gate (45% in random mode). Rates are free-running, not host-synced.
// Model 4 echo time is 0.06 * 20^A seconds; model 3 plate decay is 0.8 * 10^A
// seconds before brightness-dependent damping. Conservative wet levels retain
// the onset and register balance. PURE CS-80 makes this software layer inert.

void buildModern(Builder& add)
{
    add("Delay Brass", "Brass", { "modern", "brilliance-swell", "aftertouch", "velocity" },
        "Play clipped midrange chords; the opening brass swell leaves gaps for 375 ms echoes. Lean into pressure to brighten held notes.",
        brass().wave(I, true, false, 0.025f).wave(II, true, false)
               .hpf(I, 35.0f).hpf(II, 85.0f).lpf(I, 820.0f, 0.16f).lpf(II, 1480.0f, 0.19f)
               .feg(I, 0.24f, 0.48f, 0.085f, 1.1f, 0.22f).feg(II, 0.32f, 0.58f, 0.14f, 0.75f, 0.16f)
               .eg(I, 0.018f, 1.2f, 0.76f, 0.19f).eg(II, 0.035f, 0.70f, 0.48f, 0.14f)
               .level(I, 0.96f, 0.38f).level(II, 0.90f, 0.25f)
               .touch(I, 0.15f, 0.58f, 0.30f, 0.24f).touch(II, 0.24f, 0.66f, 0.40f, 0.20f)
               .subAt(0.03f, 0.055f).mix(0.34f).detune(2.2f)
               .fx(0, fxDelay, 0.22f).g(P_fx1Model, 4.0f).g(P_fx1ParamA, 0.612f).g(P_fx1ParamB, 0.50f)
               .fx(1, fxPlate, 0.08f).g(P_fx2Model, 3.0f).g(P_fx2ParamA, 0.22f).g(P_fx2ParamB, 0.42f));

    add("Plate Pad", "Pads", { "modern", "feet-layer", "pwm", "aftertouch" },
        "Hold open chords around C3-C5; a soft lower pulse blooms beneath an octave sheen. Release between changes to hear the warm plate tail.",
        pad().wave(I, false, true, 0.12f).pw(I, 0.53f, 0.10f, 0.18f).wave(II, true, false).feet(II, f4)
             .hpf(I, 50.0f).hpf(II, 260.0f).lpf(I, 1650.0f, 0.08f).lpf(II, 4400.0f, 0.06f)
             .feg(I, 0.16f, 0.18f, 0.62f, 2.8f, 0.9f).feg(II, 0.24f, 0.25f, 1.0f, 2.1f, 1.2f)
             .eg(I, 0.42f, 2.8f, 0.84f, 1.25f).eg(II, 0.85f, 2.1f, 0.63f, 1.75f)
             .level(I, 0.92f, 0.48f).level(II, 0.82f, 0.28f)
             .touch(I, 0.05f, 0.36f, 0.14f, 0.25f).touch(II, 0.08f, 0.32f, 0.20f, 0.25f)
             .pan(I, -0.12f).pan(II, 0.12f).mix(0.23f).detune(1.8f).g(P_chorusOn, 0.0f)
             .fx(0, fxPlate, 0.26f).g(P_fx1Model, 3.0f).g(P_fx1ParamA, 0.48f).g(P_fx1ParamB, 0.40f));

    add("Phaser Strings", "Strings", { "modern", "dual-rank", "pwm", "aftertouch" },
        "Bow connected chords in C3-C5 and hold the ends of phrases. A bright saw section and quieter pulse band expose the phaser's slow moving notches.",
        strings().wave(I, true, false).wave(II, false, true).pw(II, 0.64f, 0.08f, 0.29f)
                 .hpf(I, 110.0f).hpf(II, 240.0f).lpf(I, 4700.0f, 0.07f).lpf(II, 6200.0f, 0.05f)
                 .feg(I, 0.10f, 0.12f, 0.09f, 1.1f, 0.32f).feg(II, 0.16f, 0.08f, 0.22f, 1.4f, 0.46f)
                 .eg(I, 0.09f, 0.85f, 0.84f, 0.40f).eg(II, 0.19f, 1.2f, 0.65f, 0.65f)
                 .level(I, 0.93f, 0.52f).level(II, 0.83f, 0.38f)
                 .touch(I, 0.10f, 0.38f, 0.13f, 0.20f).touch(II, 0.08f, 0.30f, 0.12f, 0.14f)
                 .pan(I, -0.08f).pan(II, 0.08f).mix(0.32f).detune(2.8f).g(P_chorusOn, 0.0f)
                 .fx(0, fxPhaser, 0.34f));

    add("Drive Lead", "Leads", { "modern", "aftertouch", "velocity", "dual-rank" },
        "Play firm lines in C4-C6: a pulse body carries a shorter saw edge into warm drive. Pressure adds bite and restrained vibrato.",
        lead().wave(I, false, true, 0.05f).pw(I, 0.55f).wave(II, true, false)
              .hpf(I, 45.0f).hpf(II, 150.0f).lpf(I, 1450.0f, 0.16f).lpf(II, 2700.0f, 0.12f)
              .feg(I, 0.03f, 0.22f, 0.008f, 0.72f, 0.12f).feg(II, 0.0f, 0.28f, 0.004f, 0.36f, 0.09f)
              .eg(I, 0.006f, 1.0f, 0.88f, 0.13f).eg(II, 0.011f, 0.65f, 0.50f, 0.10f)
              .level(I, 0.96f, 0.34f).level(II, 0.88f, 0.20f)
              .touch(I, 0.15f, 0.56f, 0.18f, 0.30f).touch(II, 0.24f, 0.68f, 0.28f, 0.25f)
              .subAt(0.035f, 0.09f).bend(0.0f).glide(0.018f).mix(0.23f).detune(0.35f)
              .fx(0, fxDrive, 0.43f).g(P_fx1Model, 5.0f).g(P_fx1ParamA, 0.42f).g(P_fx1ParamB, 0.36f)
              .g(P_fx1Oversampling, 2.0f));

    add("Arp Poly", "Sequences", { "modern", "lfo", "velocity", "percussive", "feet-layer" },
        "Hold two to four notes around C3-C4 for eight notes per second across two octaves. A short octave accent and one-second filter cycle articulate the ascent.",
        poly().wave(I, false, true, 0.04f).pw(I, 0.58f).wave(II, true, false).feet(II, f4)
              .hpf(I, 38.0f).hpf(II, 160.0f).lpf(I, 1050.0f, 0.17f).lpf(II, 2500.0f, 0.13f)
              .feg(I, 0.0f, 0.48f, 0.002f, 0.42f, 0.07f).feg(II, 0.0f, 0.58f, 0.001f, 0.16f, 0.04f)
              .eg(I, 0.002f, 1.05f, 0.12f, 0.08f).eg(II, 0.001f, 0.30f, 0.0f, 0.05f)
              .level(I, 0.94f, 0.32f).level(II, 0.86f, 0.11f)
              .touch(I, 0.14f, 0.75f, 0.10f, 0.10f).touch(II, 0.24f, 0.95f, 0.0f, 0.0f)
              .mix(0.22f).detune(0.6f).g(P_chorusOn, 0.0f)
              .arp(arpUp, 8.0f, 2).lfo(0, 1.0f, 0.12f, toFilter).g(P_lfo1Wave, 1.0f));

    add("Arp Bells", "Sequences", { "modern", "ring-mod", "velocity", "percussive", "feet-layer" },
        "Hold a small chord around C4 for four notes per second, rising and falling over two octaves. A short third-harmonic strike leaves space for 375 ms echoes.",
        bell().wave(I, false, false, 1.0f).feet(I, f8).wave(II, false, false, 0.72f).feet(II, f223)
              .hpf(I, 30.0f).hpf(II, 100.0f).lpf(Both, 6000.0f, 0.02f)
              .feg(Both, 0.0f, 0.0f, 0.001f, 0.3f, 0.10f)
              .eg(I, 0.002f, 2.4f, 0.0f, 0.22f).eg(II, 0.001f, 0.52f, 0.0f, 0.10f)
              .level(I, 0.0f, 0.25f).level(II, 0.0f, 0.10f)
              .touch(I, 0.0f, 0.80f, 0.0f, 0.0f).touch(II, 0.0f, 0.95f, 0.0f, 0.0f)
              .ring(211.0f, 0.0f, 0.001f, 0.25f, 0.20f).sub(subSine, 5.0f, 0.0f).subAt(0.0f, 0.0f)
              .pan(Both, 0.0f).mix(0.20f).detune(0.0f).res(0.02f).g(P_chorusOn, 0.0f).g(P_modWheelSub, 0.0f)
              .g(P_voiceDispersion, 0.04f).g(P_drift, 0.015f).g(P_analogNoise, 0.0f)
              .arp(arpUpDown, 4.0f, 2).fx(0, fxDelay, 0.17f)
              .g(P_fx1Model, 4.0f).g(P_fx1ParamA, 0.612f).g(P_fx1ParamB, 0.48f));

    add("Arp Bass", "Sequences", { "modern", "lfo", "velocity", "percussive", "feet-layer" },
        "Hold a root and fifth around C3 for a six-note-per-second bass pattern. A compact 16' body and short 8' snap follow a gentle four-step filter cycle.",
        bass().wave(I, true, false, 0.22f).wave(II, false, true).pw(II, 0.66f)
              .hpf(I, 22.0f).hpf(II, 65.0f).lpf(I, 530.0f, 0.22f).lpf(II, 1600.0f, 0.16f)
              .feg(I, 0.0f, 0.50f, 0.002f, 0.40f, 0.06f).feg(II, 0.0f, 0.62f, 0.001f, 0.13f, 0.04f)
              .eg(I, 0.002f, 1.2f, 0.18f, 0.075f).eg(II, 0.0015f, 0.24f, 0.0f, 0.045f)
              .level(I, 1.0f, 0.32f).level(II, 0.78f, 0.12f)
              .touch(I, 0.10f, 0.72f, 0.06f, 0.04f).touch(II, 0.20f, 0.94f, 0.0f, 0.0f)
              .mix(0.20f).detune(0.0f).res(0.18f)
              .arp(arpUp, 6.0f, 1).lfo(0, 1.5f, 0.11f, toFilter).g(P_lfo1Wave, 1.0f));

    add("LFO Sweep Pad", "Pads", { "modern", "lfo", "pwm", "chorus", "dual-rank" },
        "Hold spacious chords for at least ten seconds. A broad triangle filter sweep moves over a warm low body, with slower irregular stereo drift.",
        pad().wave(I, true, false, 0.09f).wave(II, false, true).pw(II, 0.62f, 0.08f, 0.19f)
             .hpf(I, 28.0f).hpf(II, 105.0f).lpf(I, 1020.0f, 0.25f).lpf(II, 2200.0f, 0.28f)
             .feg(I, 0.12f, 0.16f, 0.38f, 2.0f, 0.8f).feg(II, 0.20f, 0.20f, 0.70f, 2.3f, 1.0f)
             .eg(I, 0.32f, 2.4f, 0.86f, 1.35f).eg(II, 0.65f, 3.0f, 0.72f, 1.8f)
             .level(I, 0.92f, 0.50f).level(II, 0.86f, 0.37f)
             .touch(I, 0.04f, 0.34f, 0.12f, 0.18f).touch(II, 0.06f, 0.38f, 0.18f, 0.24f)
             .pan(I, -0.12f).pan(II, 0.12f).mix(0.35f).detune(3.2f).res(0.15f).chorus(0.18f, 0.23f)
             .lfo(0, 0.10f, 0.40f, toFilter).g(P_lfo1Wave, 1.0f)
             .lfo(1, 0.07f, 0.14f, toPan).g(P_lfo2Wave, 6.0f));

    add("LFO Pan Keys", "Keys", { "modern", "lfo", "velocity", "sine", "feet-layer" },
        "Play relaxed C3-C6 chords and let them ring. The rounded body rocks left to right once per second while its brighter octave strike fades first.",
        keys().wave(I, false, true, 0.90f).pw(I, 0.50f).wave(II, false, true, 0.32f).pw(II, 0.72f).feet(II, f4)
              .hpf(I, 35.0f).hpf(II, 650.0f).lpf(I, 1300.0f, 0.04f).lpf(II, 4400.0f, 0.07f)
              .feg(I, 0.0f, 0.16f, 0.003f, 1.6f, 0.30f).feg(II, 0.0f, 0.28f, 0.0015f, 0.42f, 0.12f)
              .eg(I, 0.003f, 5.6f, 0.0f, 0.60f).eg(II, 0.0017f, 1.2f, 0.0f, 0.16f)
              .level(I, 0.18f, 0.32f).level(II, 0.46f, 0.10f)
              .touch(I, 0.12f, 0.78f, 0.0f, 0.0f).touch(II, 0.24f, 0.92f, 0.0f, 0.0f)
              .pan(Both, 0.0f).mix(0.18f).detune(0.45f)
              .lfo(0, 1.0f, 0.72f, toPan).g(P_lfo1Wave, 1.0f)
              .fx(0, fxPlate, 0.08f).g(P_fx1Model, 3.0f).g(P_fx1ParamA, 0.12f).g(P_fx1ParamB, 0.38f));

    add("PWM LFO Strings", "Strings", { "modern", "lfo", "pwm", "dual-rank", "aftertouch" },
        "Sustain midrange chords: two differently biased pulse ranks share a slow triangle width sweep. The later, brighter rank adds a gentle shimmer.",
        strings().wave(Both, false, true).pw(I, 0.53f, 0.0f).pw(II, 0.65f, 0.0f)
                 .hpf(I, 85.0f).hpf(II, 240.0f).lpf(I, 3100.0f, 0.08f).lpf(II, 4700.0f, 0.06f)
                 .feg(I, 0.08f, 0.16f, 0.10f, 1.2f, 0.48f).feg(II, 0.18f, 0.10f, 0.24f, 1.6f, 0.65f)
                 .eg(I, 0.11f, 1.0f, 0.87f, 0.55f).eg(II, 0.24f, 1.5f, 0.72f, 0.80f)
                 .level(I, 0.94f, 0.50f).level(II, 0.82f, 0.40f)
                 .touch(I, 0.08f, 0.34f, 0.14f, 0.18f).touch(II, 0.05f, 0.28f, 0.10f, 0.16f)
                 .mix(0.37f).detune(1.3f).g(P_chorusOn, 0.0f)
                 .lfo(0, 0.22f, 0.56f, toPwm).g(P_lfo1Wave, 1.0f));

    add("Stereo Delay Pluck", "Plucked", { "modern", "velocity", "percussive", "feet-layer" },
        "Play sparse picked figures around C3-C5. A warm pulse string and short octave pick feed 250 ms stereo echoes; lift early for clean gaps.",
        plucked().wave(I, false, true, 0.20f).pw(I, 0.54f).wave(II, true, false).feet(II, f4)
                 .hpf(I, 40.0f).hpf(II, 650.0f).lpf(I, 1350.0f, 0.10f).lpf(II, 5000.0f, 0.07f)
                 .feg(I, 0.0f, 0.32f, 0.002f, 1.35f, 0.22f).feg(II, 0.0f, 0.28f, 0.001f, 0.18f, 0.06f)
                 .eg(I, 0.002f, 3.2f, 0.0f, 0.35f).eg(II, 0.001f, 0.27f, 0.0f, 0.07f)
                 .level(I, 0.80f, 0.28f).level(II, 0.78f, 0.08f)
                 .touch(I, 0.16f, 0.78f, 0.0f, 0.0f).touch(II, 0.25f, 0.96f, 0.0f, 0.0f)
                 .pan(I, -0.20f).pan(II, 0.28f).mix(0.18f).detune(0.0f)
                 .fx(0, fxDelay, 0.18f).g(P_fx1Model, 4.0f).g(P_fx1ParamA, 0.4764f).g(P_fx1ParamB, 0.42f));

    add("Ambient Bells", "Bells", { "modern", "ring-mod", "sine", "feet-layer", "velocity" },
        "Strike widely spaced notes around C4-C6 and allow the decay to clear. A rounded fundamental outlasts the upper fifth; quiet 560 ms echoes open into a dark plate.",
        bell().wave(I, false, false, 1.0f).feet(I, f8).wave(II, false, false, 0.48f).feet(II, f513)
              .hpf(Both, 30.0f).lpf(Both, 9000.0f, 0.02f).feg(Both, 0.0f, 0.0f, 0.001f, 0.3f, 0.10f)
              .eg(I, 0.003f, 5.8f, 0.0f, 2.0f).eg(II, 0.001f, 1.55f, 0.0f, 0.60f)
              .level(I, 0.0f, 0.25f).level(II, 0.0f, 0.09f)
              .touch(I, 0.0f, 0.80f, 0.0f, 0.0f).touch(II, 0.0f, 0.94f, 0.0f, 0.0f)
              .ring(137.0f, 0.04f, 0.001f, 2.8f, 0.18f).sub(subSine, 5.0f, 0.0f).subAt(0.0f, 0.0f)
              .pan(I, -0.10f).pan(II, 0.10f).mix(0.24f).detune(0.20f).res(0.02f)
              .g(P_chorusOn, 0.0f).g(P_modWheelSub, 0.0f)
              .g(P_voiceDispersion, 0.04f).g(P_drift, 0.01f).g(P_analogNoise, 0.0f)
              .fx(0, fxDelay, 0.16f).g(P_fx1Model, 4.0f).g(P_fx1ParamA, 0.7456f).g(P_fx1ParamB, 0.66f)
              .fx(1, fxPlate, 0.24f).g(P_fx2Model, 3.0f).g(P_fx2ParamA, 0.58f).g(P_fx2ParamB, 0.36f));

    add("Ensemble Plus", "Strings", { "modern", "chorus", "pwm", "dual-rank", "aftertouch" },
        "Use open midrange voicings and sustain through the staggered onset. Light original chorus and a shallow three-tap ensemble broaden the saw/pulse section.",
        strings().wave(I, true, false).wave(II, false, true).pw(II, 0.56f, 0.08f, 0.27f)
                 .hpf(I, 95.0f).hpf(II, 190.0f).lpf(I, 3000.0f, 0.08f).lpf(II, 4900.0f, 0.06f)
                 .feg(I, 0.10f, 0.16f, 0.12f, 1.4f, 0.48f).feg(II, 0.18f, 0.12f, 0.27f, 1.7f, 0.70f)
                 .eg(I, 0.10f, 1.1f, 0.85f, 0.60f).eg(II, 0.22f, 1.3f, 0.73f, 0.85f)
                 .level(I, 0.95f, 0.52f).level(II, 0.86f, 0.43f)
                 .touch(I, 0.09f, 0.32f, 0.15f, 0.18f).touch(II, 0.07f, 0.29f, 0.12f, 0.15f)
                 .mix(0.44f).detune(1.5f).chorus(0.16f, 0.23f)
                 .fx(0, fxChorus, 0.25f).g(P_fx1Model, 1.0f).g(P_fx1ParamA, 0.32f).g(P_fx1ParamB, 0.30f));

    add("Space Lead", "Leads", { "modern", "glide", "aftertouch", "dual-rank" },
        "Connect single lines in C4-C6 with short slides and leave room for 400 ms echoes. Pressure introduces gentle vibrato; the dry release keeps phrase endings clear.",
        lead().wave(I, true, false, 0.16f).wave(II, false, true).pw(II, 0.56f, 0.06f, 0.18f)
              .hpf(I, 40.0f).hpf(II, 160.0f).lpf(I, 1200.0f, 0.13f).lpf(II, 2450.0f, 0.11f)
              .feg(I, 0.08f, 0.20f, 0.04f, 1.1f, 0.25f).feg(II, 0.12f, 0.28f, 0.065f, 0.72f, 0.18f)
              .eg(I, 0.018f, 1.5f, 0.90f, 0.27f).eg(II, 0.038f, 1.0f, 0.55f, 0.20f)
              .level(I, 0.90f, 0.34f).level(II, 0.80f, 0.22f)
              .touch(I, 0.10f, 0.62f, 0.18f, 0.24f).touch(II, 0.18f, 0.64f, 0.28f, 0.20f)
              .subAt(0.03f, 0.08f).g(P_modWheelSub, 0.05f).bend(0.0f).glide(0.09f).mix(0.24f).detune(0.45f)
              .fx(0, fxDelay, 0.22f).g(P_fx1Model, 4.0f).g(P_fx1ParamA, 0.6333f).g(P_fx1ParamB, 0.55f)
              .fx(1, fxPlate, 0.12f).g(P_fx2Model, 3.0f).g(P_fx2ParamA, 0.33f).g(P_fx2ParamB, 0.40f));

    add("Random Arp", "Sequences", { "modern", "lfo", "velocity", "percussive", "feet-layer" },
        "Hold a small chord near C3 for six random notes per second over two octaves. Sample-and-hold brightness changes at half the note rate; short releases separate the notes.",
        poly().wave(I, false, true, 0.06f).pw(I, 0.68f).wave(II, true, false).feet(II, f4)
              .hpf(I, 55.0f).hpf(II, 260.0f).lpf(I, 1450.0f, 0.25f).lpf(II, 3400.0f, 0.16f)
              .feg(I, 0.0f, 0.38f, 0.002f, 0.30f, 0.06f).feg(II, 0.0f, 0.44f, 0.001f, 0.14f, 0.04f)
              .eg(I, 0.002f, 0.85f, 0.08f, 0.065f).eg(II, 0.001f, 0.22f, 0.0f, 0.035f)
              .level(I, 0.91f, 0.31f).level(II, 0.83f, 0.10f)
              .touch(I, 0.12f, 0.74f, 0.08f, 0.07f).touch(II, 0.24f, 0.94f, 0.0f, 0.0f)
              .mix(0.17f).detune(0.0f).g(P_chorusOn, 0.0f)
              .arp(arpRandom, 6.0f, 2).lfo(0, 3.0f, 0.13f, toFilter).g(P_lfo1Wave, 5.0f));

    add("Drive Bass", "Bass", { "modern", "velocity", "feet-layer", "sine" },
        "Play short bass lines around C3. A firm 16' pulse-and-sine body anchors a brief 8' rasp; firmer strikes add level and bite, while parallel drive gently smooths the edge.",
        bass().wave(I, false, true, 0.30f).pw(I, 0.53f).wave(II, true, false)
              .hpf(I, 22.0f).hpf(II, 140.0f).lpf(I, 680.0f, 0.15f).lpf(II, 2100.0f, 0.12f)
              .feg(I, 0.0f, 0.28f, 0.004f, 0.75f, 0.10f).feg(II, 0.0f, 0.54f, 0.0015f, 0.24f, 0.06f)
              .eg(I, 0.003f, 1.4f, 0.78f, 0.12f).eg(II, 0.0015f, 0.35f, 0.08f, 0.065f)
              .level(I, 1.0f, 0.34f).level(II, 0.84f, 0.10f)
              .touch(I, 0.08f, 0.66f, 0.04f, 0.04f).touch(II, 0.20f, 0.94f, 0.12f, 0.04f)
              .mix(0.19f).detune(0.0f).res(0.10f).g(P_voiceDispersion, 0.05f).g(P_drift, 0.015f)
              .fx(0, fxDrive, 0.28f).g(P_fx1Model, 5.0f).g(P_fx1ParamA, 0.34f).g(P_fx1ParamB, 0.24f)
              .g(P_fx1Oversampling, 2.0f));

    add("Phased Polysynth", "Polysynth", { "modern", "velocity", "dual-rank", "percussive" },
        "Use compact rhythmic chords in C3-C5. A broad saw body follows a sharper pulse accent; the phaser animates the held tail, with a short plate after release.",
        poly().wave(I, true, false, 0.035f).wave(II, false, true).pw(II, 0.73f, 0.05f, 0.37f)
              .hpf(I, 65.0f).hpf(II, 150.0f).lpf(I, 2200.0f, 0.10f).lpf(II, 4200.0f, 0.08f)
              .feg(I, 0.0f, 0.25f, 0.005f, 0.80f, 0.16f).feg(II, 0.0f, 0.33f, 0.003f, 0.35f, 0.10f)
              .eg(I, 0.005f, 1.8f, 0.50f, 0.23f).eg(II, 0.003f, 0.65f, 0.10f, 0.14f)
              .level(I, 0.94f, 0.34f).level(II, 0.86f, 0.17f)
              .touch(I, 0.15f, 0.66f, 0.14f, 0.16f).touch(II, 0.22f, 0.86f, 0.18f, 0.12f)
              .mix(0.31f).detune(1.8f).g(P_chorusOn, 0.0f)
              .fx(0, fxPhaser, 0.38f).fx(1, fxPlate, 0.09f)
              .g(P_fx2Model, 3.0f).g(P_fx2ParamA, 0.16f).g(P_fx2ParamB, 0.50f));

    add("Tremolo LFO Organ", "Organ", { "modern", "lfo", "tremolo", "sine", "feet-layer", "aftertouch" },
        "Hold chords around C3-C5 for a steady four-pulse-per-second tremolo. The 8' body has a little reed edge; pressure brings forward the sustained 4' stop.",
        organ().wave(I, false, true, 0.88f).pw(I, 0.50f).feet(I, f8)
               .wave(II, false, false, 0.62f).feet(II, f4)
               .hpf(I, 34.0f).lpf(I, 1150.0f, 0.05f).level(I, 0.18f, 0.55f).level(II, 0.0f, 0.36f)
               .feg(Both, 0.0f, 0.0f, 0.01f, 0.3f, 0.06f)
               .eg(I, 0.010f, 0.3f, 1.0f, 0.09f).eg(II, 0.012f, 0.4f, 1.0f, 0.075f)
               .touch(Both, 0.0f, 0.0f, 0.0f, 0.0f).touch(II, 0.0f, 0.0f, 0.0f, 0.14f)
               .pan(Both, 0.0f).mix(0.25f).detune(0.0f)
               .lfo(0, 4.0f, 0.80f, toAmp).g(P_lfo1Wave, 1.0f));
}
