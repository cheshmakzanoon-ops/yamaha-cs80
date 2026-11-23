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
        effect()
               .feet(II, f4).hpf(I, 42.0f).feg(I, 0.0f, 0.0f, 0.002f, 0.5f, 0.2f).feg(II, 0.0f, 0.0f, 0.002f, 0.5f, 0.2f)
               .level(II, 0.44f, 0.21f).touch(I, 0.035f, 0.40f, 0.15f, 0.19f).touch(II, 0.08f, 0.36f, 0.22f, 0.12f)
               .lfo(0, 7.5f, 0.65f, toFilter).g(P_lfo1Wave, 5.0f).mix(0.22f).res(0.12f).pw(II, 0.67f).hpf(II, 310.0f)
               .lpf(I, 710.0f, 0.49f).lpf(II, 2060.0f, 0.36f).eg(I, 0.004f, 1.2f, 0.92f, 0.14f)
               .eg(II, 0.002f, 0.24f, 0.19f, 0.062f).contour(-0.06f, -0.21f, -0.03f, 0.08f));
    add("Random Pitch Blips", "FX", { "modern", "lfo", "percussive" },
        "Modern sample-and-hold picks small, unquantized pitch steps around the played key at 6.5 Hz. A matching square amplitude LFO separates the sine-and-pulse tone into blips.",
        effect()
               .wave(I, false, true, 0.81f).wave(II, false, true, 0.14f).feet(II, f4).hpf(I, 28.0f).lpf(I, 2080.0f, 0.035f)
               .feg(I, 0.0f, 0.0f, 0.002f, 0.5f, 0.2f).feg(II, 0.0f, 0.0f, 0.002f, 0.5f, 0.2f).level(I, 0.75f, 0.17f)
               .level(II, 0.54f, 0.115f).touch(I, 0.0f, 0.055f, 0.0f, 0.0f).touch(II, 0.0f, 0.055f, 0.0f, 0.0f)
               .lfo(0, 6.5f, 0.95f, toPitch).g(P_lfo1Wave, 5.0f).lfo(1, 6.5f, 1.0f, toAmp).g(P_lfo2Wave, 4.0f).mix(0.17f)
               .res(0.025f).pw(I, 0.50f, 0.0f).pw(II, 0.72f).hpf(II, 540.0f).lpf(II, 5800.0f, 0.025f)
               .eg(I, 0.003f, 1.1f, 1.0f, 0.045f).eg(II, 0.0015f, 0.105f, 0.24f, 0.028f).g(P_voiceDispersion, 0.025f)
               .g(P_drift, 0.0f).contour(-0.04f, -0.19f, -0.025f, 0.075f));
    add("Siren", "FX", { "sub-mod", "sine", "aftertouch" },
        "A sine body and quiet octave harmonic follow a smooth three-semitone peak-to-peak SUB wail. Rounded onset and release avoid clicks; pressure quickens the siren.",
        effect()
               .wave(I, false, false, 1.0f).wave(II, false, false, 0.29f).feet(II, f4).level(I, 0.0f, 0.52f).level(II, 0.0f, 0.32f)
               .touch(I, 0.0f, 0.29f, 0.0f, 0.25f).touch(II, 0.0f, 0.20f, 0.0f, 0.18f).sub(subSine, 0.70f, 1.0f, 0.0f, 0.0f)
               .subAt(0.12f, 0.0f, 0.0f).mix(0.15f).eg(I, 0.21f, 2.1f, 1.0f, 0.91f).eg(II, 0.12f, 1.45f, 0.52f, 0.57f)
               .contour(0.0f, 0.0f, -0.075f, 0.095f).g(P_voiceDispersion, 0.02f).g(P_drift, 0.0f));
    add("Laser Zap", "FX", { "resonance", "noise", "percussive" },
        "A resonant pulse sweep drops sharply after the strike, with no bypass sine masking the descent. A separate, very short noise flash adds the laser's initial snap.",
        effect()
               .wave(I, false, true).pw(I, 0.52f).wave(II, false, false, 0.0f, 0.25f).hpf(I, 26.0f).level(I, 0.78f, 0.24f)
               .level(II, 0.65f, 0.10f).touch(I, 0.035f, 0.78f, 0.0f, 0.0f).touch(II, 0.10f, 0.96f, 0.0f, 0.0f).res(0.24f)
               .mix(0.135f).lpf(I, 165.0f, 0.72f).hpf(II, 2200.0f).lpf(II, 11800.0f, 0.025f)
               .feg(I, 0.0f, 1.0f, 0.001f, 0.255f, 0.14f).feg(II, 0.0f, 0.18f, 0.001f, 0.021f, 0.018f)
               .eg(I, 0.001f, 0.86f, 0.0f, 0.26f).eg(II, 0.001f, 0.051f, 0.0f, 0.025f).g(P_drift, 0.0f)
               .contour(-0.08f, -0.22f, -0.03f, 0.07f));
    add("Wind", "FX", { "noise", "sub-mod", "aftertouch" },
        "A broad low wind bed supports a separate airy gust that arrives later. Shallow SUB motion avoids a regular throbbing cycle; pressure brings forward the upper air and the unequal releases let it subside.",
        effect()
               .wave(I, false, false, 0.0f, 1.0f).wave(II, false, false, 0.0f, 0.85f).level(I, 1.0f, 0.72f).level(II, 0.95f, 0.50f)
               .touch(I, 0.0f, 0.24f, 0.10f, 0.43f).touch(II, 0.0f, 0.16f, 0.18f, 0.59f).sub(subSine, 0.70f, 0.0f, 0.025f, 0.035f)
               .mix(0.35f).hpf(I, 43.0f, 0.02f).hpf(II, 940.0f, 0.015f).lpf(I, 1040.0f, 0.13f).lpf(II, 6300.0f, 0.025f)
               .feg(I, 0.24f, 0.14f, 2.3f, 6.4f, 3.5f).feg(II, 0.31f, 0.32f, 1.05f, 3.7f, 2.4f).eg(I, 1.65f, 6.2f, 0.96f, 4.6f)
               .eg(II, 0.71f, 3.9f, 0.61f, 2.85f).pan(I, -0.035f).pan(II, 0.085f).contour(0.20f, -0.48f, -0.035f, 0.08f));
    add("Surf", "FX", { "noise", "sub-mod", "dual-rank" },
        "One long breaking wave unfolds per note: the low surge rises first, then brighter spray blooms and drains away. Broad noise bands, unequal decays and very shallow SUB motion keep the wash from repeating like a short loop.",
        effect()
               .wave(I, false, false, 0.0f, 1.0f).wave(II, false, false, 0.0f, 0.90f).level(I, 1.0f, 0.74f).level(II, 1.0f, 0.64f)
               .touch(I, 0.0f, 0.27f, 0.0f, 0.20f).touch(II, 0.0f, 0.36f, 0.0f, 0.25f).sub(subSine, 0.70f, 0.0f, 0.025f, 0.035f)
               .mix(0.54f).res(0.035f).hpf(I, 32.0f, 0.015f).hpf(II, 850.0f).lpf(I, 930.0f, 0.035f).lpf(II, 8200.0f, 0.02f)
               .feg(I, 0.25f, 0.18f, 1.8f, 6.7f, 3.1f).feg(II, 0.48f, 0.49f, 2.4f, 3.5f, 1.8f).eg(I, 1.35f, 8.5f, 0.0f, 3.4f)
               .eg(II, 1.9f, 3.8f, 0.0f, 1.9f).pan(I, -0.045f).pan(II, 0.075f).contour(0.17f, -0.40f, -0.045f, 0.07f));
    add("Sci-Fi Drone", "FX", { "ring-mod", "feet-layer", "sub-mod" },
        "A dark 16' saw-and-sine foundation supports a later third-harmonic pulse. Slow ring beating and gentle filter drift animate the held drone without a sharp attack.",
        effect()
               .feet(I, f16).feet(II, f513).wave(I, true, false, 0.33f).wave(II, false, true, 0.08f).hpf(I, 23.0f)
               .lpf(I, 540.0f, 0.12f).eg(I, 1.35f, 4.3f, 0.96f, 4.2f).sub(subSine, 0.70f, 0.015f, 0.07f, 0.05f)
               .ring(3.1f, 0.12f, 2.8f, 5.2f, 0.38f).mix(0.27f).detune(0.8f).pw(II, 0.63f, 0.065f, 0.09f).hpf(II, 420.0f)
               .lpf(II, 1750.0f, 0.24f).feg(I, 0.28f, 0.21f, 1.85f, 6.1f, 3.7f).feg(II, 0.47f, 0.36f, 3.6f, 6.2f, 3.3f)
               .eg(II, 2.95f, 6.3f, 0.47f, 4.4f).touch(II, 0.06f, 0.27f, 0.28f, 0.31f).touch(I, 0.02f, 0.24f, 0.10f, 0.18f)
               .contour(-0.12f, -0.25f, -0.05f, 0.06f));
    add("Robot Voice", "FX", { "ring-mod", "resonance", "aftertouch" },
        "A hollow pulse band and a brighter saw band close after a short consonant-like onset. A 73 Hz ring carrier adds robotic buzz; pressure opens the two bands.",
        effect()
               .wave(I, false, true).wave(II, true, false).eg(I, 0.012f, 0.83f, 0.68f, 0.22f).level(I, 0.65f, 0.90f)
               .level(II, 0.50f, 0.70f).touch(I, 0.05f, 0.62f, 0.24f, 0.38f).touch(II, 0.10f, 0.86f, 0.31f, 0.29f)
               .ring(73.0f, 0.0f, 0.01f, 0.8f, 0.74f).mix(0.28f).res(0.10f).pw(I, 0.535f).hpf(I, 255.0f, 0.15f)
               .hpf(II, 1350.0f, 0.18f).lpf(I, 1150.0f, 0.43f).lpf(II, 2900.0f, 0.40f).feg(I, 0.18f, 0.38f, 0.071f, 0.57f, 0.21f)
               .feg(II, 0.09f, 0.43f, 0.019f, 0.12f, 0.085f).eg(II, 0.005f, 0.24f, 0.16f, 0.078f)
               .contour(0.16f, -0.31f, -0.04f, 0.08f));
    add("Alarm", "FX", { "sub-mod", "feet-layer" },
        "A clear 4' call alternates between two pitches three semitones apart. A quieter low pulse gives the onset weight while square SUB level motion accents the two-tone warning.",
        effect()
               .wave(I, false, true, 0.81f).wave(II, false, true).feet(I, f4).feet(II, f8).pw(I, 0.50f).pw(II, 0.58f).hpf(I, 52.0f)
               .feg(I, 0.0f, 0.0f, 0.002f, 0.5f, 0.15f).feg(II, 0.0f, 0.0f, 0.002f, 0.5f, 0.15f).level(I, 0.30f, 0.48f)
               .level(II, 0.55f, 0.31f).touch(I, 0.0f, 0.20f, 0.0f, 0.0f).touch(II, 0.0f, 0.20f, 0.0f, 0.0f)
               .sub(subSquare, 2.8f, 1.0f, 0.0f, 0.18f).mix(0.18f).res(0.025f).hpf(II, 135.0f).lpf(I, 4100.0f, 0.025f)
               .lpf(II, 1780.0f, 0.035f).eg(I, 0.016f, 1.2f, 1.0f, 0.095f).eg(II, 0.003f, 0.105f, 0.11f, 0.052f).g(P_drift, 0.0f)
               .g(P_voiceDispersion, 0.015f).contour(0.0f, -0.17f, -0.045f, 0.085f));
    add("Space Sweep", "FX", { "ring-mod", "brilliance-swell", "dual-rank" },
        "A low body and later upper pulse bloom as the ring carrier rises, then falls over several seconds. Different filter and amplitude swells make the motion unfold in layers.",
        effect()
               .feet(I, f16).feet(II, f4).wave(I, true, false, 0.69f).wave(II, false, true, 0.12f).hpf(I, 32.0f)
               .lpf(I, 610.0f, 0.16f).lpf(II, 4100.0f, 0.12f).level(I, 0.38f, 0.38f).level(II, 0.60f, 0.30f)
               .ring(31.0f, 0.86f, 2.4f, 4.5f, 0.63f).mix(0.28f).pw(II, 0.66f, 0.045f, 0.145f).hpf(II, 580.0f)
               .feg(I, 0.31f, 0.49f, 1.9f, 5.9f, 3.3f).feg(II, 0.49f, 0.78f, 3.1f, 4.5f, 2.7f).eg(I, 0.58f, 6.1f, 0.91f, 3.7f)
               .eg(II, 1.4f, 4.5f, 0.22f, 2.9f).touch(II, 0.08f, 0.39f, 0.22f, 0.24f).touch(I, 0.025f, 0.30f, 0.08f, 0.17f)
               .contour(-0.11f, -0.26f, -0.04f, 0.08f));
    add("Bubbles", "FX", { "modern", "lfo", "resonance", "noise" },
        "An 11 Hz modern sample-and-hold LFO jumps two resonant filter bands. A soft pulse supplies the hollow bubble body and a little filtered noise adds the popping edge.",
        effect()
               .wave(I, false, true).wave(II, false, false, 0.0f, 0.18f).pw(I, 0.53f).feg(I, 0.0f, 0.0f, 0.002f, 0.5f, 0.2f)
               .feg(II, 0.0f, 0.0f, 0.002f, 0.5f, 0.2f).level(I, 0.75f, 0.33f).level(II, 0.72f, 0.19f)
               .touch(I, 0.04f, 0.35f, 0.10f, 0.16f).touch(II, 0.075f, 0.31f, 0.13f, 0.095f).lfo(0, 11.0f, 0.70f, toFilter)
               .g(P_lfo1Wave, 5.0f).mix(0.19f).res(0.11f).hpf(I, 82.0f, 0.045f).hpf(II, 1350.0f).lpf(I, 560.0f, 0.66f)
               .lpf(II, 2850.0f, 0.26f).eg(I, 0.002f, 2.1f, 0.92f, 0.13f).eg(II, 0.001f, 0.13f, 0.11f, 0.052f)
               .lfo(1, 2.3f, 0.16f, toAmp).g(P_lfo2Wave, 6.0f).contour(0.06f, -0.24f, -0.03f, 0.075f));
    add("Thunder", "FX", { "noise", "percussive", "sub-mod" },
        "A brief broadband crack leads into a slower low rumble with a long falling tail. Reduced resonance avoids a fixed pitched boom; shallow SUB movement roughens the roll and velocity controls the impact.",
        effect()
               .wave(I, false, false, 0.0f, 1.0f).wave(II, false, false, 0.0f, 1.0f).level(I, 1.0f, 0.66f).level(II, 1.0f, 0.42f)
               .touch(I, 0.025f, 1.0f, 0.0f, 0.0f).touch(II, 0.11f, 1.0f, 0.0f, 0.0f).sub(subSine, 0.70f, 0.0f, 0.025f, 0.045f)
               .mix(0.32f).res(0.04f).hpf(I, 20.0f, 0.02f).hpf(II, 910.0f).lpf(I, 270.0f, 0.13f).lpf(II, 11100.0f, 0.025f)
               .feg(I, 0.22f, 0.39f, 0.072f, 3.8f, 3.1f).feg(II, 0.0f, 0.26f, 0.001f, 0.085f, 0.055f)
               .eg(I, 0.081f, 8.8f, 0.0f, 6.0f).eg(II, 0.001f, 0.225f, 0.0f, 0.115f).pan(I, -0.025f).pan(II, 0.075f)
               .contour(0.12f, -0.50f, -0.075f, 0.04f));
    add("Rain", "FX", { "noise", "modern", "lfo" },
        "A continuous rain bed supports fine upper droplets. Sixteen-Hz random level changes combine with slower, smoothly changing filter and intensity fluctuations, avoiding one obvious repeating pulse.",
        effect()
               .wave(I, false, false, 0.0f, 1.0f).wave(II, false, false, 0.0f, 1.0f).feg(I, 0.035f, 0.06f, 0.42f, 3.1f, 1.0f)
               .feg(II, 0.0f, 0.13f, 0.002f, 0.12f, 0.15f).level(I, 1.0f, 0.91f).level(II, 1.0f, 0.58f).lfo(0, 16.0f, 0.52f, toAmp)
               .g(P_lfo1Wave, 5.0f).mix(0.44f).res(0.02f).hpf(I, 460.0f).hpf(II, 3200.0f).lpf(I, 3100.0f, 0.02f)
               .lpf(II, 12500.0f, 0.01f).eg(I, 0.43f, 2.7f, 1.0f, 1.15f).eg(II, 0.012f, 0.28f, 0.51f, 0.36f)
               .touch(I, 0.0f, 0.32f, 0.0f, 0.18f).touch(II, 0.04f, 0.28f, 0.075f, 0.20f).pan(I, -0.045f).pan(II, 0.10f)
               .lfo(1, 0.23f, 0.085f, toFilter).g(P_lfo2Wave, 6.0f).lfo(2, 3.7f, 0.12f, toAmp).g(P_lfo3Wave, 6.0f)
               .contour(0.11f, -0.36f, -0.04f, 0.08f));
    add("Helicopter", "FX", { "sub-mod", "noise", "feet-layer" },
        "A low 16' pulse engine and a separate rotor-noise band throb at 11.5 Hz. SUB level and filter pulses leave the underlying engine audible; pressure accelerates the rotor.",
        effect()
               .feet(I, f16).wave(I, false, true, 0.0f, 0.18f).wave(II, false, false, 0.0f, 0.65f).hpf(I, 24.0f)
               .level(I, 0.85f, 0.42f).level(II, 0.81f, 0.28f).touch(I, 0.0f, 0.12f, 0.02f, 0.15f)
               .touch(II, 0.02f, 0.14f, 0.12f, 0.23f).sub(subSquare, 11.5f, 0.0f, 0.32f, 0.95f).subAt(0.14f, 0.0f, 0.04f)
               .mix(0.37f).pw(I, 0.57f).hpf(II, 480.0f).lpf(I, 420.0f, 0.16f).lpf(II, 2800.0f, 0.11f)
               .feg(I, 0.20f, 0.12f, 0.61f, 2.1f, 1.1f).feg(II, 0.23f, 0.34f, 0.26f, 1.2f, 0.66f).eg(I, 0.52f, 2.2f, 1.0f, 1.3f)
               .eg(II, 0.23f, 1.1f, 0.77f, 0.75f).contour(-0.10f, -0.26f, -0.055f, 0.045f));
    add("UFO Landing", "FX", { "sub-mod", "ring-mod", "dual-rank" },
        "A low sine body follows repeating downward SUB glides while the shared ring carrier settles over the phrase. Release all keys and any sustain hold before the next attack to restart that sweep; a shorter upper pulse edge marks each note.",
        effect()
               .feet(I, f16).feet(II, f4).wave(I, false, false, 0.95f).wave(II, false, true, 0.25f).level(I, 0.0f, 0.45f)
               .level(II, 0.31f, 0.24f).touch(I, 0.0f, 0.32f, 0.0f, 0.15f).sub(subSawDown, 0.70f, 0.90f, 0.0f, 0.0f)
               .ring(173.0f, 0.32f, 0.004f, 3.5f, 0.34f).mix(0.22f).pw(II, 0.69f).hpf(II, 680.0f).lpf(II, 3500.0f, 0.18f)
               .feg(II, 0.08f, 0.43f, 0.007f, 1.15f, 0.64f).eg(I, 0.24f, 6.1f, 0.72f, 3.3f).eg(II, 0.034f, 0.97f, 0.035f, 0.42f)
               .touch(II, 0.16f, 0.44f, 0.13f, 0.17f).contour(-0.12f, -0.22f, -0.045f, 0.085f));
    add("Telemetry", "FX", { "sub-mod", "feet-layer", "percussive" },
        "A high narrow pulse alternates pitches at 7 Hz, with a sharper 2' onset edge. Brief release and shallow level pulses keep the electronic signal dry and articulated.",
        effect()
               .wave(I, false, true).wave(II, false, true).feet(I, f4).feet(II, f2).hpf(I, 190.0f).lpf(I, 4300.0f, 0.045f)
               .lpf(II, 7600.0f, 0.035f).level(I, 0.90f, 0.42f).level(II, 0.63f, 0.23f).touch(I, 0.0f, 0.25f, 0.0f, 0.0f)
               .touch(II, 0.0f, 0.25f, 0.0f, 0.0f).sub(subSquare, 7.0f, 0.65f, 0.09f, 0.35f).mix(0.155f).res(0.04f).pw(I, 0.78f)
               .pw(II, 0.69f).hpf(II, 1800.0f).feg(I, 0.0f, 0.12f, 0.001f, 0.18f, 0.045f)
               .feg(II, 0.0f, 0.28f, 0.001f, 0.046f, 0.025f).eg(I, 0.003f, 1.6f, 0.98f, 0.046f)
               .eg(II, 0.001f, 0.086f, 0.055f, 0.022f).g(P_drift, 0.0f).g(P_voiceDispersion, 0.015f)
               .contour(-0.02f, -0.24f, -0.035f, 0.09f));
    add("Clockwork", "FX", { "percussive", "sub-mod", "modern", "lfo", "noise" },
        "Six-Hz modern amplitude pulses form dry mechanical ticks from a high pulse and thin noise edge. Slower SUB filter steps add a tick-tock change of color.",
        effect()
               .wave(I, false, true).wave(II, false, false, 0.0f, 0.60f).feet(I, f4).feg(I, 0.0f, 0.0f, 0.001f, 0.5f, 0.1f)
               .feg(II, 0.0f, 0.0f, 0.001f, 0.5f, 0.1f).level(I, 1.0f, 0.175f).level(II, 1.0f, 0.125f)
               .touch(I, 0.0f, 0.05f, 0.0f, 0.0f).touch(II, 0.0f, 0.05f, 0.0f, 0.0f).sub(subSquare, 3.0f, 0.0f, 0.20f, 0.0f)
               .lfo(0, 6.0f, 1.0f, toAmp).g(P_lfo1Wave, 4.0f).mix(0.18f).res(0.04f).pw(I, 0.72f).hpf(I, 770.0f, 0.04f)
               .hpf(II, 4100.0f).lpf(I, 4600.0f, 0.22f).lpf(II, 11500.0f, 0.015f).eg(I, 0.0015f, 1.3f, 0.91f, 0.032f)
               .eg(II, 0.001f, 0.054f, 0.21f, 0.018f).g(P_voiceDispersion, 0.02f).g(P_drift, 0.0f)
               .contour(0.03f, -0.22f, -0.02f, 0.08f));
    add("Deep Space", "FX", { "noise", "ring-mod", "dual-rank" },
        "A very slow 16' sine bloom carries a later, cloudy noise band. Gentle sub-hertz ring beating and long independent releases leave a deep, moving tail.",
        effect()
               .feet(I, f16).wave(I, false, false, 0.95f).wave(II, false, false, 0.0f, 0.30f).level(I, 0.0f, 0.43f)
               .level(II, 0.76f, 0.29f).touch(I, 0.0f, 0.23f, 0.0f, 0.21f).ring(0.37f, 0.06f, 3.0f, 5.0f, 0.40f).mix(0.18f)
               .hpf(II, 460.0f, 0.035f).lpf(II, 1700.0f, 0.18f).feg(II, 0.48f, 0.26f, 4.3f, 6.6f, 4.8f)
               .eg(I, 1.85f, 7.5f, 1.0f, 7.8f).eg(II, 3.8f, 5.5f, 0.34f, 6.5f).touch(II, 0.025f, 0.17f, 0.23f, 0.33f)
               .pan(I, -0.015f).pan(II, 0.11f).contour(0.03f, -0.28f, -0.065f, 0.08f));
    add("Metal Scrape", "FX", { "ring-mod", "noise", "velocity" },
        "A resonant saw-and-noise body follows a short high noise scrape. The ring carrier falls quickly after impact, adding a rough metallic edge to the closing filter bands.",
        effect()
               .wave(I, true, false, 0.0f, 0.40f).wave(II, false, false, 0.0f, 1.0f).level(I, 1.0f, 0.62f).level(II, 1.0f, 0.35f)
               .touch(I, 0.13f, 0.92f, 0.23f, 0.37f).touch(II, 0.11f, 1.0f, 0.18f, 0.20f).ring(219.0f, 0.22f, 0.010f, 0.65f, 0.75f)
               .mix(0.34f).res(0.10f).hpf(I, 510.0f, 0.08f).hpf(II, 3100.0f, 0.03f).lpf(I, 3600.0f, 0.41f)
               .lpf(II, 12200.0f, 0.045f).feg(I, 0.34f, 0.48f, 0.095f, 1.85f, 0.69f).feg(II, 0.05f, 0.21f, 0.003f, 0.17f, 0.095f)
               .eg(I, 0.051f, 2.8f, 0.035f, 0.71f).eg(II, 0.002f, 0.23f, 0.0f, 0.085f).contour(-0.02f, -0.29f, -0.035f, 0.08f));
    add("Machine Hum", "FX", { "ring-mod", "feet-layer", "dual-rank" },
        "A steady 16' pulse motor supports a quieter upper mechanical buzz. A fixed 50 Hz ring carrier supplies sidebands, with a gradual start and a short coast after release.",
        effect()
               .feet(I, f16).feet(II, f8).wave(I, false, true, 0.23f).wave(II, false, true, 0.0f, 0.06f).pw(I, 0.50f).hpf(I, 22.0f)
               .feg(I, 0.12f, 0.06f, 0.34f, 1.6f, 0.69f).touch(I, 0.0f, 0.10f, 0.0f, 0.10f).touch(II, 0.0f, 0.10f, 0.07f, 0.15f)
               .ring(50.0f, 0.0f, 0.01f, 1.0f, 0.28f).mix(0.20f).detune(0.20f).pw(II, 0.58f).hpf(II, 290.0f, 0.03f)
               .lpf(I, 350.0f, 0.06f).lpf(II, 1840.0f, 0.16f).feg(II, 0.29f, 0.14f, 0.74f, 2.9f, 0.83f)
               .eg(I, 0.32f, 2.0f, 1.0f, 1.28f).eg(II, 0.61f, 3.1f, 0.24f, 0.85f).g(P_drift, 0.008f)
               .contour(-0.12f, -0.22f, -0.065f, 0.06f));
    add("Sweep Riser", "FX", { "brilliance-swell", "resonance", "noise" },
        "Hold a note for the five-second resonant rise: a low saw body opens across six filter octaves while a later noise layer blooms above it. The peak then settles gradually.",
        effect()
               .wave(I, true, false, 0.0f, 0.15f).wave(II, false, false, 0.0f, 0.65f).hpf(I, 27.0f)
               .feg(I, 1.0f, 1.0f, 5.0f, 6.8f, 2.4f).level(I, 0.70f, 0.42f).level(II, 0.75f, 0.32f)
               .touch(I, 0.025f, 0.24f, 0.035f, 0.08f).touch(II, 0.09f, 0.34f, 0.15f, 0.14f).res(0.16f).mix(0.29f)
               .hpf(II, 1350.0f, 0.035f).lpf(I, 610.0f, 0.59f).lpf(II, 6400.0f, 0.12f).feg(II, 1.0f, 0.74f, 5.5f, 5.8f, 1.8f)
               .eg(I, 1.15f, 8.6f, 1.0f, 2.7f).eg(II, 2.2f, 6.9f, 0.65f, 1.9f).contour(-0.09f, -0.24f, -0.045f, 0.075f));
    add("Downer", "FX", { "brilliance-swell", "noise", "dual-rank" },
        "A bright held saw falls into a low filter setting when the key is released. The longer body release keeps that downward sweep audible after the shorter noisy upper edge fades.",
        effect()
               .wave(I, true, false).wave(II, true, false, 0.0f, 0.20f).feet(II, f4).hpf(I, 36.0f)
               .touch(I, 0.02f, 0.28f, 0.0f, 0.13f).touch(II, 0.04f, 0.24f, 0.0f, 0.10f).mix(0.21f).res(0.14f).pw(II, 0.61f)
               .hpf(II, 650.0f).lpf(I, 3300.0f, 0.35f).lpf(II, 7600.0f, 0.11f).feg(I, 1.0f, 0.18f, 0.005f, 0.95f, 3.8f)
               .feg(II, 1.0f, 0.34f, 0.003f, 0.19f, 2.5f).eg(I, 0.009f, 2.3f, 0.94f, 5.1f).eg(II, 0.003f, 0.31f, 0.13f, 2.2f)
               .contour(-0.07f, -0.23f, -0.035f, 0.08f));
    add("Glitch Sub", "FX", { "sub-mod", "resonance", "feet-layer" },
        "Thirty-Hz SUB filter steps roughen a low pulse into a digital-sounding buzz. A shorter upper saw edge marks each keystroke, while shallow level pulses reinforce the flutter.",
        effect()
               .feet(I, f16).feet(II, f8).wave(I, false, true).wave(II, true, false).hpf(I, 24.0f).level(I, 0.80f, 0.40f)
               .level(II, 0.60f, 0.30f).touch(I, 0.015f, 0.32f, 0.04f, 0.12f).touch(II, 0.06f, 0.41f, 0.08f, 0.09f)
               .sub(subSquare, 30.0f, 0.0f, 0.70f, 0.40f).mix(0.18f).res(0.09f).pw(I, 0.59f).hpf(II, 570.0f).lpf(I, 470.0f, 0.52f)
               .lpf(II, 2350.0f, 0.22f).feg(I, 0.06f, 0.27f, 0.002f, 0.55f, 0.11f).feg(II, 0.08f, 0.40f, 0.001f, 0.08f, 0.038f)
               .eg(I, 0.002f, 1.4f, 0.96f, 0.068f).eg(II, 0.001f, 0.12f, 0.08f, 0.035f).g(P_drift, 0.0f)
               .contour(-0.13f, -0.22f, -0.06f, 0.06f));
    add("Breath FX", "FX", { "noise", "aftertouch", "dual-rank" },
        "A soft low breath band supports a faster upper-air layer. Pressure raises the airy rank more strongly and gently opens both filters, giving a controllable exhale and fade.",
        effect()
               .wave(I, false, false, 0.0f, 1.0f).wave(II, false, false, 0.0f, 1.0f).level(I, 1.0f, 0.82f).level(II, 1.0f, 0.54f)
               .touch(I, 0.0f, 0.43f, 0.11f, 0.88f).touch(II, 0.0f, 0.31f, 0.19f, 1.0f).mix(0.37f).res(0.035f)
               .hpf(I, 240.0f, 0.045f).hpf(II, 1900.0f, 0.025f).lpf(I, 1380.0f, 0.18f).lpf(II, 8700.0f, 0.035f)
               .feg(I, 0.15f, 0.10f, 0.23f, 1.2f, 0.42f).feg(II, 0.05f, 0.17f, 0.029f, 0.32f, 0.18f)
               .eg(I, 0.18f, 1.6f, 0.85f, 0.62f).eg(II, 0.023f, 0.48f, 0.47f, 0.25f).contour(0.13f, -0.39f, -0.04f, 0.08f));
}
