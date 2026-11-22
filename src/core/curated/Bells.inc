// Bells voicings. Included inside namespace curated by CuratedLibrary.inc.
// Existing program names and insertion order are stable host/MIDI identities.
// Rank I carries the resonating body; rank II usually supplies the shorter
// impact or upper mode. Sine bypasses the filters, so only the pulse/saw part
// of a strike is brightened by its filter envelope or initial brilliance.

Program bell()
{
    Program x;
    x.wave(I, false, false, 1.0f).wave(II, false, true, 0.38f).pw(II, 0.54f)
     .feet(I, f8).feet(II, f4).hpf(I, 30.0f).hpf(II, 120.0f)
     .lpf(I, 6000.0f, 0.0f).lpf(II, 4100.0f, 0.10f)
     .feg(I, 0.0f, 0.0f, 0.002f, 0.5f, 0.4f).feg(II, 0.0f, 0.28f, 0.001f, 0.19f, 0.16f)
     .eg(I, 0.003f, 3.8f, 0.0f, 2.8f).eg(II, 0.001f, 0.55f, 0.0f, 0.30f)
     .level(I, 0.0f, 0.22f).level(II, 0.34f, 0.10f)
     .touch(I, 0.0f, 0.72f, 0.0f, 0.0f).touch(II, 0.12f, 0.82f, 0.0f, 0.0f)
     .sub(subSine, 5.0f, 0.0f, 0.0f, 0.0f).subAt(0.0f, 0.0f, 0.0f)
     .ring(307.0f, 0.0f, 0.001f, 0.8f, 0.36f).mix(0.30f).detune(0.0f).res(0.035f)
     .g(P_voiceDispersion, 0.06f).g(P_drift, 0.015f).g(P_analogNoise, 0.0f).volume(0.6f);
    return x;
}

void buildBells(Builder& add)
{
    add("Ring Bell I", "Bells", { "ring-mod", "sine", "velocity", "dual-rank" },
        "A clear 8' sine body rings beneath a brief octave strike. A fixed 307 Hz ring carrier adds inharmonic sidebands; harder playing brings out the impact.",
        bell().eg(I, 0.003f, 4.2f, 0.0f, 3.1f).eg(II, 0.001f, 0.48f, 0.0f, 0.26f)
              .ring(307.0f, 0.0f, 0.001f, 0.8f, 0.42f).mix(0.28f));
    add("Ring Bell II", "Bells", { "ring-mod", "feet-layer", "velocity" },
        "A warmer bell body with a sharp 2 2/3' upper mode. Its lower ring carrier and shorter, brighter impact distinguish it from Ring Bell I.",
        bell().feet(II, f223).wave(II, false, true, 0.55f).lpf(II, 5600.0f, 0.13f)
              .eg(I, 0.002f, 3.4f, 0.0f, 2.5f).eg(II, 0.001f, 0.34f, 0.0f, 0.20f)
              .ring(191.0f, 0.0f, 0.001f, 0.7f, 0.48f).level(II, 0.27f, 0.08f).mix(0.36f));
    add("Tubular Bells", "Bells", { "ring-mod", "feet-layer", "dual-rank" },
        "A long 16' tube body follows a bright, quickly fading upper strike. Moderate ring sidebands add metal while the dry low mode keeps the note grounded.",
        bell().feet(I, f16).feet(II, f223).wave(II, false, true, 0.66f)
              .hpf(II, 260.0f).lpf(II, 5600.0f, 0.08f).level(II, 0.22f, 0.10f)
              .eg(I, 0.004f, 6.0f, 0.0f, 4.8f).eg(II, 0.001f, 0.72f, 0.0f, 0.48f)
              .ring(137.0f, 0.0f, 0.001f, 1.0f, 0.36f).mix(0.34f));
    add("Glockenspiel", "Bells", { "ring-mod", "feet-layer", "percussive" },
        "A bright 4' metal bar sustains after a hard 2' attack. The two decay times preserve a clear struck pitch beneath the high ring shimmer.",
        bell().feet(I, f4).feet(II, f2).wave(II, false, true, 0.58f)
              .hpf(II, 350.0f).lpf(II, 6900.0f, 0.07f).level(II, 0.23f, 0.07f)
              .eg(I, 0.0015f, 3.1f, 0.0f, 2.1f).eg(II, 0.001f, 0.24f, 0.0f, 0.15f)
              .ring(463.0f, 0.0f, 0.001f, 0.6f, 0.24f).mix(0.29f));
    add("Celeste Bells", "Bells", { "ring-mod", "chorus", "velocity" },
        "A rounded sine body and soft octave hammer suggest a celeste. Light ring color and restrained chorus leave a delicate, stable center to the note.",
        bell().wave(I, false, false, 0.90f).wave(II, false, true, 0.46f)
              .lpf(II, 2400.0f, 0.06f).feg(II, 0.0f, 0.15f, 0.003f, 0.18f, 0.15f)
              .level(II, 0.14f, 0.09f).eg(I, 0.006f, 3.2f, 0.0f, 2.2f).eg(II, 0.003f, 0.46f, 0.0f, 0.25f)
              .ring(211.0f, 0.0f, 0.001f, 0.7f, 0.16f).chorus(0.13f, 0.30f).mix(0.25f));
    add("Music Box", "Bells", { "ring-mod", "percussive", "velocity" },
        "A small 4' tine keeps a short, pure tail after a tiny 2' pluck. A trace of ring modulation supplies metallic color without obscuring the melody.",
        bell().feet(I, f4).feet(II, f2).wave(II, false, true, 0.30f).pw(II, 0.61f)
              .lpf(II, 3900.0f, 0.08f).feg(II, 0.0f, 0.20f, 0.001f, 0.10f, 0.09f)
              .level(II, 0.18f, 0.06f).eg(I, 0.002f, 2.4f, 0.0f, 1.4f).eg(II, 0.001f, 0.18f, 0.0f, 0.11f)
              .ring(419.0f, 0.0f, 0.001f, 0.5f, 0.12f).mix(0.23f));
    add("Gong", "Bells", { "ring-mod", "noise", "dual-rank" },
        "A deep sine mode blooms under a noisy upper clang. The ring carrier settles after impact, and the shorter metal layer recedes into a long low resonance.",
        bell().feet(I, f16).feet(II, f4).wave(II, true, false, 0.18f, 0.08f)
              .hpf(II, 220.0f).lpf(II, 3300.0f, 0.24f).feg(II, 0.0f, 0.52f, 0.008f, 0.70f, 0.55f)
              .level(II, 0.50f, 0.11f).eg(I, 0.025f, 6.8f, 0.0f, 5.5f).eg(II, 0.008f, 1.8f, 0.0f, 1.1f)
              .ring(53.0f, 0.24f, 0.025f, 3.2f, 0.62f).mix(0.42f).res(0.08f));
    add("Temple Bell", "Bells", { "ring-mod", "feet-layer", "dual-rank" },
        "A deep 16' bell with a clear upper mode and a slowly settling ring carrier. The upper strike fades early, leaving a calm, long low tail.",
        bell().feet(I, f16).feet(II, f513).wave(II, false, true, 0.72f)
              .lpf(II, 2500.0f, 0.12f).level(II, 0.16f, 0.11f)
              .eg(I, 0.007f, 6.2f, 0.0f, 4.9f).eg(II, 0.002f, 1.15f, 0.0f, 0.72f)
              .ring(83.0f, 0.16f, 0.008f, 2.3f, 0.48f).mix(0.32f));
    add("Marimba", "Bells", { "percussive", "velocity", "dual-rank" },
        "An unmodulated sine body is struck by a short, filtered fourth-harmonic pulse. The upper wooden knock dies well before the rounded body; velocity emphasizes the mallet.",
        bell().feet(II, f2).wave(II, false, true, 0.18f).pw(II, 0.53f)
              .hpf(II, 180.0f).lpf(II, 3100.0f, 0.10f).feg(II, 0.0f, 0.25f, 0.001f, 0.13f, 0.10f)
              .level(II, 0.44f, 0.07f).touch(II, 0.16f, 0.92f, 0.0f, 0.0f)
              .eg(I, 0.003f, 2.7f, 0.0f, 1.5f).eg(II, 0.001f, 0.22f, 0.0f, 0.14f)
              .ring(307.0f, 0.0f, 0.001f, 0.8f, 0.0f).mix(0.29f));
    add("Vibraphone", "Bells", { "sub-mod", "sine", "dual-rank" },
        "A long sine bar and shorter octave overtone share smooth 5.2 Hz SUB tremolo. The amplitude motion leaves a continuous ringing body between its crests.",
        bell().wave(II, false, false, 0.68f).level(II, 0.0f, 0.11f)
              .touch(II, 0.0f, 0.78f, 0.0f, 0.0f)
              .eg(I, 0.003f, 4.8f, 0.0f, 3.2f).eg(II, 0.001f, 1.15f, 0.0f, 0.72f)
              .ring(307.0f, 0.0f, 0.001f, 0.8f, 0.0f).sub(subSine, 5.2f, 0.0f, 0.0f, 0.43f).mix(0.27f));
    add("Xylophone", "Bells", { "percussive", "velocity", "feet-layer" },
        "A bright 4' wooden body follows a very brief 2' knock. No ring or vibrato is added, so fast lines keep a dry, clearly pitched attack.",
        bell().feet(I, f4).feet(II, f2).wave(II, false, true, 0.15f).pw(II, 0.56f)
              .lpf(II, 5200.0f, 0.12f).feg(II, 0.0f, 0.36f, 0.001f, 0.09f, 0.08f)
              .level(II, 0.47f, 0.07f).eg(I, 0.001f, 1.65f, 0.0f, 0.95f).eg(II, 0.001f, 0.19f, 0.0f, 0.12f)
              .ring(307.0f, 0.0f, 0.001f, 0.8f, 0.0f).mix(0.32f));
    add("Crystal Bell", "Bells", { "ring-mod", "feet-layer", "velocity" },
        "A high sine body and a brittle upper strike produce a glassy bell. Stronger high-frequency ring sidebands hang over the clean dry mode as the impact fades.",
        bell().feet(I, f4).feet(II, f2).wave(II, false, true, 0.72f)
              .hpf(II, 500.0f).lpf(II, 7200.0f, 0.10f).level(II, 0.22f, 0.08f)
              .eg(I, 0.002f, 3.9f, 0.0f, 2.8f).eg(II, 0.001f, 0.62f, 0.0f, 0.39f)
              .ring(431.0f, 0.0f, 0.001f, 0.8f, 0.49f).mix(0.35f));
    add("Bell Pad", "Bells", { "ring-mod", "chorus", "dual-rank" },
        "A struck upper edge gives way to a quiet sustained bell body. Moderate ring color, restrained chorus and unequal long releases support held chords.",
        bell().wave(I, false, false, 0.88f).wave(II, false, true, 0.40f)
              .lpf(II, 2700.0f, 0.08f).level(II, 0.20f, 0.12f)
              .eg(I, 0.008f, 3.8f, 0.28f, 4.2f).eg(II, 0.001f, 0.65f, 0.045f, 2.2f)
              .ring(173.0f, 0.0f, 0.001f, 1.0f, 0.29f).chorus(0.24f, 0.28f).mix(0.32f));
    add("Ring Sweep Bell", "Bells", { "ring-mod", "dual-rank", "velocity" },
        "A fast ring-envelope rise sweeps the carrier upward at impact, then it falls back over the resonating sine body. The upper strike is shorter than the moving ring tail.",
        bell().feet(II, f223).wave(II, false, true, 0.48f).lpf(II, 4400.0f, 0.12f)
              .eg(I, 0.003f, 4.6f, 0.0f, 3.2f).eg(II, 0.001f, 0.38f, 0.0f, 0.24f)
              .ring(119.0f, 0.43f, 0.004f, 1.8f, 0.55f).mix(0.31f));
    add("Steel Drum", "Bells", { "ring-mod", "percussive", "dual-rank" },
        "A rounded sine-and-pulse body rings after an octave mallet strike. Mild ring sidebands and a closing body filter suggest a shallow steel pan.",
        bell().wave(I, false, true, 0.78f).wave(II, false, true, 0.58f).pw(I, 0.52f).pw(II, 0.57f)
              .hpf(I, 40.0f).lpf(I, 980.0f, 0.26f).lpf(II, 3200.0f, 0.13f)
              .feg(I, 0.0f, 0.25f, 0.002f, 0.42f, 0.30f).feg(II, 0.0f, 0.34f, 0.001f, 0.16f, 0.12f)
              .level(I, 0.20f, 0.20f).level(II, 0.22f, 0.08f).touch(I, 0.08f, 0.75f, 0.0f, 0.0f)
              .eg(I, 0.002f, 2.8f, 0.0f, 1.7f).eg(II, 0.001f, 0.32f, 0.0f, 0.21f)
              .ring(157.0f, 0.0f, 0.001f, 0.7f, 0.18f).mix(0.30f));
    add("Chimes", "Bells", { "ring-mod", "feet-layer", "dual-rank" },
        "High 4' and 2' modes ring with unequal long decays. A light pulse edge marks the strike, then the pure upper chime and metallic sidebands linger.",
        bell().feet(I, f4).feet(II, f2).wave(II, false, true, 0.85f)
              .lpf(II, 6100.0f, 0.06f).level(II, 0.11f, 0.12f)
              .eg(I, 0.003f, 5.8f, 0.0f, 4.5f).eg(II, 0.001f, 1.65f, 0.0f, 1.05f)
              .ring(389.0f, 0.0f, 0.001f, 1.0f, 0.38f).mix(0.36f));
    add("Dark Bell", "Bells", { "ring-mod", "feet-layer", "dual-rank" },
        "A low 16' sine hangs under a subdued 8' strike. The short filtered upper edge and low ring carrier keep the attack audible without a bright metallic wash.",
        bell().feet(I, f16).feet(II, f8).wave(II, false, true, 0.42f)
              .lpf(II, 1050.0f, 0.12f).feg(II, 0.0f, 0.18f, 0.003f, 0.23f, 0.19f)
              .level(II, 0.17f, 0.10f).eg(I, 0.006f, 4.8f, 0.0f, 3.6f).eg(II, 0.003f, 0.60f, 0.0f, 0.37f)
              .ring(97.0f, 0.0f, 0.001f, 0.9f, 0.34f).mix(0.25f));
    add("Bell Choir", "Bells", { "ring-mod", "feet-layer", "chorus" },
        "An 8' bell and a 5 1/3' bell sing a fifth apart. The upper voice decays sooner, with modest chorus and ring color joining the two resonances.",
        bell().feet(II, f513).wave(II, false, true, 0.82f).lpf(II, 3200.0f, 0.07f)
              .level(II, 0.13f, 0.15f).eg(I, 0.005f, 4.8f, 0.0f, 3.6f).eg(II, 0.002f, 2.5f, 0.0f, 1.8f)
              .ring(171.0f, 0.0f, 0.001f, 1.0f, 0.24f).chorus(0.18f, 0.31f).detune(0.7f).mix(0.43f));
    add("Metallic Keys", "Bells", { "ring-mod", "percussive", "velocity" },
        "A high-passed saw body is struck by a much shorter, brighter octave saw. Ring sidebands give a metallic edge while a little bypass sine anchors the pitched tail.",
        bell().wave(I, true, false, 0.30f).wave(II, true, false)
              .hpf(I, 220.0f).hpf(II, 850.0f).lpf(I, 1700.0f, 0.22f).lpf(II, 5900.0f, 0.16f)
              .feg(I, 0.0f, 0.25f, 0.002f, 0.48f, 0.35f).feg(II, 0.0f, 0.45f, 0.001f, 0.17f, 0.13f)
              .level(I, 0.95f, 0.2667f).level(II, 0.70f, 0.0976f)
              .touch(I, 0.09f, 1.0f, 0.0f, 0.0f).touch(II, 0.12f, 1.0f, 0.0f, 0.0f)
              .eg(I, 0.002f, 2.8f, 0.0f, 1.7f).eg(II, 0.001f, 0.30f, 0.0f, 0.19f)
              .ring(277.0f, 0.0f, 0.001f, 0.7f, 0.44f).mix(0.34f));
    add("Agogo", "Bells", { "ring-mod", "percussive", "velocity" },
        "A hollow high pulse body and a short upper knock make a compact metal-bell hit. Strong ring color and resonant filtering give the strike a nasal clang.",
        bell().feet(I, f4).feet(II, f223).wave(I, false, true, 0.34f).wave(II, false, true, 0.18f)
              .pw(I, 0.61f).pw(II, 0.72f).hpf(I, 300.0f).hpf(II, 700.0f)
              .lpf(I, 2300.0f, 0.38f).lpf(II, 5100.0f, 0.25f)
              .feg(I, 0.0f, 0.25f, 0.001f, 0.28f, 0.18f).feg(II, 0.0f, 0.38f, 0.001f, 0.12f, 0.09f)
              .level(I, 0.95f, 0.2436f).level(II, 0.70f, 0.0854f)
              .touch(I, 0.10f, 1.0f, 0.0f, 0.0f).touch(II, 0.12f, 1.0f, 0.0f, 0.0f)
              .eg(I, 0.001f, 1.45f, 0.0f, 0.82f).eg(II, 0.001f, 0.18f, 0.0f, 0.12f)
              .ring(325.0f, 0.0f, 0.001f, 0.6f, 0.58f).res(0.10f).mix(0.33f));
    add("Clock Bells", "Bells", { "ring-mod", "feet-layer", "dual-rank" },
        "The 4' and 2 2/3' ranks sound a true fifth apart. A small upper impact gives way to two clear, unequal bell decays with restrained ring color.",
        bell().feet(I, f4).feet(II, f223).wave(II, false, true, 0.74f)
              .lpf(II, 4200.0f, 0.06f).level(II, 0.12f, 0.15f)
              .eg(I, 0.003f, 3.8f, 0.0f, 2.8f).eg(II, 0.001f, 2.4f, 0.0f, 1.7f)
              .ring(245.0f, 0.0f, 0.001f, 0.8f, 0.22f).detune(0.0f).mix(0.42f));
    add("Ring Mallet", "Bells", { "ring-mod", "velocity", "dual-rank" },
        "A rounded 8' body follows a short 2 2/3' mallet strike. Stronger initial-level response on the upper rank makes hard hits brighter and more metallic.",
        bell().feet(II, f223).wave(II, false, true, 0.55f).lpf(II, 2700.0f, 0.13f)
              .feg(II, 0.0f, 0.34f, 0.001f, 0.14f, 0.10f)
              .level(I, 0.0f, 0.16f).level(II, 0.32f, 0.055f)
              .touch(I, 0.0f, 0.86f, 0.0f, 0.0f).touch(II, 0.20f, 1.0f, 0.0f, 0.0f)
              .eg(I, 0.002f, 2.5f, 0.0f, 1.5f).eg(II, 0.001f, 0.25f, 0.0f, 0.16f)
              .ring(247.0f, 0.0f, 0.001f, 0.7f, 0.33f).mix(0.34f));
}
