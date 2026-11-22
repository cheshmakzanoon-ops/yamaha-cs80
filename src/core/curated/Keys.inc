// Keys voicings. Included inside namespace curated by CuratedLibrary.inc.
// Existing program names and insertion order are stable host/MIDI identities.
// Synthesized instrument interpretations: the resonant body and strike have
// separate envelopes. Decay times settle by roughly 60 dB, not one time constant.

Program keys()
{
    Program x;
    x.wave(I, false, true, 0.82f).pw(I, 0.5f).feet(I, f8)
     .wave(II, false, true, 0.10f).pw(II, 0.68f).feet(II, f8)
     .hpf(I, 35.0f).lpf(I, 1550.0f, 0.06f).level(I, 0.28f, 0.34f)
     .hpf(II, 620.0f).lpf(II, 3600.0f, 0.12f).level(II, 0.86f, 0.14f)
     .feg(I, 0.0f, 0.24f, 0.003f, 1.25f, 0.20f)
     .feg(II, 0.0f, 0.42f, 0.0015f, 0.34f, 0.10f)
     .eg(I, 0.003f, 4.3f, 0.0f, 0.40f).eg(II, 0.0015f, 0.68f, 0.0f, 0.12f)
     .touch(I, 0.22f, 0.76f, 0.0f, 0.0f).touch(II, 0.34f, 0.92f, 0.0f, 0.0f)
     .sub(subSine, 5.0f, 0.0f).subAt(0.0f, 0.0f)
     .mix(0.25f).detune(0.65f).res(0.05f)
     .g(P_voiceDispersion, 0.08f).g(P_drift, 0.025f).g(P_analogNoise, 0.0f).volume(0.6f);
    return x;
}

void buildKeys(Builder& add)
{
    add("Electric Piano I", "Keys", { "velocity", "sine", "dual-rank" },
        "A rounded 8' resonator rings beneath a separately decaying strike. Soft touches stay mellow; firm strikes bring forward the short upper bark. Release the key to damp the body.",
        keys());
    add("Electric Piano II", "Keys", { "velocity", "chorus", "feet-layer" },
        "A warm 8' body supports a shorter 4' tine-like edge. Light chorus widens the decay without obscuring the strike; velocity moves the sound from round to bright.",
        keys().wave(I, false, true, 0.70f).level(I, 0.32f, 0.36f)
              .wave(II, false, true, 0.46f).feet(II, f4).pw(II, 0.70f)
              .hpf(II, 480.0f).lpf(II, 5200.0f, 0.08f).level(II, 0.42f, 0.12f)
              .eg(I, 0.0035f, 4.9f, 0.0f, 0.48f).eg(II, 0.0018f, 1.1f, 0.0f, 0.18f)
              .feg(II, 0.0f, 0.28f, 0.0015f, 0.45f, 0.12f)
              .touch(II, 0.24f, 0.86f, 0.0f, 0.0f).mix(0.20f).detune(1.2f).chorus(0.22f, 0.38f));
    add("Tine Piano", "Keys", { "ring-mod", "velocity", "sine" },
        "A long rounded body and a shorter third-harmonic strike, with a faint fixed-frequency ring edge. Firm strikes emphasize the metallic attack; the body continues after the upper rank fades.",
        keys().wave(I, false, true, 0.90f).level(I, 0.16f, 0.34f).lpf(I, 1200.0f, 0.04f)
              .wave(II, false, false, 0.72f).feet(II, f223).level(II, 0.0f, 0.08f)
              .eg(I, 0.0025f, 5.2f, 0.0f, 0.46f).eg(II, 0.0012f, 1.25f, 0.0f, 0.15f)
              .touch(II, 0.0f, 0.88f, 0.0f, 0.0f).ring(475.0f, 0.0f, 0.001f, 0.5f, 0.075f)
              .mix(0.17f).detune(0.0f));
    add("Clavichord I", "Keys", { "velocity", "percussive" },
        "A dry narrow-pulse string body with a short tangent-like upper attack. The two ranks decay independently, and harder strikes add both level and bite.",
        keys().wave(I, false, true).wave(II, false, true, 0.0f, 0.008f)
              .pw(I, 0.76f).pw(II, 0.84f).hpf(I, 110.0f).hpf(II, 850.0f)
              .lpf(I, 2200.0f, 0.12f).lpf(II, 4600.0f, 0.08f)
              .level(I, 1.0f, 0.33f).level(II, 0.86f, 0.10f)
              .feg(I, 0.0f, 0.40f, 0.0015f, 0.55f, 0.10f).feg(II, 0.0f, 0.24f, 0.001f, 0.14f, 0.06f)
              .eg(I, 0.0018f, 2.4f, 0.0f, 0.16f).eg(II, 0.001f, 0.23f, 0.0f, 0.065f)
              .touch(I, 0.25f, 0.78f, 0.0f, 0.0f).touch(II, 0.32f, 0.92f, 0.0f, 0.0f)
              .mix(0.24f).detune(0.0f));
    add("Clavichord II", "Keys", { "velocity", "resonance", "percussive" },
        "A darker, nasal clavichord interpretation with a resonant opening bite and a dry string decay. The narrow upper rank supplies the short attack rather than a held pad.",
        keys().wave(I, false, true).wave(II, false, true).pw(I, 0.70f).pw(II, 0.81f)
              .hpf(I, 100.0f).hpf(II, 520.0f).lpf(I, 1250.0f, 0.34f).lpf(II, 2800.0f, 0.22f)
              .level(I, 1.0f, 0.34f).level(II, 0.92f, 0.12f).res(0.22f)
              .feg(I, 0.0f, 0.58f, 0.002f, 0.72f, 0.11f).feg(II, 0.0f, 0.42f, 0.001f, 0.26f, 0.07f)
              .eg(I, 0.002f, 2.0f, 0.0f, 0.14f).eg(II, 0.0012f, 0.36f, 0.0f, 0.07f)
              .touch(I, 0.24f, 0.78f, 0.0f, 0.0f).touch(II, 0.38f, 0.92f, 0.0f, 0.0f)
              .mix(0.27f).detune(0.25f));
    add("Funky Clavichord", "Keys", { "velocity", "resonance", "percussive" },
        "A strong resonant snap over a compact string body. Velocity opens the attack dramatically, while the low high-pass setting preserves the note's weight.",
        keys().wave(I, false, true).pw(I, 0.66f).wave(II, true, false)
              .hpf(I, 65.0f).hpf(II, 210.0f).lpf(I, 730.0f, 0.35f).lpf(II, 1180.0f, 0.42f)
              .feg(I, 0.0f, 0.61f, 0.002f, 0.62f, 0.08f).feg(II, 0.0f, 0.74f, 0.001f, 0.28f, 0.06f)
              .eg(I, 0.0018f, 1.65f, 0.0f, 0.12f).eg(II, 0.001f, 0.40f, 0.0f, 0.07f)
              .level(I, 1.0f, 0.24f).level(II, 0.95f, 0.10f)
              .touch(I, 0.38f, 0.84f, 0.0f, 0.0f).touch(II, 0.54f, 1.0f, 0.0f, 0.0f)
              .mix(0.34f).res(0.27f).detune(0.0f));
    add("Harpsichord I", "Keys", { "percussive", "dual-rank" },
        "A bright 8' plucked register with a short quill-like edge and a longer string body. Nearly even strike level and quick key-off damping give it a deliberate harpsichord response.",
        keys().wave(I, true, true).pw(I, 0.68f).wave(II, false, true, 0.0f, 0.008f).pw(II, 0.82f)
              .hpf(I, 85.0f).hpf(II, 1200.0f).lpf(I, 4900.0f, 0.06f).lpf(II, 8000.0f, 0.04f)
              .feg(I, 0.0f, 0.12f, 0.001f, 0.9f, 0.08f).feg(II, 0.0f, 0.16f, 0.001f, 0.12f, 0.04f)
              .eg(I, 0.0012f, 3.2f, 0.0f, 0.13f).eg(II, 0.001f, 0.18f, 0.0f, 0.05f)
              .level(I, 0.86f, 0.96f).level(II, 0.78f, 0.82f)
              .touch(Both, 0.02f, 0.035f, 0.0f, 0.0f).mix(0.17f).detune(0.0f).res(0.03f));
    add("Harpsichord II", "Keys", { "percussive", "feet-layer" },
        "Coupled 8' and 4' plucked registers: the octave is bright at the strike and decays sooner than the lower strings. Little velocity variation keeps the double-register character even.",
        keys().wave(I, true, true).wave(II, false, true).pw(I, 0.69f).pw(II, 0.76f).feet(II, f4)
              .hpf(I, 75.0f).hpf(II, 180.0f).lpf(I, 4300.0f, 0.05f).lpf(II, 7200.0f, 0.05f)
              .feg(I, 0.0f, 0.14f, 0.001f, 1.1f, 0.10f).feg(II, 0.0f, 0.20f, 0.001f, 0.50f, 0.07f)
              .eg(I, 0.0013f, 3.5f, 0.0f, 0.16f).eg(II, 0.001f, 1.85f, 0.0f, 0.12f)
              .level(I, 0.88f, 0.94f).level(II, 0.70f, 0.86f)
              .touch(Both, 0.025f, 0.045f, 0.0f, 0.0f).mix(0.28f).detune(0.0f).res(0.035f));
    add("Lute Harpsichord", "Keys", { "percussive" },
        "A muted buff-stop interpretation: dark plucked strings with a restrained, quickly vanishing quill edge. The body decays naturally and damps promptly on release.",
        keys().wave(I, true, false, 0.12f).wave(II, false, true).pw(II, 0.72f)
              .hpf(I, 45.0f).hpf(II, 640.0f).lpf(I, 1250.0f, 0.05f).lpf(II, 2500.0f, 0.08f)
              .feg(I, 0.0f, 0.20f, 0.002f, 0.60f, 0.09f).feg(II, 0.0f, 0.16f, 0.001f, 0.18f, 0.05f)
              .eg(I, 0.0018f, 1.95f, 0.0f, 0.12f).eg(II, 0.001f, 0.20f, 0.0f, 0.05f)
              .level(I, 0.80f, 0.93f).level(II, 0.65f, 0.70f)
              .touch(Both, 0.02f, 0.04f, 0.0f, 0.0f).mix(0.13f).detune(0.0f).res(0.025f));
    add("Toy Piano", "Keys", { "ring-mod", "percussive", "velocity" },
        "A small high-register metal-bar interpretation: a 2' resonator under a short narrow-pulse strike, with a modest inharmonic ring edge. The attack is much shorter than the ringing body.",
        keys().wave(I, false, false, 0.96f).feet(I, f2).level(I, 0.0f, 0.30f)
              .wave(II, false, true, 0.12f).feet(II, f2).pw(II, 0.82f)
              .hpf(II, 1500.0f).lpf(II, 9000.0f, 0.10f).level(II, 0.54f, 0.10f)
              .eg(I, 0.0018f, 2.65f, 0.0f, 0.58f).eg(II, 0.001f, 0.20f, 0.0f, 0.065f)
              .touch(I, 0.0f, 0.74f, 0.0f, 0.0f).touch(II, 0.16f, 0.90f, 0.0f, 0.0f)
              .ring(460.0f, 0.0f, 0.001f, 0.3f, 0.18f).mix(0.14f).detune(0.0f));
    add("Honky Keys", "Keys", { "detune", "velocity", "dual-rank" },
        "Two independently decaying 8' string ranks give a deliberately detuned upright-like beating. A strong strike brightens the attack, then both ranks settle into a rounded decay.",
        keys().wave(I, true, false, 0.30f).wave(II, false, true, 0.22f).pw(II, 0.56f)
              .hpf(Both, 45.0f).lpf(I, 2000.0f, 0.06f).lpf(II, 2700.0f, 0.05f)
              .level(I, 0.60f, 0.34f).level(II, 0.66f, 0.30f)
              .feg(I, 0.0f, 0.32f, 0.002f, 1.1f, 0.20f).feg(II, 0.0f, 0.27f, 0.003f, 0.8f, 0.16f)
              .eg(I, 0.0025f, 4.6f, 0.0f, 0.32f).eg(II, 0.0035f, 3.8f, 0.0f, 0.27f)
              .touch(I, 0.25f, 0.74f, 0.0f, 0.0f).touch(II, 0.30f, 0.78f, 0.0f, 0.0f)
              .mix(0.43f).detune(12.5f).g(P_drift, 0.035f));
    add("Vibes Keys", "Keys", { "sub-mod", "sine", "percussive", "velocity" },
        "A long 8' sine body with a shorter octave strike. Gentle 5 Hz amplitude motion shades the ringing decay; release damps it. Velocity makes the upper attack more apparent.",
        keys().wave(I, false, false, 0.98f).wave(II, false, false, 0.58f).feet(II, f4)
              .level(I, 0.0f, 0.32f).level(II, 0.0f, 0.10f)
              .eg(I, 0.003f, 6.5f, 0.0f, 0.75f).eg(II, 0.0015f, 1.55f, 0.0f, 0.25f)
              .touch(I, 0.0f, 0.75f, 0.0f, 0.0f).touch(II, 0.0f, 0.90f, 0.0f, 0.0f)
              .sub(subSine, 5.0f, 0.0f, 0.0f, 0.16f).mix(0.16f).detune(0.0f));
    add("Celesta", "Keys", { "sine", "feet-layer", "percussive", "velocity" },
        "A delicate 4' bell-like body with a brief 2' attack partial. The upper rank disappears first, leaving a clean high-register decay with no held sustain.",
        keys().wave(I, false, false, 0.95f).feet(I, f4)
              .wave(II, false, true, 0.45f).feet(II, f2).pw(II, 0.5f)
              .hpf(II, 1800.0f).lpf(II, 7600.0f, 0.04f)
              .level(I, 0.0f, 0.29f).level(II, 0.14f, 0.09f)
              .eg(I, 0.002f, 3.7f, 0.0f, 0.48f).eg(II, 0.0012f, 0.64f, 0.0f, 0.12f)
              .touch(I, 0.0f, 0.78f, 0.0f, 0.0f).touch(II, 0.12f, 0.86f, 0.0f, 0.0f)
              .mix(0.19f).detune(0.0f));
    add("Saw Pulse Piano", "Keys", { "velocity", "dual-rank" },
        "A clearly synthetic piano: a saw body decays beneath a quicker pulse hammer. Firm strikes produce a broad bright attack; the two filters close at different rates.",
        keys().wave(I, true, false, 0.18f).wave(II, false, true).pw(II, 0.63f)
              .hpf(I, 45.0f).hpf(II, 180.0f).lpf(I, 1800.0f, 0.08f).lpf(II, 3400.0f, 0.12f)
              .level(I, 0.80f, 0.32f).level(II, 0.92f, 0.14f)
              .feg(I, 0.0f, 0.37f, 0.003f, 1.5f, 0.20f).feg(II, 0.0f, 0.48f, 0.0015f, 0.50f, 0.12f)
              .eg(I, 0.003f, 3.8f, 0.0f, 0.34f).eg(II, 0.0015f, 0.94f, 0.0f, 0.15f)
              .touch(I, 0.28f, 0.76f, 0.0f, 0.0f).touch(II, 0.38f, 0.92f, 0.0f, 0.0f)
              .mix(0.30f).detune(1.3f));
    add("Soft Keys", "Keys", { "velocity", "sine" },
        "A soft rounded resonator with a subdued felt-like strike. A slightly cushioned onset and restrained upper rank suit quiet chords; harder playing still has usable dynamic range.",
        keys().wave(I, false, true, 0.94f).level(I, 0.10f, 0.32f).lpf(I, 900.0f, 0.025f)
              .wave(II, false, true, 0.08f).pw(II, 0.53f).lpf(II, 1900.0f, 0.04f)
              .level(II, 0.34f, 0.08f).eg(I, 0.009f, 4.8f, 0.0f, 0.46f).eg(II, 0.004f, 0.48f, 0.0f, 0.12f)
              .feg(I, 0.0f, 0.10f, 0.007f, 1.0f, 0.20f).feg(II, 0.0f, 0.16f, 0.003f, 0.30f, 0.10f)
              .touch(I, 0.08f, 0.70f, 0.0f, 0.0f).touch(II, 0.16f, 0.76f, 0.0f, 0.0f)
              .mix(0.14f).detune(0.0f));
    add("Chorus Keys", "Keys", { "chorus", "velocity" },
        "Bright electric keys with a clear transient and a lightly chorused resonant body. The upper strike decays quickly enough to keep chord changes distinct.",
        keys().wave(I, false, true, 0.70f).lpf(I, 2400.0f, 0.05f).level(I, 0.38f, 0.34f)
              .wave(II, true, false, 0.12f).lpf(II, 6200.0f, 0.06f).hpf(II, 900.0f)
              .eg(I, 0.003f, 4.1f, 0.0f, 0.42f).eg(II, 0.0015f, 0.56f, 0.0f, 0.12f)
              .feg(II, 0.0f, 0.30f, 0.001f, 0.26f, 0.10f)
              .touch(II, 0.24f, 0.90f, 0.0f, 0.0f).mix(0.22f).detune(1.0f).chorus(0.28f, 0.52f));
    add("Stage Keys", "Keys", { "velocity", "dual-rank" },
        "A forward piano-like attack over a long, rounded resonator. The bright rank fades first, giving sustained chords room without leaving a static organ-like tail.",
        keys().wave(I, true, false, 0.48f).lpf(I, 1650.0f, 0.07f).level(I, 0.48f, 0.35f)
              .wave(II, false, true, 0.18f).pw(II, 0.62f).lpf(II, 4200.0f, 0.10f)
              .hpf(II, 450.0f).level(II, 0.78f, 0.13f)
              .feg(I, 0.0f, 0.27f, 0.0025f, 1.6f, 0.24f).feg(II, 0.0f, 0.44f, 0.001f, 0.42f, 0.10f)
              .eg(I, 0.0025f, 5.8f, 0.0f, 0.35f).eg(II, 0.0013f, 1.05f, 0.0f, 0.14f)
              .touch(I, 0.20f, 0.78f, 0.0f, 0.0f).touch(II, 0.36f, 0.95f, 0.0f, 0.0f)
              .mix(0.28f).detune(0.45f));
    add("Velocity Keys", "Keys", { "velocity", "dual-rank" },
        "Wide touch dynamics: soft strikes reveal the round body, while hard strikes strongly raise the bright attack rank. Both layers decay, so held keys do not leave a fixed sustaining tone.",
        keys().wave(I, false, true, 0.86f).level(I, 0.24f, 0.12f)
              .wave(II, true, true, 0.06f).pw(II, 0.61f).hpf(II, 330.0f).lpf(II, 2200.0f, 0.14f)
              .level(II, 0.94f, 0.025f).feg(II, 0.0f, 0.38f, 0.001f, 0.48f, 0.12f)
              .eg(I, 0.003f, 4.6f, 0.0f, 0.38f).eg(II, 0.001f, 0.78f, 0.0f, 0.12f)
              .touch(I, 0.20f, 0.96f, 0.0f, 0.0f).touch(II, 0.56f, 1.0f, 0.0f, 0.0f)
              .mix(0.31f).detune(0.0f));
    add("Dulcimer", "Keys", { "feet-layer", "percussive", "velocity" },
        "Hammered-string interpretation with an 8' body and a shorter bright 4' strike. The opening clang settles into a clean ringing decay; harder strikes bring out the upper course.",
        keys().wave(I, true, false, 0.24f).wave(II, false, true, 0.08f).feet(II, f4).pw(II, 0.78f)
              .hpf(I, 70.0f).hpf(II, 550.0f).lpf(I, 2400.0f, 0.08f).lpf(II, 6800.0f, 0.12f)
              .level(I, 0.68f, 0.30f).level(II, 0.76f, 0.11f)
              .feg(I, 0.0f, 0.22f, 0.0015f, 1.3f, 0.4f).feg(II, 0.0f, 0.32f, 0.001f, 0.35f, 0.16f)
              .eg(I, 0.0015f, 5.1f, 0.0f, 1.1f).eg(II, 0.001f, 0.88f, 0.0f, 0.30f)
              .touch(I, 0.18f, 0.78f, 0.0f, 0.0f).touch(II, 0.26f, 0.94f, 0.0f, 0.0f)
              .mix(0.27f).detune(1.0f));
    add("Reed Piano", "Keys", { "velocity", "dual-rank" },
        "A pulse-rich reed body with a separate short bark. Hard strikes open the resonant upper edge; the warm lower body decays more slowly and damps on key release.",
        keys().wave(I, false, true, 0.34f).pw(I, 0.59f).wave(II, false, true).pw(II, 0.69f)
              .hpf(I, 45.0f).hpf(II, 350.0f).lpf(I, 1350.0f, 0.16f).lpf(II, 2300.0f, 0.25f)
              .level(I, 0.66f, 0.32f).level(II, 0.91f, 0.09f)
              .feg(I, 0.0f, 0.24f, 0.0025f, 1.25f, 0.2f).feg(II, 0.0f, 0.48f, 0.001f, 0.40f, 0.10f)
              .eg(I, 0.003f, 4.0f, 0.0f, 0.32f).eg(II, 0.0013f, 0.72f, 0.0f, 0.12f)
              .touch(I, 0.25f, 0.77f, 0.0f, 0.0f).touch(II, 0.45f, 0.98f, 0.0f, 0.0f)
              .mix(0.30f).res(0.12f).detune(0.35f));
    add("Glass Keys", "Keys", { "sine", "chorus", "percussive", "feet-layer" },
        "A clear 4' sine resonator with a shorter 2' glint. Shallow chorus gives the decay a glassy movement; both ranks fade completely under held keys.",
        keys().wave(I, false, false, 0.94f).wave(II, false, false, 0.62f).feet(I, f4).feet(II, f2)
              .level(I, 0.0f, 0.32f).level(II, 0.0f, 0.10f)
              .eg(I, 0.003f, 4.4f, 0.0f, 0.95f).eg(II, 0.0015f, 0.90f, 0.0f, 0.25f)
              .touch(I, 0.0f, 0.73f, 0.0f, 0.0f).touch(II, 0.0f, 0.88f, 0.0f, 0.0f)
              .mix(0.18f).detune(0.5f).chorus(0.24f, 0.34f));
    add("Tremolo Keys", "Keys", { "tremolo", "velocity" },
        "A rounded electric-key body with a crisp upper strike, shaded by moderate tremolo. The ringing body carries the motion after the short attack rank has faded.",
        keys().wave(I, false, true, 0.76f).lpf(I, 1750.0f, 0.06f).level(I, 0.33f, 0.35f)
              .wave(II, false, true, 0.12f).pw(II, 0.65f).lpf(II, 3200.0f, 0.09f)
              .eg(I, 0.003f, 5.2f, 0.0f, 0.50f).eg(II, 0.0015f, 0.74f, 0.0f, 0.15f)
              .feg(II, 0.0f, 0.35f, 0.0015f, 0.40f, 0.12f)
              .mix(0.23f).detune(0.5f).tremolo(0.34f, 4.8f));
    add("Contour Keys", "Keys", { "kbd-contour", "velocity" },
        "Electric keys balanced across the keyboard: a fuller low body and more present treble attack. The contour adds register contrast while the strike and body retain separate decays.",
        keys().wave(I, false, true, 0.75f).level(I, 0.34f, 0.33f).lpf(I, 1400.0f, 0.06f)
              .wave(II, true, false, 0.08f).lpf(II, 3700.0f, 0.08f).hpf(II, 520.0f)
              .eg(I, 0.0035f, 4.8f, 0.0f, 0.42f).eg(II, 0.0015f, 0.62f, 0.0f, 0.13f)
              .feg(I, 0.0f, 0.22f, 0.003f, 1.45f, 0.20f)
              .contour(-0.08f, 0.26f, 0.12f, -0.04f).mix(0.24f).detune(0.4f));
    add("Pressure Keys", "Keys", { "aftertouch", "velocity", "dual-rank" },
        "A percussive piano-like strike settles into a quiet held body. Press into that remaining body to raise and brighten it; the short hammer rank stays percussive.",
        keys().wave(I, false, true, 0.62f).lpf(I, 1450.0f, 0.12f).level(I, 0.42f, 0.22f)
              .wave(II, false, true, 0.06f).pw(II, 0.68f).level(II, 0.82f, 0.12f)
              .eg(I, 0.003f, 4.4f, 0.16f, 0.55f).eg(II, 0.0015f, 0.70f, 0.0f, 0.14f)
              .feg(I, 0.0f, 0.26f, 0.003f, 1.3f, 0.3f)
              .touch(I, 0.20f, 0.60f, 0.32f, 0.72f).touch(II, 0.30f, 0.90f, 0.0f, 0.0f)
              .mix(0.24f).detune(0.45f));
}
