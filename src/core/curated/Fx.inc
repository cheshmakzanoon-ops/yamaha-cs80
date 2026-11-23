// FX voicings. Included inside namespace curated by CuratedLibrary.inc.
// Existing program names and insertion order are stable host/MIDI identities.
// Original SUB function 4 is audio-rate white noise, not sample-and-hold.
// Rate-controlled random motion below uses the tagged modern LFO layer.
// Original SUB VCA makes exponential level motion; it cannot close the VCA.

Program effect()
{
    Program x;
    x.wave(I, true, false).wave(II, false, true).pw(II, 0.58f)
     .hpf(I, 45.0f).hpf(II, 150.0f).lpf(I, 1800.0f, 0.18f).lpf(II, 3200.0f, 0.24f)
     .feg(I, 0.08f, 0.18f, 0.025f, 0.70f, 0.40f).feg(II, 0.03f, 0.28f, 0.004f, 0.25f, 0.25f)
     .eg(I, 0.015f, 1.4f, 0.75f, 0.55f).eg(II, 0.003f, 0.45f, 0.50f, 0.35f)
     .level(I, 0.65f, 0.45f).level(II, 0.50f, 0.35f)
     .touch(I, 0.03f, 0.25f, 0.03f, 0.12f).touch(II, 0.06f, 0.28f, 0.04f, 0.16f)
     .sub(subSine, 5.0f, 0.0f, 0.0f, 0.0f).subAt(0.0f, 0.0f, 0.0f)
     .ring(80.0f, 0.0f, 0.01f, 1.0f, 0.0f).mix(0.36f).detune(0.0f).res(0.08f)
     .g(P_voiceDispersion, 0.07f).g(P_drift, 0.02f).g(P_analogNoise, 0.0f).volume(0.6f);
    return x;
}

void buildFx(Builder& add)
{
    add("Random Filter", "FX", { "modern", "lfo", "resonance" },
        "Modern sample-and-hold steps the resonant filters at 7.5 Hz. A saw body and lighter octave pulse make each held random cutoff audible as a computer-like burble.",
        effect().feet(II, f4)
                .hpf(I, 45.0f).feg(Both, 0.0f, 0.0f, 0.002f, 0.5f, 0.2f)
                .level(II, 0.42f, 0.25f).touch(Both, 0.0f, 0.22f, 0.06f, 0.10f)
                .lfo(0, 7.5f, 0.65f, toFilter).g(P_lfo1Wave, 5.0f).mix(0.25f).res(0.14f)
              .pw(II, 0.71f).hpf(II, 240.0f).lpf(I, 640.0f, 0.54f).lpf(II, 1700.0f, 0.43f).eg(I, 0.003f, 0.9f, 0.85f, 0.15f)
              .eg(II, 0.001f, 0.20f, 0.25f, 0.08f).touch(I, 0.02f, 0.32f, 0.12f, 0.15f).touch(II, 0.04f, 0.26f, 0.19f, 0.12f));
    add("Random Pitch Blips", "FX", { "modern", "lfo", "percussive" },
        "Modern sample-and-hold picks small, unquantized pitch steps around the played key at 6.5 Hz. A matching square amplitude LFO separates the sine-and-pulse tone into blips.",
        effect().wave(I, false, true, 0.72f).wave(II, false, true, 0.18f)
                .feet(II, f4).hpf(I, 35.0f).lpf(I, 2400.0f, 0.06f)
                .feg(Both, 0.0f, 0.0f, 0.002f, 0.5f, 0.2f)
                .level(I, 0.75f, 0.17f).level(II, 0.60f, 0.14f).touch(Both, 0.0f, 0.055f, 0.0f, 0.0f)
                .lfo(0, 6.5f, 0.95f, toPitch).g(P_lfo1Wave, 5.0f)
                .lfo(1, 6.5f, 1.0f, toAmp).g(P_lfo2Wave, 4.0f).mix(0.20f).res(0.025f)
              .pw(I, 0.50f, 0.0f).pw(II, 0.68f, 0.0f).hpf(II, 370.0f).lpf(II, 4900.0f, 0.045f).eg(I, 0.002f, 0.85f, 1.0f, 0.065f)
              .eg(II, 0.001f, 0.15f, 0.32f, 0.04f).g(P_voiceDispersion, 0.025f).g(P_drift, 0.0f));
    add("Siren", "FX", { "sub-mod", "sine", "aftertouch" },
        "A sine body and quiet octave harmonic follow a smooth three-semitone peak-to-peak SUB wail. Rounded onset and release avoid clicks; pressure quickens the siren.",
        effect().wave(I, false, false, 1.0f).wave(II, false, false, 0.35f).feet(II, f4)
                .level(I, 0.0f, 0.49f).level(II, 0.0f, 0.37f).touch(Both, 0.0f, 0.20f, 0.0f, 0.15f)
                .sub(subSine, 0.70f, 1.0f, 0.0f, 0.0f).subAt(0.15f, 0.0f, 0.0f).mix(0.18f)
              .eg(I, 0.15f, 1.6f, 1.0f, 0.75f).eg(II, 0.085f, 1.1f, 0.63f, 0.46f).touch(I, 0.0f, 0.24f, 0.0f, 0.22f)
              .touch(II, 0.0f, 0.17f, 0.0f, 0.14f).contour(0.0f, 0.0f, -0.06f, 0.08f).g(P_voiceDispersion, 0.02f)
              .g(P_drift, 0.0f));
    add("Laser Zap", "FX", { "resonance", "noise", "percussive" },
        "A resonant pulse sweep drops sharply after the strike, with no bypass sine masking the descent. A separate, very short noise flash adds the laser's initial snap.",
        effect().wave(I, false, true).pw(I, 0.50f).wave(II, false, false, 0.0f, 0.25f)
                .hpf(I, 30.0f)
                .level(I, 0.78f, 0.24f).level(II, 0.65f, 0.10f).touch(Both, 0.0f, 0.62f, 0.0f, 0.0f)
                .res(0.28f).mix(0.16f)
              .lpf(I, 185.0f, 0.76f).hpf(II, 1600.0f).lpf(II, 10000.0f, 0.055f).feg(I, 0.0f, 1.0f, 0.001f, 0.31f, 0.18f)
              .feg(II, 0.0f, 0.12f, 0.001f, 0.034f, 0.025f).eg(I, 0.001f, 1.05f, 0.0f, 0.34f)
              .eg(II, 0.001f, 0.075f, 0.0f, 0.040f).g(P_drift, 0.0f));
    add("Wind", "FX", { "noise", "sub-mod", "aftertouch" },
        "A low noise bed and a separate airy gust band move through a slow SUB filter swell. Pressure brings up the upper air; soft, unequal releases let the gust subside.",
        effect().wave(I, false, false, 0.0f, 1.0f).wave(II, false, false, 0.0f, 0.85f)
                .level(I, 1.0f, 0.72f).level(II, 0.95f, 0.50f)
                .touch(I, 0.0f, 0.30f, 0.14f, 0.52f).touch(II, 0.0f, 0.20f, 0.24f, 0.62f)
                .sub(subSine, 0.70f, 0.0f, 0.12f, 0.15f).mix(0.42f)
              .hpf(I, 55.0f, 0.06f).hpf(II, 1200.0f, 0.035f).lpf(I, 870.0f, 0.35f).lpf(II, 5100.0f, 0.07f)
              .feg(I, 0.34f, 0.21f, 1.65f, 5.3f, 2.8f).feg(II, 0.28f, 0.47f, 0.63f, 2.6f, 1.9f).eg(I, 1.3f, 5.0f, 0.94f, 3.8f)
              .eg(II, 0.34f, 2.7f, 0.48f, 2.3f).pan(I, -0.06f).pan(II, 0.14f));
    add("Surf", "FX", { "noise", "sub-mod", "dual-rank" },
        "A slow falling SUB saw shapes repeated crests of bright wash over a continuous low surf bed. Separate noise bands and envelope times give each wave weight and spray.",
        effect().wave(I, false, false, 0.0f, 1.0f).wave(II, false, false, 0.0f, 0.90f)
                .level(I, 1.0f, 0.70f).level(II, 1.0f, 0.60f)
                .touch(I, 0.0f, 0.34f, 0.0f, 0.26f).touch(II, 0.0f, 0.30f, 0.0f, 0.23f)
                .sub(subSawDown, 0.70f, 0.0f, 0.18f, 0.36f).mix(0.58f).res(0.035f)
              .hpf(I, 42.0f).hpf(II, 720.0f).lpf(I, 780.0f, 0.07f).lpf(II, 7100.0f, 0.035f)
              .feg(I, 0.22f, 0.13f, 0.65f, 3.4f, 2.6f).feg(II, 0.36f, 0.38f, 0.30f, 2.8f, 2.3f).eg(I, 1.45f, 4.1f, 0.91f, 3.6f)
              .eg(II, 0.83f, 2.8f, 0.60f, 2.7f).pan(I, -0.08f).pan(II, 0.11f));
    add("Sci-Fi Drone", "FX", { "ring-mod", "feet-layer", "sub-mod" },
        "A dark 16' saw-and-sine foundation supports a later third-harmonic pulse. Slow ring beating and gentle filter drift animate the held drone without a sharp attack.",
        effect().feet(I, f16).feet(II, f513).wave(I, true, false, 0.25f).wave(II, false, true, 0.08f)
                .hpf(I, 25.0f).lpf(I, 460.0f, 0.15f)
                .eg(I, 1.1f, 3.0f, 0.93f, 3.5f)
                .sub(subSine, 0.70f, 0.015f, 0.07f, 0.05f).ring(3.1f, 0.12f, 2.8f, 5.2f, 0.38f)
                .mix(0.31f).detune(1.2f)
              .pw(II, 0.67f, 0.09f, 0.11f).hpf(II, 310.0f).lpf(II, 1480.0f, 0.29f).feg(I, 0.24f, 0.17f, 1.45f, 5.4f, 3.1f)
              .feg(II, 0.43f, 0.41f, 3.05f, 5.8f, 2.8f).eg(II, 2.55f, 5.5f, 0.53f, 3.7f).touch(II, 0.05f, 0.23f, 0.24f, 0.28f));
    add("Robot Voice", "FX", { "ring-mod", "resonance", "aftertouch" },
        "A hollow pulse band and a brighter saw band close after a short consonant-like onset. A 73 Hz ring carrier adds robotic buzz; pressure opens the two bands.",
        effect().wave(I, false, true).wave(II, true, false)
                .eg(I, 0.008f, 0.65f, 0.58f, 0.18f)
                .level(I, 0.65f, 0.90f).level(II, 0.50f, 0.70f)
                .touch(I, 0.04f, 0.70f, 0.18f, 0.40f).touch(II, 0.07f, 0.80f, 0.24f, 0.32f)
                .ring(73.0f, 0.0f, 0.01f, 0.8f, 0.74f).mix(0.32f).res(0.10f)
              .pw(I, 0.51f).hpf(I, 210.0f, 0.18f).hpf(II, 1120.0f, 0.22f).lpf(I, 980.0f, 0.48f).lpf(II, 2530.0f, 0.45f)
              .feg(I, 0.24f, 0.50f, 0.055f, 0.45f, 0.18f).feg(II, 0.15f, 0.32f, 0.027f, 0.16f, 0.11f)
              .eg(II, 0.008f, 0.31f, 0.23f, 0.10f).contour(0.12f, -0.24f, -0.02f, 0.06f));
    add("Alarm", "FX", { "sub-mod", "feet-layer" },
        "A clear 4' call alternates between two pitches three semitones apart. A quieter low pulse gives the onset weight while square SUB level motion accents the two-tone warning.",
        effect().wave(I, false, true, 0.70f).wave(II, false, true).feet(I, f4).feet(II, f8)
                .pw(I, 0.50f).pw(II, 0.54f).hpf(I, 60.0f)
                .feg(Both, 0.0f, 0.0f, 0.002f, 0.5f, 0.15f)
                .level(I, 0.35f, 0.46f).level(II, 0.60f, 0.36f).touch(Both, 0.0f, 0.20f, 0.0f, 0.0f)
                .sub(subSquare, 2.8f, 1.0f, 0.0f, 0.18f).mix(0.22f).res(0.025f)
              .hpf(II, 95.0f).lpf(I, 3700.0f, 0.045f).lpf(II, 1430.0f, 0.06f).eg(I, 0.012f, 1.0f, 1.0f, 0.12f)
              .eg(II, 0.002f, 0.13f, 0.14f, 0.075f).g(P_drift, 0.0f).g(P_voiceDispersion, 0.015f)
              .contour(0.0f, -0.12f, -0.04f, 0.07f));
    add("Space Sweep", "FX", { "ring-mod", "brilliance-swell", "dual-rank" },
        "A low body and later upper pulse bloom as the ring carrier rises, then falls over several seconds. Different filter and amplitude swells make the motion unfold in layers.",
        effect().feet(I, f16).feet(II, f4).wave(I, true, false, 0.80f).wave(II, false, true, 0.12f)
                .hpf(I, 40.0f).lpf(I, 480.0f, 0.20f).lpf(II, 3400.0f, 0.16f)
                .level(I, 0.38f, 0.38f).level(II, 0.60f, 0.30f)
                .ring(31.0f, 0.86f, 2.4f, 4.5f, 0.63f).mix(0.32f)
              .pw(II, 0.62f, 0.07f, 0.19f).hpf(II, 430.0f).feg(I, 0.26f, 0.58f, 1.45f, 5.2f, 2.8f)
              .feg(II, 0.40f, 0.83f, 2.65f, 3.9f, 2.4f).eg(I, 0.43f, 5.2f, 0.86f, 3.1f).eg(II, 1.12f, 3.8f, 0.29f, 2.6f)
              .touch(II, 0.07f, 0.32f, 0.18f, 0.21f));
    add("Bubbles", "FX", { "modern", "lfo", "resonance", "noise" },
        "An 11 Hz modern sample-and-hold LFO jumps two resonant filter bands. A soft pulse supplies the hollow bubble body and a little filtered noise adds the popping edge.",
        effect().wave(I, false, true).wave(II, false, false, 0.0f, 0.18f).pw(I, 0.50f)
                .feg(Both, 0.0f, 0.0f, 0.002f, 0.5f, 0.2f)
                .level(I, 0.75f, 0.33f).level(II, 0.72f, 0.19f).touch(Both, 0.0f, 0.24f, 0.0f, 0.08f)
                .lfo(0, 11.0f, 0.70f, toFilter).g(P_lfo1Wave, 5.0f).mix(0.22f).res(0.14f)
              .hpf(I, 95.0f, 0.08f).hpf(II, 1100.0f).lpf(I, 475.0f, 0.71f).lpf(II, 2300.0f, 0.33f)
              .eg(I, 0.001f, 1.8f, 0.84f, 0.16f).eg(II, 0.001f, 0.18f, 0.16f, 0.07f).touch(I, 0.03f, 0.30f, 0.07f, 0.13f)
              .touch(II, 0.06f, 0.22f, 0.10f, 0.08f));
    add("Thunder", "FX", { "noise", "percussive", "sub-mod" },
        "A short bright noise crack precedes a much longer low filtered rumble. Slow, shallow SUB motion moves the rolling tail; velocity controls the force of the strike.",
        effect().wave(I, false, false, 0.0f, 1.0f).wave(II, false, false, 0.0f, 1.0f)
                .level(I, 1.0f, 0.70f).level(II, 1.0f, 0.48f)
                .touch(I, 0.0f, 1.0f, 0.0f, 0.0f).touch(II, 0.08f, 1.0f, 0.0f, 0.0f)
                .sub(subSine, 0.70f, 0.0f, 0.05f, 0.12f).mix(0.36f).res(0.08f)
              .hpf(I, 20.0f, 0.04f).hpf(II, 720.0f).lpf(I, 185.0f, 0.34f).lpf(II, 8700.0f, 0.065f)
              .feg(I, 0.14f, 0.62f, 0.022f, 2.7f, 2.6f).feg(II, 0.0f, 0.38f, 0.001f, 0.13f, 0.09f)
              .eg(I, 0.055f, 8.2f, 0.0f, 5.2f).eg(II, 0.001f, 0.34f, 0.0f, 0.19f).pan(I, -0.04f).pan(II, 0.10f));
    add("Rain", "FX", { "noise", "modern", "lfo" },
        "A continuous midband rain bed sits below finer upper droplets. Modern sample-and-hold changes the noise level at 16 Hz, with the quieter upper band flickering more deeply.",
        effect().wave(I, false, false, 0.0f, 1.0f).wave(II, false, false, 0.0f, 1.0f)
                .feg(Both, 0.0f, 0.0f, 0.002f, 0.5f, 0.3f)
                .level(I, 1.0f, 0.90f).level(II, 1.0f, 0.55f)
                .lfo(0, 16.0f, 0.85f, toAmp).g(P_lfo1Wave, 5.0f).mix(0.48f).res(0.02f)
              .hpf(I, 380.0f).hpf(II, 2900.0f).lpf(I, 2600.0f, 0.035f).lpf(II, 11000.0f, 0.02f).eg(I, 0.28f, 1.6f, 0.96f, 0.85f)
              .eg(II, 0.008f, 0.19f, 0.37f, 0.23f).touch(I, 0.0f, 0.40f, 0.0f, 0.12f).touch(II, 0.03f, 0.25f, 0.06f, 0.16f)
              .pan(I, -0.065f).pan(II, 0.11f));
    add("Helicopter", "FX", { "sub-mod", "noise", "feet-layer" },
        "A low 16' pulse engine and a separate rotor-noise band throb at 11.5 Hz. SUB level and filter pulses leave the underlying engine audible; pressure accelerates the rotor.",
        effect().feet(I, f16).wave(I, false, true, 0.0f, 0.18f).wave(II, false, false, 0.0f, 0.65f)
                .hpf(I, 30.0f)
                .level(I, 0.80f, 0.40f).level(II, 0.78f, 0.31f)
                .touch(I, 0.0f, 0.12f, 0.02f, 0.15f).touch(II, 0.0f, 0.10f, 0.08f, 0.18f)
                .sub(subSquare, 11.5f, 0.0f, 0.32f, 0.95f).subAt(0.14f, 0.0f, 0.04f).mix(0.42f)
              .pw(I, 0.62f).hpf(II, 360.0f).lpf(I, 330.0f, 0.23f).lpf(II, 2200.0f, 0.17f)
              .feg(I, 0.25f, 0.15f, 0.42f, 1.5f, 0.85f).feg(II, 0.17f, 0.29f, 0.18f, 0.85f, 0.50f).eg(I, 0.37f, 1.5f, 1.0f, 1.0f)
              .eg(II, 0.14f, 0.7f, 0.71f, 0.58f));
    add("UFO Landing", "FX", { "sub-mod", "ring-mod", "dual-rank" },
        "A low sine body follows repeating downward SUB glides while the shared ring carrier settles over the phrase. Release all keys and any sustain hold before the next attack to restart that sweep; a shorter upper pulse edge marks each note.",
        effect().feet(I, f16).feet(II, f4).wave(I, false, false, 0.95f).wave(II, false, true, 0.25f)
                .level(I, 0.0f, 0.45f).level(II, 0.35f, 0.28f)
                .touch(I, 0.0f, 0.28f, 0.0f, 0.12f)
                .sub(subSawDown, 0.70f, 0.90f, 0.0f, 0.0f).ring(173.0f, 0.32f, 0.004f, 3.5f, 0.34f).mix(0.26f)
              .pw(II, 0.64f).hpf(II, 480.0f).lpf(II, 2850.0f, 0.23f).feg(II, 0.05f, 0.52f, 0.004f, 0.85f, 0.53f)
              .eg(I, 0.17f, 5.0f, 0.62f, 2.65f).eg(II, 0.021f, 1.35f, 0.07f, 0.61f).touch(II, 0.13f, 0.38f, 0.10f, 0.14f)
              .contour(-0.08f, -0.14f, -0.04f, 0.07f));
    add("Telemetry", "FX", { "sub-mod", "feet-layer", "percussive" },
        "A high narrow pulse alternates pitches at 7 Hz, with a sharper 2' onset edge. Brief release and shallow level pulses keep the electronic signal dry and articulated.",
        effect().wave(Both, false, true).feet(I, f4).feet(II, f2)
                .hpf(I, 150.0f).lpf(I, 3500.0f, 0.08f).lpf(II, 6000.0f, 0.06f)
                .level(I, 0.90f, 0.42f).level(II, 0.70f, 0.30f).touch(Both, 0.0f, 0.25f, 0.0f, 0.0f)
                .sub(subSquare, 7.0f, 0.65f, 0.09f, 0.35f).mix(0.18f).res(0.04f)
              .pw(I, 0.82f).pw(II, 0.74f).hpf(II, 1400.0f).feg(I, 0.0f, 0.08f, 0.001f, 0.22f, 0.06f)
              .feg(II, 0.0f, 0.34f, 0.001f, 0.07f, 0.035f).eg(I, 0.002f, 1.4f, 0.92f, 0.06f).eg(II, 0.001f, 0.12f, 0.09f, 0.035f)
              .g(P_drift, 0.0f).g(P_voiceDispersion, 0.015f));
    add("Clockwork", "FX", { "percussive", "sub-mod", "modern", "lfo", "noise" },
        "Six-Hz modern amplitude pulses form dry mechanical ticks from a high pulse and thin noise edge. Slower SUB filter steps add a tick-tock change of color.",
        effect().wave(I, false, true).wave(II, false, false, 0.0f, 0.60f).feet(I, f4)
                .feg(Both, 0.0f, 0.0f, 0.001f, 0.5f, 0.1f)
                // Base CV stays below 0.25 at full velocity, so the negative LFO half closes both ranks.
                .level(I, 1.0f, 0.18f).level(II, 1.0f, 0.14f).touch(Both, 0.0f, 0.05f, 0.0f, 0.0f)
                .sub(subSquare, 3.0f, 0.0f, 0.20f, 0.0f)
                .lfo(0, 6.0f, 1.0f, toAmp).g(P_lfo1Wave, 4.0f).mix(0.22f).res(0.06f)
              .pw(I, 0.69f).hpf(I, 620.0f, 0.08f).hpf(II, 3400.0f).lpf(I, 3800.0f, 0.29f).lpf(II, 10000.0f, 0.025f)
              .eg(I, 0.001f, 1.1f, 0.85f, 0.045f).eg(II, 0.001f, 0.075f, 0.27f, 0.025f).g(P_voiceDispersion, 0.02f)
              .g(P_drift, 0.0f));
    add("Deep Space", "FX", { "noise", "ring-mod", "dual-rank" },
        "A very slow 16' sine bloom carries a later, cloudy noise band. Gentle sub-hertz ring beating and long independent releases leave a deep, moving tail.",
        effect().feet(I, f16).wave(I, false, false, 0.95f).wave(II, false, false, 0.0f, 0.30f)
                .level(I, 0.0f, 0.43f).level(II, 0.80f, 0.32f)
                .touch(I, 0.0f, 0.20f, 0.0f, 0.18f)
                .ring(0.37f, 0.06f, 3.0f, 5.0f, 0.40f).mix(0.22f)
              .hpf(II, 370.0f, 0.07f).lpf(II, 1370.0f, 0.29f).feg(II, 0.41f, 0.34f, 3.5f, 5.6f, 3.9f)
              .eg(I, 1.45f, 6.5f, 0.98f, 6.8f).eg(II, 3.2f, 4.2f, 0.41f, 5.7f).touch(II, 0.02f, 0.13f, 0.20f, 0.28f)
              .pan(I, -0.025f).pan(II, 0.15f));
    add("Metal Scrape", "FX", { "ring-mod", "noise", "velocity" },
        "A resonant saw-and-noise body follows a short high noise scrape. The ring carrier falls quickly after impact, adding a rough metallic edge to the closing filter bands.",
        effect().wave(I, true, false, 0.0f, 0.40f).wave(II, false, false, 0.0f, 1.0f)
                .level(I, 1.0f, 0.62f).level(II, 1.0f, 0.35f)
                .touch(I, 0.10f, 0.95f, 0.18f, 0.32f).touch(II, 0.08f, 1.0f, 0.14f, 0.18f)
                .ring(219.0f, 0.22f, 0.010f, 0.65f, 0.75f).mix(0.40f).res(0.14f)
              .hpf(I, 420.0f, 0.13f).hpf(II, 2400.0f, 0.07f).lpf(I, 2950.0f, 0.51f).lpf(II, 10500.0f, 0.075f)
              .feg(I, 0.41f, 0.56f, 0.065f, 1.45f, 0.57f).feg(II, 0.08f, 0.28f, 0.002f, 0.22f, 0.13f)
              .eg(I, 0.035f, 2.35f, 0.07f, 0.55f).eg(II, 0.0015f, 0.31f, 0.0f, 0.12f));
    add("Machine Hum", "FX", { "ring-mod", "feet-layer", "dual-rank" },
        "A steady 16' pulse motor supports a quieter upper mechanical buzz. A fixed 50 Hz ring carrier supplies sidebands, with a gradual start and a short coast after release.",
        effect().feet(I, f16).feet(II, f8).wave(I, false, true, 0.14f).wave(II, false, true, 0.0f, 0.06f)
                .pw(I, 0.50f).hpf(I, 25.0f)
                .feg(I, 0.10f, 0.08f, 0.20f, 1.0f, 0.5f)
                .touch(I, 0.0f, 0.10f, 0.0f, 0.10f).touch(II, 0.0f, 0.10f, 0.07f, 0.15f)
                .ring(50.0f, 0.0f, 0.01f, 1.0f, 0.28f).mix(0.24f).detune(0.35f)
              .pw(II, 0.61f).hpf(II, 220.0f, 0.06f).lpf(I, 280.0f, 0.09f).lpf(II, 1450.0f, 0.21f)
              .feg(II, 0.22f, 0.18f, 0.52f, 2.2f, 0.65f).eg(I, 0.21f, 1.5f, 1.0f, 1.0f).eg(II, 0.44f, 2.7f, 0.19f, 0.67f)
              .g(P_drift, 0.008f));
    add("Sweep Riser", "FX", { "brilliance-swell", "resonance", "noise" },
        "Hold a note for the five-second resonant rise: a low saw body opens across six filter octaves while a later noise layer blooms above it. The peak then settles gradually.",
        effect().wave(I, true, false, 0.0f, 0.15f).wave(II, false, false, 0.0f, 0.65f)
                .hpf(I, 30.0f)
                .feg(I, 1.0f, 1.0f, 5.0f, 6.0f, 2.0f)
                .level(I, 0.70f, 0.42f).level(II, 0.75f, 0.32f).touch(Both, 0.0f, 0.20f, 0.0f, 0.0f)
                .res(0.20f).mix(0.34f)
              .hpf(II, 1050.0f, 0.07f).lpf(I, 530.0f, 0.66f).lpf(II, 5200.0f, 0.18f).feg(II, 1.0f, 0.68f, 5.5f, 6.2f, 2.1f)
              .eg(I, 0.95f, 7.5f, 1.0f, 2.25f).eg(II, 1.85f, 6.0f, 0.72f, 1.55f).touch(II, 0.07f, 0.28f, 0.11f, 0.10f));
    add("Downer", "FX", { "brilliance-swell", "noise", "dual-rank" },
        "A bright held saw falls into a low filter setting when the key is released. The longer body release keeps that downward sweep audible after the shorter noisy upper edge fades.",
        effect().wave(I, true, false).wave(II, true, false, 0.0f, 0.20f).feet(II, f4)
                .hpf(I, 45.0f)
                .touch(I, 0.0f, 0.20f, 0.0f, 0.10f).touch(II, 0.0f, 0.15f, 0.0f, 0.08f)
                .mix(0.25f).res(0.18f)
              .pw(II, 0.61f).hpf(II, 480.0f).lpf(I, 2750.0f, 0.42f).lpf(II, 6500.0f, 0.17f)
              .feg(I, 1.0f, 0.22f, 0.003f, 0.75f, 3.3f).feg(II, 1.0f, 0.43f, 0.003f, 0.24f, 2.2f)
              .eg(I, 0.006f, 1.8f, 0.88f, 4.4f).eg(II, 0.002f, 0.42f, 0.19f, 2.65f));
    add("Glitch Sub", "FX", { "sub-mod", "resonance", "feet-layer" },
        "Thirty-Hz SUB filter steps roughen a low pulse into a digital-sounding buzz. A shorter upper saw edge marks each keystroke, while shallow level pulses reinforce the flutter.",
        effect().feet(I, f16).feet(II, f8).wave(I, false, true).wave(II, true, false)
                .hpf(I, 30.0f)
                .level(I, 0.80f, 0.40f).level(II, 0.60f, 0.30f).touch(Both, 0.0f, 0.24f, 0.0f, 0.08f)
                .sub(subSquare, 30.0f, 0.0f, 0.70f, 0.40f).mix(0.22f).res(0.12f)
              .pw(I, 0.64f).hpf(II, 420.0f).lpf(I, 385.0f, 0.61f).lpf(II, 1900.0f, 0.29f)
              .feg(I, 0.08f, 0.33f, 0.001f, 0.42f, 0.09f).feg(II, 0.10f, 0.48f, 0.001f, 0.11f, 0.055f)
              .eg(I, 0.001f, 1.05f, 0.92f, 0.085f).eg(II, 0.001f, 0.17f, 0.12f, 0.05f).g(P_drift, 0.0f));
    add("Breath FX", "FX", { "noise", "aftertouch", "dual-rank" },
        "A soft low breath band supports a faster upper-air layer. Pressure raises the airy rank more strongly and gently opens both filters, giving a controllable exhale and fade.",
        effect().wave(I, false, false, 0.0f, 1.0f).wave(II, false, false, 0.0f, 1.0f)
                .level(I, 1.0f, 0.90f).level(II, 1.0f, 0.65f)
                .touch(I, 0.0f, 0.55f, 0.14f, 0.90f).touch(II, 0.0f, 0.40f, 0.22f, 1.0f)
                .mix(0.43f).res(0.055f)
              .hpf(I, 190.0f, 0.08f).hpf(II, 1650.0f, 0.05f).lpf(I, 1120.0f, 0.26f).lpf(II, 7600.0f, 0.075f)
              .feg(I, 0.18f, 0.13f, 0.14f, 0.85f, 0.31f).feg(II, 0.08f, 0.22f, 0.018f, 0.23f, 0.12f)
              .eg(I, 0.11f, 1.1f, 0.79f, 0.42f).eg(II, 0.014f, 0.35f, 0.40f, 0.17f));
}
