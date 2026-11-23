// Bells voicings. Included inside namespace curated by CuratedLibrary.inc.
// Existing program names and insertion order are stable host/MIDI identities.
// Tracked metal/wood modes supply the body and decay structure. The second rank
// retains a separately controllable strike or upper course. Fixed ring carriers
// remain where they are part of the named electronic character.
// Wide modal bandwidth is paired with instrument-specific high-mode losses,
// mallet timing and keyboard contours. Short wood strikes, lingering metal
// bars, paired bell courses and swept electronic clangs keep separate roles.
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
        "A clear bell body with independently fading metal modes feeds the fixed 307 Hz ring carrier. The short octave strike and velocity-sensitive impact open into a long, evolving metallic tail.",
        bell()
              .level(I, 0.0f, 0.264f).level(II, 0.34f, 0.12f).touch(I, 0.0f, 0.864f, 0.0f, 0.0f)
              .touch(II, 0.12f, 0.984f, 0.0f, 0.0f).hpf(I, 24.0f).hpf(II, 620.0f).lpf(I, 10500.0f, 0.015f)
              .lpf(II, 13000.0f, 0.045f).eg(I, 0.0032f, 5.5f, 0.0f, 3.8f).eg(II, 0.001f, 0.31f, 0.0f, 0.17f)
              .feg(II, 0.0f, 0.15f, 0.001f, 0.13f, 0.09f).body(I, body::Model::bell, 0.91f, 0.62f, 1.55f)
              .body(II, body::Model::bell, 0.58f, 0.91f, 0.29f).ring(307.0f, 0.0f, 0.001f, 1.1f, 0.36f).mix(0.20f)
              .contour(-0.07f, -0.21f, -0.035f, 0.09f));
    add("Ring Bell II", "Bells", { "ring-mod", "feet-layer", "velocity" },
        "A darker modal bell body and a bright upper twelfth feed the lower 191 Hz ring carrier. Fast upper-mode losses produce a sharper clang and more covered tail than Ring Bell I.",
        bell()
              .feet(II, f223).wave(II, false, true, 0.55f).level(I, 0.0f, 0.305556f).level(II, 0.27f, 0.097561f)
              .touch(I, 0.0f, 1.0f, 0.0f, 0.0f).touch(II, 0.12f, 1.0f, 0.0f, 0.0f).hpf(I, 24.0f).hpf(II, 920.0f)
              .lpf(I, 7200.0f, 0.02f).lpf(II, 14500.0f, 0.055f).eg(I, 0.0024f, 3.65f, 0.0f, 2.4f)
              .eg(II, 0.001f, 0.23f, 0.0f, 0.13f).feg(II, 0.0f, 0.18f, 0.001f, 0.10f, 0.065f)
              .body(I, body::Model::bell, 0.94f, 0.32f, 0.83f).body(II, body::Model::bell, 0.68f, 0.96f, 0.25f)
              .ring(191.0f, 0.0f, 0.001f, 0.63f, 0.41f).mix(0.25f).contour(-0.02f, -0.19f, -0.025f, 0.08f));
    add("Tubular Bells", "Bells", { "ring-mod", "feet-layer", "dual-rank" },
        "A deep tube-like modal body rings below a brief bright upper strike. Tracked inharmonic modes keep their relationships across the keyboard; a small fixed ring blend adds color without replacing the note center.",
        bell()
              .feet(I, f16).feet(II, f223).wave(II, false, true, 0.66f).level(II, 0.22f, 0.10f).hpf(I, 20.0f).hpf(II, 560.0f)
              .lpf(I, 10000.0f, 0.015f).lpf(II, 15000.0f, 0.035f).feg(II, 0.0f, 0.12f, 0.001f, 0.23f, 0.20f)
              .eg(I, 0.0042f, 8.4f, 0.0f, 6.1f).eg(II, 0.0013f, 0.49f, 0.0f, 0.31f)
              .body(I, body::Model::bell, 1.0f, 0.65f, 2.60f).body(II, body::Model::bell, 0.93f, 0.91f, 0.52f)
              .touch(I, 0.015f, 0.88f, 0.0f, 0.0f).touch(II, 0.045f, 0.97f, 0.0f, 0.0f)
              .ring(137.0f, 0.0f, 0.001f, 1.2f, 0.025f).mix(0.19f).contour(-0.11f, -0.23f, -0.055f, 0.115f));
    add("Glockenspiel", "Bells", { "feet-layer", "percussive" },
        "A hard, high-register metal-bar strike excites sparse inharmonic modes that track each key. The brightest modes fall away first above a clear ringing pitch; the upper rank adds a tiny hard-mallet accent.",
        bell()
              .feet(I, f4).feet(II, f2).wave(II, false, true, 0.58f).level(II, 0.23f, 0.07f)
              .ring(463.0f, 0.0f, 0.001f, 0.6f, 0.0f).hpf(I, 42.0f).hpf(II, 1350.0f).lpf(I, 15500.0f, 0.01f)
              .lpf(II, 17500.0f, 0.025f).feg(II, 0.0f, 0.12f, 0.001f, 0.065f, 0.045f).eg(I, 0.0015f, 4.3f, 0.0f, 2.8f)
              .eg(II, 0.001f, 0.13f, 0.0f, 0.075f).body(I, body::Model::celesta, 1.0f, 0.98f, 1.38f)
              .body(II, body::Model::bell, 0.88f, 0.99f, 0.25f).touch(I, 0.0f, 0.87f, 0.0f, 0.0f)
              .touch(II, 0.035f, 0.98f, 0.0f, 0.0f).mix(0.16f).contour(-0.03f, -0.27f, -0.015f, 0.12f));
    add("Celeste Bells", "Bells", { "chorus", "velocity" },
        "A soft hammer excites a delicate metal-bar spectrum with an independently fading upper accent. Gentle chorus shades the modal decay while the unmodulated pitch remains clear.",
        bell()
              .wave(I, false, false, 0.90f).wave(II, false, true, 0.46f).level(II, 0.14f, 0.09f)
              .ring(211.0f, 0.0f, 0.001f, 0.7f, 0.0f).hpf(I, 26.0f).hpf(II, 760.0f).lpf(I, 7600.0f, 0.01f)
              .lpf(II, 10200.0f, 0.025f).feg(II, 0.0f, 0.06f, 0.0025f, 0.13f, 0.10f).eg(I, 0.005f, 4.1f, 0.0f, 2.6f)
              .eg(II, 0.002f, 0.28f, 0.0f, 0.17f).body(I, body::Model::celesta, 1.0f, 0.29f, 1.42f)
              .body(II, body::Model::celesta, 0.85f, 0.61f, 0.37f).touch(I, 0.0f, 0.89f, 0.0f, 0.0f)
              .touch(II, 0.035f, 0.96f, 0.0f, 0.0f).chorus(0.09f, 0.24f).mix(0.16f).contour(-0.055f, -0.18f, -0.025f, 0.10f));
    add("Music Box", "Bells", { "percussive", "velocity" },
        "A small bright tine pluck rings into a simple clean tail. Sparse high metal modes decay quickly after the tooth-like attack, keeping close melody notes intelligible without a fixed ring carrier.",
        bell()
              .feet(I, f4).feet(II, f2).wave(II, false, true, 0.30f).pw(II, 0.61f).level(II, 0.18f, 0.06f)
              .ring(419.0f, 0.0f, 0.001f, 0.5f, 0.0f).hpf(I, 52.0f).hpf(II, 1850.0f).lpf(I, 12500.0f, 0.01f)
              .lpf(II, 15500.0f, 0.025f).feg(II, 0.0f, 0.09f, 0.001f, 0.055f, 0.035f).eg(I, 0.0017f, 2.65f, 0.0f, 1.35f)
              .eg(II, 0.001f, 0.095f, 0.0f, 0.055f).body(I, body::Model::celesta, 1.0f, 0.76f, 0.63f)
              .body(II, body::Model::tine, 0.92f, 0.94f, 0.25f).touch(I, 0.0f, 0.78f, 0.0f, 0.0f)
              .touch(II, 0.025f, 0.91f, 0.0f, 0.0f).mix(0.13f).contour(-0.025f, -0.24f, -0.01f, 0.10f));
    add("Gong", "Bells", { "ring-mod", "noise", "dual-rank" },
        "A deep collection of metal modes blooms beneath a noisy upper clang. The ring sweep animates the impact while unequal modal losses reveal a long, complex low resonance as the noise recedes.",
        bell()
              .feet(I, f16).feet(II, f4).res(0.08f).level(I, 0.0f, 0.305556f).level(II, 0.50f, 0.134146f)
              .touch(I, 0.0f, 1.0f, 0.0f, 0.0f).touch(II, 0.12f, 1.0f, 0.0f, 0.0f).hpf(I, 20.0f).hpf(II, 540.0f)
              .lpf(I, 8200.0f, 0.045f).lpf(II, 9400.0f, 0.16f).wave(II, true, false, 0.12f, 0.10f)
              .feg(II, 0.15f, 0.42f, 0.035f, 1.25f, 0.70f).eg(I, 0.042f, 10.5f, 0.0f, 7.3f).eg(II, 0.012f, 1.35f, 0.0f, 0.85f)
              .body(I, body::Model::bell, 0.97f, 0.56f, 2.95f).body(II, body::Model::bell, 0.39f, 0.95f, 0.68f)
              .ring(53.0f, 0.16f, 0.045f, 4.25f, 0.39f).mix(0.30f).contour(-0.14f, -0.28f, -0.065f, 0.11f));
    add("Temple Bell", "Bells", { "ring-mod", "feet-layer", "dual-rank" },
        "A deep struck metal body carries slowly separating modal decays under a brief upper clang. The stable key-tracked center survives a restrained low ring movement and a long, calm release.",
        bell()
              .feet(I, f16).feet(II, f513).wave(II, false, true, 0.72f).level(II, 0.16f, 0.11f).hpf(I, 20.0f).hpf(II, 410.0f)
              .lpf(I, 6600.0f, 0.015f).lpf(II, 10200.0f, 0.035f).feg(II, 0.0f, 0.10f, 0.002f, 0.30f, 0.25f)
              .eg(I, 0.008f, 8.7f, 0.0f, 6.2f).eg(II, 0.0025f, 0.78f, 0.0f, 0.47f)
              .body(I, body::Model::bell, 0.995f, 0.37f, 2.75f).body(II, body::Model::bell, 0.94f, 0.73f, 0.67f)
              .touch(I, 0.0f, 0.85f, 0.0f, 0.0f).touch(II, 0.04f, 0.95f, 0.0f, 0.0f).ring(83.0f, 0.045f, 0.012f, 3.4f, 0.095f)
              .mix(0.20f).contour(-0.12f, -0.24f, -0.06f, 0.10f));
    add("Marimba", "Bells", { "percussive", "velocity", "dual-rank" },
        "A wooden-bar spectrum carries a soft fundamental and quickly damped higher modes. A separate brief fourth-harmonic mallet knock supplies the strike; hard playing opens the upper wood color without a ring carrier.",
        bell()
              .feet(II, f2).wave(II, false, true, 0.18f).pw(II, 0.53f).level(II, 0.44f, 0.07f)
              .ring(307.0f, 0.0f, 0.001f, 0.8f, 0.0f).hpf(I, 22.0f).hpf(II, 650.0f).lpf(I, 8800.0f, 0.015f)
              .lpf(II, 10000.0f, 0.035f).feg(II, 0.0f, 0.10f, 0.001f, 0.065f, 0.045f).eg(I, 0.0037f, 2.7f, 0.0f, 1.25f)
              .eg(II, 0.001f, 0.10f, 0.0f, 0.065f).body(I, body::Model::marimba, 1.0f, 0.49f, 0.84f)
              .body(II, body::Model::marimba, 0.82f, 0.94f, 0.25f).touch(I, 0.015f, 0.92f, 0.0f, 0.0f)
              .touch(II, 0.055f, 1.0f, 0.0f, 0.0f).mix(0.22f).contour(-0.055f, -0.22f, -0.03f, 0.13f));
    add("Vibraphone", "Bells", { "sub-mod", "sine", "dual-rank" },
        "A long metal-bar body carries unequal higher-mode decays and a separately controlled fourth-harmonic strike. Smooth 5.2 Hz tremolo moves the full resonance while a continuous pitched body remains between the crests.",
        bell()
              .feet(II, f2).wave(II, false, false, 0.68f).level(II, 0.0f, 0.11f).ring(307.0f, 0.0f, 0.001f, 0.8f, 0.0f)
              .hpf(I, 22.0f).hpf(II, 140.0f).lpf(I, 11000.0f, 0.015f).lpf(II, 12500.0f, 0.025f)
              .feg(II, 0.0f, 0.0f, 0.0015f, 0.45f, 0.30f).eg(I, 0.0032f, 6.4f, 0.0f, 3.8f).eg(II, 0.0014f, 1.20f, 0.0f, 0.65f)
              .body(I, body::Model::vibes, 0.97f, 0.37f, 2.10f).touch(I, 0.0f, 0.90f, 0.0f, 0.0f)
              .touch(II, 0.0f, 0.90f, 0.0f, 0.0f).sub(subSine, 5.2f, 0.0f, 0.0f, 0.38f).mix(0.22f)
              .contour(-0.09f, -0.18f, 0.0f, 0.10f));
    add("Xylophone", "Bells", { "percussive", "velocity", "feet-layer" },
        "A hard high-register wooden-bar strike has a bright opening and a compact pitched decay. Fast independent losses remove the upper knock early, keeping repeated notes dry and distinct.",
        bell()
              .feet(I, f4).feet(II, f2).wave(II, false, true, 0.15f).pw(II, 0.56f).level(II, 0.47f, 0.07f)
              .ring(307.0f, 0.0f, 0.001f, 0.8f, 0.0f).hpf(I, 45.0f).hpf(II, 1200.0f).lpf(I, 15000.0f, 0.015f)
              .lpf(II, 17000.0f, 0.035f).feg(II, 0.0f, 0.12f, 0.001f, 0.045f, 0.025f).eg(I, 0.0011f, 1.45f, 0.0f, 0.60f)
              .eg(II, 0.001f, 0.075f, 0.0f, 0.04f).body(I, body::Model::marimba, 0.99f, 1.0f, 0.29f)
              .body(II, body::Model::marimba, 0.80f, 0.99f, 0.25f).touch(I, 0.0f, 0.88f, 0.0f, 0.0f)
              .touch(II, 0.05f, 0.97f, 0.0f, 0.0f).mix(0.19f).contour(-0.025f, -0.29f, -0.01f, 0.14f));
    add("Crystal Bell", "Bells", { "ring-mod", "feet-layer", "velocity" },
        "Bright tracked metal modes feed a glassy ring color beneath the brittle upper strike. Unequal modal decays reveal a clear high body as the brilliant impact disappears.",
        bell()
              .feet(I, f4).feet(II, f2).wave(II, false, true, 0.72f).level(II, 0.22f, 0.08f).hpf(I, 36.0f).hpf(II, 1450.0f)
              .lpf(I, 16000.0f, 0.015f).lpf(II, 17500.0f, 0.035f).feg(II, 0.0f, 0.13f, 0.001f, 0.14f, 0.10f)
              .eg(I, 0.0023f, 5.2f, 0.0f, 3.6f).eg(II, 0.001f, 0.41f, 0.0f, 0.25f)
              .body(I, body::Model::bell, 0.99f, 0.95f, 1.95f).body(II, body::Model::celesta, 0.84f, 0.98f, 0.41f)
              .touch(I, 0.0f, 0.88f, 0.0f, 0.0f).touch(II, 0.045f, 0.99f, 0.0f, 0.0f).ring(431.0f, 0.0f, 0.001f, 1.0f, 0.19f)
              .mix(0.22f).contour(-0.035f, -0.28f, -0.02f, 0.12f));
    add("Bell Pad", "Bells", { "ring-mod", "chorus", "dual-rank" },
        "A struck modal upper edge dissolves into a quiet sustained bell-and-sine bed. Slowly changing partial balance, shallow chorus and unequal releases support chords without holding the initial clang at a fixed brightness.",
        bell()
              .wave(I, false, false, 0.88f).wave(II, false, true, 0.40f).level(II, 0.20f, 0.12f).hpf(I, 24.0f).hpf(II, 560.0f)
              .lpf(I, 8400.0f, 0.02f).lpf(II, 10800.0f, 0.045f).feg(II, 0.0f, 0.12f, 0.002f, 0.30f, 0.80f)
              .eg(I, 0.018f, 5.5f, 0.30f, 5.4f).eg(II, 0.002f, 0.44f, 0.028f, 2.7f)
              .body(I, body::Model::bell, 0.66f, 0.43f, 2.65f).body(II, body::Model::celesta, 0.78f, 0.75f, 0.52f)
              .touch(I, 0.0f, 0.79f, 0.06f, 0.14f).touch(II, 0.045f, 0.92f, 0.0f, 0.0f).chorus(0.18f, 0.22f)
              .ring(173.0f, 0.0f, 0.001f, 1.4f, 0.15f).mix(0.21f).contour(-0.09f, -0.20f, -0.045f, 0.08f));
    add("Ring Sweep Bell", "Bells", { "ring-mod", "dual-rank", "velocity" },
        "A fast carrier sweep crosses the inharmonic modes at impact, then falls over the ringing body. Independent modal losses and a short upper strike keep the moving tail distinct from the initial clang.",
        bell()
              .feet(II, f223).wave(II, false, true, 0.48f).level(I, 0.0f, 0.305556f).level(II, 0.34f, 0.121951f)
              .touch(I, 0.0f, 1.0f, 0.0f, 0.0f).touch(II, 0.12f, 1.0f, 0.0f, 0.0f).hpf(I, 25.0f).hpf(II, 1100.0f)
              .lpf(I, 11500.0f, 0.02f).lpf(II, 15000.0f, 0.045f).feg(II, 0.0f, 0.17f, 0.001f, 0.12f, 0.085f)
              .eg(I, 0.003f, 5.9f, 0.0f, 4.1f).eg(II, 0.001f, 0.25f, 0.0f, 0.15f)
              .body(I, body::Model::bell, 0.94f, 0.66f, 1.85f).body(II, body::Model::bell, 0.63f, 0.95f, 0.28f)
              .ring(119.0f, 0.34f, 0.008f, 2.7f, 0.43f).mix(0.21f).contour(-0.06f, -0.22f, -0.035f, 0.095f));
    add("Steel Drum", "Bells", { "ring-mod", "percussive", "dual-rank" },
        "A rounded metal body has independently fading higher modes and a short octave mallet attack. The clean tuned center carries the melody while mild ring color and the remaining resonant pulse add a shallow-pan edge.",
        bell()
              .wave(I, false, true, 0.78f).wave(II, false, true, 0.58f).pw(I, 0.52f).pw(II, 0.57f).level(I, 0.20f, 0.20f)
              .level(II, 0.22f, 0.08f).hpf(I, 28.0f).hpf(II, 430.0f).lpf(I, 6100.0f, 0.16f).lpf(II, 9800.0f, 0.06f)
              .feg(I, 0.0f, 0.12f, 0.002f, 0.25f, 0.20f).feg(II, 0.0f, 0.13f, 0.001f, 0.09f, 0.065f)
              .eg(I, 0.0028f, 2.65f, 0.0f, 1.15f).eg(II, 0.001f, 0.19f, 0.0f, 0.115f)
              .body(I, body::Model::vibes, 0.94f, 0.80f, 0.48f).body(II, body::Model::vibes, 0.75f, 0.95f, 0.25f)
              .touch(I, 0.035f, 0.90f, 0.0f, 0.0f).touch(II, 0.055f, 0.99f, 0.0f, 0.0f)
              .ring(157.0f, 0.0f, 0.001f, 0.55f, 0.025f).mix(0.26f).contour(-0.025f, -0.18f, -0.02f, 0.10f));
    add("Chimes", "Bells", { "ring-mod", "feet-layer", "dual-rank" },
        "High struck metal modes and a quieter octave course ring with unequal long decays. The opening glint falls away into clear tracked resonances with only a trace of fixed-carrier color.",
        bell()
              .feet(I, f4).feet(II, f2).wave(II, false, true, 0.85f).level(II, 0.11f, 0.12f).hpf(I, 38.0f).hpf(II, 760.0f)
              .lpf(I, 14500.0f, 0.01f).lpf(II, 17500.0f, 0.02f).feg(II, 0.0f, 0.10f, 0.0015f, 0.42f, 0.34f)
              .eg(I, 0.0033f, 8.1f, 0.0f, 5.8f).eg(II, 0.0015f, 1.32f, 0.0f, 0.84f)
              .body(I, body::Model::bell, 0.995f, 0.85f, 2.40f).body(II, body::Model::celesta, 0.98f, 0.91f, 1.38f)
              .touch(I, 0.0f, 0.85f, 0.0f, 0.0f).touch(II, 0.035f, 0.94f, 0.0f, 0.0f).ring(389.0f, 0.0f, 0.001f, 1.3f, 0.025f)
              .mix(0.24f).contour(-0.06f, -0.27f, -0.035f, 0.13f));
    add("Dark Bell", "Bells", { "ring-mod", "feet-layer", "dual-rank" },
        "A low modal resonance supports a subdued short upper strike. The dark body loses its highest modes quickly, then leaves a long low tail with restrained ring color instead of a bright metallic wash.",
        bell()
              .feet(I, f16).feet(II, f8).wave(II, false, true, 0.42f).level(II, 0.17f, 0.10f).hpf(I, 20.0f).hpf(II, 410.0f)
              .lpf(I, 3900.0f, 0.025f).lpf(II, 5400.0f, 0.06f).feg(II, 0.0f, 0.10f, 0.003f, 0.18f, 0.14f)
              .eg(I, 0.006f, 6.8f, 0.0f, 4.7f).eg(II, 0.0025f, 0.42f, 0.0f, 0.26f)
              .body(I, body::Model::bell, 0.995f, 0.11f, 1.90f).body(II, body::Model::bell, 0.85f, 0.38f, 0.39f)
              .touch(I, 0.0f, 0.84f, 0.0f, 0.0f).touch(II, 0.04f, 0.95f, 0.0f, 0.0f).ring(97.0f, 0.0f, 0.001f, 1.1f, 0.14f)
              .mix(0.16f).contour(-0.13f, -0.22f, -0.06f, 0.09f));
    add("Bell Choir", "Bells", { "ring-mod", "feet-layer", "chorus" },
        "Two tracked modal bells sound a fifth apart, with different brightness and decay. The upper bell recedes earlier while light chorus and a restrained ring blend join the remaining resonances.",
        bell()
              .feet(II, f513).wave(II, false, true, 0.82f).level(II, 0.13f, 0.15f).hpf(I, 24.0f).hpf(II, 130.0f)
              .lpf(I, 9600.0f, 0.015f).lpf(II, 12800.0f, 0.025f).feg(II, 0.0f, 0.08f, 0.002f, 0.65f, 0.50f)
              .eg(I, 0.0045f, 6.3f, 0.0f, 4.7f).eg(II, 0.003f, 3.1f, 0.0f, 2.1f)
              .body(I, body::Model::bell, 0.99f, 0.54f, 1.95f).body(II, body::Model::bell, 0.99f, 0.68f, 1.12f)
              .touch(I, 0.0f, 0.83f, 0.0f, 0.0f).touch(II, 0.025f, 0.89f, 0.0f, 0.0f).chorus(0.12f, 0.25f)
              .ring(171.0f, 0.0f, 0.001f, 1.2f, 0.06f).detune(0.18f).mix(0.35f).contour(-0.07f, -0.23f, -0.04f, 0.11f));
    add("Metallic Keys", "Bells", { "ring-mod", "percussive", "velocity" },
        "A hard reed-like strike and modal metal body retain a deliberately synthetic ring edge. Fast high-partial decay takes the initial clang away before the pitched tail, while the residual filtered saw keeps the texture forward.",
        bell()
              .wave(I, true, false, 0.30f).wave(II, true, false).level(I, 0.95f, 0.82f).level(II, 0.70f, 0.53f)
              .touch(I, 0.09f, 1.0f, 0.0f, 0.0f).touch(II, 0.12f, 1.0f, 0.0f, 0.0f).hpf(I, 120.0f).hpf(II, 1600.0f)
              .lpf(I, 4900.0f, 0.18f).lpf(II, 12500.0f, 0.11f).feg(I, 0.0f, 0.18f, 0.002f, 0.37f, 0.25f)
              .feg(II, 0.0f, 0.29f, 0.001f, 0.115f, 0.08f).eg(I, 0.0022f, 2.65f, 0.0f, 1.30f)
              .eg(II, 0.001f, 0.20f, 0.0f, 0.12f).body(I, body::Model::reed, 0.77f, 0.84f, 0.57f)
              .body(II, body::Model::bell, 0.72f, 0.98f, 0.25f).bodySize(I, 0.78f).ring(277.0f, 0.0f, 0.001f, 0.55f, 0.29f)
              .mix(0.23f).contour(-0.025f, -0.22f, -0.025f, 0.095f));
    add("Agogo", "Bells", { "ring-mod", "percussive", "velocity" },
        "A compact hollow metal clang has a short upper knock and rapidly damping inharmonic modes. A restrained pulse resonance preserves its nasal attack while the tracked body keeps the strike recognizable across notes.",
        bell()
              .feet(I, f4).feet(II, f223).wave(I, false, true, 0.34f).wave(II, false, true, 0.18f).pw(I, 0.61f).pw(II, 0.72f)
              .level(I, 0.95f, 0.2436f).level(II, 0.70f, 0.0854f).touch(I, 0.10f, 1.0f, 0.0f, 0.0f)
              .touch(II, 0.12f, 1.0f, 0.0f, 0.0f).hpf(I, 240.0f).hpf(II, 1250.0f).lpf(I, 7600.0f, 0.28f)
              .lpf(II, 11500.0f, 0.16f).feg(I, 0.0f, 0.17f, 0.001f, 0.18f, 0.12f).feg(II, 0.0f, 0.24f, 0.001f, 0.075f, 0.05f)
              .eg(I, 0.0014f, 1.30f, 0.0f, 0.55f).eg(II, 0.001f, 0.115f, 0.0f, 0.07f)
              .body(I, body::Model::bell, 0.93f, 0.88f, 0.31f).body(II, body::Model::bell, 0.76f, 1.0f, 0.25f)
              .ring(325.0f, 0.0f, 0.001f, 0.42f, 0.16f).res(0.075f).mix(0.21f).contour(0.025f, -0.25f, -0.015f, 0.11f));
    add("Clock Bells", "Bells", { "ring-mod", "feet-layer", "dual-rank" },
        "Two clear modal bells at 4-foot and 2-2/3-foot pitch sound a true fifth apart. A concise upper impact yields to unequal ringing decays with little fixed-carrier interference.",
        bell()
              .feet(I, f4).feet(II, f223).wave(II, false, true, 0.74f).level(II, 0.12f, 0.15f).detune(0.0f).hpf(I, 35.0f)
              .hpf(II, 240.0f).lpf(I, 11500.0f, 0.01f).lpf(II, 14500.0f, 0.025f).feg(II, 0.0f, 0.07f, 0.0015f, 0.52f, 0.40f)
              .eg(I, 0.0026f, 4.9f, 0.0f, 3.3f).eg(II, 0.0018f, 2.85f, 0.0f, 1.9f)
              .body(I, body::Model::bell, 0.995f, 0.62f, 1.58f).body(II, body::Model::bell, 0.99f, 0.77f, 1.08f)
              .touch(I, 0.0f, 0.82f, 0.0f, 0.0f).touch(II, 0.02f, 0.91f, 0.0f, 0.0f).ring(245.0f, 0.0f, 0.001f, 1.0f, 0.025f)
              .mix(0.34f).contour(-0.04f, -0.24f, -0.025f, 0.11f));
    add("Ring Mallet", "Bells", { "ring-mod", "velocity", "dual-rank" },
        "A rounded wooden-mode body supports a short metallic upper mallet, then feeds the ring circuit. Hard strikes expose the upper clang; its rapid losses leave a softer resonating body behind.",
        bell()
              .feet(II, f223).wave(II, false, true, 0.55f).level(I, 0.0f, 0.16f).level(II, 0.32f, 0.055f).hpf(I, 24.0f)
              .hpf(II, 1050.0f).lpf(I, 7200.0f, 0.035f).lpf(II, 12400.0f, 0.065f).feg(II, 0.0f, 0.19f, 0.001f, 0.085f, 0.06f)
              .eg(I, 0.0027f, 2.7f, 0.0f, 1.35f).eg(II, 0.001f, 0.17f, 0.0f, 0.10f)
              .body(I, body::Model::marimba, 0.94f, 0.52f, 0.71f).body(II, body::Model::bell, 0.81f, 0.92f, 0.25f)
              .touch(I, 0.0f, 0.96f, 0.0f, 0.0f).touch(II, 0.11f, 1.0f, 0.0f, 0.0f).ring(247.0f, 0.0f, 0.001f, 0.52f, 0.22f)
              .mix(0.24f).contour(-0.035f, -0.20f, -0.025f, 0.10f));
}
