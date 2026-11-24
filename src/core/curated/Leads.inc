// Leads voicings. Included inside namespace curated by CuratedLibrary.inc.
// Existing program names and insertion order are stable host/MIDI identities.
// A playable center tone precedes expression. Pressure/wheel pitch depths are
// restrained except for explicitly named trill and wide-detune techniques.
// Every named voice owns its fixed/initial/after-touch balance. Small fixed
// floors leave room for touch; the pressure-led voices deliberately retain a
// different balance. Rank-II accents end before their sustained foundations.
// The three bypass-sine voices are articulated in the VCA, not an idle filter.

Program lead()
{
    Program x;
    x.wave(I, true, false, 0.05f).wave(II, false, true).pw(II, 0.58f)
     .hpf(I, 55.0f).hpf(II, 110.0f).lpf(I, 1850.0f, 0.18f).lpf(II, 2850.0f, 0.14f)
     .feg(I, 0.06f, 0.26f, 0.018f, 0.9f, 0.23f).feg(II, 0.0f, 0.33f, 0.011f, 0.55f, 0.17f)
     .eg(I, 0.012f, 1.3f, 0.88f, 0.24f).eg(II, 0.020f, 0.9f, 0.72f, 0.18f)
     .touch(I, 0.16f, 0.56f, 0.26f, 0.30f).touch(II, 0.24f, 0.58f, 0.36f, 0.24f)
     .sub(subSine, 5.2f, 0.0f).subAt(0.04f, 0.095f).g(P_modWheelSub, 0.055f)
     .bend(0.055f).glide(0.025f).detune(1.5f).res(0.14f)
     .level(I, 1.0f, 0.36f).level(II, 0.88f, 0.30f).mix(0.32f)
     .g(P_voiceDispersion, 0.10f).g(P_drift, 0.035f).g(P_analogNoise, 0.0f).volume(0.6f);
    return x;
}

void buildLeads(Builder& add)
{
    add("Touch Lead", "Leads", { "aftertouch", "init-bend", "velocity" },
        "A firm saw body with a lighter pulse edge. Velocity shapes the onset; pressure adds level, brilliance and gentle vibrato. The wheel supplies additional controlled vibrato.",
        lead().bend(0.045f).mix(0.20f).feg(I, 0.07f, 0.23f, 0.017f, 1.50f, 0.23f)
              .feg(II, 0.12f, 0.34f, 0.034f, 0.48f, 0.15f).eg(I, 0.007f, 2.0f, 0.96f, 0.15f)
              .eg(II, 0.018f, 0.52f, 0.34f, 0.1f).pw(II, 0.59f, 0.012f, 0.27f).touch(I, 0.16f, 0.96f, 0.27f, 0.49f)
              .touch(II, 0.24f, 1.0f, 0.40f, 0.38f).hpf(I, 44.0f).hpf(II, 190.0f).lpf(I, 2050.0f, 0.13f)
              .lpf(II, 3350.0f, 0.10f).contour(-0.065f, -0.16f, -0.025f, 0.07f)
              .level(I, 1.0f, 0.035f).level(II, 0.88f, 0.018f));
    add("Aftertouch Vibrato Lead", "Leads", { "aftertouch", "sub-mod" },
        "A centered, singing lead whose vibrato grows with pressure to roughly a quarter-tone peak-to-peak. The quiet dry tone is stable; pressure supplies most of the expressive level and vibrato, while velocity gives a smaller onset contrast.",
        lead().eg(I, 0.032f, 1.8f, 0.98f, 0.23f).eg(II, 0.051f, 0.7f, 0.52f, 0.14f).sub(subSine, 5.0f, 0.0f)
              .subAt(0.055f, 0.16f).g(P_modWheelSub, 0.04f).detune(0.25f).bend(0.0f).mix(0.16f)
              .wave(I, true, false, 0.14f).hpf(II, 230.0f).lpf(I, 1850.0f, 0.09f).lpf(II, 3250.0f, 0.11f)
              .feg(I, 0.04f, 0.16f, 0.032f, 1.70f, 0.25f).feg(II, 0.10f, 0.23f, 0.057f, 0.65f, 0.19f)
              .touch(I, 0.10f, 0.34f, 0.16f, 0.78f).touch(II, 0.16f, 0.42f, 0.24f, 0.62f).hpf(I, 42.0f)
              .g(P_drift, 0.012f).contour(-0.07f, -0.14f, -0.025f, 0.075f)
              .level(I, 1.0f, 0.18f).level(II, 0.88f, 0.075f));
    add("Ribbon Glide Lead", "Leads", { "ribbon", "glide", "aftertouch" },
        "A smooth, centered lead for ribbon gestures. Short portamento and a small upward onset scoop connect phrases; a restrained upper edge keeps large slides readable.",
        lead().wave(I, true, false, 0.14f).lpf(I, 1720.0f, 0.085f).lpf(II, 2750.0f, 0.10f).glide(0.075f).bend(0.055f)
              .detune(0.0f).subAt(0.03f, 0.08f).mix(0.21f).pw(II, 0.565f, 0.016f, 0.21f)
              .feg(I, 0.035f, 0.16f, 0.038f, 1.85f, 0.26f).feg(II, 0.085f, 0.24f, 0.066f, 0.72f, 0.19f)
              .eg(I, 0.012f, 2.15f, 0.98f, 0.18f).eg(II, 0.026f, 0.94f, 0.54f, 0.12f)
              .touch(II, 0.16f, 0.96f, 0.28f, 0.36f).contour(-0.085f, -0.17f, -0.035f, 0.085f).hpf(I, 40.0f)
              .hpf(II, 190.0f).touch(I, 0.12f, 0.88f, 0.21f, 0.4f)
              .level(I, 1.0f, 0.07f).level(II, 0.88f, 0.035f));
    add("Noir Lead", "Leads", { "aftertouch", "brilliance-swell", "init-bend", "chorus" },
        "A dark saw body blooms into a later brass edge. Pressure opens the sustained tone and adds controlled vibrato; moderate chorus and a long release support spacious single lines.",
        lead().wave(II, true, false).hpf(I, 35.0f).hpf(II, 135.0f).lpf(I, 910.0f, 0.18f).lpf(II, 1650.0f, 0.14f)
              .touch(I, 0.10f, 0.84f, 0.38f, 0.56f).subAt(0.04f, 0.12f).bend(0.085f).glide(0.06f).chorus(0.22f, 0.29f)
              .detune(3.2f).res(0.20f).mix(0.34f).wave(I, true, false, 0.18f).pw(II, 0.62f, 0.035f, 0.22f)
              .feg(I, 0.17f, 0.28f, 0.14f, 2.25f, 0.39f).feg(II, 0.25f, 0.39f, 0.25f, 1.05f, 0.28f)
              .eg(I, 0.042f, 2.5f, 0.96f, 0.56f).eg(II, 0.11f, 1.15f, 0.53f, 0.34f)
              .touch(II, 0.12f, 0.94f, 0.33f, 0.52f).contour(-0.10f, -0.16f, -0.03f, 0.065f)
              .level(I, 1.0f, 0.09f).level(II, 0.88f, 0.045f));
    add("Neo-Noir Brass Lead", "Leads", { "aftertouch", "chorus", "brilliance-swell" },
        "A dark harmonic brass body supports a slower saw-and-pulse bloom and a sustained release. Pressure brings out the brighter crown while moderate detune supplies width.",
        lead().wave(I, true, false, 0.10f).wave(II, true, true).pw(II, 0.565f, 0.065f, 0.17f).lpf(I, 1950.0f, 0.11f)
              .lpf(II, 1410.0f, 0.12f).chorus(0.30f, 0.23f).detune(5.6f).subAt(0.035f, 0.09f).bend(0.04f).mix(0.37f)
              .feg(I, 0.28f, 0.42f, 0.12f, 2.55f, 0.44f).feg(II, 0.39f, 0.53f, 0.22f, 1.12f, 0.31f)
              .eg(I, 0.046f, 2.4f, 0.96f, 0.55f).eg(II, 0.118f, 1.08f, 0.44f, 0.3f).hpf(I, 32.0f).hpf(II, 185.0f)
              .touch(I, 0.16f, 0.88f, 0.31f, 0.5f).touch(II, 0.24f, 0.97f, 0.42f, 0.48f)
              .contour(-0.09f, -0.18f, -0.025f, 0.06f)
              .level(I, 1.0f, 0.08f).level(II, 0.88f, 0.04f)
              .body(I, body::Model::brass, 0.44f, 0.28f, 1.25f).bodySize(I, 1.35f));
    add("Glide Lead", "Leads", { "glide", "aftertouch" },
        "A steady saw-and-pulse tone with long, smooth portamento between notes. Gentle envelope motion keeps the glide audible without adding an onset pitch scoop.",
        lead().glide(0.22f).bend(0.0f).detune(0.35f).lpf(I, 1950.0f, 0.10f).lpf(II, 3050.0f, 0.075f).mix(0.16f)
              .wave(I, true, false, 0.18f).pw(II, 0.55f, 0.014f, 0.21f).feg(I, 0.02f, 0.12f, 0.052f, 2.0f, 0.23f)
              .feg(II, 0.06f, 0.18f, 0.078f, 0.76f, 0.17f).eg(I, 0.011f, 2.25f, 1.0f, 0.18f)
              .eg(II, 0.024f, 0.98f, 0.49f, 0.12f).touch(II, 0.13f, 0.94f, 0.22f, 0.33f).hpf(I, 42.0f).hpf(II, 195.0f)
              .touch(I, 0.11f, 0.9f, 0.20f, 0.41f).contour(-0.07f, -0.16f, -0.025f, 0.08f)
              .level(I, 1.0f, 0.065f).level(II, 0.88f, 0.032f));
    add("Sine Lead", "Leads", { "sine", "aftertouch" },
        "One pure 8' sine with a rounded onset. Pressure controls level and a gentle vibrato; filter brilliance is intentionally inactive on this bypass-sine voice.",
        lead().wave(I, false, false, 1.0f).wave(II, false, false).mix(0.0f).detune(0.0f).level(I, 0.0f, 0.035f).level(II, 0.0f, 0.0f).touch(II, 0.0f, 0.0f, 0.0f, 0.0f).subAt(0.025f, 0.085f).bend(0.0f).glide(0.020f)
              .eg(I, 0.019f, 1.4f, 1.0f, 0.105f).touch(I, 0.0f, 0.96f, 0.0f, 0.52f).contour(0.0f, 0.0f, -0.095f, 0.18f)
              .g(P_voiceDispersion, 0.012f).g(P_drift, 0.0f));
    add("Whistle Lead", "Leads", { "sine", "init-bend", "aftertouch" },
        "A single 4' sine with a small upward scoop and quick breath-like articulation. Pressure adds restrained vibrato and level; no lower-octave rank is mixed in.",
        lead().wave(I, false, false, 1.0f).wave(II, false, false).feet(I, f4).mix(0.0f).detune(0.0f)
              .level(I, 0.0f, 0.025f).level(II, 0.0f, 0.0f).touch(II, 0.0f, 0.0f, 0.0f, 0.0f).sub(subSine, 5.4f, 0.0f)
              .subAt(0.018f, 0.060f).g(P_modWheelSub, 0.035f).bend(0.065f).glide(0.028f)
              .eg(I, 0.024f, 1.1f, 0.99f, 0.075f).touch(I, 0.0f, 0.94f, 0.0f, 0.56f)
              .contour(0.0f, 0.0f, -0.06f, 0.145f).g(P_voiceDispersion, 0.014f).g(P_drift, 0.0f));
    add("Square Lead", "Leads", { "aftertouch" },
        "A hollow 8' square body with a quieter same-register square edge. Nearly unison tuning and dry, quick articulation preserve the centered odd-harmonic character.",
        lead().wave(I, false, true).wave(II, false, true).pw(I, 0.5f).pw(II, 0.5f).lpf(I, 2650.0f, 0.055f)
              .lpf(II, 4100.0f, 0.04f).detune(0.0f).bend(0.0f).res(0.06f).mix(0.19f).hpf(II, 285.0f)
              .feg(I, 0.0f, 0.13f, 0.016f, 1.65f, 0.17f).feg(II, 0.03f, 0.21f, 0.007f, 0.36f, 0.10f)
              .eg(I, 0.0045f, 1.95f, 0.97f, 0.095f).eg(II, 0.011f, 0.67f, 0.25f, 0.055f)
              .touch(II, 0.16f, 0.98f, 0.21f, 0.28f).contour(-0.075f, -0.20f, -0.025f, 0.075f).hpf(I, 44.0f)
              .touch(I, 0.11f, 0.95f, 0.19f, 0.38f)
              .level(I, 1.0f, 0.04f).level(II, 0.88f, 0.025f));
    add("Narrow Pulse Lead", "Leads", { "aftertouch", "velocity" },
        "A narrow pulse body with a lighter, differently voiced pulse edge. The dry, nasal tone has a clear attack and restrained vibrato for agile lines.",
        lead().wave(I, false, true).wave(II, false, true).hpf(I, 85.0f).lpf(I, 3050.0f, 0.11f)
              .lpf(II, 4700.0f, 0.085f).eg(I, 0.0055f, 1.3f, 0.93f, 0.09f).touch(I, 0.14f, 0.96f, 0.16f, 0.34f)
              .detune(0.35f).subAt(0.03f, 0.08f).res(0.10f).mix(0.22f).pw(I, 0.765f, 0.012f, 0.35f)
              .pw(II, 0.665f, 0.022f, 0.25f).hpf(II, 325.0f).feg(I, 0.015f, 0.21f, 0.008f, 1.35f, 0.14f)
              .feg(II, 0.0f, 0.31f, 0.004f, 0.28f, 0.085f).eg(II, 0.009f, 0.36f, 0.21f, 0.06f)
              .touch(II, 0.21f, 1.0f, 0.27f, 0.28f).contour(-0.05f, -0.24f, -0.025f, 0.04f)
              .level(I, 1.0f, 0.04f).level(II, 0.88f, 0.02f)
              .bend(0.02f));
    add("Resonant Lead", "Leads", { "resonance", "aftertouch" },
        "A moving resonant edge above a warmer body. Pressure raises the filter focus while modest low-cut settings retain a pitched foundation.",
        lead().lpf(I, 1080.0f, 0.22f).lpf(II, 1650.0f, 0.46f).hpf(I, 35.0f).level(II, 0.80f, 0.025f).res(0.30f)
              .mix(0.25f).subAt(0.025f, 0.07f).feg(I, 0.015f, 0.38f, 0.012f, 1.55f, 0.18f)
              .feg(II, 0.0f, 0.59f, 0.004f, 0.29f, 0.10f).eg(I, 0.006f, 1.85f, 0.95f, 0.11f)
              .eg(II, 0.0035f, 0.29f, 0.2f, 0.06f).hpf(II, 275.0f).touch(I, 0.18f, 0.94f, 0.27f, 0.42f)
              .touch(II, 0.23f, 0.98f, 0.36f, 0.36f).contour(-0.035f, -0.24f, -0.02f, 0.04f)
              .level(I, 1.0f, 0.045f));
    add("Screaming Lead", "Leads", { "resonance", "aftertouch" },
        "A focused, aggressive upper resonance responds strongly to pressure. A quieter saw-and-sine body holds the pitch beneath the brighter edge; vibrato stays controlled.",
        lead().wave(I, true, false, 0.19f).wave(II, false, true).pw(II, 0.72f).lpf(I, 1040.0f, 0.19f)
              .lpf(II, 2050.0f, 0.58f).level(I, 0.88f, 0.045f).level(II, 0.78f, 0.02f).res(0.42f).mix(0.31f)
              .subAt(0.035f, 0.095f).detune(0.4f).hpf(I, 60.0f).hpf(II, 420.0f)
              .feg(I, 0.0f, 0.24f, 0.008f, 1.5f, 0.16f).feg(II, 0.0f, 0.37f, 0.003f, 0.28f, 0.09f)
              .eg(II, 0.0025f, 0.42f, 0.3f, 0.07f).touch(I, 0.16f, 0.96f, 0.23f, 0.46f)
              .touch(II, 0.22f, 1.0f, 0.34f, 0.44f).contour(-0.035f, -0.28f, -0.02f, 0.045f)
              .eg(I, 0.005f, 1.65f, 0.92f, 0.125f));
    add("Fifth Lead", "Leads", { "feet-layer", "aftertouch" },
        "An 8' body with a lighter 5 1/3' rank at a harmonic fifth. The upper voice has its own brighter filter and slower onset, preserving clarity in the interval.",
        lead().feet(II, f513).wave(II, true, false).lpf(II, 4500.0f, 0.07f).level(II, 0.82f, 0.035f).detune(0.0f)
              .mix(0.25f).bend(0.0f).pw(II, 0.51f, 0.016f, 0.31f).hpf(II, 330.0f)
              .feg(I, 0.025f, 0.18f, 0.019f, 1.6f, 0.20f).feg(II, 0.015f, 0.23f, 0.032f, 0.59f, 0.14f)
              .eg(II, 0.034f, 0.82f, 0.56f, 0.13f).touch(II, 0.11f, 0.88f, 0.15f, 0.32f).g(P_drift, 0.015f)
              .hpf(I, 43.0f).lpf(I, 2120.0f, 0.11f).eg(I, 0.01f, 1.85f, 0.95f, 0.17f)
              .touch(I, 0.12f, 0.9f, 0.21f, 0.4f).contour(-0.06f, -0.20f, -0.025f, 0.065f)
              .level(I, 1.0f, 0.065f));
    add("Octave Lead", "Leads", { "feet-layer", "aftertouch" },
        "A solid 8' saw under a sustained 4' pulse crown. The octave has a higher cutoff and a lighter level, so its presence stays clear without taking over the lead.",
        lead().feet(II, f4).pw(II, 0.52f).lpf(II, 5400.0f, 0.055f).level(II, 0.78f, 0.025f).detune(0.0f).mix(0.23f)
              .subAt(0.03f, 0.08f).hpf(II, 485.0f).feg(I, 0.02f, 0.18f, 0.017f, 1.62f, 0.20f)
              .feg(II, 0.0f, 0.27f, 0.006f, 0.42f, 0.095f).eg(I, 0.008f, 2.0f, 0.97f, 0.15f)
              .eg(II, 0.016f, 0.6f, 0.42f, 0.09f).touch(II, 0.14f, 0.98f, 0.17f, 0.3f)
              .contour(-0.065f, -0.23f, -0.035f, 0.06f).hpf(I, 43.0f).lpf(I, 2100.0f, 0.11f)
              .touch(I, 0.12f, 0.92f, 0.21f, 0.4f)
              .level(I, 1.0f, 0.065f));
    add("Soft Lead", "Leads", { "aftertouch", "sine" },
        "A rounded saw-and-sine body with a quiet soft pulse edge. Gently staggered attacks and restrained pressure brightness keep slow phrases warm and centered.",
        lead().lpf(I, 1050.0f, 0.045f).lpf(II, 1850.0f, 0.055f).hpf(I, 35.0f).hpf(II, 135.0f)
              .touch(I, 0.055f, 0.86f, 0.12f, 0.44f).mix(0.18f).res(0.06f).subAt(0.025f, 0.07f).bend(0.0f)
              .wave(I, true, false, 0.30f).pw(II, 0.515f, 0.010f, 0.15f).feg(I, 0.03f, 0.085f, 0.052f, 2.05f, 0.27f)
              .feg(II, 0.075f, 0.15f, 0.087f, 0.86f, 0.20f).eg(I, 0.041f, 2.25f, 0.98f, 0.29f)
              .eg(II, 0.08f, 0.96f, 0.42f, 0.17f).touch(II, 0.09f, 0.9f, 0.19f, 0.36f)
              .contour(-0.09f, -0.13f, -0.04f, 0.09f)
              .level(I, 1.0f, 0.09f).level(II, 0.88f, 0.045f));
    add("Init Bend Lead", "Leads", { "init-bend", "velocity" },
        "A clear upward scoop begins below each struck note and settles quickly into a bright sustained lead. Harder strikes deepen the scoop; there is no constant pitch modulation.",
        lead().bend(0.42f).glide(0.0f).lpf(I, 2250.0f, 0.10f).lpf(II, 3700.0f, 0.095f).subAt(0.02f, 0.055f)
              .detune(0.35f).mix(0.25f).hpf(II, 235.0f).feg(I, 0.0f, 0.25f, 0.011f, 1.25f, 0.15f)
              .feg(II, 0.0f, 0.36f, 0.004f, 0.24f, 0.085f).eg(I, 0.0045f, 1.75f, 0.96f, 0.11f)
              .eg(II, 0.008f, 0.52f, 0.2f, 0.065f).touch(I, 0.18f, 0.98f, 0.19f, 0.34f)
              .touch(II, 0.25f, 1.0f, 0.27f, 0.27f).hpf(I, 45.0f).contour(-0.045f, -0.20f, -0.02f, 0.055f)
              .level(I, 1.0f, 0.04f).level(II, 0.88f, 0.02f));
    add("Pressure Lead", "Leads", { "aftertouch" },
        "Requires poly or channel aftertouch: hold a key and press to bring the sound out of silence. On screen, drag down on a held key. Pressure controls level and brightness; releasing pressure fades the tone.",
        lead().wave(I, true, false, 0.10f).wave(II, false, true).lpf(I, 1720.0f, 0.085f).lpf(II, 2750.0f, 0.1f)
              .level(I, 1.0f, 0.0f).level(II, 0.90f, 0.0f).touch(I, 0.0f, 0.0f, 0.25f, 1.0f)
              .touch(II, 0.0f, 0.0f, 0.32f, 0.88f).subAt(0.03f, 0.08f).bend(0.0f).glide(0.032f).detune(0.25f)
              .mix(0.21f).pw(II, 0.56f, 0.014f, 0.21f).hpf(I, 32.0f).hpf(II, 225.0f)
              .feg(I, 0.025f, 0.10f, 0.038f, 2.15f, 0.23f).feg(II, 0.075f, 0.18f, 0.068f, 0.95f, 0.18f)
              .eg(I, 0.009f, 1.9f, 1.0f, 0.16f).eg(II, 0.023f, 0.82f, 0.88f, 0.11f).contour(-0.05f, -0.18f, 0.0f, 0.0f));
    add("Velocity Lead", "Leads", { "velocity" },
        "Strike velocity controls a wide range of level and brightness. The quiet static gain lets soft notes stay soft, while hard strikes reveal a more prominent pulse edge.",
        lead().level(I, 1.0f, 0.012f).level(II, 0.88f, 0.004f).lpf(I, 1390.0f, 0.085f).lpf(II, 2400.0f, 0.11f)
              .subAt(0.02f, 0.055f).bend(0.02f).mix(0.26f).pw(II, 0.64f).feg(I, 0.015f, 0.20f, 0.011f, 1.48f, 0.18f)
              .feg(II, 0.0f, 0.35f, 0.004f, 0.27f, 0.095f).eg(I, 0.005f, 2.0f, 0.96f, 0.12f)
              .eg(II, 0.003f, 0.3f, 0.12f, 0.065f).touch(I, 0.4f, 1.0f, 0.13f, 0.15f)
              .touch(II, 0.58f, 1.0f, 0.18f, 0.11f).hpf(II, 240.0f).hpf(I, 43.0f)
              .contour(-0.05f, -0.20f, -0.025f, 0.06f));
    add("Detuned Lead", "Leads", { "detune", "dual-rank" },
        "Two sustained saw ranks with a deliberate 12-cent offset. Their different filters and attacks create a broad beating lead without added chorus or deep pressure vibrato.",
        lead().wave(I, true, false).wave(II, true, false).lpf(I, 2650.0f, 0.085f).lpf(II, 3350.0f, 0.07f)
              .level(II, 0.94f, 0.045f).detune(12.0f).mix(0.42f).subAt(0.02f, 0.045f).g(P_modWheelSub, 0.035f)
              .bend(0.0f).res(0.10f).hpf(II, 220.0f).feg(I, 0.02f, 0.19f, 0.016f, 1.8f, 0.22f)
              .feg(II, 0.045f, 0.29f, 0.031f, 0.59f, 0.16f).eg(I, 0.008f, 2.1f, 0.97f, 0.19f)
              .eg(II, 0.023f, 0.9f, 0.69f, 0.14f).touch(II, 0.18f, 0.96f, 0.25f, 0.32f)
              .contour(-0.065f, -0.21f, -0.025f, 0.065f).hpf(I, 44.0f).touch(I, 0.12f, 0.9f, 0.21f, 0.38f)
              .g(P_drift, 0.018f)
              .level(I, 1.0f, 0.07f));
    add("Sub Vibrato Lead", "Leads", { "sub-mod", "aftertouch" },
        "A smooth lead with a continuous, restrained 5.2 Hz vibrato. Pressure adds a small amount of depth and level, allowing the held tone to sing without a wide pitch wobble.",
        lead().lpf(I, 1820.0f, 0.07f).lpf(II, 2850.0f, 0.085f).eg(I, 0.024f, 1.7f, 0.97f, 0.23f)
              .sub(subSine, 5.2f, 0.08f).subAt(0.025f, 0.045f).g(P_modWheelSub, 0.035f).detune(0.0f).bend(0.0f)
              .mix(0.21f).wave(I, true, false, 0.20f).pw(II, 0.535f, 0.014f, 0.18f)
              .feg(I, 0.035f, 0.13f, 0.032f, 1.85f, 0.23f).feg(II, 0.09f, 0.21f, 0.055f, 0.72f, 0.17f)
              .eg(II, 0.033f, 1.1f, 0.52f, 0.14f).touch(II, 0.12f, 0.92f, 0.19f, 0.36f).hpf(I, 42.0f).hpf(II, 210.0f)
              .touch(I, 0.11f, 0.88f, 0.20f, 0.4f).g(P_drift, 0.01f).contour(-0.075f, -0.16f, -0.03f, 0.08f)
              .level(I, 1.0f, 0.075f).level(II, 0.88f, 0.035f));
    add("Trill Lead", "Leads", { "sub-mod" },
        "A square SUB alternates approximately one semitone below and above the played pitch at 6 Hz. Pressure and wheel pitch routes are disabled so the interval remains deliberate.",
        lead().wave(I, false, true).pw(I, 0.5f).wave(II, true, false).lpf(I, 2550.0f, 0.055f).lpf(II, 3700.0f, 0.055f)
              .feg(I, 0.0f, 0.14f, 0.009f, 1.42f, 0.12f).feg(II, 0.0f, 0.23f, 0.004f, 0.26f, 0.075f)
              .sub(subSquare, 6.0f, 0.6666667f).subAt(0.0f, 0.0f).g(P_modWheelSub, 0.0f).bend(0.0f).glide(0.0f)
              .detune(0.0f).mix(0.17f).res(0.06f).hpf(II, 295.0f).eg(I, 0.004f, 1.85f, 1.0f, 0.07f)
              .eg(II, 0.008f, 0.42f, 0.28f, 0.045f).touch(II, 0.14f, 0.96f, 0.15f, 0.23f).g(P_voiceDispersion, 0.035f)
              .hpf(I, 44.0f).touch(I, 0.11f, 0.94f, 0.19f, 0.3f).g(P_drift, 0.0f)
              .contour(-0.05f, -0.20f, -0.02f, 0.055f)
              .level(I, 1.0f, 0.04f).level(II, 0.88f, 0.02f));
    add("Glissando Lead", "Leads", { "glide" },
        "A clearly articulated lead whose portamento steps through semitones. The steady sustain and simple filter motion keep the discrete glissando easy to hear.",
        lead().glide(0.19f, 2).bend(0.0f).detune(0.0f).pw(I, 0.54f).wave(II, true, false).lpf(I, 2200.0f, 0.07f)
              .lpf(II, 3400.0f, 0.055f).feg(I, 0.04f, 0.13f, 0.052f, 1.9f, 0.25f)
              .feg(II, 0.09f, 0.19f, 0.083f, 0.80f, 0.19f).subAt(0.02f, 0.045f).mix(0.19f).res(0.08f)
              .wave(I, true, false, 0.17f).pw(II, 0.56f, 0.02f, 0.25f).eg(I, 0.013f, 2.1f, 0.99f, 0.19f)
              .eg(II, 0.03f, 0.85f, 0.52f, 0.13f).touch(II, 0.12f, 0.94f, 0.20f, 0.35f).hpf(I, 44.0f).hpf(II, 200.0f)
              .touch(I, 0.12f, 0.89f, 0.21f, 0.4f).contour(-0.06f, -0.18f, -0.025f, 0.075f)
              .level(I, 1.0f, 0.07f).level(II, 0.88f, 0.035f));
    add("Brass Lead", "Leads", { "brilliance-swell", "aftertouch" },
        "A compact circuit brass body is supported by a harmonic lip spectrum, then a brighter saw edge. The short accent settles into a centered sustain; velocity controls breath effort and pressure adds bloom. Separate short releases keep single lines clear.",
        lead().wave(I, true, false, 0.08f).wave(II, true, false).hpf(I, 28.0f).lpf(I, 3900.0f, 0.09f)
              .lpf(II, 4200.0f, 0.1f).detune(0.5f).bend(0.045f).res(0.16f).mix(0.30f)
              .feg(I, 0.12f, 0.34f, 0.025f, 1.8f, 0.16f).feg(II, 0.22f, 0.46f, 0.052f, 0.62f, 0.11f)
              .eg(I, 0.013f, 1.8f, 0.96f, 0.12f).eg(II, 0.028f, 0.62f, 0.46f, 0.085f).hpf(II, 260.0f)
              .touch(I, 0.19f, 0.9f, 0.28f, 0.47f).touch(II, 0.27f, 0.97f, 0.38f, 0.39f)
              .contour(-0.055f, -0.19f, -0.025f, 0.06f)
              .level(I, 1.0f, 0.05f).level(II, 0.88f, 0.03f)
              .body(I, body::Model::brass, 0.35f, 0.6f, 0.95f).bodySize(I, 1.03f));
    add("Reed Lead", "Leads", { "aftertouch", "dual-rank" },
        "A focused clarinet-like harmonic body meets a quieter double-reed and pulse edge. A definite tongue onset and short breath release keep lines dry and articulate; pressure adds nasal intensity and restrained vibrato without removing the fundamental.",
        lead().wave(I, false, true).wave(II, false, true).hpf(I, 32.0f).lpf(I, 6400.0f, 0.08f).lpf(II, 5800.0f, 0.12f)
              .eg(I, 0.012f, 1.55f, 0.96f, 0.075f).touch(I, 0.09f, 0.9f, 0.14f, 0.44f).detune(0.0f)
              .subAt(0.035f, 0.075f).res(0.10f).mix(0.20f).pw(I, 0.50f, 0.007f, 0.22f).pw(II, 0.585f, 0.014f, 0.29f)
              .hpf(II, 460.0f, 0.04f).feg(I, 0.035f, 0.12f, 0.018f, 1.25f, 0.08f)
              .feg(II, 0.07f, 0.22f, 0.029f, 0.36f, 0.065f).eg(II, 0.023f, 0.69f, 0.4f, 0.055f)
              .touch(II, 0.17f, 0.96f, 0.30f, 0.39f).contour(-0.06f, -0.22f, -0.025f, 0.055f)
              .level(I, 1.0f, 0.06f).level(II, 0.88f, 0.035f)
              .body(I, body::Model::clarinet, 0.78f, 0.56f, 1.0f).bodySize(I, 1.0f)
              .body(II, body::Model::oboe, 0.32f, 0.65f, 0.9f).bodySize(II, 0.93f)
              .bend(0.0f));
    add("Wah Lead", "Leads", { "aftertouch", "sub-mod" },
        "Pressure brings a 3.8 Hz SUB sine into the resonant filter route. The unpressed note stays centered; leaning in adds a rhythmic wah without also creating pitch vibrato.",
        lead().hpf(I, 32.0f).lpf(I, 1450.0f, 0.21f).lpf(II, 2300.0f, 0.32f).touch(I, 0.09f, 0.89f, 0.08f, 0.38f)
              .sub(subSine, 3.8f, 0.0f).subAt(0.08f, 0.0f, 0.38f).g(P_modWheelSub, 0.0f).res(0.23f).detune(0.25f)
              .bend(0.0f).mix(0.26f).hpf(II, 280.0f).pw(II, 0.615f).feg(I, 0.10f, 0.24f, 0.079f, 1.5f, 0.19f)
              .feg(II, 0.17f, 0.38f, 0.12f, 0.58f, 0.14f).eg(I, 0.008f, 2.0f, 0.98f, 0.14f)
              .eg(II, 0.023f, 0.72f, 0.39f, 0.09f).touch(II, 0.17f, 0.95f, 0.33f, 0.34f)
              .contour(-0.045f, -0.20f, -0.025f, 0.055f)
              .level(I, 1.0f, 0.065f).level(II, 0.88f, 0.035f));
    add("Ring Lead", "Leads", { "ring-mod", "aftertouch" },
        "A clean center tone mixed with 280 Hz ring sidebands for a metallic edge. The restrained wet blend preserves pitch identity while pressure brightens the source.",
        lead().pw(I, 0.54f).wave(II, true, false).lpf(I, 2400.0f, 0.07f).lpf(II, 3950.0f, 0.085f)
              .eg(I, 0.011f, 1.7f, 0.96f, 0.18f).ring(280.0f, 0.0f, 0.016f, 1.2f, 0.19f).detune(0.0f)
              .subAt(0.025f, 0.055f).bend(0.0f).mix(0.20f).res(0.09f).wave(I, true, false, 0.25f).pw(II, 0.57f)
              .hpf(II, 255.0f).feg(I, 0.02f, 0.12f, 0.020f, 1.85f, 0.20f).feg(II, 0.0f, 0.24f, 0.007f, 0.35f, 0.11f)
              .eg(II, 0.011f, 0.42f, 0.24f, 0.09f).touch(II, 0.14f, 0.97f, 0.19f, 0.34f)
              .contour(-0.06f, -0.21f, -0.025f, 0.08f).hpf(I, 43.0f).touch(I, 0.11f, 0.91f, 0.21f, 0.4f)
              .level(I, 1.0f, 0.06f).level(II, 0.88f, 0.025f));
    add("Hollow Lead", "Leads", { "pwm", "aftertouch" },
        "A near-square body and a quieter moving pulse create a hollow tone with slow harmonic motion. The lower-width body stays stable while the second rank carries most of the PWM.",
        lead().wave(I, false, true).wave(II, false, true).lpf(I, 2450.0f, 0.045f).lpf(II, 3750.0f, 0.055f)
              .eg(I, 0.018f, 1.8f, 0.97f, 0.21f).detune(0.15f).bend(0.0f).subAt(0.03f, 0.07f).res(0.06f).mix(0.24f)
              .pw(I, 0.50f, 0.006f, 0.19f).pw(II, 0.535f, 0.038f, 0.23f).hpf(II, 460.0f, 0.095f)
              .feg(I, 0.02f, 0.115f, 0.025f, 1.65f, 0.19f).feg(II, 0.06f, 0.23f, 0.040f, 0.50f, 0.13f)
              .eg(II, 0.029f, 0.8f, 0.42f, 0.12f).touch(II, 0.13f, 0.94f, 0.23f, 0.35f).hpf(I, 44.0f)
              .touch(I, 0.10f, 0.9f, 0.19f, 0.41f).contour(-0.07f, -0.19f, -0.03f, 0.07f)
              .level(I, 1.0f, 0.07f).level(II, 0.88f, 0.035f));
    add("Ether Lead", "Leads", { "sine", "glide", "aftertouch" },
        "One 8' sine with a soft onset, long glide and expressive pressure level. Gentle pressure vibrato gives a theremin-like gesture without an unintended beating second oscillator.",
        lead().wave(I, false, false, 1.0f).wave(II, false, false).mix(0.0f).detune(0.0f).level(I, 0.0f, 0.018f).level(II, 0.0f, 0.0f).touch(II, 0.0f, 0.0f, 0.0f, 0.0f).glide(0.32f).bend(0.0f)
              .sub(subSine, 5.0f, 0.0f).subAt(0.04f, 0.115f).g(P_modWheelSub, 0.055f).eg(I, 0.073f, 1.5f, 1.0f, 0.38f)
              .touch(I, 0.0f, 0.72f, 0.0f, 0.93f).contour(0.0f, 0.0f, -0.08f, 0.20f).g(P_voiceDispersion, 0.01f)
              .g(P_drift, 0.0f));
    add("Solo Synth", "Leads", { "aftertouch", "velocity" },
        "A bright, agile saw lead with a lightly supporting pulse edge. Quick articulation, useful velocity contrast and modest pressure vibrato suit exposed melodic lines.",
        lead().lpf(I, 3250.0f, 0.07f).lpf(II, 5000.0f, 0.055f).touch(I, 0.16f, 0.98f, 0.16f, 0.35f).detune(0.3f)
              .bend(0.025f).glide(0.015f).subAt(0.03f, 0.085f).res(0.08f).mix(0.20f).pw(II, 0.60f, 0.016f, 0.25f)
              .hpf(II, 255.0f).feg(I, 0.05f, 0.22f, 0.020f, 1.78f, 0.22f).feg(II, 0.09f, 0.31f, 0.039f, 0.55f, 0.16f)
              .eg(I, 0.005f, 2.1f, 0.98f, 0.105f).eg(II, 0.013f, 0.52f, 0.28f, 0.065f)
              .touch(II, 0.20f, 1.0f, 0.30f, 0.33f).hpf(I, 44.0f).contour(-0.045f, -0.23f, -0.025f, 0.05f)
              .level(I, 1.0f, 0.035f).level(II, 0.88f, 0.02f));
    add("Contour Lead", "Leads", { "kbd-contour", "aftertouch" },
        "A rounded lower register opens progressively into brighter upper notes. A gentle high-key level trim balances that added brilliance while retaining clear solo articulation.",
        lead().lpf(I, 1800.0f, 0.085f).lpf(II, 3050.0f, 0.07f).eg(I, 0.015f, 1.7f, 0.96f, 0.17f).detune(0.4f)
              .subAt(0.03f, 0.08f).mix(0.25f).hpf(II, 250.0f).pw(II, 0.565f, 0.013f, 0.23f)
              .feg(I, 0.035f, 0.18f, 0.024f, 1.9f, 0.25f).feg(II, 0.075f, 0.28f, 0.042f, 0.62f, 0.17f)
              .eg(II, 0.028f, 0.7f, 0.38f, 0.11f).touch(II, 0.19f, 0.96f, 0.28f, 0.35f)
              .contour(-0.18f, 0.27f, 0.035f, -0.09f).hpf(I, 44.0f).touch(I, 0.12f, 0.9f, 0.21f, 0.4f)
              .level(I, 1.0f, 0.065f).level(II, 0.88f, 0.035f));
}
