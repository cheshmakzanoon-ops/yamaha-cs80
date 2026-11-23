// Brass voicings. Included inside namespace curated by CuratedLibrary.inc.
// Existing program names and insertion order are stable host/MIDI identities.
// Harmonic lip/body spectra supplement the circuit voices where appropriate.
// These are programmed synthesizer interpretations, not sampled instruments.
// Cutoffs are referenced to MIDI 60. Low HP settings leave room for the
// upward HP movement caused by initial/after-touch brilliance. The two ranks
// deliberately have different spectral and articulation roles.
// Acoustic bodies use open passbands so the lip/body formants survive the
// panel filters. Resonant size distinguishes bore/bell colors at the same
// played pitch; size values are authored voicing scales, not measured bores.

Program brass()
{
    Program x;
    x.wave(I, true, false, 0.035f).wave(II, true, false)
     .hpf(I, 42.0f).hpf(II, 68.0f).lpf(I, 760.0f, 0.18f).lpf(II, 1120.0f, 0.14f)
     .feg(I, 0.24f, 0.52f, 0.12f, 1.7f, 0.38f).feg(II, 0.18f, 0.40f, 0.085f, 1.3f, 0.30f)
     .eg(I, 0.035f, 1.8f, 0.82f, 0.42f).eg(II, 0.045f, 1.4f, 0.76f, 0.34f)
     .touch(I, 0.20f, 0.58f, 0.32f, 0.25f).touch(II, 0.26f, 0.62f, 0.40f, 0.22f)
     .level(I, 1.0f, 0.48f).level(II, 0.94f, 0.40f)
     .sub(subSine, 5.1f, 0.0f).subAt(0.04f, 0.065f)
     .g(P_modWheelSub, 0.055f).g(P_voiceDispersion, 0.18f)
     .g(P_drift, 0.08f).g(P_analogNoise, 0.012f)
     .mix(0.43f).res(0.16f).detune(3.2f).volume(0.6f);
    return x;
}

void buildBrass(Builder& add)
{
    add("Brass Swell I", "Brass", { "brilliance-swell", "aftertouch", "dual-rank" },
        "A rounded harmonic body enters before the brighter circuit rank completes its swell. The lip-like attack settles into a stable center; pressure opens the held chord and the wheel adds restrained vibrato.",
        brass().lpf(I, 2700.0f, 0.12f).lpf(II, 2400.0f, 0.15f).body(I, body::Model::brass, 0.55f, 0.40f, 1.0f)
               .bodySize(I, 1.12f).body(II, body::Model::brass, 0.28f, 0.65f, 1.0f).bodySize(II, 0.92f)
               .feg(I, 0.38f, 0.34f, 0.61f, 2.7f, 0.58f).feg(II, 0.46f, 0.40f, 0.82f, 1.7f, 0.45f)
               .eg(I, 0.085f, 2.4f, 0.88f, 0.68f).eg(II, 0.16f, 1.6f, 0.72f, 0.52f).touch(I, 0.12f, 0.56f, 0.36f, 0.30f)
               .touch(II, 0.18f, 0.64f, 0.44f, 0.25f).mix(0.38f).detune(3.5f).contour(-0.04f, -0.14f, -0.035f, 0.035f));
    add("Brass Swell II", "Brass", { "brilliance-swell", "aftertouch", "feet-layer" },
        "A modeled 16' brass foundation supports an 8' circuit crown. Staggered filter rises broaden the swell while the darker lower spectrum preserves weight; pressure brings forward the upper rank.",
        brass().feet(I, f16).wave(I, true, false, 0.06f).hpf(I, 20.0f).hpf(II, 54.0f).lpf(I, 2200.0f, 0.11f)
               .lpf(II, 3300.0f, 0.12f).body(I, body::Model::brass, 0.64f, 0.30f, 1.2f).bodySize(I, 1.45f)
               .body(II, body::Model::brass, 0.39f, 0.55f, 1.0f).bodySize(II, 1.10f)
               .feg(I, 0.36f, 0.31f, 0.79f, 3.0f, 0.61f).feg(II, 0.48f, 0.43f, 1.05f, 1.9f, 0.53f)
               .eg(I, 0.11f, 2.3f, 0.91f, 0.71f).eg(II, 0.23f, 1.6f, 0.71f, 0.55f).touch(I, 0.11f, 0.61f, 0.28f, 0.31f)
               .touch(II, 0.16f, 0.66f, 0.40f, 0.28f).mix(0.32f).detune(2.8f).res(0.15f)
               .contour(-0.07f, -0.16f, -0.06f, 0.025f));
    add("Pressure Swell Brass", "Brass", { "aftertouch", "brilliance-swell" },
        "A quiet harmonic brass body waits under the fingers. Pressure supplies most of the level and brightens both the modeled body and the circuit edge; easing pressure returns to the soft bed.",
        brass().level(I, 1.0f, 0.12f).level(II, 1.0f, 0.09f).hpf(I, 20.0f).hpf(II, 40.0f).lpf(I, 1900.0f, 0.10f)
               .lpf(II, 3200.0f, 0.12f).body(I, body::Model::brass, 0.68f, 0.22f, 1.15f).bodySize(I, 1.25f)
               .body(II, body::Model::brass, 0.35f, 0.48f, 1.0f).bodySize(II, 1.05f)
               .feg(I, 0.10f, 0.12f, 0.22f, 1.8f, 0.35f).feg(II, 0.16f, 0.18f, 0.29f, 1.1f, 0.27f)
               .eg(I, 0.11f, 2.0f, 0.93f, 0.61f).eg(II, 0.18f, 1.4f, 0.79f, 0.48f).touch(I, 0.025f, 0.06f, 0.54f, 0.92f)
               .touch(II, 0.035f, 0.08f, 0.63f, 0.96f).mix(0.36f).subAt(0.018f, 0.028f).detune(1.9f)
               .contour(-0.05f, -0.15f, -0.035f, 0.025f));
    add("Resonant Swell Brass", "Brass", { "brilliance-swell", "resonance" },
        "A warm circuit body rises ahead of a more focused resonant accent. The brighter peak relaxes sooner, keeping the sustained chord full without leaving a static nasal ring.",
        brass().touch(II, 0.18f, 0.55f, 0.28f, 0.16f).lpf(I, 740.0f, 0.22f).lpf(II, 1180.0f, 0.40f).res(0.30f)
               .feg(I, 0.38f, 0.42f, 0.40f, 2.1f, 0.63f).feg(II, 0.55f, 0.58f, 0.69f, 0.93f, 0.39f)
               .eg(I, 0.07f, 2.1f, 0.82f, 0.65f).eg(II, 0.145f, 1.15f, 0.56f, 0.43f).pw(II, 0.61f, 0.045f, 0.16f)
               .mix(0.25f).detune(2.6f).contour(-0.03f, -0.12f, -0.02f, 0.025f));
    add("Noir Swell Brass", "Brass", { "brilliance-swell", "aftertouch", "chorus", "sub-mod" },
        "A dark circuit body opens slowly beneath a later luminous upper rank. Reduced chorus and a steadier pitch center leave the evolving filter bloom clear; pressure adds intensity and gentle vibrato.",
        brass().sub(subSine, 5.0f, 0.0f).subAt(0.06f, 0.105f).bend(0.08f).mix(0.46f).pan(I, -0.24f).pan(II, 0.34f)
               .lpf(I, 780.0f, 0.15f).lpf(II, 1470.0f, 0.10f).feg(I, 0.46f, 0.50f, 0.64f, 3.6f, 1.2f)
               .feg(II, 0.60f, 0.43f, 1.02f, 1.65f, 0.92f).eg(I, 0.17f, 2.7f, 0.89f, 1.8f)
               .eg(II, 0.32f, 1.8f, 0.70f, 1.25f).touch(I, 0.10f, 0.51f, 0.40f, 0.34f)
               .touch(II, 0.15f, 0.59f, 0.54f, 0.29f).chorus(0.30f, 0.25f).detune(5.1f)
               .contour(-0.06f, -0.16f, -0.035f, 0.03f));
    add("Sforzando Brass", "Brass", { "brilliance-swell", "velocity" },
        "A bright brass attack relaxes rapidly into a quieter sustained harmonic body. The separate upper accent decays away, while velocity changes its spectral bite as well as its weight.",
        brass().level(I, 1.0f, 0.72f).level(II, 1.0f, 0.40f).hpf(I, 22.0f).hpf(II, 180.0f).lpf(I, 4200.0f, 0.09f)
               .lpf(II, 6800.0f, 0.11f).body(I, body::Model::brass, 0.76f, 0.68f, 0.85f).bodySize(I, 0.90f)
               .body(II, body::Model::brass, 0.52f, 0.90f, 0.70f).bodySize(II, 0.84f)
               .feg(I, 0.02f, 0.38f, 0.018f, 0.36f, 0.19f).feg(II, 0.0f, 0.45f, 0.006f, 0.22f, 0.10f)
               .eg(I, 0.012f, 0.48f, 0.35f, 0.25f).eg(II, 0.0025f, 0.30f, 0.0f, 0.105f)
               .touch(I, 0.14f, 0.80f, 0.26f, 0.22f).touch(II, 0.22f, 1.0f, 0.24f, 0.12f).mix(0.34f).detune(1.1f)
               .contour(-0.04f, -0.20f, -0.03f, 0.04f));
    add("Brass Stab", "Brass", { "velocity", "percussive" },
        "A compact, dry brass hit with a shaped harmonic attack beneath a shorter circuit bite. Both ranks die away on a held key; tight key releases keep repeated chords distinct.",
        brass().level(I, 1.0f, 0.28f).level(II, 1.0f, 0.20f).subAt(0.0f, 0.0f).hpf(I, 28.0f).hpf(II, 120.0f)
               .lpf(I, 3900.0f, 0.10f).lpf(II, 4100.0f, 0.16f).body(I, body::Model::brass, 0.68f, 0.72f, 0.60f)
               .bodySize(I, 0.93f).feg(I, 0.0f, 0.36f, 0.010f, 0.35f, 0.075f).feg(II, 0.0f, 0.52f, 0.004f, 0.21f, 0.055f)
               .eg(I, 0.0048f, 0.66f, 0.0f, 0.095f).eg(II, 0.0025f, 0.28f, 0.0f, 0.060f)
               .touch(I, 0.20f, 0.86f, 0.0f, 0.0f).touch(II, 0.31f, 1.0f, 0.0f, 0.0f).mix(0.31f).detune(1.2f)
               .contour(-0.025f, -0.18f, -0.025f, 0.04f));
    add("Horn Section", "Brass", { "dual-rank", "velocity" },
        "Two closely tuned harmonic brass ranks form a warm horn section. Their soft, staggered attacks and differently weighted upper harmonics give depth without a heavy chorus; pressure brings out the brass bloom.",
        brass().wave(I, true, false, 0.07f).wave(II, false, true).pw(II, 0.56f).mix(0.26f).res(0.10f).hpf(I, 20.0f)
               .hpf(II, 42.0f).lpf(I, 3300.0f, 0.065f).lpf(II, 4700.0f, 0.08f)
               .body(I, body::Model::brass, 0.93f, 0.32f, 1.20f).bodySize(I, 1.48f)
               .body(II, body::Model::brass, 0.74f, 0.48f, 1.10f).bodySize(II, 1.30f)
               .feg(I, 0.10f, 0.22f, 0.078f, 1.5f, 0.23f).feg(II, 0.18f, 0.30f, 0.12f, 1.05f, 0.18f)
               .eg(I, 0.048f, 1.7f, 0.91f, 0.26f).eg(II, 0.071f, 1.4f, 0.82f, 0.19f).touch(I, 0.10f, 0.68f, 0.25f, 0.30f)
               .touch(II, 0.14f, 0.72f, 0.32f, 0.22f).detune(1.2f).g(P_drift, 0.025f).g(P_voiceDispersion, 0.09f)
               .subAt(0.015f, 0.035f).contour(-0.05f, -0.19f, -0.045f, 0.03f));
    add("Covered Horn", "Brass", { "aftertouch" },
        "A dark, stable horn body with a restrained upper lip edge and rounded breath release. Pressure favors expressive level over brightness; the harmonic body remains clear through middle and lower-register phrasing.",
        brass().wave(I, true, false, 0.12f).wave(II, false, true).pw(II, 0.53f).detune(0.0f).res(0.08f)
               .g(P_voiceDispersion, 0.06f).g(P_drift, 0.02f).hpf(I, 20.0f).hpf(II, 105.0f).lpf(I, 1900.0f, 0.045f)
               .lpf(II, 2700.0f, 0.09f).body(I, body::Model::brass, 0.98f, 0.14f, 1.30f).bodySize(I, 1.65f)
               .body(II, body::Model::brass, 0.55f, 0.30f, 1.15f).bodySize(II, 1.45f)
               .feg(I, 0.07f, 0.16f, 0.075f, 1.3f, 0.20f).feg(II, 0.10f, 0.24f, 0.11f, 0.75f, 0.15f)
               .eg(I, 0.055f, 1.5f, 0.93f, 0.23f).eg(II, 0.071f, 0.95f, 0.70f, 0.17f).touch(I, 0.06f, 0.60f, 0.14f, 0.48f)
               .touch(II, 0.08f, 0.64f, 0.20f, 0.32f).mix(0.12f).subAt(0.015f, 0.035f)
               .contour(-0.06f, -0.21f, -0.045f, 0.035f));
    add("Muted Trumpet", "Brass", { "aftertouch", "dual-rank" },
        "A compact, bright harmonic brass body meets a narrow nasal circuit edge. A quick tongue-like accent settles before the held note; pressure sharpens the focus and adds a small expressive vibrato.",
        brass().wave(I, true, false).wave(II, false, true).pw(II, 0.78f).level(I, 1.0f, 0.42f).level(II, 0.82f, 0.32f)
               .res(0.20f).detune(0.0f).g(P_voiceDispersion, 0.08f).g(P_drift, 0.025f).hpf(I, 145.0f, 0.08f)
               .hpf(II, 820.0f, 0.18f).lpf(I, 5200.0f, 0.08f).lpf(II, 3500.0f, 0.20f)
               .body(I, body::Model::brass, 0.91f, 0.72f, 0.80f).bodySize(I, 0.72f)
               .feg(I, 0.02f, 0.20f, 0.019f, 0.45f, 0.12f).feg(II, 0.03f, 0.34f, 0.009f, 0.24f, 0.08f)
               .eg(I, 0.012f, 1.1f, 0.88f, 0.14f).eg(II, 0.006f, 0.38f, 0.36f, 0.095f)
               .touch(I, 0.10f, 0.70f, 0.22f, 0.35f).touch(II, 0.15f, 0.86f, 0.30f, 0.18f).mix(0.18f)
               .subAt(0.025f, 0.055f).contour(0.02f, -0.19f, -0.025f, 0.025f));
    add("Trombone Glide", "Brass", { "glide", "init-bend" },
        "A weighty 16' harmonic brass body follows a restrained lip scoop and smooth slide-like portamento. Its quieter edge rank speaks sooner, keeping low phrases articulate without chorus.",
        brass().feet(I, f16).feet(II, f16).wave(I, true, false, 0.06f).wave(II, true, false).res(0.12f).detune(0.0f)
               .hpf(I, 20.0f).hpf(II, 36.0f).lpf(I, 4100.0f, 0.075f).lpf(II, 5700.0f, 0.09f)
               .body(I, body::Model::brass, 1.0f, 0.43f, 1.20f).bodySize(I, 1.65f)
               .body(II, body::Model::brass, 0.87f, 0.61f, 1.00f).bodySize(II, 1.35f)
               .feg(I, 0.08f, 0.24f, 0.042f, 0.90f, 0.16f).feg(II, 0.10f, 0.30f, 0.025f, 0.55f, 0.13f)
               .eg(I, 0.030f, 1.5f, 0.91f, 0.20f).eg(II, 0.018f, 0.90f, 0.70f, 0.14f).touch(I, 0.11f, 0.70f, 0.29f, 0.34f)
               .touch(II, 0.16f, 0.76f, 0.36f, 0.24f).mix(0.16f).glide(0.072f).bend(0.075f).subAt(0.022f, 0.048f)
               .g(P_drift, 0.015f).g(P_voiceDispersion, 0.045f).contour(-0.07f, -0.17f, -0.055f, 0.035f));
    add("Fanfare Brass", "Brass", { "feet-layer", "velocity" },
        "An assertive 8' harmonic section carries a shorter 4' circuit crown. Velocity sharpens the lip-like onset, and the lighter octave settles quickly so the played root stays strong through the held chord.",
        brass().feet(II, f4).touch(I, 0.22f, 0.68f, 0.22f, 0.20f).touch(II, 0.30f, 0.80f, 0.28f, 0.16f).res(0.14f)
               .hpf(I, 28.0f).hpf(II, 160.0f).lpf(I, 4800.0f, 0.09f).lpf(II, 7500.0f, 0.10f)
               .body(I, body::Model::brass, 0.82f, 0.70f, 0.85f).bodySize(I, 0.88f)
               .body(II, body::Model::brass, 0.42f, 0.88f, 0.75f).bodySize(II, 0.82f)
               .feg(I, 0.03f, 0.32f, 0.028f, 0.95f, 0.17f).feg(II, 0.0f, 0.40f, 0.012f, 0.48f, 0.13f)
               .eg(I, 0.016f, 1.3f, 0.88f, 0.25f).eg(II, 0.009f, 0.62f, 0.42f, 0.17f).mix(0.23f).detune(0.9f)
               .contour(-0.035f, -0.21f, -0.025f, 0.045f));
    add("Brass Ensemble 16", "Brass", { "feet-layer", "chorus" },
        "A weighty 16' harmonic foundation supports a later 8' brass section. Restrained chorus joins the differently voiced ranks while preserving the low center and their separate entrances.",
        brass().feet(I, f16).wave(I, true, false, 0.08f).hpf(I, 20.0f).hpf(II, 56.0f).lpf(I, 2900.0f, 0.08f)
               .lpf(II, 4500.0f, 0.10f).body(I, body::Model::brass, 0.78f, 0.34f, 1.20f).bodySize(I, 1.50f)
               .body(II, body::Model::brass, 0.56f, 0.57f, 1.00f).bodySize(II, 1.15f)
               .feg(I, 0.25f, 0.28f, 0.24f, 2.4f, 0.40f).feg(II, 0.36f, 0.37f, 0.34f, 1.6f, 0.32f)
               .eg(I, 0.085f, 2.2f, 0.91f, 0.58f).eg(II, 0.13f, 1.5f, 0.72f, 0.41f).pan(I, -0.30f).pan(II, 0.12f)
               .mix(0.36f).chorus(0.23f, 0.21f).detune(3.5f).contour(-0.07f, -0.18f, -0.06f, 0.035f));
    add("Low Brass", "Brass", { "feet-layer", "dual-rank" },
        "Two 16' harmonic brass ranks combine a rounded low foundation with a smaller lip edge. A slower body attack and short breath-like release give weight to low phrases without continual pitch wobble.",
        brass().feet(I, f16).feet(II, f16).wave(I, true, false, 0.10f).wave(II, false, true).pw(II, 0.58f).res(0.10f)
               .hpf(I, 20.0f).hpf(II, 28.0f).lpf(I, 2500.0f, 0.06f).lpf(II, 3900.0f, 0.09f)
               .body(I, body::Model::brass, 0.96f, 0.27f, 1.35f).bodySize(I, 1.85f)
               .body(II, body::Model::brass, 0.82f, 0.45f, 1.15f).bodySize(II, 1.60f)
               .feg(I, 0.10f, 0.20f, 0.075f, 1.25f, 0.19f).feg(II, 0.14f, 0.30f, 0.045f, 0.80f, 0.15f)
               .eg(I, 0.052f, 1.7f, 0.93f, 0.26f).eg(II, 0.033f, 1.15f, 0.76f, 0.18f).touch(I, 0.10f, 0.69f, 0.21f, 0.32f)
               .touch(II, 0.15f, 0.73f, 0.28f, 0.24f).mix(0.22f).detune(0.8f).g(P_drift, 0.025f).subAt(0.015f, 0.032f)
               .contour(-0.08f, -0.18f, -0.065f, 0.025f));
    add("Velvet Brass", "Brass", { "chorus", "aftertouch" },
        "A soft harmonic center supports a dark circuit halo that enters later. Shallow pulse movement and restrained chorus keep the long release smooth; pressure adds warmth and level without a sharp edge.",
        brass().wave(I, true, false, 0.10f).wave(II, false, true).hpf(I, 30.0f).hpf(II, 62.0f)
               .touch(I, 0.08f, 0.42f, 0.22f, 0.34f).touch(II, 0.12f, 0.48f, 0.30f, 0.26f).res(0.08f)
               .lpf(I, 1700.0f, 0.06f).lpf(II, 1250.0f, 0.08f).body(I, body::Model::brass, 0.54f, 0.14f, 1.40f)
               .bodySize(I, 1.40f).feg(I, 0.26f, 0.25f, 0.45f, 3.0f, 0.85f).feg(II, 0.42f, 0.32f, 0.73f, 1.6f, 0.65f)
               .eg(I, 0.30f, 2.6f, 0.92f, 1.30f).eg(II, 0.55f, 1.8f, 0.69f, 0.94f).pw(II, 0.55f, 0.035f, 0.15f)
               .chorus(0.32f, 0.19f).detune(3.2f).mix(0.30f).contour(-0.07f, -0.20f, -0.035f, 0.03f));
    add("Bright Brass Poly", "Brass", { "velocity" },
        "Fast circuit brass comping with a rounded sustaining center and a brief, bright upper accent. Greater attack contrast and a tighter upper release keep rhythmic chord changes clear.",
        brass().level(I, 1.0f, 0.38f).level(II, 0.92f, 0.29f).res(0.12f).wave(I, true, false, 0.08f)
               .lpf(I, 1800.0f, 0.10f).lpf(II, 3200.0f, 0.14f).feg(I, 0.05f, 0.36f, 0.014f, 0.95f, 0.12f)
               .feg(II, 0.0f, 0.48f, 0.005f, 0.34f, 0.075f).eg(I, 0.008f, 1.0f, 0.73f, 0.19f)
               .eg(II, 0.003f, 0.43f, 0.28f, 0.10f).touch(I, 0.22f, 0.78f, 0.14f, 0.18f)
               .touch(II, 0.34f, 0.94f, 0.18f, 0.12f).mix(0.31f).detune(1.4f).contour(-0.025f, -0.17f, -0.015f, 0.04f));
    add("Brass Pad Swell", "Brass", { "brilliance-swell", "chorus" },
        "A soft harmonic core takes over a second to rise while a later circuit rank completes the bright bloom. The upper section remains lighter during the long overlap, giving sustained chords depth and a clear center.",
        brass().touch(I, 0.06f, 0.38f, 0.32f, 0.32f).touch(II, 0.10f, 0.44f, 0.40f, 0.28f).lpf(I, 2400.0f, 0.08f)
               .lpf(II, 1600.0f, 0.11f).body(I, body::Model::brass, 0.52f, 0.33f, 1.40f).bodySize(I, 1.30f)
               .feg(I, 0.42f, 0.35f, 1.30f, 3.8f, 1.2f).feg(II, 0.58f, 0.48f, 1.82f, 2.4f, 1.0f)
               .eg(I, 1.05f, 3.2f, 0.93f, 1.85f).eg(II, 1.65f, 2.5f, 0.72f, 1.55f).mix(0.36f).detune(4.3f)
               .chorus(0.27f, 0.18f).contour(-0.08f, -0.16f, -0.04f, 0.025f));
    add("Fifth Brass", "Brass", { "feet-layer" },
        "An 8' harmonic brass body carries a softly entering 5 1/3' circuit fifth. The fifth relaxes sooner and stays subordinate, preserving the played root in open chord voicings.",
        brass().feet(II, f513).detune(0.0f).res(0.13f).hpf(I, 24.0f).hpf(II, 115.0f).lpf(I, 3300.0f, 0.09f)
               .lpf(II, 2100.0f, 0.11f).body(I, body::Model::brass, 0.66f, 0.42f, 1.10f).bodySize(I, 1.15f)
               .feg(I, 0.14f, 0.30f, 0.095f, 1.5f, 0.30f).feg(II, 0.32f, 0.34f, 0.20f, 0.90f, 0.22f)
               .eg(I, 0.028f, 1.8f, 0.90f, 0.40f).eg(II, 0.090f, 1.0f, 0.44f, 0.25f).level(II, 0.88f, 0.34f).mix(0.19f)
               .contour(-0.045f, -0.19f, -0.035f, 0.035f));
    add("Wah Brass", "Brass", { "aftertouch", "sub-mod" },
        "A full circuit brass body acquires a rhythmic filter wah as pressure feeds the sine SUB into the VCF. Differently shaped rank envelopes preserve a warm center beneath the moving edge; the wheel adds a small vibrato.",
        brass().hpf(I, 28.0f).hpf(II, 48.0f).res(0.21f).lpf(I, 860.0f, 0.24f).lpf(II, 1680.0f, 0.30f)
               .feg(I, 0.12f, 0.24f, 0.055f, 1.4f, 0.25f).feg(II, 0.26f, 0.38f, 0.17f, 0.72f, 0.18f)
               .eg(I, 0.028f, 1.6f, 0.85f, 0.34f).eg(II, 0.051f, 1.1f, 0.64f, 0.25f).touch(I, 0.14f, 0.64f, 0.10f, 0.28f)
               .touch(II, 0.20f, 0.70f, 0.16f, 0.25f).sub(subSine, 3.7f, 0.0f, 0.0f).subAt(0.08f, 0.0f, 0.31f).mix(0.34f)
               .detune(1.6f).contour(-0.03f, -0.13f, -0.025f, 0.025f));
    add("Synth Horns", "Brass", { "pwm", "dual-rank" },
        "A firm saw body precedes a moving saw-and-pulse horn edge. Slower, shallower pulse motion and a shorter upper bloom keep the synth identity rich while reducing the static, buzzy blend.",
        brass().wave(II, true, true).level(II, 0.76f, 0.38f).wave(I, true, false, 0.055f).pw(II, 0.60f, 0.09f, 0.19f)
               .lpf(I, 1050.0f, 0.14f).lpf(II, 1840.0f, 0.13f).feg(I, 0.16f, 0.40f, 0.064f, 1.4f, 0.29f)
               .feg(II, 0.34f, 0.46f, 0.185f, 0.76f, 0.21f).eg(I, 0.022f, 1.7f, 0.86f, 0.38f)
               .eg(II, 0.072f, 0.93f, 0.60f, 0.25f).mix(0.31f).detune(3.1f).contour(-0.035f, -0.14f, -0.025f, 0.04f));
    add("Brass Bite", "Brass", { "velocity", "resonance" },
        "Soft strikes retain a rounded circuit body; hard strikes add a much shorter resonant upper bite. The accent drops away before the body, leaving pressure to shape the held bloom.",
        brass().level(I, 1.0f, 0.27f).level(II, 0.92f, 0.18f).hpf(I, 22.0f).hpf(II, 65.0f).lpf(I, 780.0f, 0.24f)
               .lpf(II, 1320.0f, 0.34f).res(0.26f).feg(I, 0.08f, 0.40f, 0.034f, 1.2f, 0.19f)
               .feg(II, 0.0f, 0.59f, 0.012f, 0.31f, 0.11f).eg(I, 0.019f, 1.5f, 0.82f, 0.28f)
               .eg(II, 0.006f, 0.43f, 0.22f, 0.12f).touch(I, 0.34f, 0.88f, 0.16f, 0.22f)
               .touch(II, 0.52f, 1.0f, 0.22f, 0.15f).mix(0.28f).detune(1.2f).contour(-0.025f, -0.18f, -0.02f, 0.04f));
    add("Distant Brass", "Brass", { "chorus" },
        "A dark harmonic center supports a softened, later circuit section. Slower entrances and a long diffuse release suggest distance, while reduced chorus preserves chord pitch and the upper edge stays subdued.",
        brass().wave(I, true, false, 0.06f).touch(I, 0.07f, 0.43f, 0.18f, 0.28f).touch(II, 0.10f, 0.48f, 0.23f, 0.22f)
               .res(0.07f).hpf(I, 20.0f).hpf(II, 90.0f).lpf(I, 1350.0f, 0.045f).lpf(II, 760.0f, 0.07f)
               .body(I, body::Model::brass, 0.58f, 0.10f, 1.50f).bodySize(I, 1.55f)
               .feg(I, 0.27f, 0.23f, 0.43f, 3.1f, 1.15f).feg(II, 0.38f, 0.26f, 0.69f, 1.85f, 0.90f)
               .eg(I, 0.28f, 2.8f, 0.90f, 1.90f).eg(II, 0.56f, 2.1f, 0.56f, 1.65f).mix(0.25f).chorus(0.38f, 0.17f)
               .detune(4.2f).contour(-0.08f, -0.23f, -0.035f, 0.025f));
    add("Brass Crescendo", "Brass", { "brilliance-swell", "aftertouch" },
        "A harmonic brass body rises for 1.6 seconds before the brighter upper section completes the crescendo. Hold the chord through the opening, then use pressure to increase harmonic intensity without retriggering the entrance.",
        brass().level(I, 1.0f, 0.66f).level(II, 0.96f, 0.58f).hpf(I, 24.0f).hpf(II, 48.0f).lpf(I, 2900.0f, 0.095f)
               .lpf(II, 4300.0f, 0.12f).body(I, body::Model::brass, 0.69f, 0.36f, 1.35f).bodySize(I, 1.18f)
               .body(II, body::Model::brass, 0.43f, 0.61f, 1.20f).bodySize(II, 0.95f)
               .feg(I, 0.52f, 0.41f, 1.82f, 3.8f, 0.75f).feg(II, 0.65f, 0.50f, 2.20f, 2.3f, 0.62f)
               .eg(I, 1.6f, 3.5f, 0.96f, 1.05f).eg(II, 2.15f, 2.3f, 0.78f, 0.82f).touch(I, 0.04f, 0.32f, 0.32f, 0.42f)
               .touch(II, 0.07f, 0.40f, 0.42f, 0.34f).mix(0.36f).detune(2.8f).contour(-0.055f, -0.17f, -0.04f, 0.03f));
    add("Cuivre Brass", "Brass", { "aftertouch", "resonance" },
        "A rounded harmonic brass body retains its root while pressure introduces a focused, overblown circuit edge. The upper lip-like accent settles quickly after the attack, leaving a responsive contrast between soft and forceful playing.",
        brass().wave(I, true, false, 0.07f).wave(II, false, true).pw(II, 0.71f).level(II, 0.88f, 0.16f).hpf(I, 20.0f)
               .hpf(II, 170.0f, 0.10f).lpf(I, 3400.0f, 0.10f).lpf(II, 2800.0f, 0.33f)
               .body(I, body::Model::brass, 0.89f, 0.40f, 1.0f).bodySize(I, 1.25f)
               .feg(I, 0.10f, 0.22f, 0.060f, 1.4f, 0.25f).feg(II, 0.16f, 0.35f, 0.035f, 0.65f, 0.16f)
               .eg(I, 0.028f, 1.8f, 0.90f, 0.30f).eg(II, 0.012f, 0.72f, 0.48f, 0.18f).touch(I, 0.12f, 0.70f, 0.30f, 0.34f)
               .touch(II, 0.24f, 0.52f, 0.66f, 0.64f).mix(0.30f).res(0.20f).detune(0.6f)
               .contour(-0.04f, -0.19f, -0.03f, 0.035f));
    add("Tuba", "Brass", { "velocity", "aftertouch" },
        "A single dry 16' harmonic brass body with a deep fundamental and dark upper spectrum. A gentle tongue onset and compact breath release support low separated phrases; pressure adds weight without ensemble detuning.",
        brass().mix(0.0f).feet(I, f16).wave(I, true, false, 0.22f).level(I, 1.0f, 0.42f).detune(0.0f).res(0.08f)
               .hpf(I, 20.0f).lpf(I, 2300.0f, 0.055f).body(I, body::Model::brass, 1.0f, 0.18f, 1.50f).bodySize(I, 2.20f)
               .feg(I, 0.07f, 0.18f, 0.052f, 1.2f, 0.14f).eg(I, 0.040f, 1.8f, 0.93f, 0.21f)
               .touch(I, 0.10f, 0.74f, 0.22f, 0.37f).subAt(0.012f, 0.024f).g(P_modWheelSub, 0.025f)
               .g(P_voiceDispersion, 0.04f).g(P_drift, 0.012f).contour(-0.045f, -0.20f, -0.06f, 0.03f));
    add("Brass Octaves", "Brass", { "feet-layer" },
        "Straight-tuned 8' and 4' brass ranks combine harmonic depth with a lighter circuit edge. The upper octave speaks slightly later and settles lower, remaining present without competing with the root.",
        brass().feet(II, f4).detune(0.0f).g(P_voiceDispersion, 0.08f).g(P_drift, 0.025f).hpf(I, 28.0f).hpf(II, 95.0f)
               .lpf(I, 3500.0f, 0.09f).lpf(II, 5900.0f, 0.08f).body(I, body::Model::brass, 0.66f, 0.45f, 1.10f)
               .bodySize(I, 1.12f).body(II, body::Model::brass, 0.43f, 0.61f, 1.0f).bodySize(II, 0.97f)
               .feg(I, 0.16f, 0.32f, 0.11f, 1.7f, 0.30f).feg(II, 0.28f, 0.34f, 0.19f, 1.1f, 0.23f)
               .eg(I, 0.032f, 1.8f, 0.90f, 0.36f).eg(II, 0.066f, 1.2f, 0.64f, 0.27f).mix(0.27f)
               .contour(-0.04f, -0.19f, -0.03f, 0.04f));
    add("Swell And Fall", "Brass", { "brilliance-swell" },
        "A warm harmonic-supported brass swell becomes darker after key release. The brighter circuit rank opens later and falls away sooner, producing a deliberate rise and closing gesture instead of a static pad tail.",
        brass().lpf(I, 2600.0f, 0.12f).lpf(II, 1700.0f, 0.16f).body(I, body::Model::brass, 0.54f, 0.34f, 1.25f)
               .bodySize(I, 1.25f).feg(I, 0.82f, 0.40f, 0.38f, 2.3f, 0.48f).feg(II, 0.47f, 0.44f, 0.81f, 1.0f, 0.16f)
               .eg(I, 0.09f, 2.1f, 0.89f, 2.2f).eg(II, 0.16f, 1.3f, 0.64f, 1.55f).mix(0.34f).detune(3.1f)
               .contour(-0.05f, -0.17f, -0.035f, 0.03f));
    add("Reed Brass", "Brass", { "dual-rank", "aftertouch" },
        "A stable harmonic brass body combines with a narrow reed-colored upper rank. Their staggered attacks and different pressure response let the reed edge emerge expressively without replacing the brass foundation.",
        brass().wave(I, true, false, 0.04f).wave(II, false, true).res(0.18f).hpf(I, 26.0f).hpf(II, 220.0f, 0.06f)
               .lpf(I, 3600.0f, 0.10f).lpf(II, 4800.0f, 0.16f).body(I, body::Model::brass, 0.79f, 0.48f, 1.0f)
               .bodySize(I, 1.08f).body(II, body::Model::oboe, 0.46f, 0.62f, 0.90f).bodySize(II, 0.90f)
               .feg(I, 0.08f, 0.23f, 0.044f, 1.3f, 0.19f).feg(II, 0.14f, 0.32f, 0.024f, 0.62f, 0.14f)
               .eg(I, 0.021f, 1.7f, 0.90f, 0.25f).eg(II, 0.012f, 0.85f, 0.59f, 0.17f).pw(II, 0.65f)
               .touch(I, 0.12f, 0.72f, 0.24f, 0.34f).touch(II, 0.18f, 0.79f, 0.36f, 0.26f).mix(0.25f).detune(0.35f)
               .subAt(0.03f, 0.06f).contour(-0.035f, -0.19f, -0.025f, 0.035f));
    add("Brass Glide Lead", "Brass", { "glide", "aftertouch" },
        "A centered harmonic brass lead follows a small upward scoop and smooth portamento. A softer circuit edge enters behind the body, while pressure supplies brightness and restrained vibrato for a single focused line.",
        brass().wave(II, false, true).pw(II, 0.60f).hpf(I, 24.0f).hpf(II, 100.0f).lpf(I, 3900.0f, 0.11f)
               .lpf(II, 2300.0f, 0.13f).body(I, body::Model::brass, 0.76f, 0.52f, 1.0f).bodySize(I, 1.05f)
               .feg(I, 0.09f, 0.30f, 0.055f, 1.3f, 0.24f).feg(II, 0.30f, 0.32f, 0.15f, 0.68f, 0.16f)
               .eg(I, 0.017f, 1.6f, 0.91f, 0.27f).eg(II, 0.043f, 1.0f, 0.62f, 0.19f).touch(I, 0.12f, 0.74f, 0.32f, 0.36f)
               .touch(II, 0.18f, 0.79f, 0.40f, 0.26f).glide(0.065f).bend(0.060f).subAt(0.03f, 0.075f).mix(0.17f)
               .detune(0.0f).contour(-0.025f, -0.17f, -0.015f, 0.045f));
    add("Tremolo Brass", "Brass", { "tremolo", "sub-mod" },
        "A soft harmonic-supported brass pad pulses gently at 4.6 Hz through the original SUB VCA route. The later, lighter circuit rank and restrained stereo tremolo preserve a rounded center through the motion.",
        brass().sub(subSine, 4.6f, 0.0f, 0.0f, 0.20f).subAt(0.0f, 0.035f).lpf(I, 2400.0f, 0.08f).lpf(II, 1400.0f, 0.10f)
               .body(I, body::Model::brass, 0.52f, 0.30f, 1.30f).bodySize(I, 1.25f)
               .feg(I, 0.20f, 0.27f, 0.32f, 2.4f, 0.51f).feg(II, 0.36f, 0.34f, 0.52f, 1.4f, 0.40f)
               .eg(I, 0.19f, 2.2f, 0.93f, 0.85f).eg(II, 0.38f, 1.5f, 0.66f, 0.69f).tremolo(0.27f, 4.6f).mix(0.32f)
               .detune(2.8f).contour(-0.055f, -0.18f, -0.035f, 0.025f));
    add("Contour Brass", "Brass", { "kbd-contour" },
        "A broad harmonic brass body and lighter circuit crown are balanced for wide keyboard voicings. A gentle brilliance contour and restrained treble level keep upper notes from becoming sharp while low chords retain their center.",
        brass().hpf(I, 32.0f).hpf(II, 62.0f).lpf(I, 3100.0f, 0.10f).lpf(II, 1720.0f, 0.13f)
               .body(I, body::Model::brass, 0.68f, 0.38f, 1.15f).bodySize(I, 1.20f)
               .feg(I, 0.14f, 0.30f, 0.11f, 1.6f, 0.30f).feg(II, 0.30f, 0.38f, 0.24f, 0.96f, 0.25f)
               .eg(I, 0.035f, 1.9f, 0.90f, 0.42f).eg(II, 0.095f, 1.05f, 0.59f, 0.27f)
               .contour(-0.09f, -0.20f, -0.045f, 0.050f).mix(0.28f).detune(2.1f));
    add("Ring Breath Brass", "Brass", { "ring-mod" },
        "A slow, shallow ring component shades a warm harmonic-supported chord. The lighter circuit rank opens later, and the ring motion emerges gently so held chords breathe without a hard amplitude pulse.",
        brass().subAt(0.02f, 0.045f).lpf(I, 2400.0f, 0.095f).lpf(II, 1500.0f, 0.13f)
               .body(I, body::Model::brass, 0.52f, 0.31f, 1.30f).bodySize(I, 1.28f)
               .feg(I, 0.20f, 0.31f, 0.28f, 2.5f, 0.52f).feg(II, 0.43f, 0.38f, 0.56f, 1.15f, 0.47f)
               .eg(I, 0.12f, 2.2f, 0.90f, 0.82f).eg(II, 0.21f, 1.4f, 0.67f, 0.60f).ring(2.2f, 0.0f, 0.75f, 2.3f, 0.065f)
               .mix(0.34f).detune(2.2f).contour(-0.05f, -0.17f, -0.035f, 0.03f));

}
