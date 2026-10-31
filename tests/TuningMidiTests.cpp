// Phase 6 modern layer: Scala/KBM microtuning, MIDI Tuning Standard SysEx,
// MPE zone negotiation (RPN 0 / RPN 6 MCM, upper zone), CC88 high-resolution
// velocity, unison and sixteen-voice modes, ribbon MIDI CC. All inert in PURE
// CS-80 except the controller adapters. Software checks, not hardware claims.
#include "core/Engine.h"
#include "core/MpeZones.h"
#include "core/Tuning.h"
#include "core/VoiceModes.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace cs80;
namespace tn = cs80::tuning;

namespace
{
int checks = 0, failures = 0;
void check(bool ok, const std::string& what)
{
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL: %s\n", what.c_str()); }
}
std::string readFile(const std::string& name)
{
    std::ifstream in(std::string(CS80_TUNING_DATA_DIR) + "/" + name, std::ios::binary);
    std::stringstream s;
    s << in.rdbuf();
    return s.str();
}
tn::TuningTable table(const std::string& scl, const std::string& kbm = {})
{
    tn::Scale scale;
    check(tn::parseScale(readFile(scl), scale).empty(), "parse " + scl);
    tn::KeyboardMapping map = tn::defaultMapping(scale);
    if (!kbm.empty()) check(tn::parseKeyboardMapping(readFile(kbm), map).empty(), "parse " + kbm);
    tn::TuningTable t;
    check(tn::buildTable(scale, map, t).empty(), "build " + scl + " " + kbm);
    return t;
}
std::vector<float> render(Engine& e, int frames)
{
    std::vector<float> out(static_cast<std::size_t>(frames) * 2);
    for (int i = 0; i < frames; i += 256) e.render(out.data() + static_cast<std::size_t>(i) * 2, std::min(256, frames - i));
    return out;
}
ParamValues quiet(float parity = 0.0f)
{
    ParamValues p;
    p[P_parityMode] = parity;
    p[P_voiceDispersion] = p[P_drift] = p[P_analogNoise] = 0.0f;
    p[P_detune] = 0.0f;
    return p;
}
float semitonesOf(const Engine& e, int voice, float reference)
{
    return 12.0f * std::log2(e.getRankInfo(voice, 0).oscillatorHz / midiToHz(reference));
}

void testScala()
{
    const auto tet = table("12tet.scl");
    bool equal = true;
    for (int n = 0; n < 128; ++n) equal = equal && std::abs(tet.pitch[(std::size_t) n] - n) < 1e-4f && tet.mapped[(std::size_t) n];
    check(equal, "12-TET Scala file reproduces the default tuning");
    const auto just = table("just5.scl");
    check(std::abs(just.pitch[64] - (60.0f + 12.0f * std::log2(1.25f))) < 1e-4f, "just major third above middle C");
    check(std::abs(just.pitch[67] - (60.0f + 12.0f * std::log2(1.5f))) < 1e-4f, "just fifth");
    check(std::abs(just.pitch[72] - 72.0f) < 1e-4f && std::abs(just.pitch[48] - 48.0f) < 1e-4f, "just octaves");
    const auto meantone = table("meantone.scl");
    check(std::abs(meantone.pitch[67] - (60.0f + 6.9657843f)) < 1e-4f, "quarter-comma meantone fifth");
    const auto edo19 = table("19edo.scl");
    check(std::abs(edo19.pitch[61] - (60.0f + 1200.0f / 19.0f / 100.0f)) < 1e-4f
              && std::abs(edo19.pitch[79] - 72.0f) < 1e-4f, "19-EDO steps and period");
    const auto a432 = table("12tet.scl", "a432.kbm");
    check(std::abs(a432.pitch[69] - tn::TuningTable::pitchFromHz(432.0)) < 1e-4f
              && std::abs(a432.pitch[57] - tn::TuningTable::pitchFromHz(216.0)) < 1e-4f, "KBM reference 432 Hz");
    const auto white = table("diatonic7.scl", "white-keys.kbm");
    check(std::abs(white.pitch[69] - 69.0f) < 1e-4f, "white-key map: A4 is the 440 Hz reference");
    check(std::abs(white.pitch[60] - tn::TuningTable::pitchFromHz(440.0 * 3.0 / 5.0)) < 1e-4f, "white-key map: C is the tonic");
    check(!white.mapped[61] && !white.mapped[63] && white.mapped[62] && white.mapped[64], "black keys unmapped (x)");
    check(std::abs(white.pitch[72] - tn::TuningTable::pitchFromHz(528.0)) < 1e-4f, "white-key map: formal octave degree 7");
    // Malformed input is rejected with a reason, never half-applied.
    tn::Scale s;
    check(!tn::parseScale("only a description\n", s).empty(), "too-short scale rejected");
    check(!tn::parseScale("d\n2\n100.0\nabc\n", s).empty(), "non-numeric pitch rejected");
    check(!tn::parseScale("d\n1\n0/1\n", s).empty(), "non-positive ratio rejected");
    tn::KeyboardMapping m;
    check(!tn::parseKeyboardMapping("1\n2\n", m).empty(), "too-short mapping rejected");
}

void testMtsSysEx()
{
    tn::TuningTable t;
    // Real-time single-note tuning: note 60 to 60.5, note 61 "no change".
    const std::vector<std::uint8_t> single { 0xF0, 0x7F, 0x7F, 0x08, 0x02, 0x00, 0x02,
        60, 60, 0x40, 0x00, 61, 0x7F, 0x7F, 0x7F, 0xF7 };
    check(tn::applyMtsSysEx(single.data(), (int) single.size(), t), "single-note change applied");
    check(std::abs(t.pitch[60] - 60.5f) < 1e-6f && t.pitch[61] == 61.0f, "single-note values and no-change marker");
    // Octave 1-byte: C +10 cents, all others 0.
    std::vector<std::uint8_t> octave { 0xF0, 0x7E, 0x7F, 0x08, 0x08, 0x03, 0x7F, 0x7F };
    for (int pc = 0; pc < 12; ++pc) octave.push_back(pc == 0 ? 74 : 64);
    octave.push_back(0xF7);
    tn::TuningTable o;
    check(tn::applyMtsSysEx(octave.data(), (int) octave.size(), o) && std::abs(o.pitch[48] - 48.1f) < 1e-6f
              && o.pitch[50] == 50.0f, "scale/octave 1-byte tuning");
    // Bulk dump: every note +50 cents.
    std::vector<std::uint8_t> bulk { 0xF0, 0x7E, 0x7F, 0x08, 0x01, 0x00 };
    for (int i = 0; i < 16; ++i) bulk.push_back(' ');
    for (int n = 0; n < 128; ++n) { bulk.push_back((std::uint8_t) n); bulk.push_back(0x40); bulk.push_back(0x00); }
    bulk.push_back(0x00); bulk.push_back(0xF7);
    tn::TuningTable b;
    check(tn::applyMtsSysEx(bulk.data(), (int) bulk.size(), b) && std::abs(b.pitch[100] - 100.5f) < 1e-6f, "bulk dump");
    // Other device, truncated and non-MTS messages are ignored.
    auto other = single; other[2] = 0x05;
    tn::TuningTable u;
    check(!tn::applyMtsSysEx(other.data(), (int) other.size(), u, 0x10) && u.isEqualTempered(), "other device ignored");
    const std::uint8_t junk[] { 0xF0, 0x43, 0x10, 0xF7 };
    check(!tn::applyMtsSysEx(junk, 4, u) && !tn::applyMtsSysEx(single.data(), 5, u), "non-MTS and truncated ignored");
}

void testEngineTuning()
{
    auto p = quiet();
    Engine e; e.prepare(48000.0); e.setParams(p);
    e.setScalaTuning(table("just5.scl"));
    e.noteOn(64, 0.8f); render(e, 64);
    check(std::abs(semitonesOf(e, 0, 64.0f)) < 1e-4f, "tuning_mode 0 ignores a loaded scale");
    p[P_tuningMode] = 1.0f; e.setParams(p); render(e, 4800);
    check(std::abs(semitonesOf(e, 0, 60.0f + 12.0f * std::log2(1.25f))) < 1e-3f, "Scala mode retunes the sounding note");
    e.setScalaTuning(table("diatonic7.scl", "white-keys.kbm"));
    e.noteOn(61, 0.8f);
    check(e.activeVoices() == 1, "an unmapped key does not sound");
    // MTS SysEx (tuning_mode 2).
    p[P_tuningMode] = 2.0f;
    Engine m; m.prepare(48000.0); m.setParams(p);
    const std::vector<std::uint8_t> single { 0xF0, 0x7F, 0x7F, 0x08, 0x02, 0x00, 0x01, 69, 69, 0x20, 0x00, 0xF7 };
    check(m.handleSysEx(single.data(), (int) single.size()), "engine accepts MTS SysEx");
    m.noteOn(69, 0.8f); render(m, 64);
    check(std::abs(semitonesOf(m, 0, 69.25f)) < 1e-3f, "MTS-tuned note plays a quarter tone sharp");
    // PURE CS-80 forces 12-TET.
    auto pure = p; pure[P_parityMode] = 1.0f;
    Engine q; q.prepare(48000.0); q.setParams(pure);
    q.handleSysEx(single.data(), (int) single.size());
    q.noteOn(69, 0.8f); render(q, 64);
    check(std::abs(semitonesOf(q, 0, 69.0f)) < 1e-3f, "PURE CS-80 ignores microtuning");
}

void testMpe()
{
    MpeZoneLayout z;
    z.reset();
    check(z.role(1) == MpeZoneLayout::Role::lowerMaster && z.role(16) == MpeZoneLayout::Role::lowerMember,
          "default: one lower zone with 15 members");
    auto rpn = [&](int ch, int msb, int lsb, int value) {
        z.controlChange(ch, 101, msb); z.controlChange(ch, 100, lsb); z.controlChange(ch, 6, value);
    };
    rpn(1, 0, 6, 7);
    rpn(16, 0, 6, 7);
    check(z.lowerMemberCount() == 7 && z.upperMemberCount() == 7, "MCM negotiates lower 7 and upper 7");
    check(z.role(8) == MpeZoneLayout::Role::lowerMember && z.role(9) == MpeZoneLayout::Role::upperMember
              && z.role(16) == MpeZoneLayout::Role::upperMaster, "zone roles follow the layout");
    rpn(16, 0, 6, 10);
    check(z.upperMemberCount() == 10 && z.lowerMemberCount() == 4, "a larger upper zone shrinks the lower zone");
    rpn(2, 0, 0, 24);
    check(z.memberRange(false) == 24.0f, "RPN 0 on a member sets the member bend range");
    z.controlChange(2, 101, 127); z.controlChange(2, 100, 127); z.controlChange(2, 6, 2);
    check(z.memberRange(false) == 24.0f, "null RPN deselects");

    // Engine: zone-negotiated bend and the upper zone.
    auto p = quiet(); p[P_mpeOn] = 1.0f;
    Engine e; e.prepare(48000.0); e.setParams(p);
    auto cc = [&](int ch, int n, int v) { e.controlChange(n, v / 127.0f, ch); };
    cc(1, 101, 0); cc(1, 100, 6); cc(1, 6, 7);
    cc(16, 101, 0); cc(16, 100, 6); cc(16, 6, 7);
    cc(16, 101, 0); cc(16, 100, 0); cc(16, 6, 12); // upper master range 12
    e.noteOn(60, 0.8f, 3);  // lower member
    e.noteOn(72, 0.8f, 10); // upper member
    e.pitchBend(16, 1.0f);   // upper master: +12
    render(e, 64);
    check(std::abs(semitonesOf(e, 0, 60.0f)) < 1e-3f && std::abs(semitonesOf(e, 1, 84.0f)) < 1e-3f,
          "upper-zone master bend moves only upper-zone notes, by its negotiated range");
    e.pitchBend(10, -0.5f); render(e, 64);
    check(std::abs(semitonesOf(e, 1, 84.0f - 24.0f)) < 1e-3f, "member bend uses the member range (default 48)");
    e.channelPressure(16, 0.7f);
    check(e.getVoiceInfo(1).pressure == 0.7f && e.getVoiceInfo(0).pressure == 0.0f, "upper master pressure stays in its zone");
    // RPN is ignored without MPE and in PURE CS-80; the default layout is unchanged.
    Engine plain; plain.prepare(48000.0); plain.setParams(quiet());
    plain.controlChange(101, 0.0f, 1); plain.controlChange(100, 0.0f, 1); plain.controlChange(6, 24 / 127.0f, 1);
    plain.noteOn(60, 0.8f); plain.pitchBend(1, 1.0f); render(plain, 64);
    check(std::abs(semitonesOf(plain, 0, 62.0f)) < 1e-3f, "without MPE, RPN does not change the bend range");

    HighResolutionVelocity hr;
    check(hr.noteOnVelocity(1, 100) == 100.0f * (1.0f / 127.0f), "no CC88: 7-bit velocity exactly as JUCE getFloatVelocity");
    hr.controlChange(1, 64);
    check(std::abs(hr.noteOnVelocity(1, 100) - (100.0f * 128.0f + 64.0f) / 16383.0f) < 1e-7f, "CC88 adds the low 7 bits");
    check(hr.noteOnVelocity(1, 100) == 100.0f * (1.0f / 127.0f), "the prefix is consumed by one note-on");
}

void testVoiceModes()
{
    for (int mode : {2, 3, 4})
    {
        const int n = mode == 2 ? 2 : mode == 3 ? 4 : 8;
        auto p = quiet(); p[P_voiceMode] = (float) mode; p[P_unisonDetune] = 20.0f;
        Engine e; e.prepare(48000.0); e.setParams(p);
        e.noteOn(60, 0.8f); render(e, 64);
        check(e.activeVoices() == n, "unison allocates " + std::to_string(n) + " circuits");
        float lo = 1e9f, hi = -1e9f;
        for (int v = 0; v < n; ++v) { const float s = semitonesOf(e, v, 60.0f); lo = std::min(lo, s); hi = std::max(hi, s); }
        check(std::abs(lo + 0.1f) < 1e-3f && std::abs(hi - 0.1f) < 1e-3f, "unison detune spreads symmetrically over 20 cents");
        e.noteOff(60);
        int held = 0;
        for (int v = 0; v < 8; ++v) held += e.getVoiceInfo(v).held ? 1 : 0;
        check(held == 0, "note-off releases every unison circuit");
        auto pure = p; pure[P_parityMode] = 1.0f;
        Engine q; q.prepare(48000.0); q.setParams(pure); q.noteOn(60, 0.8f);
        check(q.activeVoices() == 1 && q.getVoiceMode() == 0, "PURE CS-80 keeps eight single voices");
    }
    // Sixteen voices: two instruments, the second takes keys when the first is full.
    auto p = quiet(); p[P_voiceMode] = 1.0f;
    Engine a, b; a.prepare(48000.0); b.prepare(48000.0); a.setParams(p); b.setParams(p);
    check(a.getVoiceMode() == 1, "sixteen-voice mode accepted");
    for (int k = 0; k < 16; ++k) SixteenVoiceRouter::noteOnTarget(a, b).noteOn(40 + k, 0.8f);
    check(a.activeVoices() == 8 && b.activeVoices() == 8, "sixteen held keys fill both instruments");
}

void testRibbonCc()
{
    auto p = quiet();
    Engine off; off.prepare(48000.0); off.setParams(p);
    off.noteOn(60, 0.8f); off.controlChange(16, 1.0f); render(off, 64);
    check(off.getVoiceInfo(0).ribbonRatio == 1.0f, "ribbon CC is off by default");
    p[P_ribbonCcEnable] = 1.0f;
    for (float parity : {0.0f, 1.0f})
    {
        p[P_parityMode] = parity;
        Engine e; e.prepare(48000.0); e.setParams(p);
        e.noteOn(60, 0.8f);
        e.controlChange(16, 127.0f / 127.0f); render(e, 64);
        check(e.getVoiceInfo(0).ribbonRatio == ribbonFrequencyRatio(1.0f), "CC16 127 is full ribbon travel");
        e.controlChange(16, 64.0f / 127.0f); render(e, 64);
        check(e.getVoiceInfo(0).ribbonRatio == 1.0f, "CC16 64 is the ribbon centre");
        e.controlChange(16, 0.0f); render(e, 64);
        check(e.getVoiceInfo(0).ribbonRatio == ribbonFrequencyRatio(-1.0f), "CC16 0 is full downward travel");
    }
    p[P_parityMode] = 0.0f; p[P_ribbonCc] = 2.0f;
    Engine other; other.prepare(48000.0); other.setParams(p);
    other.noteOn(60, 0.8f); other.controlChange(16, 1.0f); other.controlChange(2, 1.0f); render(other, 64);
    check(other.getVoiceInfo(0).ribbonRatio == ribbonFrequencyRatio(1.0f), "the ribbon CC number is assignable");
}
} // namespace

int main()
{
    testScala();
    testMtsSysEx();
    testEngineTuning();
    testMpe();
    testVoiceModes();
    testRibbonCc();
    std::printf("Tuning / MIDI / voice modes: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
