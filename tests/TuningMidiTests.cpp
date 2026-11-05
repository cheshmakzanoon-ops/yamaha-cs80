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
#include <limits>
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

void testMtsMessageBoundaries()
{
    using Bytes = std::vector<std::uint8_t>;
    const Bytes single { 0xF0, 0x7F, 0x7F, 0x08, 0x02, 0, 2,
                         60, 60, 0x40, 0, 64, 64, 0x20, 0, 0xF7 };
    const Bytes bank { 0xF0, 0x7F, 0x7F, 0x08, 0x07, 3, 4, 2,
                       60, 60, 0x40, 0, 64, 64, 0x20, 0, 0xF7 };
    Bytes bulk { 0xF0, 0x7E, 0x7F, 0x08, 0x01, 0 };
    bulk.insert(bulk.end(), 16, ' ');
    for (int n = 0; n < 128; ++n)
        bulk.insert(bulk.end(), { static_cast<std::uint8_t>(n), 0x40, 0 });
    bulk.insert(bulk.end(), { 0, 0xF7 }); // legacy bulk checksum is intentionally ignored
    Bytes octave1 { 0xF0, 0x7F, 0x7F, 0x08, 0x08, 3, 0x7F, 0x7F };
    octave1.insert(octave1.end(), 12, 74);
    octave1.push_back(0xF7);
    Bytes octave2 { 0xF0, 0x7F, 0x7F, 0x08, 0x09, 3, 0x7F, 0x7F };
    for (int n = 0; n < 12; ++n) octave2.insert(octave2.end(), { 0x50, 0 });
    octave2.push_back(0xF7);
    auto nonRealtime = [](Bytes message) { message[1] = 0x7E; return message; };
    const std::vector<Bytes> valid { single, bank, nonRealtime(bank), bulk,
                                   octave1, nonRealtime(octave1), octave2, nonRealtime(octave2) };

    // Use a non-default table, including unmapped keys, so a failed transaction
    // cannot hide a reset-to-12-TET or a change to the mapping bits.
    tn::TuningTable before;
    for (int n = 0; n < 128; ++n)
    {
        before.pitch[static_cast<std::size_t>(n)] += 0.125f;
        before.mapped[static_cast<std::size_t>(n)] = n % 3 != 0;
    }
    auto rejectedUnchanged = [&](const Bytes& bytes) {
        auto actual = before;
        return !tn::applyMtsSysEx(bytes.data(), static_cast<int>(bytes.size()), actual)
            && actual.pitch == before.pitch && actual.mapped == before.mapped;
    };
    std::size_t malformedCases = 0;
    for (std::size_t fixture = 0; fixture < valid.size(); ++fixture)
    {
        const auto& bytes = valid[fixture];
        auto accepted = before;
        check(tn::applyMtsSysEx(bytes.data(), static_cast<int>(bytes.size()), accepted),
              "complete MTS boundary fixture accepted " + std::to_string(fixture));
        const float expected64 = fixture == 3 ? 64.5f : fixture == 4 || fixture == 5 ? 64.1f : 64.25f;
        check(accepted.pitch[64] == expected64 && accepted.mapped[64],
              "accepted MTS boundary fixture has the expected pitch " + std::to_string(fixture));
        bool prefixesRejected = true, extraRejected = true, highBitsRejected = true;
        // Every prematurely terminated message, even after a complete first
        // note, must be rejected before ANY table entry can change.
        for (std::size_t kept = 1; kept + 1 < bytes.size(); ++kept)
        {
            Bytes truncated(bytes.begin(), bytes.begin() + kept);
            truncated.push_back(0xF7);
            prefixesRejected &= rejectedUnchanged(truncated);
            ++malformedCases;
        }
        for (int extra : { 1, 4, 17 })
        {
            auto oversized = bytes;
            oversized.insert(oversized.end() - 1, extra, 0);
            extraRejected &= rejectedUnchanged(oversized);
            ++malformedCases;
        }
        // A status byte is not 7-bit SysEx data. Corrupt every position,
        // especially late entries that used to allow earlier notes to apply.
        for (std::size_t at = 1; at + 1 < bytes.size(); ++at)
            for (const int invalid : { 0x80, 0xF0, 0xF7, 0xFF })
            {
                auto corrupt = bytes;
                corrupt[at] = static_cast<std::uint8_t>(invalid);
                highBitsRejected &= rejectedUnchanged(corrupt);
                ++malformedCases;
            }
        const auto label = std::to_string(fixture);
        check(prefixesRejected, "all truncated MTS payloads rejected atomically " + label);
        check(extraRejected, "all oversized MTS payloads rejected atomically " + label);
        check(highBitsRejected, "all non-data MTS bytes rejected atomically " + label);
    }
    std::printf("MTS boundary matrix: %zu malformed messages\n", malformedCases);

    // The sentinel is valid data, not a malformed frequency. It must leave
    // both the old pitch and mapped flag untouched while other entries apply.
    auto noChange = single;
    noChange[8] = noChange[9] = noChange[10] = 0x7F;
    auto sentinel = before;
    check(tn::applyMtsSysEx(noChange.data(), static_cast<int>(noChange.size()), sentinel)
          && sentinel.pitch[60] == before.pitch[60] && sentinel.mapped[60] == before.mapped[60]
          && sentinel.pitch[64] == 64.25f && sentinel.mapped[64],
          "valid no-change sentinel preserves its entry while the next note applies");
    noChange[12] = noChange[13] = noChange[14] = 0x7F;
    check(rejectedUnchanged(noChange), "all-no-change MTS payload leaves the table untouched");
    const Bytes empty { 0xF0, 0x7F, 0x7F, 0x08, 0x02, 0, 0, 0xF7 };
    check(rejectedUnchanged(empty), "empty MTS update remains a no-op");
    Bytes maximum { 0xF0, 0x7F, 0x7F, 0x08, 0x02, 0, 127 };
    for (int n = 0; n < 127; ++n)
        maximum.insert(maximum.end(), { static_cast<std::uint8_t>(n), static_cast<std::uint8_t>(n), 0x20, 0 });
    maximum.push_back(0xF7);
    auto maximumTable = before;
    bool maximumApplied = tn::applyMtsSysEx(maximum.data(), static_cast<int>(maximum.size()), maximumTable);
    for (int n = 0; n < 127; ++n)
        maximumApplied &= maximumTable.pitch[static_cast<std::size_t>(n)] == n + 0.25f
                       && maximumTable.mapped[static_cast<std::size_t>(n)];
    check(maximumApplied && maximumTable.pitch[127] == before.pitch[127],
          "maximum seven-bit count applies every complete note and preserves the unaddressed one");
    const Bytes topNote { 0xF0, 0x7F, 0x7F, 0x08, 0x02, 0, 1, 127, 127, 127, 126, 0xF7 };
    check(tn::applyMtsSysEx(topNote.data(), static_cast<int>(topNote.size()), maximumTable)
          && maximumTable.pitch[127] == 127.0f + 16382.0f / 16384.0f,
          "highest legal key and pitch are data, not status bytes or the no-change sentinel");

    // A malformed performance message must not retune a sounding voice or
    // perturb its continuation. This crosses the parser/Engine boundary.
    auto p = quiet(); p[P_tuningMode] = 2;
    Engine actual, reference;
    for (auto* e : { &actual, &reference })
    {
        e->prepare(48000.0); e->setParams(p); e->setMtsTuning(before);
        e->noteOn(64, 0.8f); render(*e, 256);
    }
    auto truncated = single;
    truncated.resize(12);
    truncated.back() = 0xF7;
    // Make the first (formerly partially accepted) entry affect the held key.
    truncated[7] = 64; truncated[8] = 76;
    check(!actual.handleSysEx(truncated.data(), static_cast<int>(truncated.size())),
          "engine rejects truncated two-note MTS message");
    check(actual.getMtsTuning().pitch == before.pitch && actual.getMtsTuning().mapped == before.mapped,
          "engine preserves the complete table after a malformed MTS message");
    check(render(actual, 4096) == render(reference, 4096),
          "rejected MTS message leaves sounding audio continuation bit-identical");
    check(actual.handleSysEx(single.data(), static_cast<int>(single.size())),
          "a valid MTS message still applies after a rejected one");
    render(actual, 64);
    check(std::abs(semitonesOf(actual, 0, 64.25f)) < 1e-3f,
          "valid MTS message retunes the sounding note after rejection");
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


// Generated ARP notes obey the same mapping admission as direct onsets. Keep
// excluded steps as rests: the next octave may be mapped, and the table can
// change while its source key remains held.
void testArpeggiatorTuningMap()
{
    const auto white = table("diatonic7.scl", "white-keys.kbm");
    tn::Scale scale;
    check(tn::parseScale(readFile("12tet.scl"), scale).empty(), "parse ARP range scale");
    auto range = tn::defaultMapping(scale);
    range.first = 60;
    range.last = 71;
    tn::TuningTable narrow;
    check(tn::buildTable(scale, range, narrow).empty(), "build ARP range mapping");

    const auto heldVoice = [](const Engine& engine) {
        for (int voice = 0; voice < 8; ++voice)
            if (const auto info = engine.getVoiceInfo(voice); info.active && info.held)
                return voice;
        return -1;
    };
    const auto heldNote = [&](const Engine& engine) {
        const int voice = heldVoice(engine);
        return voice < 0 ? -1 : engine.getVoiceInfo(voice).note;
    };
    const auto load = [](Engine& engine, const tn::TuningTable& mapping, int mode) {
        if (mode == 1) engine.setScalaTuning(mapping);
        else engine.setMtsTuning(mapping); // Table API supports mapped flags; no SysEx claim.
    };

    for (bool source : {false, true})
        for (int mode : {1, 2})
        {
            auto p = quiet();
            p[P_sourcePerformance] = source ? 1.0f : 0.0f;
            p[P_tuningMode] = (float) mode;
            p[P_arpOn] = 1.0f;
            p[P_arpRate] = 16.0f; // 3,000 host frames per step at 48 kHz.
            const std::string label = std::string(source ? "Circuit" : "Legacy")
                + " ARP tuning " + std::to_string(mode) + ": ";

            Engine excluded;
            excluded.prepare(48000.0);
            excluded.setParams(p);
            load(excluded, white, mode);
            excluded.noteOn(61, 0.8f, 2);
            render(excluded, 1);
            check(excluded.activeVoices() == 0, label + "bundled KBM x key allocates no voice");

            auto octaves = p;
            octaves[P_arpOctaves] = 2.0f;
            Engine ranged;
            ranged.prepare(48000.0);
            ranged.setParams(octaves);
            load(ranged, narrow, mode);
            ranged.noteOn(60, 0.8f, 2);
            render(ranged, 1);
            check(heldNote(ranged) == 60, label + "mapped root starts");
            render(ranged, 3001);
            check(heldNote(ranged) == -1, label + "excluded generated octave is a timed rest");
            render(ranged, 3000);
            check(heldNote(ranged) == 60, label + "next mapped step keeps the scheduled clock");

            Engine dynamic;
            dynamic.prepare(48000.0);
            dynamic.setParams(p);
            tn::TuningTable live;
            load(dynamic, live, mode);
            dynamic.noteOn(60, 0.8f, 2);
            render(dynamic, 1);
            check(heldNote(dynamic) == 60, label + "initial table admits onset");
            live.mapped[60] = false;
            load(dynamic, live, mode);
            render(dynamic, 0);
            check(heldNote(dynamic) == 60, label + "table edit preserves the existing gate");
            render(dynamic, 3001);
            check(heldNote(dynamic) == -1, label + "next onset uses the current table");
            live.mapped[60] = true;
            load(dynamic, live, mode);
            render(dynamic, 3000);
            check(heldNote(dynamic) == 60, label + "remapping resumes without a new key strike");

            Engine generated;
            generated.prepare(48000.0);
            generated.setParams(octaves);
            tn::TuningTable hole;
            hole.mapped[60] = false;
            load(generated, hole, mode);
            generated.noteOn(60, 0.8f, 2);
            render(generated, 1);
            check(heldNote(generated) == -1, label + "unmapped root step rests");
            render(generated, 3001);
            check(heldNote(generated) == 72, label + "mapped octave of an unmapped root still plays");

            Engine allocation;
            allocation.prepare(48000.0);
            allocation.setParams(p);
            load(allocation, hole, mode);
            allocation.noteOn(60, 0.3f, 2);
            allocation.noteOn(64, 0.7f, 7);
            render(allocation, 1);
            check(allocation.activeVoices() == 0, label + "rest does not consume an allocator slot");
            render(allocation, 3001);
            const auto info = allocation.getVoiceInfo(0);
            check(heldVoice(allocation) == 0 && info.note == 64
                      && info.channel == 7 && info.velocity == 0.7f,
                  label + "next mapped note keeps allocator, channel and velocity");

            // Merely loading exclusions does not enable them in 12-TET mode.
            p[P_tuningMode] = 0.0f;
            Engine bypass;
            bypass.prepare(48000.0);
            bypass.setParams(p);
            load(bypass, white, mode);
            bypass.noteOn(61, 0.8f, 2);
            render(bypass, 1);
            check(heldNote(bypass) == 61, label + "tuning mode zero ignores loaded exclusions");
        }
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

// Damper inheritance is limited to negotiated zone members. Unassigned
// channels remain conventional MIDI channels. Own-channel CC64 is deliberately
// retained as this engine's compatibility behavior, including on MPE members.
void testMpePedalZoneBoundaries()
{
    const std::array<std::array<int, 2>, 7> layouts {{
        {{15, 0}}, {{0, 15}}, {{7, 7}}, {{2, 2}}, {{0, 2}}, {{2, 0}}, {{0, 0}}
    }};
    // Legacy/Circuit with MPE off/on, plus PURE with requested MPE ignored.
    for (int policy = 0; policy < 5; ++policy)
    {
        Engine e; e.prepare(48000.0);
        auto p = quiet(policy == 4 ? 1.0f : 0.0f);
        p[P_sourcePerformance] = policy >= 2 ? 1.0f : 0.0f;
        p[P_mpeOn] = policy % 2 != 0 || policy == 4 ? 1.0f : 0.0f;
        p[P_kasScanTiming] = 0.0f;
        p[P_sustainMode] = p[P_masterSustain] = 0.0f;
        e.setParams(p);
        const bool mpe = policy == 1 || policy == 3;
        auto cc = [&](int channel, int controller, int value) {
            e.controlChange(controller, value / 127.0f, channel);
        };
        auto zone = [&](int channel, int members) {
            cc(channel, 101, 0); cc(channel, 100, 6); cc(channel, 6, members);
        };
        auto voice = [&](int channel) {
            for (int i = 0; i < 8; ++i)
            {
                const auto v = e.getVoiceInfo(i);
                if (v.active && v.note == 60 && v.channel == channel) return v;
            }
            return Engine::VoiceInfo {};
        };
        for (const auto& layout : layouts)
        {
            e.allNotesOff(false);
            zone(1, 0); zone(16, 0);
            zone(1, layout[0]); zone(16, layout[1]);
            for (int channel = 1; channel <= 16; ++channel)
                for (int pedal : {1, 8, 16, channel})
                {
                    e.allNotesOff(false); // Clears every pedal between fixtures.
                    e.noteOn(60, 0.8f, channel);
                    check(voice(channel).held, "pedal zone fixture allocates its note");
                    cc(pedal, 64, 127);
                    e.noteOff(60, channel);
                    const bool lowerMember = layout[0] > 0 && channel >= 2 && channel <= 1 + layout[0];
                    const bool upperMember = layout[1] > 0 && channel <= 15 && channel >= 16 - layout[1];
                    const bool expected = policy != 4 && (pedal == channel
                        || (mpe && ((lowerMember && pedal == 1) || (upperMember && pedal == 16))));
                    const auto released = voice(channel);
                    const auto label = "policy " + std::to_string(policy) + " zones "
                        + std::to_string(layout[0]) + "/" + std::to_string(layout[1])
                        + " note channel " + std::to_string(channel) + " pedal " + std::to_string(pedal);
                    check(!released.held && released.pedalHeld == expected, label + " correct pedal scope");
                    cc(pedal, 64, 0);
                    check(!voice(channel).pedalHeld, label + " pedal-up releases its hold");
                }
        }
        if (!mpe) continue;
        zone(1, 2); zone(16, 2);
        for (int sustainMode : {0, 1})
            for (bool channelRelease : {false, true})
                for (bool masterFirst : {false, true})
                    for (int channel : {2, 8, 15})
                    {
                        p[P_sustainMode] = static_cast<float>(sustainMode);
                        e.setParams(p);
                        const int master = channel == 15 ? 16 : 1;
                        e.allNotesOff(false);
                        e.noteOn(60, 0.8f, channel);
                        cc(channel, 64, 127); cc(master, 64, 127);
                        if (channelRelease) cc(channel, 123, 0);
                        else e.noteOff(60, channel);
                        check(voice(channel).pedalHeld, "own pedal holds beside a master pedal");
                        cc(7, 64, 127); cc(7, 64, 0);
                        check(voice(channel).pedalHeld, "unrelated pedal-up does not release a held note");
                        cc(masterFirst ? master : channel, 64, 0);
                        check(voice(channel).pedalHeld == (masterFirst || channel != 8),
                              "first pedal-up leaves only an applicable second hold");
                        cc(masterFirst ? channel : master, 64, 0);
                        check(!voice(channel).pedalHeld, "last relevant pedal-up releases the note");
                        cc(masterFirst ? channel : master, 64, 0);
                        check(!voice(channel).pedalHeld, "repeated pedal-up remains released");
                    }
    }
}

// Master pressure/CC74 applies only inside an assigned MPE zone. The global
// latest-channel-pressure matrix source and historical non-MPE routing remain.
void testMpeExpressionZoneBoundaries()
{
    using namespace modern;
    struct Fixture
    {
        Engine engine;
        ParamValues params;
        bool pure = false, mpe = false;
        explicit Fixture(int policy) : params(quiet(policy == 4 ? 1.0f : 0.0f))
        {
            pure = policy == 4;
            mpe = policy == 1 || policy == 3;
            params[P_sourcePerformance] = policy >= 2 ? 1.0f : 0.0f;
            params[P_mpeOn] = mpe || pure ? 1.0f : 0.0f;
            params[P_kasScanTiming] = params[P_arpOn] = params[P_voiceMode] = 0.0f;
            params[P_pressureCurve] = params[P_pressureThreshold] = 0.0f;
            params[P_keyRelease] = 5.0f;
            params[P_mod1Amount] = params[P_mod2Amount] = params[P_mod3Amount] = 1.0f;
            ModernConfig config;
            config.slots[0] = { ModSource::polyPressure, ModSource::none, ModDestination::pitch, ModTarget::both };
            config.slots[1] = { ModSource::timbre, ModSource::none, ModDestination::pan, ModTarget::both };
            config.slots[2] = { ModSource::channelPressure, ModSource::none, ModDestination::fx1Mix, ModTarget::both };
            engine.prepare(48000.0);
            engine.setModernConfig(config);
            engine.setParams(params);
        }
        void zone(int channel, int members)
        {
            engine.controlChange(101, 0.0f, channel);
            engine.controlChange(100, 6.0f / 127.0f, channel);
            engine.controlChange(6, members / 127.0f, channel);
        }
        void layout(int lower, int upper)
        {
            zone(1, 0); zone(16, 0); zone(1, lower); zone(16, upper);
            if (mpe)
                check(engine.getMpeZones().lowerMemberCount() == lower
                          && engine.getMpeZones().upperMemberCount() == upper,
                      "expression fixture accepts the requested zone layout");
        }
        int start(int channel, bool releaseTail = false)
        {
            engine.allNotesOff(false);
            engine.noteOn(60, 0.8f, channel);
            int index = -1;
            for (int i = 0; i < 8; ++i)
            {
                const auto v = engine.getVoiceInfo(i);
                if (v.active && v.held && v.note == 60 && v.channel == channel) index = i;
            }
            check(index >= 0, "expression fixture allocates its requested channel");
            if (releaseTail)
            {
                render(engine, 256);
                engine.noteOff(60, channel);
                check(engine.getVoiceInfo(index).active && !engine.getVoiceInfo(index).held,
                      "expression fixture retains a sounding release tail");
            }
            return index;
        }
        void send(int channel, float value)
        {
            engine.channelPressure(channel, value);
            engine.controlChange(74, value, channel);
        }
        void verify(int index, float expected, int frames, const std::string& label)
        {
            check(index >= 0 && engine.getVoiceInfo(index).pressure == expected,
                  label + " per-voice pressure target");
            render(engine, frames);
            check(engine.getVoiceInfo(index).active, label + " note or tail remains active");
            const auto& rank = engine.getModernRankOffsets(index, 0);
            const float modernExpected = pure ? 0.0f : expected;
            check(std::abs(rank.pitch - 12.0f * modernExpected) < 0.001f,
                  label + " per-voice pressure matrix route");
            check(std::abs(rank.pan - modernExpected) < 0.0001f,
                  label + " per-voice CC74 matrix route");
        }
    };

    const std::array<std::array<int, 2>, 7> layouts {{
        {{15, 0}}, {{0, 15}}, {{7, 7}}, {{2, 2}}, {{0, 2}}, {{2, 0}}, {{0, 0}}
    }};
    // Stable non-MPE Legacy/Circuit, MPE Legacy/Circuit, and PURE's ignored MPE.
    for (int policy = 0; policy < 5; ++policy)
    {
        Fixture f(policy);
        for (const auto& layout : layouts)
        {
            f.layout(layout[0], layout[1]);
            for (int channel = 1; channel <= 16; ++channel)
                for (int controller : {1, 8, 16, channel})
                {
                    const int index = f.start(channel);
                    const bool lower = layout[0] > 0 && channel <= 1 + layout[0];
                    const bool upper = layout[1] > 0 && channel >= 16 - layout[1];
                    const bool applies = controller == channel || (f.mpe
                        ? ((lower && controller == 1) || (upper && controller == 16))
                        : controller == 1);
                    const auto label = "expression policy " + std::to_string(policy) + " layout "
                        + std::to_string(layout[0]) + "/" + std::to_string(layout[1])
                        + " note " + std::to_string(channel) + " controller " + std::to_string(controller);
                    f.send(controller, 0.75f);
                    f.verify(index, applies ? 0.75f : 0.0f, 2, label);
                }
        }
    }

    for (int policy : {1, 3})
        for (bool tail : {false, true})
        {
            // Live shrink/disable can make a retained voice unassigned. Stop
            // only its master inheritance; its own channel still controls it.
            for (const auto change : {std::array<int, 4>{3, 1, 1, 2},
                                      std::array<int, 4>{14, 16, 2, 1},
                                      std::array<int, 4>{2, 1, 0, 2},
                                      std::array<int, 4>{15, 16, 2, 0}})
            {
                Fixture f(policy); f.layout(2, 2);
                const int channel = change[0], master = change[1];
                const int index = f.start(channel, tail);
                f.send(master, 0.25f); f.verify(index, 0.25f, 3000, "assigned master before shrink");
                f.layout(change[2], change[3]);
                check(f.engine.getMpeZones().role(channel) == MpeZoneLayout::Role::none,
                      "reconfiguration makes the retained channel unassigned");
                f.send(master, 0.75f); f.verify(index, 0.25f, 3000, "unassigned voice ignores old master");
                f.send(channel, 0.5f); f.verify(index, 0.5f, 3000, "unassigned voice keeps own channel");
            }
            // A formerly unassigned voice can enter the lower zone without
            // changing its captured non-upper ownership classification.
            {
                Fixture f(policy); f.layout(2, 2);
                const int index = f.start(8, tail);
                f.send(1, 0.75f); f.verify(index, 0.0f, 3000, "gap before lower expansion");
                f.zone(1, 7);
                f.send(1, 0.5f); f.verify(index, 0.5f, 3000, "lower expansion enables its master");
            }
            // The MPE condition is essential: a gap channel carried into
            // non-MPE retains the historical channel-1 broadcast, then loses
            // that cross-channel inheritance again when MPE is re-enabled.
            {
                Fixture f(policy); f.layout(2, 2);
                const int index = f.start(8, tail);
                f.send(1, 0.75f); f.verify(index, 0.0f, 3000, "gap voice before MPE off");
                f.params[P_mpeOn] = 0.0f; f.engine.setParams(f.params);
                f.send(1, 0.5f); f.verify(index, 0.5f, 3000, "MPE off restores gap broadcast");
                f.params[P_mpeOn] = 1.0f; f.engine.setParams(f.params);
                f.send(1, 0.25f); f.verify(index, 0.5f, 3000, "MPE on excludes gap again");
                f.send(8, 0.25f); f.verify(index, 0.25f, 3000, "gap own channel after MPE toggles");
            }
            // Preserve existing captured-upper semantics outside Role::none.
            // This is compatibility coverage, not a new live-retag policy.
            {
                Fixture f(policy); f.layout(2, 2);
                const int index = f.start(14, tail);
                f.send(16, 0.25f); f.verify(index, 0.25f, 3000, "captured upper initial control");
                f.layout(14, 0); // Channel 14 is assigned lower, still captured upper.
                f.send(1, 0.75f); f.verify(index, 0.25f, 3000, "assigned voice is not live-retagged");
                f.send(14, 0.5f); f.verify(index, 0.5f, 3000, "retained upper own channel");
            }
            {
                Fixture f(policy); f.layout(2, 2);
                const int index = f.start(15, tail);
                f.send(16, 0.25f); f.verify(index, 0.25f, 3000, "upper voice before MPE toggle");
                f.params[P_mpeOn] = 0.0f; f.engine.setParams(f.params);
                f.send(1, 0.75f); f.verify(index, 0.25f, 3000, "MPE off preserves captured upper branch");
                f.send(16, 0.5f); f.verify(index, 0.5f, 3000, "MPE off retains existing upper master");
                f.params[P_mpeOn] = 1.0f; f.engine.setParams(f.params);
                f.send(16, 0.25f); f.verify(index, 0.25f, 3000, "MPE on resumes same upper ownership");
            }
        }

    // Rejection is only per-voice inheritance. Latest channel pressure remains
    // a global matrix source, even when it does not address this voice.
    for (int policy : {1, 3})
    {
        Fixture f(policy); f.layout(2, 2);
        const int index = f.start(8);
        f.send(1, 0.75f); f.verify(index, 0.0f, 3000, "global tap preserves unrelated voice targets");
        check(std::abs(f.engine.getModernGlobalOffsets().fxMix[0] - 0.75f) < 0.0001f,
              "latest channel pressure stays globally available to the matrix");
        f.send(8, 0.5f); f.verify(index, 0.5f, 3000, "own channel before malformed expression");
        f.send(8, std::numeric_limits<float>::quiet_NaN());
        f.verify(index, 0.5f, 3000, "nonfinite own-channel expression remains ignored");
    }
}

// Data Entry belongs to the last selected parameter family. Unsupported NRPNs
// must not reuse an earlier MPE RPN (MIDI.org, About MIDI Part 3, MIDI Messages).
void testMpeNrpnIsolation()
{
    const auto exercise = [](const std::string& path, int firstNrpn, auto cc, auto zones)
    {
        const auto rpn = [&](int channel, int parameter, int value) {
            cc(channel, 101, 0); cc(channel, 100, parameter); cc(channel, 6, value);
        };
        const std::string label = path + " NRPN CC" + std::to_string(firstNrpn) + ": ";
        rpn(1, 6, 7); rpn(16, 6, 7);
        check(zones().lowerMemberCount() == 7 && zones().upperMemberCount() == 7,
              label + "fixture has two seven-member zones");
        for (int channel : { 1, 16 })
        {
            // Each selector independently terminates the prior RPN, including
            // an incomplete NRPN pair and either selector order.
            cc(channel, firstNrpn, 1); cc(channel, 6, 3);
            check(zones().lowerMemberCount() == 7 && zones().upperMemberCount() == 7,
                  label + "first NRPN selector prevents stale MCM data entry");
            cc(channel, firstNrpn == 99 ? 98 : 99, 2); cc(channel, 6, 4);
            check(zones().lowerMemberCount() == 7 && zones().upperMemberCount() == 7,
                  label + "complete NRPN pair cannot change either zone");
        }
        rpn(1, 6, 7); rpn(16, 6, 7); // Isolate range checks from a failing zone check.
        // NRPN cancellation is per channel and covers both data-entry bytes,
        // whether the prior RPN selected a master or a member bend range.
        for (int channel : { 1, 2, 10, 16 })
        {
            const bool upper = channel >= 10;
            const bool master = channel == 1 || channel == 16;
            const auto range = [&] {
                return master ? zones().masterRange(upper) : zones().memberRange(upper);
            };
            rpn(channel, 0, 24);
            check(range() == 24.0f, label + "legitimate RPN bend range applies");
            cc(channel, firstNrpn, 1); cc(channel, 38, 50);
            check(range() == 24.0f, label + "NRPN fine data cannot reuse an RPN MSB");
            cc(channel, 6, 3); cc(channel, 38, 25);
            check(range() == 24.0f, label + "NRPN coarse and fine data preserve bend range");
            // Reselect a supported RPN with the alternate byte order.
            cc(channel, 100, 0); cc(channel, 101, 0); cc(channel, 6, 12); cc(channel, 38, 50);
            check(range() == 12.5f, label + "RPN reselection restores coarse/fine data entry");
        }
        rpn(2, 0, 24); rpn(10, 0, 36);
        cc(2, firstNrpn, 1); cc(10, 6, 40);
        check(zones().memberRange(false) == 24.0f && zones().memberRange(true) == 40.0f,
              label + "NRPN selection leaves another channel's RPN selected");
        // Registered selector bytes remain cached independently of NRPN bytes.
        // Returning with just one RPN selector retains the other RPN byte, and
        // must not reuse the unsupported transaction's data MSB.
        for (int rpnSelector : { 100, 101 })
        {
            rpn(2, 0, 24);
            cc(2, 99, 2); cc(2, 98, 6); cc(2, 6, 3);
            cc(2, rpnSelector, 0); cc(2, 38, 50);
            check(zones().memberRange(false) == 24.0f,
                  label + "RPN return does not reuse NRPN data MSB");
            cc(2, 6, 13);
            check(zones().memberRange(false) == 13.0f,
                  label + "single-byte RPN return retains the other registered selector");
        }
        rpn(1, 6, 5);
        check(zones().lowerMemberCount() == 5 && zones().upperMemberCount() == 7,
              label + "MCM reselection still updates the intended zone");
    };
    for (int firstNrpn : { 98, 99 })
    {
        MpeZoneLayout layout;
        exercise("layout", firstNrpn,
                 [&](int channel, int controller, int value) { layout.controlChange(channel, controller, value); },
                 [&]() -> const MpeZoneLayout& { return layout; });
        auto p = quiet(); p[P_mpeOn] = 1.0f;
        Engine engine; engine.prepare(48000.0); engine.setParams(p);
        exercise("engine", firstNrpn,
                 [&](int channel, int controller, int value) { engine.controlChange(controller, value / 127.0f, channel); },
                 [&]() -> const MpeZoneLayout& { return engine.getMpeZones(); });
    }
}


// An MCM with zero members removes only its addressed zone. With no second
// master channel reserved, the opposite zone may still own all 15 members.
void testMpeZeroMemberConfiguration()
{
    const auto exercise = [](const std::string& label, int master, int members, auto cc, auto zones)
    {
        const auto rpn = [&](int channel, int parameter, int value) {
            cc(channel, 101, 0); cc(channel, 100, parameter); cc(channel, 6, value);
        };
        rpn(1, 6, 0); rpn(16, 6, 0);
        rpn(master, 6, members);
        if (members > 0)
        {
            rpn(master, 0, 7);
            rpn(master == 1 ? 2 : 15, 0, 23);
        }
        std::array<MpeZoneLayout::Role, 16> before {};
        for (int channel = 1; channel <= 16; ++channel)
            before[(size_t) channel - 1] = zones().role(channel);
        const bool upper = master == 16;
        const float masterRange = zones().masterRange(upper);
        const float memberRange = zones().memberRange(upper);
        for (int repeat = 0; repeat < 2; ++repeat)
        {
            rpn(17 - master, 6, 0);
            const auto& z = zones();
            check(z.lowerMemberCount() == (upper ? 0 : members)
                      && z.upperMemberCount() == (upper ? members : 0),
                  label + "disabling the opposite zone preserves the active member count");
            bool sameRoles = true;
            for (int channel = 1; channel <= 16; ++channel)
                sameRoles &= z.role(channel) == before[(size_t) channel - 1];
            check(sameRoles, label + "disabling the opposite zone preserves every channel role");
            check(z.masterRange(upper) == masterRange && z.memberRange(upper) == memberRange,
                  label + "disabling the opposite zone preserves active negotiated ranges");
        }
    };
    for (int master : { 1, 16 })
        for (int members = 0; members <= 15; ++members)
        {
            const auto label = "MCM master " + std::to_string(master) + " members "
                + std::to_string(members) + ": ";
            MpeZoneLayout layout;
            exercise("layout " + label, master, members,
                     [&](int channel, int controller, int value) { layout.controlChange(channel, controller, value); },
                     [&]() -> const MpeZoneLayout& { return layout; });
            for (float source : { 0.0f, 1.0f })
            {
                auto p = quiet(); p[P_sourcePerformance] = source; p[P_mpeOn] = 1.0f;
                Engine engine; engine.prepare(48000.0); engine.setParams(p);
                exercise("engine " + label, master, members,
                         [&](int channel, int controller, int value) { engine.controlChange(controller, value / 127.0f, channel); },
                         [&]() -> const MpeZoneLayout& { return engine.getMpeZones(); });
                if (members == 15)
                {
                    // The other zone's former master is a valid endpoint member.
                    // Negotiate there, then bend an already allocated note.
                    const int endpoint = 17 - master;
                    engine.controlChange(101, 0.0f, endpoint);
                    engine.controlChange(100, 0.0f, endpoint);
                    engine.controlChange(6, 12.0f / 127.0f, endpoint);
                    engine.noteOn(60, 0.8f, endpoint);
                    engine.pitchBend(endpoint, 0.5f);
                    check(engine.getVoiceInfo(0).bend == 6.0f,
                          label + "endpoint remains a member with its negotiated bend range");
                    render(engine, 1);
                    check(std::abs(semitonesOf(engine, 0, 66.0f)) < 1e-3f,
                          label + "endpoint note renders at the member bend pitch");
                }
            }
        }
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
        // ws2 merge, intentional circuit-engine change: the ribbon is smoothed
        // over 5 ms (Smoothing.h), so each position is read after 0.2 s.
        e.controlChange(16, 127.0f / 127.0f); render(e, 9600);
        check(e.getVoiceInfo(0).ribbonRatio == ribbonFrequencyRatio(1.0f), "CC16 127 is full ribbon travel");
        e.controlChange(16, 64.0f / 127.0f); render(e, 9600);
        check(e.getVoiceInfo(0).ribbonRatio == 1.0f, "CC16 64 is the ribbon centre");
        e.controlChange(16, 0.0f); render(e, 9600);
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
    testMtsMessageBoundaries();
    testEngineTuning();
    testArpeggiatorTuningMap();
    testMpe();
    testMpePedalZoneBoundaries();
    testMpeExpressionZoneBoundaries();
    testMpeNrpnIsolation();
    testMpeZeroMemberConfiguration();
    testVoiceModes();
    testRibbonCc();
    std::printf("Tuning / MIDI / voice modes: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
