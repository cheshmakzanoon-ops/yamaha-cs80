// Plucked voicings. Included inside namespace curated by CuratedLibrary.inc.
// Existing program names and insertion order are stable host/MIDI identities.
// Independent body and pick envelopes describe synthesized interpretations,
// with deliberate touch dynamics and no implied samples or physical model.

Program plucked()
{
    Program x;
    x.wave(I, true, false, 0.16f).wave(II, false, true, 0.0f, 0.012f).pw(II, 0.72f)
     .feet(Both, f8).hpf(I, 45.0f).hpf(II, 850.0f)
     .lpf(I, 1150.0f, 0.10f).lpf(II, 4200.0f, 0.10f)
     .feg(I, 0.0f, 0.36f, 0.0025f, 1.15f, 0.24f)
     .feg(II, 0.0f, 0.26f, 0.0012f, 0.20f, 0.07f)
     .eg(I, 0.0025f, 3.7f, 0.0f, 0.65f).eg(II, 0.0012f, 0.32f, 0.0f, 0.09f)
     .level(I, 0.88f, 0.35f).level(II, 0.88f, 0.12f)
     .touch(I, 0.18f, 0.72f, 0.0f, 0.0f).touch(II, 0.28f, 0.94f, 0.0f, 0.0f)
     .sub(subSine, 5.1f, 0.0f).subAt(0.0f, 0.0f)
     .mix(0.22f).detune(0.0f).res(0.06f)
     .g(P_voiceDispersion, 0.08f).g(P_drift, 0.025f).g(P_analogNoise, 0.0f).volume(0.6f);
    return x;
}

void buildPlucked(Builder& add)
{
    add("Guitar I", "Plucked", { "velocity", "dual-rank" },
        "A warm 8' string body beneath a short, brighter pick rank. Harder strikes increase the pick as well as the body; the upper edge fades first, leaving a dry natural-style decay.",
        plucked());
    add("Guitar II", "Plucked", { "velocity", "dual-rank" },
        "A brighter, longer-ringing guitar interpretation with a firm pick and a clear string center. The two ranks have independent decays, so the attack does not remain as a sustained buzz.",
        plucked().wave(I, true, false, 0.12f).lpf(I, 1750.0f, 0.11f)
                 .wave(II, false, true, 0.0f, 0.018f).pw(II, 0.76f).lpf(II, 6200.0f, 0.08f)
                 .feg(I, 0.0f, 0.32f, 0.002f, 1.45f, 0.32f).feg(II, 0.0f, 0.30f, 0.001f, 0.28f, 0.09f)
                 .eg(I, 0.002f, 4.8f, 0.0f, 0.85f).eg(II, 0.001f, 0.50f, 0.0f, 0.12f)
                 .touch(I, 0.20f, 0.73f, 0.0f, 0.0f).touch(II, 0.30f, 0.96f, 0.0f, 0.0f)
                 .mix(0.25f).detune(0.4f));
    add("Funky Pluck I", "Plucked", { "resonance", "velocity", "percussive" },
        "A compact resonant snap over a rounded pulse body. The opening edge closes quickly and both ranks decay fully, making repeated short notes articulate cleanly.",
        plucked().wave(I, false, true, 0.10f).pw(I, 0.64f).wave(II, true, false)
                 .hpf(I, 38.0f).hpf(II, 220.0f).lpf(I, 680.0f, 0.34f).lpf(II, 1080.0f, 0.45f)
                 .feg(I, 0.0f, 0.67f, 0.002f, 0.62f, 0.10f).feg(II, 0.0f, 0.78f, 0.001f, 0.24f, 0.06f)
                 .eg(I, 0.0018f, 1.45f, 0.0f, 0.14f).eg(II, 0.001f, 0.30f, 0.0f, 0.07f)
                 .level(I, 1.0f, 0.30f).level(II, 0.96f, 0.10f)
                 .touch(I, 0.28f, 0.78f, 0.0f, 0.0f).touch(II, 0.42f, 0.98f, 0.0f, 0.0f)
                 .res(0.28f).mix(0.31f));
    add("Funky Pluck II", "Plucked", { "resonance", "velocity" },
        "A broader wah-shaped pluck: the body filter closes more slowly while a short upper snap marks the strike. Hold for the rounded decay or release early for a damped phrase.",
        plucked().wave(I, false, true, 0.08f).pw(I, 0.69f).wave(II, true, false)
                 .hpf(I, 42.0f).hpf(II, 320.0f).lpf(I, 560.0f, 0.37f).lpf(II, 1550.0f, 0.28f)
                 .feg(I, 0.0f, 0.72f, 0.008f, 1.65f, 0.18f).feg(II, 0.0f, 0.52f, 0.002f, 0.47f, 0.09f)
                 .eg(I, 0.003f, 2.9f, 0.0f, 0.30f).eg(II, 0.0015f, 0.62f, 0.0f, 0.12f)
                 .level(I, 1.0f, 0.34f).level(II, 0.84f, 0.14f)
                 .touch(I, 0.24f, 0.74f, 0.0f, 0.0f).touch(II, 0.30f, 0.90f, 0.0f, 0.0f)
                 .mix(0.25f).res(0.25f));
    add("Harp", "Plucked", { "sine", "feet-layer", "velocity" },
        "A long 8' resonant body with a shorter 4' finger-plucked glint. The upper harmonics fade first, leaving a round clear tail; release controls the damping.",
        plucked().wave(I, true, false, 0.78f).wave(II, false, true, 0.46f).feet(II, f4).pw(II, 0.55f)
                 .hpf(I, 30.0f).hpf(II, 480.0f).lpf(I, 1700.0f, 0.025f).lpf(II, 4200.0f, 0.035f)
                 .level(I, 0.18f, 0.32f).level(II, 0.34f, 0.10f)
                 .feg(I, 0.0f, 0.12f, 0.003f, 2.1f, 0.65f).feg(II, 0.0f, 0.24f, 0.0015f, 0.65f, 0.25f)
                 .eg(I, 0.003f, 6.8f, 0.0f, 2.3f).eg(II, 0.0015f, 1.95f, 0.0f, 0.65f)
                 .touch(I, 0.08f, 0.76f, 0.0f, 0.0f).touch(II, 0.18f, 0.92f, 0.0f, 0.0f)
                 .mix(0.24f).res(0.025f).detune(0.0f));
    add("Koto", "Plucked", { "init-bend", "velocity", "percussive" },
        "A focused narrow-pulse string with a sharp plectrum edge and a slight upward settling of the initial pitch. The pick rank dies quickly while the string body rings.",
        plucked().wave(I, false, true, 0.07f).pw(I, 0.78f)
                 .wave(II, false, true, 0.0f, 0.025f).pw(II, 0.88f)
                 .hpf(I, 100.0f).hpf(II, 1050.0f).lpf(I, 2350.0f, 0.17f).lpf(II, 6400.0f, 0.12f)
                 .feg(I, 0.0f, 0.40f, 0.0015f, 0.95f, 0.25f).feg(II, 0.0f, 0.26f, 0.001f, 0.14f, 0.05f)
                 .eg(I, 0.0018f, 3.3f, 0.0f, 0.62f).eg(II, 0.001f, 0.19f, 0.0f, 0.06f)
                 .level(I, 0.96f, 0.32f).level(II, 0.82f, 0.10f)
                 .touch(I, 0.20f, 0.76f, 0.0f, 0.0f).touch(II, 0.28f, 0.94f, 0.0f, 0.0f)
                 .mix(0.23f).bend(0.08f).res(0.10f));
    add("Banjo", "Plucked", { "percussive", "velocity", "noise" },
        "A short nasal string body with a very brief noisy pick edge. The high harmonics fall away quickly, giving a dry banjo-like articulation suited to repeated notes.",
        plucked().wave(I, false, true, 0.035f).pw(I, 0.84f)
                 .wave(II, false, true, 0.0f, 0.055f).pw(II, 0.90f)
                 .hpf(I, 180.0f, 0.06f).hpf(II, 1250.0f).lpf(I, 2850.0f, 0.22f).lpf(II, 6500.0f, 0.08f)
                 .feg(I, 0.0f, 0.31f, 0.0015f, 0.42f, 0.10f).feg(II, 0.0f, 0.24f, 0.001f, 0.10f, 0.04f)
                 .eg(I, 0.0015f, 1.85f, 0.0f, 0.18f).eg(II, 0.001f, 0.14f, 0.0f, 0.045f)
                 .level(I, 1.0f, 0.34f).level(II, 0.82f, 0.10f)
                 .touch(I, 0.20f, 0.74f, 0.0f, 0.0f).touch(II, 0.26f, 0.94f, 0.0f, 0.0f)
                 .mix(0.19f).res(0.12f));
    add("Mandolin", "Plucked", { "sub-mod", "detune", "velocity", "dual-rank" },
        "Two close 8' string courses with independent decays and a faint, smooth level flutter. Repeated strikes make the tremolo-picking pattern; the paired ranks supply the bright compact attacks.",
        plucked().wave(I, true, false, 0.08f).wave(II, false, true, 0.06f).pw(II, 0.72f)
                 .hpf(I, 100.0f).hpf(II, 140.0f).lpf(I, 2450.0f, 0.10f).lpf(II, 3200.0f, 0.12f)
                 .level(I, 0.92f, 0.34f).level(II, 0.78f, 0.28f)
                 .feg(I, 0.0f, 0.30f, 0.0018f, 0.76f, 0.12f).feg(II, 0.0f, 0.35f, 0.001f, 0.52f, 0.10f)
                 .eg(I, 0.0018f, 3.2f, 0.0f, 0.28f).eg(II, 0.0025f, 2.65f, 0.0f, 0.22f)
                 .touch(I, 0.20f, 0.72f, 0.0f, 0.0f).touch(II, 0.24f, 0.78f, 0.0f, 0.0f)
                 .sub(subSine, 12.0f, 0.0f, 0.0f, 0.025f).mix(0.43f).detune(3.4f));
    add("Drone Pluck", "Plucked", { "ring-mod", "resonance", "aftertouch" },
        "A resonant picked onset settles into a quiet held drone with a faint metallic ring. Pressure raises the remaining body after the short pick rank has disappeared.",
        plucked().wave(I, false, true, 0.22f).pw(I, 0.64f).wave(II, true, false)
                 .hpf(I, 40.0f).hpf(II, 420.0f).lpf(I, 980.0f, 0.26f).lpf(II, 2600.0f, 0.18f)
                 .feg(I, 0.0f, 0.48f, 0.003f, 1.4f, 0.50f).feg(II, 0.0f, 0.40f, 0.001f, 0.28f, 0.08f)
                 .eg(I, 0.0025f, 3.0f, 0.18f, 0.90f).eg(II, 0.001f, 0.40f, 0.0f, 0.10f)
                 .level(I, 0.90f, 0.30f).touch(I, 0.16f, 0.68f, 0.20f, 0.36f)
                 .ring(110.0f, 0.0f, 0.001f, 1.0f, 0.09f).mix(0.21f).res(0.20f));
    add("Nylon Pluck", "Plucked", { "sine", "velocity", "noise" },
        "A warm string body balances a rounded fundamental with filtered saw harmonics. A separate soft finger transient and trace of air disappear rapidly while the clear body decays, without chorus or detuning.",
        plucked().wave(I, true, false, 0.35f).wave(II, false, true, 0.0f, 0.06f).pw(II, 0.55f)
                 .hpf(I, 35.0f).hpf(II, 350.0f).lpf(I, 1200.0f, 0.035f).lpf(II, 2800.0f, 0.06f)
                 .level(I, 0.65f, 0.34f).level(II, 0.82f, 0.10f)
                 .feg(I, 0.0f, 0.21f, 0.0035f, 1.3f, 0.3f).feg(II, 0.0f, 0.12f, 0.001f, 0.12f, 0.04f)
                 .eg(I, 0.0035f, 4.1f, 0.0f, 0.75f).eg(II, 0.001f, 0.18f, 0.0f, 0.05f)
                 .touch(I, 0.12f, 0.72f, 0.0f, 0.0f).touch(II, 0.08f, 0.90f, 0.0f, 0.0f)
                 .mix(0.20f).res(0.025f).detune(0.0f));
    add("Steel Pluck", "Plucked", { "feet-layer", "velocity", "dual-rank" },
        "Paired 8' and 4' steel-string courses with a bright opening shimmer. The octave course fades faster than the body, and a very small tuning offset adds restrained beating.",
        plucked().wave(I, true, true, 0.10f).pw(I, 0.58f)
                 .wave(II, false, true, 0.12f).pw(II, 0.73f).feet(II, f4)
                 .hpf(I, 55.0f).hpf(II, 190.0f).lpf(I, 2150.0f, 0.08f).lpf(II, 6100.0f, 0.10f)
                 .level(I, 0.70f, 0.34f).level(II, 0.61f, 0.20f)
                 .feg(I, 0.0f, 0.31f, 0.002f, 1.4f, 0.34f).feg(II, 0.0f, 0.26f, 0.0012f, 0.65f, 0.25f)
                 .eg(I, 0.002f, 4.9f, 0.0f, 0.85f).eg(II, 0.0012f, 2.8f, 0.0f, 0.55f)
                 .touch(I, 0.20f, 0.74f, 0.0f, 0.0f).touch(II, 0.28f, 0.86f, 0.0f, 0.0f)
                 .mix(0.29f).detune(1.5f));
    add("Muted Pluck", "Plucked", { "percussive", "velocity", "noise" },
        "A palm-muted pulse body with a brief pick click. Both ranks die quickly even while the note is held; the low body retains weight beneath the short upper attack.",
        plucked().wave(I, false, true, 0.12f).pw(I, 0.58f)
                 .wave(II, false, true, 0.0f, 0.045f).pw(II, 0.80f)
                 .hpf(I, 35.0f).hpf(II, 1200.0f).lpf(I, 760.0f, 0.12f).lpf(II, 4500.0f, 0.08f)
                 .feg(I, 0.0f, 0.34f, 0.0018f, 0.26f, 0.06f).feg(II, 0.0f, 0.20f, 0.001f, 0.075f, 0.035f)
                 .eg(I, 0.0018f, 0.62f, 0.0f, 0.09f).eg(II, 0.001f, 0.11f, 0.0f, 0.04f)
                 .level(I, 0.91f, 0.30f).level(II, 0.82f, 0.10f)
                 .touch(I, 0.19f, 0.76f, 0.0f, 0.0f).touch(II, 0.25f, 0.94f, 0.0f, 0.0f)
                 .mix(0.19f).res(0.08f));
    add("Kalimba", "Plucked", { "sine", "feet-layer", "percussive", "velocity" },
        "A clear 4' tine-like body with a brief 2' strike harmonic. The upper ping falls away early, leaving a stable round decay; velocity determines how prominently it speaks.",
        plucked().wave(I, false, false, 0.96f).feet(I, f4)
                 .wave(II, false, true, 0.54f).pw(II, 0.5f).feet(II, f2)
                 .hpf(II, 1700.0f).lpf(II, 7800.0f, 0.06f)
                 .level(I, 0.0f, 0.30f).level(II, 0.20f, 0.08f)
                 .eg(I, 0.002f, 3.25f, 0.0f, 0.70f).eg(II, 0.001f, 0.38f, 0.0f, 0.12f)
                 .feg(II, 0.0f, 0.17f, 0.001f, 0.19f, 0.075f)
                 .touch(I, 0.0f, 0.78f, 0.0f, 0.0f).touch(II, 0.14f, 0.94f, 0.0f, 0.0f)
                 .mix(0.23f).res(0.025f).detune(0.0f));
    add("Zither", "Plucked", { "chorus", "velocity", "dual-rank" },
        "Bright paired 8' strings with a long body decay and an independently fading upper course. Shallow chorus and a small tuning offset supply a restrained ensemble shimmer.",
        plucked().wave(I, true, false, 0.12f).wave(II, false, true, 0.06f).pw(II, 0.76f)
                 .hpf(I, 75.0f).hpf(II, 140.0f).lpf(I, 2600.0f, 0.08f).lpf(II, 4400.0f, 0.10f)
                 .level(I, 0.82f, 0.33f).level(II, 0.70f, 0.23f)
                 .feg(I, 0.0f, 0.25f, 0.0018f, 1.4f, 0.65f).feg(II, 0.0f, 0.31f, 0.001f, 0.65f, 0.42f)
                 .eg(I, 0.0018f, 5.6f, 0.0f, 1.8f).eg(II, 0.001f, 2.7f, 0.0f, 1.15f)
                 .touch(I, 0.18f, 0.73f, 0.0f, 0.0f).touch(II, 0.24f, 0.84f, 0.0f, 0.0f)
                 .mix(0.34f).detune(2.2f).chorus(0.16f, 0.28f));
    add("Dulcimer Pluck", "Plucked", { "feet-layer", "percussive", "velocity" },
        "An articulate 8' saw body with a bright, short 4' pulse pick. The octave edge decays much sooner than the lower string, giving a clear plucked-course attack.",
        plucked().wave(I, true, false, 0.15f).wave(II, false, true, 0.04f).feet(II, f4).pw(II, 0.79f)
                 .hpf(I, 65.0f).hpf(II, 520.0f).lpf(I, 1900.0f, 0.10f).lpf(II, 5800.0f, 0.14f)
                 .level(I, 0.82f, 0.33f).level(II, 0.78f, 0.11f)
                 .feg(I, 0.0f, 0.31f, 0.002f, 1.0f, 0.28f).feg(II, 0.0f, 0.38f, 0.001f, 0.25f, 0.10f)
                 .eg(I, 0.002f, 3.45f, 0.0f, 0.75f).eg(II, 0.001f, 0.58f, 0.0f, 0.16f)
                 .touch(I, 0.20f, 0.75f, 0.0f, 0.0f).touch(II, 0.29f, 0.96f, 0.0f, 0.0f)
                 .mix(0.27f).detune(0.6f));
    add("Pressure Pluck", "Plucked", { "aftertouch", "velocity", "dual-rank" },
        "A short picked edge over a decaying body that leaves a quiet held bed. Press into the remaining body to swell and brighten it after the pick; release the key to damp it.",
        plucked().wave(I, true, false, 0.24f).wave(II, false, true).pw(II, 0.70f)
                 .hpf(I, 35.0f).hpf(II, 650.0f).lpf(I, 1050.0f, 0.14f).lpf(II, 3400.0f, 0.12f)
                 .level(I, 0.82f, 0.23f).level(II, 0.86f, 0.11f)
                 .feg(I, 0.0f, 0.36f, 0.003f, 1.2f, 0.35f).feg(II, 0.0f, 0.31f, 0.001f, 0.26f, 0.08f)
                 .eg(I, 0.0025f, 3.0f, 0.14f, 0.68f).eg(II, 0.0012f, 0.42f, 0.0f, 0.10f)
                 .touch(I, 0.16f, 0.62f, 0.32f, 0.68f).touch(II, 0.26f, 0.94f, 0.0f, 0.0f)
                 .mix(0.22f).res(0.09f));
}
