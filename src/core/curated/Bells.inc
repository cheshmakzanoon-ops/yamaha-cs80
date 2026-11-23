// Bells voicings. Included inside namespace curated by CuratedLibrary.inc.
// Existing program names and insertion order are stable host/MIDI identities.
// Tracked metal/wood modes supply the body and decay structure. The second rank
// retains a separately controllable strike or upper course. Fixed ring carriers
// remain where they are part of the named electronic character.
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
        bell().eg(I, 0.003f, 4.2f, 0.0f, 3.1f).eg(II, 0.001f, 0.48f, 0.0f, 0.26f)
              .ring(307.0f, 0.0f, 0.001f, 0.8f, 0.42f).mix(0.28f)
              .body(I, body::Model::bell, 0.82f, 0.54f, 1.30f).body(II, body::Model::bell, 0.42f, 0.83f, 0.34f)
              .eg(I, 0.0025f, 4.8f, 0.0f, 3.3f).mix(0.22f)
              .level(I, 0.0f, 0.264f).level(II, 0.34f, 0.12f)
              .touch(I, 0.0f, 0.864f, 0.0f, 0.0f).touch(II, 0.12f, 0.984f, 0.0f, 0.0f));
    add("Ring Bell II", "Bells", { "ring-mod", "feet-layer", "velocity" },
        "A darker modal bell body and a bright upper twelfth feed the lower 191 Hz ring carrier. Fast upper-mode losses produce a sharper clang and more covered tail than Ring Bell I.",
        bell().feet(II, f223).wave(II, false, true, 0.55f).lpf(II, 5600.0f, 0.13f)
              .eg(I, 0.002f, 3.4f, 0.0f, 2.5f).eg(II, 0.001f, 0.34f, 0.0f, 0.20f)
              .ring(191.0f, 0.0f, 0.001f, 0.7f, 0.48f).level(II, 0.27f, 0.08f).mix(0.36f)
              .body(I, body::Model::bell, 0.87f, 0.36f, 0.91f).body(II, body::Model::bell, 0.56f, 0.91f, 0.29f)
              .eg(I, 0.002f, 3.9f, 0.0f, 2.7f).mix(0.29f)
              .level(I, 0.0f, 0.305556f).level(II, 0.27f, 0.097561f)
              .touch(I, 0.0f, 1.0f, 0.0f, 0.0f).touch(II, 0.12f, 1.0f, 0.0f, 0.0f));
    add("Tubular Bells", "Bells", { "ring-mod", "feet-layer", "dual-rank" },
        "A deep tube-like modal body rings below a brief bright upper strike. Tracked inharmonic modes keep their relationships across the keyboard; a small fixed ring blend adds color without replacing the note center.",
        bell().feet(I, f16).feet(II, f223).wave(II, false, true, 0.66f)
              .hpf(II, 260.0f).lpf(II, 5600.0f, 0.08f).level(II, 0.22f, 0.10f)
              .eg(I, 0.004f, 6.0f, 0.0f, 4.8f).eg(II, 0.001f, 0.72f, 0.0f, 0.48f)
              .ring(137.0f, 0.0f, 0.001f, 1.0f, 0.36f).mix(0.34f)
              .body(I, body::Model::bell, 1.0f, 0.57f, 2.05f).body(II, body::Model::bell, 0.85f, 0.81f, 0.65f)
              .ring(137.0f, 0.0f, 0.001f, 1.0f, 0.08f).eg(I, 0.003f, 7.1f, 0.0f, 5.2f).mix(0.22f));
    add("Glockenspiel", "Bells", { "feet-layer", "percussive" },
        "A hard, high-register metal-bar strike excites sparse inharmonic modes that track each key. The brightest modes fall away first above a clear ringing pitch; the upper rank adds a tiny hard-mallet accent.",
        bell().feet(I, f4).feet(II, f2).wave(II, false, true, 0.58f)
              .hpf(II, 350.0f).lpf(II, 6900.0f, 0.07f).level(II, 0.23f, 0.07f)
              .eg(I, 0.0015f, 3.1f, 0.0f, 2.1f).eg(II, 0.001f, 0.24f, 0.0f, 0.15f)
              .ring(463.0f, 0.0f, 0.001f, 0.6f, 0.24f).mix(0.29f)
              .body(I, body::Model::celesta, 1.0f, 0.89f, 1.13f).body(II, body::Model::bell, 0.80f, 0.94f, 0.34f)
              .ring(463.0f, 0.0f, 0.001f, 0.6f, 0.0f).eg(I, 0.0012f, 3.8f, 0.0f, 2.4f).mix(0.18f));
    add("Celeste Bells", "Bells", { "chorus", "velocity" },
        "A soft hammer excites a delicate metal-bar spectrum with an independently fading upper accent. Gentle chorus shades the modal decay while the unmodulated pitch remains clear.",
        bell().wave(I, false, false, 0.90f).wave(II, false, true, 0.46f)
              .lpf(II, 2400.0f, 0.06f).feg(II, 0.0f, 0.15f, 0.003f, 0.18f, 0.15f)
              .level(II, 0.14f, 0.09f).eg(I, 0.006f, 3.2f, 0.0f, 2.2f).eg(II, 0.003f, 0.46f, 0.0f, 0.25f)
              .ring(211.0f, 0.0f, 0.001f, 0.7f, 0.16f).chorus(0.13f, 0.30f).mix(0.25f)
              .body(I, body::Model::celesta, 1.0f, 0.35f, 1.31f).body(II, body::Model::celesta, 0.78f, 0.62f, 0.49f)
              .ring(211.0f, 0.0f, 0.001f, 0.7f, 0.0f).eg(I, 0.0035f, 3.8f, 0.0f, 2.5f).mix(0.18f));
    add("Music Box", "Bells", { "percussive", "velocity" },
        "A small bright tine pluck rings into a simple clean tail. Sparse high metal modes decay quickly after the tooth-like attack, keeping close melody notes intelligible without a fixed ring carrier.",
        bell().feet(I, f4).feet(II, f2).wave(II, false, true, 0.30f).pw(II, 0.61f)
              .lpf(II, 3900.0f, 0.08f).feg(II, 0.0f, 0.20f, 0.001f, 0.10f, 0.09f)
              .level(II, 0.18f, 0.06f).eg(I, 0.002f, 2.4f, 0.0f, 1.4f).eg(II, 0.001f, 0.18f, 0.0f, 0.11f)
              .ring(419.0f, 0.0f, 0.001f, 0.5f, 0.12f).mix(0.23f)
              .body(I, body::Model::celesta, 1.0f, 0.67f, 0.56f).body(II, body::Model::tine, 0.83f, 0.90f, 0.30f)
              .ring(419.0f, 0.0f, 0.001f, 0.5f, 0.0f).eg(I, 0.0013f, 2.8f, 0.0f, 1.5f).mix(0.15f));
    add("Gong", "Bells", { "ring-mod", "noise", "dual-rank" },
        "A deep collection of metal modes blooms beneath a noisy upper clang. The ring sweep animates the impact while unequal modal losses reveal a long, complex low resonance as the noise recedes.",
        bell().feet(I, f16).feet(II, f4).wave(II, true, false, 0.18f, 0.08f)
              .hpf(II, 220.0f).lpf(II, 3300.0f, 0.24f).feg(II, 0.0f, 0.52f, 0.008f, 0.70f, 0.55f)
              .level(II, 0.50f, 0.11f).eg(I, 0.025f, 6.8f, 0.0f, 5.5f).eg(II, 0.008f, 1.8f, 0.0f, 1.1f)
              .ring(53.0f, 0.24f, 0.025f, 3.2f, 0.62f).mix(0.42f).res(0.08f)
              .body(I, body::Model::bell, 0.95f, 0.44f, 2.35f).body(II, body::Model::bell, 0.45f, 0.88f, 0.83f)
              .eg(I, 0.018f, 8.2f, 0.0f, 6.3f).ring(53.0f, 0.19f, 0.022f, 3.6f, 0.45f).mix(0.34f)
              .level(I, 0.0f, 0.305556f).level(II, 0.50f, 0.134146f)
              .touch(I, 0.0f, 1.0f, 0.0f, 0.0f).touch(II, 0.12f, 1.0f, 0.0f, 0.0f));
    add("Temple Bell", "Bells", { "ring-mod", "feet-layer", "dual-rank" },
        "A deep struck metal body carries slowly separating modal decays under a brief upper clang. The stable key-tracked center survives a restrained low ring movement and a long, calm release.",
        bell().feet(I, f16).feet(II, f513).wave(II, false, true, 0.72f)
              .lpf(II, 2500.0f, 0.12f).level(II, 0.16f, 0.11f)
              .eg(I, 0.007f, 6.2f, 0.0f, 4.9f).eg(II, 0.002f, 1.15f, 0.0f, 0.72f)
              .ring(83.0f, 0.16f, 0.008f, 2.3f, 0.48f).mix(0.32f)
              .body(I, body::Model::bell, 0.98f, 0.29f, 2.20f).body(II, body::Model::bell, 0.88f, 0.66f, 0.85f)
              .ring(83.0f, 0.10f, 0.008f, 2.6f, 0.17f).eg(I, 0.005f, 7.2f, 0.0f, 5.4f).mix(0.24f));
    add("Marimba", "Bells", { "percussive", "velocity", "dual-rank" },
        "A wooden-bar spectrum carries a soft fundamental and quickly damped higher modes. A separate brief fourth-harmonic mallet knock supplies the strike; hard playing opens the upper wood color without a ring carrier.",
        bell().feet(II, f2).wave(II, false, true, 0.18f).pw(II, 0.53f)
              .hpf(II, 180.0f).lpf(II, 3100.0f, 0.10f).feg(II, 0.0f, 0.25f, 0.001f, 0.13f, 0.10f)
              .level(II, 0.44f, 0.07f).touch(II, 0.16f, 0.92f, 0.0f, 0.0f)
              .eg(I, 0.003f, 2.7f, 0.0f, 1.5f).eg(II, 0.001f, 0.22f, 0.0f, 0.14f)
              .ring(307.0f, 0.0f, 0.001f, 0.8f, 0.0f).mix(0.29f)
              .body(I, body::Model::marimba, 1.0f, 0.43f, 0.90f).body(II, body::Model::marimba, 0.73f, 0.82f, 0.27f)
              .eg(I, 0.0025f, 3.1f, 0.0f, 1.6f).eg(II, 0.001f, 0.17f, 0.0f, 0.10f).mix(0.20f));
    add("Vibraphone", "Bells", { "sub-mod", "sine", "dual-rank" },
        "A long metal-bar body carries unequal higher-mode decays and a separately controlled fourth-harmonic strike. Smooth 5.2 Hz tremolo moves the full resonance while a continuous pitched body remains between the crests.",
        bell().feet(II, f2).wave(II, false, false, 0.68f).level(II, 0.0f, 0.11f)
              .touch(II, 0.0f, 0.78f, 0.0f, 0.0f)
              .eg(I, 0.003f, 4.8f, 0.0f, 3.2f).eg(II, 0.001f, 1.15f, 0.0f, 0.72f)
              .ring(307.0f, 0.0f, 0.001f, 0.8f, 0.0f).sub(subSine, 5.2f, 0.0f, 0.0f, 0.43f).mix(0.27f)
              .body(I, body::Model::vibes, 0.95f, 0.31f, 1.78f)
              .eg(I, 0.0025f, 5.7f, 0.0f, 3.5f).eg(II, 0.001f, 1.35f, 0.0f, 0.76f).mix(0.24f));
    add("Xylophone", "Bells", { "percussive", "velocity", "feet-layer" },
        "A hard high-register wooden-bar strike has a bright opening and a compact pitched decay. Fast independent losses remove the upper knock early, keeping repeated notes dry and distinct.",
        bell().feet(I, f4).feet(II, f2).wave(II, false, true, 0.15f).pw(II, 0.56f)
              .lpf(II, 5200.0f, 0.12f).feg(II, 0.0f, 0.36f, 0.001f, 0.09f, 0.08f)
              .level(II, 0.47f, 0.07f).eg(I, 0.001f, 1.65f, 0.0f, 0.95f).eg(II, 0.001f, 0.19f, 0.0f, 0.12f)
              .ring(307.0f, 0.0f, 0.001f, 0.8f, 0.0f).mix(0.32f)
              .body(I, body::Model::marimba, 1.0f, 0.91f, 0.35f).body(II, body::Model::marimba, 0.82f, 0.98f, 0.25f)
              .eg(I, 0.001f, 1.9f, 0.0f, 0.85f).eg(II, 0.001f, 0.14f, 0.0f, 0.09f).mix(0.22f));
    add("Crystal Bell", "Bells", { "ring-mod", "feet-layer", "velocity" },
        "Bright tracked metal modes feed a glassy ring color beneath the brittle upper strike. Unequal modal decays reveal a clear high body as the brilliant impact disappears.",
        bell().feet(I, f4).feet(II, f2).wave(II, false, true, 0.72f)
              .hpf(II, 500.0f).lpf(II, 7200.0f, 0.10f).level(II, 0.22f, 0.08f)
              .eg(I, 0.002f, 3.9f, 0.0f, 2.8f).eg(II, 0.001f, 0.62f, 0.0f, 0.39f)
              .ring(431.0f, 0.0f, 0.001f, 0.8f, 0.49f).mix(0.35f)
              .body(I, body::Model::bell, 0.96f, 0.87f, 1.65f).body(II, body::Model::celesta, 0.70f, 0.94f, 0.52f)
              .ring(431.0f, 0.0f, 0.001f, 0.8f, 0.25f).eg(I, 0.0018f, 4.5f, 0.0f, 3.1f).mix(0.25f));
    add("Bell Pad", "Bells", { "ring-mod", "chorus", "dual-rank" },
        "A struck modal upper edge dissolves into a quiet sustained bell-and-sine bed. Slowly changing partial balance, shallow chorus and unequal releases support chords without holding the initial clang at a fixed brightness.",
        bell().wave(I, false, false, 0.88f).wave(II, false, true, 0.40f)
              .lpf(II, 2700.0f, 0.08f).level(II, 0.20f, 0.12f)
              .eg(I, 0.008f, 3.8f, 0.28f, 4.2f).eg(II, 0.001f, 0.65f, 0.045f, 2.2f)
              .ring(173.0f, 0.0f, 0.001f, 1.0f, 0.29f).chorus(0.24f, 0.28f).mix(0.32f)
              .body(I, body::Model::bell, 0.74f, 0.38f, 2.15f).body(II, body::Model::celesta, 0.65f, 0.67f, 0.65f)
              .eg(I, 0.006f, 4.5f, 0.28f, 4.6f).ring(173.0f, 0.0f, 0.001f, 1.0f, 0.20f).mix(0.25f));
    add("Ring Sweep Bell", "Bells", { "ring-mod", "dual-rank", "velocity" },
        "A fast carrier sweep crosses the inharmonic modes at impact, then falls over the ringing body. Independent modal losses and a short upper strike keep the moving tail distinct from the initial clang.",
        bell().feet(II, f223).wave(II, false, true, 0.48f).lpf(II, 4400.0f, 0.12f)
              .eg(I, 0.003f, 4.6f, 0.0f, 3.2f).eg(II, 0.001f, 0.38f, 0.0f, 0.24f)
              .ring(119.0f, 0.43f, 0.004f, 1.8f, 0.55f).mix(0.31f)
              .body(I, body::Model::bell, 0.90f, 0.57f, 1.48f).body(II, body::Model::bell, 0.52f, 0.88f, 0.35f)
              .eg(I, 0.0025f, 5.1f, 0.0f, 3.5f).ring(119.0f, 0.39f, 0.004f, 2.1f, 0.49f).mix(0.24f)
              .level(I, 0.0f, 0.305556f).level(II, 0.34f, 0.121951f)
              .touch(I, 0.0f, 1.0f, 0.0f, 0.0f).touch(II, 0.12f, 1.0f, 0.0f, 0.0f));
    add("Steel Drum", "Bells", { "ring-mod", "percussive", "dual-rank" },
        "A rounded metal body has independently fading higher modes and a short octave mallet attack. The clean tuned center carries the melody while mild ring color and the remaining resonant pulse add a shallow-pan edge.",
        bell().wave(I, false, true, 0.78f).wave(II, false, true, 0.58f).pw(I, 0.52f).pw(II, 0.57f)
              .hpf(I, 40.0f).lpf(I, 980.0f, 0.26f).lpf(II, 3200.0f, 0.13f)
              .feg(I, 0.0f, 0.25f, 0.002f, 0.42f, 0.30f).feg(II, 0.0f, 0.34f, 0.001f, 0.16f, 0.12f)
              .level(I, 0.20f, 0.20f).level(II, 0.22f, 0.08f).touch(I, 0.08f, 0.75f, 0.0f, 0.0f)
              .eg(I, 0.002f, 2.8f, 0.0f, 1.7f).eg(II, 0.001f, 0.32f, 0.0f, 0.21f)
              .ring(157.0f, 0.0f, 0.001f, 0.7f, 0.18f).mix(0.30f)
              .body(I, body::Model::vibes, 0.88f, 0.70f, 0.56f).body(II, body::Model::vibes, 0.61f, 0.88f, 0.28f)
              .eg(I, 0.0018f, 3.15f, 0.0f, 1.55f).ring(157.0f, 0.0f, 0.001f, 0.7f, 0.065f).mix(0.23f));
    add("Chimes", "Bells", { "ring-mod", "feet-layer", "dual-rank" },
        "High struck metal modes and a quieter octave course ring with unequal long decays. The opening glint falls away into clear tracked resonances with only a trace of fixed-carrier color.",
        bell().feet(I, f4).feet(II, f2).wave(II, false, true, 0.85f)
              .lpf(II, 6100.0f, 0.06f).level(II, 0.11f, 0.12f)
              .eg(I, 0.003f, 5.8f, 0.0f, 4.5f).eg(II, 0.001f, 1.65f, 0.0f, 1.05f)
              .ring(389.0f, 0.0f, 0.001f, 1.0f, 0.38f).mix(0.36f)
              .body(I, body::Model::bell, 0.98f, 0.75f, 1.92f).body(II, body::Model::celesta, 0.94f, 0.82f, 1.21f)
              .eg(I, 0.0025f, 6.7f, 0.0f, 4.9f).ring(389.0f, 0.0f, 0.001f, 1.0f, 0.075f).mix(0.28f));
    add("Dark Bell", "Bells", { "ring-mod", "feet-layer", "dual-rank" },
        "A low modal resonance supports a subdued short upper strike. The dark body loses its highest modes quickly, then leaves a long low tail with restrained ring color instead of a bright metallic wash.",
        bell().feet(I, f16).feet(II, f8).wave(II, false, true, 0.42f)
              .lpf(II, 1050.0f, 0.12f).feg(II, 0.0f, 0.18f, 0.003f, 0.23f, 0.19f)
              .level(II, 0.17f, 0.10f).eg(I, 0.006f, 4.8f, 0.0f, 3.6f).eg(II, 0.003f, 0.60f, 0.0f, 0.37f)
              .ring(97.0f, 0.0f, 0.001f, 0.9f, 0.34f).mix(0.25f)
              .body(I, body::Model::bell, 0.97f, 0.15f, 1.63f).body(II, body::Model::bell, 0.76f, 0.36f, 0.52f)
              .eg(I, 0.0045f, 5.6f, 0.0f, 4.0f).ring(97.0f, 0.0f, 0.001f, 0.9f, 0.20f).mix(0.18f));
    add("Bell Choir", "Bells", { "ring-mod", "feet-layer", "chorus" },
        "Two tracked modal bells sound a fifth apart, with different brightness and decay. The upper bell recedes earlier while light chorus and a restrained ring blend join the remaining resonances.",
        bell().feet(II, f513).wave(II, false, true, 0.82f).lpf(II, 3200.0f, 0.07f)
              .level(II, 0.13f, 0.15f).eg(I, 0.005f, 4.8f, 0.0f, 3.6f).eg(II, 0.002f, 2.5f, 0.0f, 1.8f)
              .ring(171.0f, 0.0f, 0.001f, 1.0f, 0.24f).chorus(0.18f, 0.31f).detune(0.7f).mix(0.43f)
              .body(I, body::Model::bell, 0.97f, 0.47f, 1.61f).body(II, body::Model::bell, 0.96f, 0.62f, 0.93f)
              .eg(I, 0.0035f, 5.4f, 0.0f, 4.0f).ring(171.0f, 0.0f, 0.001f, 1.0f, 0.12f).mix(0.38f).detune(0.3f));
    add("Metallic Keys", "Bells", { "ring-mod", "percussive", "velocity" },
        "A hard reed-like strike and modal metal body retain a deliberately synthetic ring edge. Fast high-partial decay takes the initial clang away before the pitched tail, while the residual filtered saw keeps the texture forward.",
        bell().wave(I, true, false, 0.30f).wave(II, true, false)
              .hpf(I, 220.0f).hpf(II, 850.0f).lpf(I, 1700.0f, 0.22f).lpf(II, 5900.0f, 0.16f)
              .feg(I, 0.0f, 0.25f, 0.002f, 0.48f, 0.35f).feg(II, 0.0f, 0.45f, 0.001f, 0.17f, 0.13f)
              .level(I, 0.95f, 0.2667f).level(II, 0.70f, 0.0976f)
              .touch(I, 0.09f, 1.0f, 0.0f, 0.0f).touch(II, 0.12f, 1.0f, 0.0f, 0.0f)
              .eg(I, 0.002f, 2.8f, 0.0f, 1.7f).eg(II, 0.001f, 0.30f, 0.0f, 0.19f)
              .ring(277.0f, 0.0f, 0.001f, 0.7f, 0.44f).mix(0.34f)
              .body(I, body::Model::reed, 0.67f, 0.77f, 0.68f).body(II, body::Model::bell, 0.58f, 0.94f, 0.31f)
              .eg(I, 0.0017f, 3.15f, 0.0f, 1.8f).ring(277.0f, 0.0f, 0.001f, 0.7f, 0.34f).mix(0.27f));
    add("Agogo", "Bells", { "ring-mod", "percussive", "velocity" },
        "A compact hollow metal clang has a short upper knock and rapidly damping inharmonic modes. A restrained pulse resonance preserves its nasal attack while the tracked body keeps the strike recognizable across notes.",
        bell().feet(I, f4).feet(II, f223).wave(I, false, true, 0.34f).wave(II, false, true, 0.18f)
              .pw(I, 0.61f).pw(II, 0.72f).hpf(I, 300.0f).hpf(II, 700.0f)
              .lpf(I, 2300.0f, 0.38f).lpf(II, 5100.0f, 0.25f)
              .feg(I, 0.0f, 0.25f, 0.001f, 0.28f, 0.18f).feg(II, 0.0f, 0.38f, 0.001f, 0.12f, 0.09f)
              .level(I, 0.95f, 0.2436f).level(II, 0.70f, 0.0854f)
              .touch(I, 0.10f, 1.0f, 0.0f, 0.0f).touch(II, 0.12f, 1.0f, 0.0f, 0.0f)
              .eg(I, 0.001f, 1.45f, 0.0f, 0.82f).eg(II, 0.001f, 0.18f, 0.0f, 0.12f)
              .ring(325.0f, 0.0f, 0.001f, 0.6f, 0.58f).res(0.10f).mix(0.33f)
              .body(I, body::Model::bell, 0.85f, 0.81f, 0.37f).body(II, body::Model::bell, 0.62f, 0.97f, 0.25f)
              .eg(I, 0.001f, 1.7f, 0.0f, 0.79f).ring(325.0f, 0.0f, 0.001f, 0.6f, 0.24f).mix(0.24f));
    add("Clock Bells", "Bells", { "ring-mod", "feet-layer", "dual-rank" },
        "Two clear modal bells at 4-foot and 2-2/3-foot pitch sound a true fifth apart. A concise upper impact yields to unequal ringing decays with little fixed-carrier interference.",
        bell().feet(I, f4).feet(II, f223).wave(II, false, true, 0.74f)
              .lpf(II, 4200.0f, 0.06f).level(II, 0.12f, 0.15f)
              .eg(I, 0.003f, 3.8f, 0.0f, 2.8f).eg(II, 0.001f, 2.4f, 0.0f, 1.7f)
              .ring(245.0f, 0.0f, 0.001f, 0.8f, 0.22f).detune(0.0f).mix(0.42f)
              .body(I, body::Model::bell, 0.98f, 0.53f, 1.34f).body(II, body::Model::bell, 0.96f, 0.71f, 0.91f)
              .eg(I, 0.002f, 4.25f, 0.0f, 3.0f).ring(245.0f, 0.0f, 0.001f, 0.8f, 0.085f).mix(0.37f));
    add("Ring Mallet", "Bells", { "ring-mod", "velocity", "dual-rank" },
        "A rounded wooden-mode body supports a short metallic upper mallet, then feeds the ring circuit. Hard strikes expose the upper clang; its rapid losses leave a softer resonating body behind.",
        bell().feet(II, f223).wave(II, false, true, 0.55f).lpf(II, 2700.0f, 0.13f)
              .feg(II, 0.0f, 0.34f, 0.001f, 0.14f, 0.10f)
              .level(I, 0.0f, 0.16f).level(II, 0.32f, 0.055f)
              .touch(I, 0.0f, 0.86f, 0.0f, 0.0f).touch(II, 0.20f, 1.0f, 0.0f, 0.0f)
              .eg(I, 0.002f, 2.5f, 0.0f, 1.5f).eg(II, 0.001f, 0.25f, 0.0f, 0.16f)
              .ring(247.0f, 0.0f, 0.001f, 0.7f, 0.33f).mix(0.34f)
              .body(I, body::Model::marimba, 0.88f, 0.46f, 0.83f).body(II, body::Model::bell, 0.70f, 0.85f, 0.30f)
              .eg(I, 0.0018f, 3.0f, 0.0f, 1.7f).ring(247.0f, 0.0f, 0.001f, 0.7f, 0.27f).mix(0.26f));
}
