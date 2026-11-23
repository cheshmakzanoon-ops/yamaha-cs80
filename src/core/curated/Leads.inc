// Leads voicings. Included inside namespace curated by CuratedLibrary.inc.
// Existing program names and insertion order are stable host/MIDI identities.
// A playable center tone precedes expression. Pressure/wheel pitch depths are
// restrained except for explicitly named trill and wide-detune techniques.

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
        lead().feg(I, 0.08f, 0.28f, 0.022f, 1.0f, 0.24f)
              .touch(II, 0.24f, 0.60f, 0.38f, 0.26f).bend(0.065f).mix(0.22f));
    add("Aftertouch Vibrato Lead", "Leads", { "aftertouch", "sub-mod" },
        "A centered, singing lead whose vibrato grows with pressure to roughly a quarter-tone peak-to-peak. The dry tone remains stable until pressure or wheel is applied.",
        lead().lpf(I, 1650.0f, 0.13f).lpf(II, 2650.0f, 0.12f)
              .eg(I, 0.022f, 1.4f, 0.92f, 0.32f).eg(II, 0.030f, 1.0f, 0.76f, 0.24f)
              .sub(subSine, 5.0f, 0.0f).subAt(0.055f, 0.16f).g(P_modWheelSub, 0.04f)
              .touch(I, 0.12f, 0.54f, 0.20f, 0.30f).touch(II, 0.18f, 0.55f, 0.26f, 0.22f)
              .detune(0.6f).bend(0.0f).mix(0.18f));
    add("Ribbon Glide Lead", "Leads", { "ribbon", "glide", "aftertouch" },
        "A smooth, centered lead for ribbon gestures. Short portamento and a small upward onset scoop connect phrases; a restrained upper edge keeps large slides readable.",
        lead().wave(I, true, false, 0.14f).pw(II, 0.55f).lpf(I, 1450.0f, 0.12f).lpf(II, 2400.0f, 0.14f)
              .feg(I, 0.04f, 0.20f, 0.025f, 1.1f, 0.28f).feg(II, 0.06f, 0.24f, 0.035f, 0.8f, 0.23f)
              .eg(I, 0.018f, 1.5f, 0.93f, 0.34f).eg(II, 0.030f, 1.1f, 0.75f, 0.25f)
              .glide(0.065f).bend(0.08f).detune(0.0f).subAt(0.03f, 0.08f).mix(0.24f));
    add("Noir Lead", "Leads", { "aftertouch", "brilliance-swell", "init-bend", "chorus" },
        "A dark saw body blooms into a later brass edge. Pressure opens the sustained tone and adds controlled vibrato; moderate chorus and a long release support spacious single lines.",
        lead().wave(I, true, false, 0.08f).wave(II, true, false)
              .hpf(I, 35.0f).hpf(II, 85.0f).lpf(I, 790.0f, 0.24f).lpf(II, 1380.0f, 0.20f)
              .feg(I, 0.26f, 0.55f, 0.22f, 1.9f, 0.65f).feg(II, 0.36f, 0.45f, 0.36f, 1.5f, 0.52f)
              .eg(I, 0.042f, 1.7f, 0.91f, 1.05f).eg(II, 0.080f, 1.3f, 0.77f, 0.82f)
              .touch(I, 0.12f, 0.48f, 0.46f, 0.32f).touch(II, 0.17f, 0.53f, 0.56f, 0.28f)
              .subAt(0.04f, 0.12f).bend(0.12f).glide(0.06f).chorus(0.28f, 0.34f)
              .detune(4.0f).res(0.20f).mix(0.40f));
    add("Neo-Noir Brass Lead", "Leads", { "aftertouch", "chorus", "brilliance-swell" },
        "A broader, darker brass lead with a slower upper-rank bloom and a sustained release. Pressure brings out the brighter crown while moderate detune supplies width.",
        lead().wave(I, true, false, 0.10f).wave(II, true, true).pw(II, 0.57f, 0.10f, 0.22f)
              .hpf(I, 30.0f).hpf(II, 100.0f).lpf(I, 620.0f, 0.19f).lpf(II, 1120.0f, 0.16f)
              .feg(I, 0.22f, 0.38f, 0.30f, 2.1f, 0.95f).feg(II, 0.40f, 0.47f, 0.52f, 1.8f, 0.80f)
              .eg(I, 0.075f, 1.9f, 0.92f, 1.65f).eg(II, 0.15f, 1.5f, 0.79f, 1.35f)
              .touch(I, 0.10f, 0.46f, 0.40f, 0.34f).touch(II, 0.14f, 0.50f, 0.58f, 0.30f)
              .chorus(0.40f, 0.28f).detune(7.2f).subAt(0.035f, 0.09f).bend(0.04f).mix(0.43f));
    add("Glide Lead", "Leads", { "glide", "aftertouch" },
        "A steady saw-and-pulse tone with long, smooth portamento between notes. Gentle envelope motion keeps the glide audible without adding an onset pitch scoop.",
        lead().glide(0.24f).bend(0.0f).detune(0.8f).lpf(I, 1700.0f, 0.14f).lpf(II, 2600.0f, 0.10f)
              .feg(I, 0.02f, 0.14f, 0.035f, 1.0f, 0.28f).feg(II, 0.0f, 0.16f, 0.025f, 0.8f, 0.20f)
              .eg(I, 0.018f, 1.4f, 0.95f, 0.32f).eg(II, 0.022f, 1.1f, 0.82f, 0.24f).mix(0.18f));
    add("Sine Lead", "Leads", { "sine", "aftertouch" },
        "One pure 8' sine with a rounded onset. Pressure controls level and a gentle vibrato; filter brilliance is intentionally inactive on this bypass-sine voice.",
        lead().wave(I, false, false, 1.0f).wave(II, false, false).mix(0.0f).detune(0.0f)
              .level(I, 0.0f, 0.32f).level(II, 0.0f, 0.0f)
              .eg(I, 0.018f, 0.6f, 1.0f, 0.26f).touch(I, 0.0f, 0.58f, 0.0f, 0.36f)
              .touch(II, 0.0f, 0.0f, 0.0f, 0.0f).subAt(0.03f, 0.10f)
              .bend(0.0f).glide(0.025f).g(P_drift, 0.018f).g(P_voiceDispersion, 0.05f));
    add("Whistle Lead", "Leads", { "sine", "init-bend", "aftertouch" },
        "A single 4' sine with a small upward scoop and quick breath-like articulation. Pressure adds restrained vibrato and level; no lower-octave rank is mixed in.",
        lead().wave(I, false, false, 1.0f).wave(II, false, false).feet(I, f4).mix(0.0f).detune(0.0f)
              .level(I, 0.0f, 0.28f).level(II, 0.0f, 0.0f)
              .eg(I, 0.025f, 0.5f, 0.96f, 0.14f).touch(I, 0.0f, 0.56f, 0.0f, 0.38f)
              .touch(II, 0.0f, 0.0f, 0.0f, 0.0f).sub(subSine, 5.4f, 0.0f).subAt(0.025f, 0.07f)
              .g(P_modWheelSub, 0.035f).bend(0.12f).glide(0.035f).g(P_voiceDispersion, 0.04f));
    add("Square Lead", "Leads", { "aftertouch" },
        "A hollow 8' square body with a quieter same-register square edge. Nearly unison tuning and dry, quick articulation preserve the centered odd-harmonic character.",
        lead().wave(Both, false, true).pw(Both, 0.5f).lpf(I, 2300.0f, 0.08f).lpf(II, 3600.0f, 0.06f)
              .feg(I, 0.0f, 0.18f, 0.009f, 0.75f, 0.17f).feg(II, 0.0f, 0.24f, 0.006f, 0.40f, 0.12f)
              .eg(I, 0.008f, 1.1f, 0.94f, 0.18f).eg(II, 0.010f, 0.7f, 0.64f, 0.14f)
              .detune(0.4f).bend(0.0f).res(0.06f).mix(0.22f));
    add("Narrow Pulse Lead", "Leads", { "aftertouch", "velocity" },
        "A narrow pulse body with a lighter, differently voiced pulse edge. The dry, nasal tone has a clear attack and restrained vibrato for agile lines.",
        lead().wave(Both, false, true).pw(I, 0.80f).pw(II, 0.69f)
              .hpf(I, 100.0f).hpf(II, 240.0f).lpf(I, 2700.0f, 0.16f).lpf(II, 4100.0f, 0.12f)
              .feg(I, 0.04f, 0.22f, 0.012f, 0.70f, 0.16f).feg(II, 0.0f, 0.28f, 0.006f, 0.38f, 0.11f)
              .eg(I, 0.009f, 1.0f, 0.88f, 0.17f).eg(II, 0.006f, 0.65f, 0.54f, 0.12f)
              .touch(I, 0.18f, 0.58f, 0.20f, 0.26f).touch(II, 0.22f, 0.62f, 0.24f, 0.18f)
              .detune(0.7f).subAt(0.03f, 0.08f).res(0.10f).mix(0.25f));
    add("Resonant Lead", "Leads", { "resonance", "aftertouch" },
        "A moving resonant edge above a warmer body. Pressure raises the filter focus while modest low-cut settings retain a pitched foundation.",
        lead().lpf(I, 920.0f, 0.28f).lpf(II, 1380.0f, 0.50f).hpf(I, 35.0f).hpf(II, 90.0f)
              .feg(I, 0.06f, 0.30f, 0.018f, 1.1f, 0.25f).feg(II, 0.10f, 0.42f, 0.035f, 0.75f, 0.20f)
              .touch(I, 0.14f, 0.54f, 0.30f, 0.24f).touch(II, 0.18f, 0.56f, 0.46f, 0.20f)
              .level(II, 0.80f, 0.28f).res(0.35f).mix(0.29f).subAt(0.025f, 0.07f));
    add("Screaming Lead", "Leads", { "resonance", "aftertouch" },
        "A focused, aggressive upper resonance responds strongly to pressure. A quieter saw-and-sine body holds the pitch beneath the brighter edge; vibrato stays controlled.",
        lead().wave(I, true, false, 0.12f).wave(II, false, true).pw(II, 0.72f)
              .hpf(I, 32.0f).hpf(II, 150.0f).lpf(I, 880.0f, 0.24f).lpf(II, 1650.0f, 0.62f)
              .feg(I, 0.0f, 0.24f, 0.014f, 0.85f, 0.20f).feg(II, 0.12f, 0.44f, 0.025f, 0.62f, 0.16f)
              .level(I, 0.88f, 0.34f).level(II, 0.78f, 0.22f)
              .touch(I, 0.12f, 0.50f, 0.26f, 0.22f).touch(II, 0.20f, 0.54f, 0.64f, 0.30f)
              .res(0.48f).mix(0.36f).subAt(0.035f, 0.095f).detune(1.0f));
    add("Fifth Lead", "Leads", { "feet-layer", "aftertouch" },
        "An 8' body with a lighter 5 1/3' rank at a harmonic fifth. The upper voice has its own brighter filter and slower onset, preserving clarity in the interval.",
        lead().feet(II, f513).wave(II, true, false).hpf(II, 150.0f).lpf(II, 3900.0f, 0.10f)
              .feg(II, 0.04f, 0.20f, 0.025f, 0.80f, 0.20f).eg(II, 0.032f, 1.1f, 0.76f, 0.22f)
              .level(II, 0.82f, 0.28f).touch(II, 0.12f, 0.50f, 0.24f, 0.18f)
              .detune(0.0f).mix(0.29f).bend(0.0f));
    add("Octave Lead", "Leads", { "feet-layer", "aftertouch" },
        "A solid 8' saw under a sustained 4' pulse crown. The octave has a higher cutoff and a lighter level, so its presence stays clear without taking over the lead.",
        lead().feet(II, f4).pw(II, 0.54f).hpf(II, 180.0f).lpf(II, 4700.0f, 0.08f)
              .feg(II, 0.0f, 0.22f, 0.012f, 0.55f, 0.16f).eg(II, 0.018f, 1.1f, 0.78f, 0.20f)
              .level(II, 0.78f, 0.25f).touch(II, 0.12f, 0.50f, 0.22f, 0.18f)
              .detune(0.0f).mix(0.27f).subAt(0.03f, 0.08f));
    add("Soft Lead", "Leads", { "aftertouch", "sine" },
        "A rounded saw-and-sine body with a quiet soft pulse edge. Gently staggered attacks and restrained pressure brightness keep slow phrases warm and centered.",
        lead().wave(I, true, false, 0.25f).pw(II, 0.52f).lpf(I, 830.0f, 0.07f).lpf(II, 1500.0f, 0.08f)
              .hpf(I, 35.0f).hpf(II, 90.0f).feg(I, 0.10f, 0.16f, 0.09f, 1.3f, 0.35f)
              .feg(II, 0.12f, 0.20f, 0.14f, 1.0f, 0.28f)
              .eg(I, 0.065f, 1.5f, 0.94f, 0.45f).eg(II, 0.10f, 1.2f, 0.72f, 0.34f)
              .touch(I, 0.08f, 0.52f, 0.15f, 0.32f).touch(II, 0.10f, 0.48f, 0.22f, 0.22f)
              .mix(0.22f).res(0.06f).subAt(0.025f, 0.07f).bend(0.0f));
    add("Init Bend Lead", "Leads", { "init-bend", "velocity" },
        "A clear upward scoop begins below each struck note and settles quickly into a bright sustained lead. Harder strikes deepen the scoop; there is no constant pitch modulation.",
        lead().bend(0.42f).glide(0.0f).lpf(I, 1950.0f, 0.14f).lpf(II, 3150.0f, 0.14f)
              .feg(I, 0.0f, 0.22f, 0.008f, 0.65f, 0.16f).feg(II, 0.0f, 0.28f, 0.005f, 0.40f, 0.12f)
              .eg(I, 0.007f, 1.0f, 0.89f, 0.20f).eg(II, 0.011f, 0.7f, 0.68f, 0.16f)
              .subAt(0.02f, 0.055f).detune(0.8f).mix(0.30f));
    add("Pressure Lead", "Leads", { "aftertouch" },
        "Requires poly or channel aftertouch: hold a key and press to bring the sound out of silence. On screen, drag down on a held key. Pressure controls level and brightness; releasing pressure fades the tone.",
        lead().wave(I, true, false, 0.10f).wave(II, false, true).pw(II, 0.56f)
              .hpf(I, 30.0f).hpf(II, 75.0f).lpf(I, 1250.0f, 0.12f).lpf(II, 2050.0f, 0.14f)
              .feg(I, 0.0f, 0.0f, 0.04f, 0.8f, 0.24f).feg(II, 0.0f, 0.0f, 0.05f, 0.8f, 0.20f)
              .eg(I, 0.025f, 0.8f, 1.0f, 0.32f).eg(II, 0.035f, 0.8f, 1.0f, 0.26f)
              .level(I, 1.0f, 0.0f).level(II, 0.90f, 0.0f)
              .touch(I, 0.0f, 0.0f, 0.30f, 1.0f).touch(II, 0.0f, 0.0f, 0.46f, 0.90f)
              .subAt(0.03f, 0.08f).bend(0.0f).glide(0.04f).detune(0.6f).mix(0.30f));
    add("Velocity Lead", "Leads", { "velocity" },
        "Strike velocity controls a wide range of level and brightness. The quiet static gain lets soft notes stay soft, while hard strikes reveal a more prominent pulse edge.",
        lead().level(I, 1.0f, 0.12f).level(II, 0.88f, 0.06f)
              .touch(I, 0.42f, 0.90f, 0.08f, 0.0f).touch(II, 0.58f, 0.96f, 0.10f, 0.0f)
              .lpf(I, 1150.0f, 0.12f).lpf(II, 1900.0f, 0.15f)
              .feg(I, 0.02f, 0.22f, 0.012f, 0.75f, 0.20f).feg(II, 0.0f, 0.34f, 0.007f, 0.40f, 0.15f)
              .eg(II, 0.010f, 0.85f, 0.62f, 0.16f).subAt(0.02f, 0.055f).bend(0.02f).mix(0.30f));
    add("Detuned Lead", "Leads", { "detune", "dual-rank" },
        "Two sustained saw ranks with a deliberate 12-cent offset. Their different filters and attacks create a broad beating lead without added chorus or deep pressure vibrato.",
        lead().wave(Both, true, false).lpf(I, 2300.0f, 0.12f).lpf(II, 2900.0f, 0.10f)
              .feg(I, 0.02f, 0.22f, 0.015f, 0.9f, 0.22f).feg(II, 0.04f, 0.18f, 0.028f, 1.2f, 0.24f)
              .eg(I, 0.012f, 1.4f, 0.90f, 0.28f).eg(II, 0.025f, 1.3f, 0.86f, 0.30f)
              .level(II, 0.94f, 0.32f).detune(12.0f).mix(0.46f).subAt(0.02f, 0.045f)
              .g(P_modWheelSub, 0.035f).bend(0.0f).res(0.10f));
    add("Sub Vibrato Lead", "Leads", { "sub-mod", "aftertouch" },
        "A smooth lead with a continuous, restrained 5.2 Hz vibrato. Pressure adds a small amount of depth and level, allowing the held tone to sing without a wide pitch wobble.",
        lead().wave(I, true, false, 0.12f).lpf(I, 1550.0f, 0.10f).lpf(II, 2450.0f, 0.12f)
              .eg(I, 0.025f, 1.3f, 0.94f, 0.35f).eg(II, 0.038f, 1.0f, 0.76f, 0.27f)
              .sub(subSine, 5.2f, 0.08f).subAt(0.025f, 0.045f).g(P_modWheelSub, 0.035f)
              .detune(0.0f).bend(0.0f).mix(0.25f));
    add("Trill Lead", "Leads", { "sub-mod" },
        "A square SUB alternates approximately one semitone below and above the played pitch at 6 Hz. Pressure and wheel pitch routes are disabled so the interval remains deliberate.",
        lead().wave(I, false, true).pw(I, 0.5f).wave(II, true, false)
              .lpf(I, 2200.0f, 0.08f).lpf(II, 3200.0f, 0.08f)
              .feg(Both, 0.0f, 0.12f, 0.01f, 0.7f, 0.18f)
              .eg(I, 0.008f, 0.8f, 0.96f, 0.16f).eg(II, 0.012f, 0.8f, 0.78f, 0.14f)
              .sub(subSquare, 6.0f, 0.6666667f).subAt(0.0f, 0.0f).g(P_modWheelSub, 0.0f)
              .bend(0.0f).glide(0.0f).detune(0.0f).mix(0.20f).res(0.06f));
    add("Glissando Lead", "Leads", { "glide" },
        "A clearly articulated lead whose portamento steps through semitones. The steady sustain and simple filter motion keep the discrete glissando easy to hear.",
        lead().glide(0.21f, 2).bend(0.0f).detune(0.0f)
              .wave(I, false, true, 0.12f).pw(I, 0.54f).wave(II, true, false)
              .lpf(I, 1900.0f, 0.10f).lpf(II, 2900.0f, 0.08f)
              .feg(Both, 0.0f, 0.14f, 0.012f, 0.75f, 0.20f)
              .eg(I, 0.010f, 1.2f, 0.96f, 0.26f).eg(II, 0.016f, 1.0f, 0.78f, 0.20f)
              .subAt(0.02f, 0.045f).mix(0.22f).res(0.08f));
    add("Brass Lead", "Leads", { "brilliance-swell", "aftertouch" },
        "A compact brass rise for single lines: the warm body arrives before the brighter saw edge. Pressure adds a controlled bloom; the release stays short enough for clear phrasing.",
        lead().wave(I, true, false, 0.08f).wave(II, true, false)
              .hpf(I, 35.0f).hpf(II, 80.0f).lpf(I, 780.0f, 0.18f).lpf(II, 1450.0f, 0.16f)
              .feg(I, 0.23f, 0.48f, 0.12f, 1.4f, 0.30f).feg(II, 0.30f, 0.44f, 0.19f, 1.05f, 0.24f)
              .eg(I, 0.024f, 1.5f, 0.90f, 0.34f).eg(II, 0.045f, 1.1f, 0.72f, 0.25f)
              .touch(I, 0.13f, 0.58f, 0.32f, 0.30f).touch(II, 0.20f, 0.60f, 0.44f, 0.26f)
              .detune(2.2f).bend(0.07f).res(0.16f).mix(0.35f));
    add("Reed Lead", "Leads", { "aftertouch", "dual-rank" },
        "A focused pulse body and a quieter nasal edge form a dry reed-like lead. Modest pressure brilliance and vibrato shape phrases without removing the fundamental.",
        lead().wave(Both, false, true).pw(I, 0.65f).pw(II, 0.76f)
              .hpf(I, 95.0f).hpf(II, 390.0f).lpf(I, 2150.0f, 0.19f).lpf(II, 3100.0f, 0.24f)
              .feg(I, 0.05f, 0.19f, 0.022f, 0.8f, 0.19f).feg(II, 0.07f, 0.23f, 0.014f, 0.55f, 0.14f)
              .eg(I, 0.018f, 1.2f, 0.92f, 0.22f).eg(II, 0.012f, 0.9f, 0.66f, 0.16f)
              .touch(I, 0.12f, 0.56f, 0.18f, 0.32f).touch(II, 0.16f, 0.50f, 0.22f, 0.20f)
              .detune(0.0f).subAt(0.035f, 0.075f).res(0.13f).mix(0.23f));
    add("Wah Lead", "Leads", { "aftertouch", "sub-mod" },
        "Pressure brings a 3.8 Hz SUB sine into the resonant filter route. The unpressed note stays centered; leaning in adds a rhythmic wah without also creating pitch vibrato.",
        lead().hpf(I, 32.0f).hpf(II, 80.0f).lpf(I, 1250.0f, 0.26f).lpf(II, 1900.0f, 0.38f)
              .feg(I, 0.03f, 0.18f, 0.025f, 0.9f, 0.25f).feg(II, 0.05f, 0.21f, 0.035f, 0.7f, 0.20f)
              .touch(I, 0.12f, 0.54f, 0.10f, 0.28f).touch(II, 0.16f, 0.56f, 0.14f, 0.22f)
              .sub(subSine, 3.8f, 0.0f).subAt(0.08f, 0.0f, 0.38f).g(P_modWheelSub, 0.0f)
              .res(0.27f).detune(0.6f).bend(0.0f).mix(0.30f));
    add("Ring Lead", "Leads", { "ring-mod", "aftertouch" },
        "A clean center tone mixed with 280 Hz ring sidebands for a metallic edge. The restrained wet blend preserves pitch identity while pressure brightens the source.",
        lead().wave(I, false, true, 0.18f).pw(I, 0.54f).wave(II, true, false)
              .lpf(I, 2050.0f, 0.10f).lpf(II, 3400.0f, 0.12f)
              .eg(I, 0.012f, 1.3f, 0.91f, 0.30f).eg(II, 0.020f, 1.0f, 0.69f, 0.22f)
              .ring(280.0f, 0.0f, 0.01f, 1.0f, 0.22f).detune(0.0f)
              .subAt(0.025f, 0.055f).bend(0.0f).mix(0.24f).res(0.09f));
    add("Hollow Lead", "Leads", { "pwm", "aftertouch" },
        "A near-square body and a quieter moving pulse create a hollow tone with slow harmonic motion. The lower-width body stays stable while the second rank carries most of the PWM.",
        lead().wave(Both, false, true).pw(I, 0.52f, 0.06f, 0.19f).pw(II, 0.62f, 0.23f, 0.31f)
              .lpf(I, 2050.0f, 0.07f).lpf(II, 3200.0f, 0.08f)
              .feg(I, 0.0f, 0.16f, 0.024f, 1.0f, 0.25f).feg(II, 0.04f, 0.20f, 0.045f, 1.3f, 0.30f)
              .eg(I, 0.020f, 1.4f, 0.94f, 0.34f).eg(II, 0.037f, 1.1f, 0.80f, 0.28f)
              .detune(0.8f).bend(0.0f).subAt(0.03f, 0.07f).res(0.06f).mix(0.28f));
    add("Ether Lead", "Leads", { "sine", "glide", "aftertouch" },
        "One 8' sine with a soft onset, long glide and expressive pressure level. Gentle pressure vibrato gives a theremin-like gesture without an unintended beating second oscillator.",
        lead().wave(I, false, false, 1.0f).wave(II, false, false).mix(0.0f).detune(0.0f)
              .level(I, 0.0f, 0.18f).level(II, 0.0f, 0.0f)
              .eg(I, 0.055f, 0.8f, 1.0f, 0.52f).touch(I, 0.0f, 0.34f, 0.0f, 0.62f)
              .touch(II, 0.0f, 0.0f, 0.0f, 0.0f).glide(0.28f).bend(0.0f)
              .sub(subSine, 5.0f, 0.0f).subAt(0.05f, 0.13f).g(P_modWheelSub, 0.055f)
              .g(P_voiceDispersion, 0.04f).g(P_drift, 0.015f));
    add("Solo Synth", "Leads", { "aftertouch", "velocity" },
        "A bright, agile saw lead with a lightly supporting pulse edge. Quick articulation, useful velocity contrast and modest pressure vibrato suit exposed melodic lines.",
        lead().lpf(I, 2850.0f, 0.10f).lpf(II, 4400.0f, 0.08f)
              .feg(I, 0.0f, 0.18f, 0.008f, 0.62f, 0.17f).feg(II, 0.0f, 0.22f, 0.005f, 0.36f, 0.12f)
              .eg(I, 0.006f, 1.0f, 0.92f, 0.18f).eg(II, 0.009f, 0.7f, 0.62f, 0.13f)
              .touch(I, 0.20f, 0.64f, 0.20f, 0.26f).touch(II, 0.26f, 0.62f, 0.26f, 0.20f)
              .detune(0.7f).bend(0.035f).glide(0.015f).subAt(0.03f, 0.085f).res(0.08f).mix(0.24f));
    add("Contour Lead", "Leads", { "kbd-contour", "aftertouch" },
        "A rounded lower register opens progressively into brighter upper notes. A gentle high-key level trim balances that added brilliance while retaining clear solo articulation.",
        lead().lpf(I, 1550.0f, 0.12f).lpf(II, 2550.0f, 0.10f)
              .feg(I, 0.04f, 0.22f, 0.020f, 1.0f, 0.25f).feg(II, 0.02f, 0.25f, 0.014f, 0.65f, 0.19f)
              .eg(I, 0.017f, 1.3f, 0.91f, 0.28f).eg(II, 0.025f, 0.9f, 0.73f, 0.22f)
              .contour(-0.20f, 0.35f, 0.0f, -0.08f).detune(1.0f).subAt(0.03f, 0.08f).mix(0.29f));
}
