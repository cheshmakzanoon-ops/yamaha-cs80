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
#include <memory>
#include <type_traits>
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
    // Playable-key bounds must not redefine the tuning reference's map slot.
    {
        tn::Scale scale; tn::KeyboardMapping fullMap;
        check(tn::parseScale(readFile("diatonic7.scl"), scale).empty(), "range reference scale parses");
        check(tn::parseKeyboardMapping(readFile("white-keys.kbm"), fullMap).empty(), "range reference KBM parses");
        for (int reference : {45,69,93}) for (int mappingKind = 0; mappingKind < 3; ++mappingKind)
        {
            auto map = fullMap; map.reference = reference;
            if (mappingKind == 1) map.map[9] = -1; // Preserve existing unmapped-reference policy.
            if (mappingKind == 2) { map.size = 0; map.map.clear(); }
            tn::TuningTable full;
            check(tn::buildTable(scale, map, full).empty(), "full-range reference builds");
            for (auto bounds : {std::pair{60,64}, std::pair{72,84}, std::pair{48,59},
                                std::pair{0,127}, std::pair{60,69}, std::pair{69,100}})
            {
                auto restricted = map; restricted.first = bounds.first; restricted.last = bounds.second;
                tn::TuningTable limited;
                check(tn::buildTable(scale, restricted, limited).empty(), "restricted playable range builds");
                for (int n = 0; n < 128; ++n)
                {
                    const bool playable = n >= bounds.first && n <= bounds.second && full.mapped[n];
                    check(limited.mapped[n] == playable, "range restricts admission without changing map holes");
                    if (playable) check(limited.pitch[n] == full.pitch[n], "range restriction preserves mapped-reference pitch");
                }
            }
        }
    }
    // Official Scala 1.88e oracle: formal octave advances scale degrees.
    {
        tn::Scale scale; tn::KeyboardMapping map;
        check(tn::parseScale("Nonuniform\n5\n100.0\n300.0\n500.0\n800.0\n1200.0\n",scale).empty(),"formal octave oracle scale parses");
        check(tn::parseKeyboardMapping("2\n0\n127\n60\n60\n440.0\n3\n0\n1\n",map).empty(),"formal octave oracle mapping parses");
        tn::TuningTable tuned;
        check(tn::buildTable(scale,map,tuned).empty(),"formal octave oracle table builds");
        const double oracleHz[]{174.614116,220.,261.625565,293.664768,440.,466.163762,587.329536,698.456463,932.327523,1046.502261};
        for(int n=56;n<=65;++n)
            check(std::abs(tuned.pitch[n]-tn::TuningTable::pitchFromHz(oracleHz[n-56]))<1e-4f,"nonuniform formal octave matches official Scala frequencies");
        for(int formal : {0,1,3,5,7,10}) for(int reference : {45,60,69,93})
        {
            map.octaveDegree=formal;map.reference=reference;
            check(tn::buildTable(scale,map,tuned).empty(),"formal octave/reference combination builds");
            const long long step=formal ? formal : 5;
            auto degree=[&](int note) {const long long d=note-60;const long long rep=d>=0?d/2:-((-d+1)/2);return rep*step+(d-rep*2);};
            const auto anchor=tn::degreeCents(scale,degree(reference));
            for(int n=0;n<128;++n) {
                const auto hz=440.*std::exp2((tn::degreeCents(scale,degree(n))-anchor)/1200.);
                check(std::abs(tuned.pitch[n]-tn::TuningTable::pitchFromHz(hz))<1e-4f,"formal octave maps degree before pitch including negative repetitions");
                if(step%5==0) {
                    auto oldCents=[&](int note) {const long long d=note-60;const long long rep=d>=0?d/2:-((-d+1)/2);return rep*tn::degreeCents(scale,step)+tn::degreeCents(scale,d-rep*2);};
                    const auto oldHz=440.*std::exp2((oldCents(n)-oldCents(reference))/1200.);
                    check(tuned.pitch[n]==tn::TuningTable::pitchFromHz(oldHz),"whole-scale formal periods retain exact previous arithmetic");
                }
            }
        }
    }
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

void testMtsChannelRecipients()
{
    using Bytes = std::vector<std::uint8_t>;
    for (int sub : {8, 9}) for (bool realtime : {false, true})
    for (bool source : {false, true}) for (int target = 1; target <= 16; ++target)
    for (bool addressed : {false, true})
    {
        const unsigned mask = 1u << (target - 1);
        Bytes message {0xF0, static_cast<std::uint8_t>(realtime ? 0x7F : 0x7E), 0x7F, 8,
                       static_cast<std::uint8_t>(sub), static_cast<std::uint8_t>(mask >> 14),
                       static_cast<std::uint8_t>((mask >> 7) & 127), static_cast<std::uint8_t>(mask & 127)};
        for (int pc = 0; pc < 12; ++pc)
            if (sub == 8) message.push_back(89);
            else { message.push_back(80); message.push_back(0); }
        message.push_back(0xF7);
        const int channel = addressed ? target : (target == 16 ? 1 : target + 1);
        auto p = quiet(); p[P_tuningMode] = 2; p[P_sourcePerformance] = source ? 1.f : 0.f;
        Engine actual, reference;
        for (auto* e : {&actual, &reference})
        { e->prepare(48000); e->setParams(p); e->noteOn(64, .8f, channel); render(*e, 128); }
        check(actual.handleSysEx(message.data(), static_cast<int>(message.size())),
              "channel-addressed scale message is accepted");
        tn::TuningTable shifted;
        for (auto& pitch : shifted.pitch) pitch += .25f;
        if (addressed && realtime) reference.setMtsTuning(shifted);
        check(render(actual, 256) == render(reference, 256),
              "scale message retunes held audio only for addressed channels in real time");
        for (auto* e : {&actual, &reference}) e->allNotesOff(false);
        if (addressed) reference.setMtsTuning(shifted);
        for (auto* e : {&actual, &reference}) e->noteOn(67, .7f, channel);
        check(render(actual, 256) == render(reference, 256),
              "scale message changes future-note tuning only on addressed channels");
    }
}

void testMtsChannelState()
{
    using Bytes = std::vector<std::uint8_t>;
    const auto scale = [](unsigned mask, bool realtime) {
        Bytes bytes {0xF0, static_cast<std::uint8_t>(realtime ? 0x7F : 0x7E), 0x7F, 8, 8,
                     static_cast<std::uint8_t>(mask >> 14), static_cast<std::uint8_t>((mask >> 7) & 127),
                     static_cast<std::uint8_t>(mask & 127)};
        bytes.insert(bytes.end(), 12, 89); bytes.push_back(0xF7); return bytes;
    };
    for (bool source : {false, true})
    {
        Engine e; auto p = quiet(); p[P_tuningMode] = 2; p[P_sourcePerformance] = source ? 1.f : 0.f;
        e.prepare(48000); e.setParams(p);
        for (int ch : {1, 2, 16}) e.noteOn(64, .8f, ch);
        render(e, 256);
        const auto staged = scale(1, false), live = scale(1u << 15, true);
        check(e.handleSysEx(staged.data(), int(staged.size())) && e.handleSysEx(live.data(), int(live.size())),
              "independent staged and live channel tables accepted");
        render(e, 256);
        for (int voice = 0; voice < 3; ++voice)
            check(std::abs(semitonesOf(e, voice, voice == 2 ? 64.25f : 64.f)) < 1e-3f,
                  "unrelated real-time channel update cannot activate staged held-note tuning");
        const Bytes note {0xF0, 0x7F, 0x7F, 8, 2, 0, 1, 64, 64, 64, 0, 0xF7};
        check(e.handleSysEx(note.data(), int(note.size())), "global single-note update accepted after channel scales");
        render(e, 256);
        for (int voice = 0; voice < 3; ++voice)
            check(std::abs(semitonesOf(e, voice, 64.5f)) < 1e-3f,
                  "global single-note update reaches each addressed sounding key");
        check(e.getMtsTuning(1).pitch[65] == 65.25f && e.getMtsTuning(16).pitch[65] == 65.25f
              && e.getMtsTuning(2).pitch[65] == 65.f,
              "global single-note edits preserve other pitches in independent channel tables");
        e.reset(); e.prepare(96000);
        check(e.getMtsTuning(1).pitch[65] == 65.25f && e.getMtsTuning(2).pitch[65] == 65.f,
              "reset and prepare retain per-channel tuning as before for global tuning");
        tn::TuningTable replacement;
        for (auto& pitch : replacement.pitch) pitch += .125f;
        e.setMtsTuning(replacement);
        for (int ch = 1; ch <= 16; ++ch)
            check(e.getMtsTuning(ch).pitch == replacement.pitch, "MTS-ESP global table replaces every channel");
        const auto mixed = scale((1u << 6) | (1u << 7) | (1u << 14), true);
        e.handleSysEx(mixed.data(), int(mixed.size()));
        for (int ch = 1; ch <= 16; ++ch)
            check(e.getMtsTuning(ch).pitch[60] == ((ch == 7 || ch == 8 || ch == 15) ? 60.25f : 60.125f),
                  "mixed mask independently selects channels across mask byte boundaries");
        auto malformed = mixed; malformed[5] |= 4;
        const auto before = e.getMtsTuning(7);
        check(!e.handleSysEx(malformed.data(), int(malformed.size())) && e.getMtsTuning(7).pitch == before.pitch,
              "reserved mask bits reject atomically before updating any channel");
    }
}

void testMtsChannelMappingRoutes()
{
    std::vector<std::uint8_t> scale {0xF0, 0x7F, 0x7F, 8, 8, 0, 0, 2};
    scale.insert(scale.end(), 12, 89); scale.push_back(0xF7); // only channel 2
    for (bool source : {false, true}) for (bool scan : {false, true})
    for (bool arp : {false, true}) for (int channel : {1, 2})
    {
        auto p = quiet(); p[P_tuningMode] = 2; p[P_sourcePerformance] = source ? 1.f : 0.f;
        p[P_kasScanTiming] = scan ? 1.f : 0.f; p[P_arpOn] = arp ? 1.f : 0.f; p[P_arpRate] = 16;
        p[P_arpOctaves] = 1;
        Engine e; e.prepare(48000); e.setParams(p);
        tn::TuningTable mapping; mapping.mapped[61] = false; e.setMtsTuning(mapping);
        e.handleSysEx(scale.data(), int(scale.size()));
        e.noteOn(61, .8f, channel); render(e, 1024);
        check((e.activeVoices() > 0) == (channel == 2),
              "direct, scanner and ARP generated notes use source-channel mapped flags");
        for (int v = 0; v < 8; ++v)
            if (e.getVoiceInfo(v).active)
                check(std::abs(semitonesOf(e, v, 61.25f)) < 1e-3f,
                      "admitted channel-specific note uses its own table pitch");
    }
    for (bool scan : {false, true})
    {
        auto p = quiet(); p[P_sourcePerformance] = 1; p[P_tuningMode] = 2;
        p[P_kasScanTiming] = scan ? 1.f : 0.f;
        Engine e; e.prepare(48000); e.setParams(p);
        for (int note = 60; note < 68; ++note) e.noteOn(note, .8f, 1);
        render(e, 1024); e.noteOn(40, .8f, 1); e.noteOn(41, .8f, 2); render(e, 1024);
        tn::TuningTable mapping; mapping.mapped[40] = mapping.mapped[41] = false; e.setMtsTuning(mapping);
        e.handleSysEx(scale.data(), int(scale.size()));
        e.noteOff(60, 1); render(e, 1024);
        bool promoted = false, excluded = false;
        for (int v = 0; v < 8; ++v)
        {
            const auto info = e.getVoiceInfo(v);
            promoted |= info.active && info.held && info.note == 41 && info.channel == 2;
            excluded |= info.active && info.held && info.note == 40;
        }
        check(promoted && !excluded, "waiting-key promotion uses each waiter's current channel mapping");
    }
}

void testMtsChannelLifecycle()
{
    const auto scale = [](int sub, bool realtime) {
        std::vector<std::uint8_t> bytes {0xF0, static_cast<std::uint8_t>(realtime ? 0x7F : 0x7E),
            0x7F, 8, static_cast<std::uint8_t>(sub), 0, 0, 2}; // channel 2 only
        for (int pc = 0; pc < 12; ++pc)
            if (sub == 8) bytes.push_back(89);
            else bytes.insert(bytes.end(), {80, 0});
        bytes.push_back(0xF7); return bytes;
    };
    for (int sub : {8, 9}) for (bool realtime : {false, true})
    for (bool source : {false, true}) for (int lifecycle : {0, 1, 2})
    {
        auto p = quiet(); p[P_tuningMode] = 2; p[P_keyRelease] = 1;
        p[P_sourcePerformance] = source ? 1.f : 0.f;
        Engine actual, reference;
        for (auto* e : {&actual, &reference})
        {
            e->prepare(48000); e->setParams(p);
            for (int ch : {1, 2}) e->noteOn(64, .8f, ch);
            render(*e, 256);
            for (int ch : {1, 2})
            {
                if (lifecycle == 1) e->controlChange(64, 1.f, ch);
                if (lifecycle != 0) e->noteOff(64, ch);
            }
        }
        const auto message = scale(sub, realtime);
        check(actual.handleSysEx(message.data(), int(message.size())), "lifecycle channel scale accepted");
        const auto audio = render(actual, 512);
        check(actual.getVoiceInfo(0).channel == 1 && actual.getVoiceInfo(1).channel == 2,
              "lifecycle pair retains distinct source channels");
        if (!realtime)
            check(audio == render(reference, 512), "NRT channel scale preserves mixed-channel lifecycle audio");
        for (int v = 0; v < 2; ++v)
        {
            const auto info = actual.getVoiceInfo(v);
            const bool retuned = realtime && info.channel == 2 && !(source && lifecycle == 2);
            check(info.active && info.held == (lifecycle == 0) && info.pedalHeld == (lifecycle == 1),
                  "channel tuning preserves held, pedal-held and released voice ownership");
            check(std::abs(semitonesOf(actual, v, retuned ? 64.25f : 64.f)) < 1e-3f,
                  "channel lifecycle retunes only recipient and respects circuit release latch");
        }
        for (int ch : {1, 2}) actual.noteOn(65, .8f, ch);
        render(actual, 256);
        int fresh = 0; unsigned freshChannels = 0;
        for (int v = 0; v < 8; ++v)
        {
            const auto info = actual.getVoiceInfo(v);
            if (info.active && info.held && info.note == 65)
            {
                ++fresh; freshChannels |= 1u << info.channel;
                check(std::abs(semitonesOf(actual, v, info.channel == 2 ? 65.25f : 65.f)) < 1e-3f,
                      "future notes retain per-channel tuning after each lifecycle update");
            }
        }
        check(fresh == 2 && freshChannels == 6, "one future note on each channel sounds");
    }
    for (int sub : {8, 9}) for (bool realtime : {false, true})
    {
        auto p = quiet(); p[P_tuningMode] = 2; p[P_sourcePerformance] = p[P_kasScanTiming] = 1;
        Engine e; e.prepare(48000); e.setParams(p);
        render(e, 1); // advance away from initial dispatch boundary
        for (int ch : {1, 2}) e.noteOn(64, .8f, ch);
        check(e.activeVoices() == 0, "scanner lifecycle fixture has pending onsets, not sounding voices");
        const auto message = scale(sub, realtime);
        check(e.handleSysEx(message.data(), int(message.size())), "tuning update between enqueue and dispatch accepted");
        render(e, 1024);
        check(e.activeVoices() == 2, "both queued channels dispatch after tuning change");
        check(e.getVoiceInfo(0).channel == 1 && e.getVoiceInfo(1).channel == 2,
              "scanner dispatch preserves both source channels");
        for (int v = 0; v < 2; ++v)
            check(std::abs(semitonesOf(e, v, e.getVoiceInfo(v).channel == 2 ? 64.25f : 64.f)) < 1e-3f,
                  "queued onset uses dispatch-time channel tuning for RT and NRT messages");
    }
}

void testMtsEmptyChannelMask()
{
    using Bytes = std::vector<std::uint8_t>;
    for (int sub : {8,9}) for (bool realtime : {false,true})
    {
        Bytes message {0xF0, static_cast<std::uint8_t>(realtime ? 0x7F : 0x7E),
                       0x7F,8,static_cast<std::uint8_t>(sub),0,0,0};
        for (int pc = 0; pc < 12; ++pc)
            if (sub == 8) message.push_back(89); // +25 cents
            else { message.push_back(80); message.push_back(0); }
        message.push_back(0xF7);
        tn::TuningTable before; before.pitch[64] = 64.125f; before.mapped[61] = false;
        auto after = before;
        check(!tn::applyMtsSysEx(message.data(), static_cast<int>(message.size()), after),
              "empty MTS channel mask reports no applied change");
        check(after.pitch == before.pitch && after.mapped == before.mapped,
              "empty MTS channel mask preserves entire table");
        for (bool source : {false,true})
        {
            auto p = quiet(); p[P_sourcePerformance] = source ? 1.0f : 0.0f; p[P_tuningMode] = 2.0f;
            Engine actual, reference;
            for (auto* e : {&actual,&reference})
            { e->prepare(48000); e->setParams(p); e->setMtsTuning(before); e->noteOn(64,.8f); render(*e,256); }
            check(!actual.handleSysEx(message.data(), static_cast<int>(message.size())),
                  "engine treats empty recipient mask as no-op");
            check(render(actual,1024) == render(reference,1024),
                  "empty mask preserves held-note audio");
            for (auto* e : {&actual,&reference}) e->noteOn(65,.7f);
            check(render(actual,1024) == render(reference,1024),
                  "empty mask cannot silently change future-note tuning");
        }
        message[5] = 3; message[6] = message[7] = 127;
        check(tn::applyMtsSysEx(message.data(), static_cast<int>(message.size()), after),
              "nonempty all-channel mask remains accepted");
        check(after.pitch[64] == 64.25f && after.mapped[61],
              "all-channel scale update still applies complete table");
    }
}

void testMtsNonRealtimeAdmission()
{
    using Bytes = std::vector<std::uint8_t>;
    for (const int sub : { 7, 8, 9 })
        for (const bool realtime : { false, true })
            for (const bool source : { false, true })
              for (const int lifecycle : { 0, 1, 2 })
            {
                auto p = quiet(); p[P_tuningMode] = 2; p[P_keyRelease] = 1.0f; p[P_sourcePerformance] = source ? 1.0f : 0.0f;
                Engine actual, reference;
                for (auto* e : { &actual, &reference })
                {
                    e->prepare(48000.0); e->setParams(p); e->noteOn(64, 0.8f); render(*e, 256);
                    if (lifecycle == 1) e->controlChange(64, 1.0f);
                    if (lifecycle != 0) e->noteOff(64);
                }
                Bytes message { 0xF0, static_cast<std::uint8_t>(realtime ? 0x7F : 0x7E), 0x7F, 8,
                                static_cast<std::uint8_t>(sub) };
                if (sub == 7) message.insert(message.end(), { 0, 0, 2, 64, 64, 32, 0, 65, 65, 32, 0 });
                else
                {
                    message.insert(message.end(), { 3, 127, 127 });
                    for (int i = 0; i < 12; ++i)
                        if (sub == 8) message.push_back(89); // +25 cents
                        else message.insert(message.end(), { 80, 0 }); // +0.25 semitone
                }
                message.push_back(0xF7);
                check(actual.handleSysEx(message.data(), static_cast<int>(message.size())),
                      "extended MTS message accepted");
                check(actual.getMtsTuning().pitch[64] == 64.25f && actual.getMtsTuning().pitch[65] == 65.25f,
                      "both realtime policies store future-note tuning");
                if (realtime) reference.setMtsTuning(actual.getMtsTuning());
                check(render(actual, 512) == render(reference, 512),
                      realtime ? "real-time message matches existing live-table continuation" : "non-real-time message preserves sounding continuation");
                // The source model's released key-voltage node is already
                // latched. Preserve that existing behavior, as the explicit
                // live-table reference above does; this fix does not bypass it.
                const float expectedPitch = realtime && !(source && lifecycle == 2) ? 64.25f : 64.0f;
                check(std::abs(semitonesOf(actual, 0, expectedPitch)) < 1e-3f,
                      "sounding pitch follows admission and existing release latch");
                check(actual.getVoiceInfo(0).active, "old held/pedal/release voice still sounds");
                actual.noteOn(65, 0.8f); render(actual, 64);
                check(std::abs(semitonesOf(actual, actual.getVoiceInfo(0).note == 65 ? 0 : 1, 65.25f)) < 1e-3f,
                      "new note uses stored tuning after either message policy");
            }
    // An unrelated real-time message cannot activate previously staged tuning.
    // Conversely an explicit same-value entry must still retune its held key.
    for (const bool source : { false, true }) for (const int sub : { 2, 7 })
    {
        auto p = quiet(); p[P_tuningMode] = 2; p[P_sourcePerformance] = source ? 1.0f : 0.0f;
        Engine actual, reference;
        for (auto* e : { &actual, &reference })
        { e->prepare(48000); e->setParams(p); e->noteOn(64, .8f); render(*e, 256); }
        const Bytes stage { 0xF0,0x7E,0x7F,8,7,0,0,1,64,64,32,0,0xF7 };
        check(actual.handleSysEx(stage.data(), static_cast<int>(stage.size())), "stage held key tuning");
        Bytes update { 0xF0,0x7F,0x7F,8,static_cast<std::uint8_t>(sub),0 };
        if (sub == 7) update.push_back(0);
        update.insert(update.end(), { 2,65,65,32,0,64,127,127,127,0xF7 });
        check(actual.handleSysEx(update.data(), static_cast<int>(update.size())), "unrelated update plus no-change marker");
        check(render(actual,256) == render(reference,256), "unrelated real-time entry does not activate staged held tuning");
        const int header = sub == 2 ? 7 : 8;
        update[header] = 64; update[header+1] = 64;
        check(actual.handleSysEx(update.data(), static_cast<int>(update.size())), "explicit same-value entry accepted");
        reference.setMtsTuning(actual.getMtsTuning());
        check(render(actual,256) == render(reference,256), "explicit same-value entry activates staged held tuning");
    }

}

void testUnmappedSustainAdmission()
{
    for (bool source : {false, true}) for (bool scan : {false, true})
        for (int mode : {1, 2})
        {
            auto p = quiet();
            p[P_sourcePerformance] = source ? 1.0f : 0.0f;
            p[P_kasScanTiming] = scan ? 1.0f : 0.0f;
            p[P_tuningMode] = static_cast<float>(mode);
            p[P_sustainMode] = p[P_sustainAssign] = 1.0f;
            p[P_footswitchPlugged] = p[P_footswitchPressed] = 1.0f;
            for (int rank = 0; rank < 2; ++rank)
                p[chParam(rank, CP_vR)] = p[chParam(rank, CP_fR)] = 2.0f;
            tn::TuningTable mapping; mapping.mapped[61] = false;
            Engine actual, reference;
            for (auto* e : {&actual, &reference})
            {
                e->prepare(48000); e->setParams(p);
                if (mode == 1) e->setScalaTuning(mapping); else e->setMtsTuning(mapping);
                e->noteOn(60, 0.8f); render(*e, 1024);
                e->noteOff(60); render(*e, 128);
            }
            check(actual.getVoiceInfo(0).active && !actual.getVoiceInfo(0).held,
                  "unmapped sustain fixture has an active release tail");
            actual.noteOn(61, 0.8f);
            check(render(actual, 512) == render(reference, 512),
                  "unmapped onset cannot cancel Sustain-II tail");
            check(actual.getVoiceInfo(0).active, "unmapped onset retains release voice");
            actual.noteOn(64, 0.8f); render(actual, 512);
            bool oldActive = false, newHeld = false;
            for (int v = 0; v < 8; ++v)
            { const auto info = actual.getVoiceInfo(v); oldActive |= info.active && info.note == 60;
              newHeld |= info.active && info.held && info.note == 64; }
            check(!oldActive && newHeld, "mapped onset still terminates Sustain-II tail");
        }
}

void testWaitingTuningAdmission()
{
    const auto held = [](const Engine& e, int note) {
        for (int i = 0; i < 8; ++i)
        { const auto v = e.getVoiceInfo(i); if (v.active && v.held && v.note == note) return true; }
        return false;
    };
    for (bool scan : {false, true}) for (int mode : {1, 2})
        for (bool cancel : {false, true})
        {
            auto p = quiet(); p[P_sourcePerformance] = 1.0f;
            p[P_kasScanTiming] = scan ? 1.0f : 0.0f;
            p[P_tuningMode] = static_cast<float>(mode);
            Engine e; e.prepare(48000); e.setParams(p);
            tn::TuningTable mapping;
            const auto load = [&]() { if (mode == 1) e.setScalaTuning(mapping); else e.setMtsTuning(mapping); };
            load();
            for (int note = 60; note < 68; ++note) e.noteOn(note, .8f);
            render(e, 1024);
            e.noteOn(40, .8f); e.noteOn(41, .8f); render(e, 1024);
            check(!held(e, 40) && !held(e, 41), "two keys wait behind eight held circuits");
            mapping.mapped[40] = false; load();
            e.noteOff(60); render(e, 1024);
            check(!held(e, 40), "excluded waiting key cannot sound on promotion");
            check(held(e, 41), "mapped waiter bypasses lower excluded key");
            if (cancel) { e.noteOff(40); render(e, 1024); }
            mapping.mapped[40] = true; load();
            e.noteOff(61); render(e, 1024);
            check(held(e, 40) == !cancel, "remapped waiter is retained unless its key was released");
        }
}

void testArpLiveSequenceEdits()
{
    const auto heldNote = [](const Engine& e) {
        for (int v = 0; v < 8; ++v)
        { const auto info = e.getVoiceInfo(v); if (info.active && info.held) return info.note; }
        return -1;
    };
    for (bool source : {false, true}) for (int edit : {0, 1, 2, 3, 4})
    {
        auto p = quiet(); p[P_sourcePerformance] = source ? 1.0f : 0.0f;
        p[P_arpOn] = 1.0f; p[P_arpRate] = 16.0f; p[P_arpOctaves] = edit == 4 ? 2.0f : 1.0f;
        Engine e; e.prepare(48000); e.setParams(p);
        // Played order differs from sorted order, without duplicate pitches.
        e.noteOn(67, .8f, 2); e.noteOn(60, .7f, 3); e.noteOn(64, .6f, 4);
        render(e, 1); check(heldNote(e) == 60, "ascending fixture starts at lowest key");
        std::vector<int> expected;
        if (edit == 0) { p[P_arpMode] = 1.0f; expected = {67,64,60,67,64,60}; }
        else if (edit == 1) { p[P_arpMode] = 4.0f; expected = {67,60,64,67,60,64}; }
        else if (edit == 2) { p[P_arpOctaves] = 2.0f; expected = {60,64,67,72,76,79}; }
        else if (edit == 3) { p[P_arpMode] = 2.0f; expected = {60,64,67,64,60,64}; }
        else { p[P_arpOctaves] = 1.0f; expected = {60,64,67,60,64,67}; }
        e.setParams(p); render(e, 64);
        check(heldNote(e) == 60, "live sequence edit does not interrupt current gate");
        for (int note : expected)
        {
            render(e, 3004);
            check(heldNote(e) == note, "held chord follows edited ARP sequence at scheduled steps");
        }
    }
    for (bool source : {false, true})
    {
        auto p = quiet(); p[P_sourcePerformance] = source ? 1.0f : 0.0f;
        p[P_arpOn] = 1.0f; p[P_arpRate] = 16.0f;
        Engine a, b;
        for (auto* e : {&a,&b})
        { e->prepare(48000); e->setParams(p); e->noteOn(60,.8f); e->noteOn(64,.7f); render(*e,3500); }
        a.setParams(p);
        check(render(a,9000) == render(b,9000), "unchanged parameters do not restart ARP sequence");
        for (auto* e : {&a,&b})
        { e->reset(); e->setParams(p); e->noteOn(60,.8f); e->noteOn(64,.7f); render(*e,1000); }
        p[P_arpMode] = 1.0f; a.setParams(p);
        render(a,128); render(b,128);
        check(heldNote(a) == 60 && heldNote(b) == 60, "mid-gate edit does not retrigger either voice");
        render(a,1876); render(b,1876);
        check(heldNote(a) == 64 && heldNote(b) == 64, "edited sequence uses the original next-step deadline");
    }
}

void testArpResetParameterSync()
{
    for (bool source : {false, true}) for (bool scan : {false, true})
        for (int mode : {0,1,2,3,4})
        {
            auto p = quiet(); p[P_sourcePerformance] = source ? 1.0f : 0.0f;
            p[P_kasScanTiming] = scan ? 1.0f : 0.0f;
            p[P_arpOn] = 1.0f; p[P_arpRate] = 16.0f; p[P_arpMode] = static_cast<float>(mode);
            Engine actual, reference;
            for (auto* e : {&actual,&reference})
            {
                e->prepare(48000); e->setParams(p); e->noteOn(55,.8f); render(*e,64);
                e->reset();
                check(e->activeVoices() == 0, "reset still clears previous ARP voices");
                e->noteOn(60,.8f,2); e->noteOn(64,.7f,3); render(*e,1000);
            }
            actual.setParams(p);
            check(render(actual,9000) == render(reference,9000),
                  "post-reset unchanged parameter sync preserves new ARP chord and clock");
            p[P_arpOn] = 0.0f; actual.setParams(p); render(actual,64);
            bool anyHeld = false;
            for (int v = 0; v < 8; ++v) anyHeld |= actual.getVoiceInfo(v).held;
            check(!anyHeld, "real ARP disable still stops post-reset pattern");
        }
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
            // Each routing case starts from explicitly neutral expression.
            // All Notes Off preserves member controllers, including the raw
            // pressure/CC74 now inherited by a later accepted onset.
            send(channel, 0.0f);
            engine.noteOn(60, 0.8f, channel);
            int index = -1;
            for (int i = 0; i < 8; ++i)
            {
                const auto v = engine.getVoiceInfo(i);
                if (v.active && v.held && v.note == 60 && v.channel == channel) index = i;
            }
            check(index >= 0, "expression fixture allocates its requested channel");
            check(index >= 0 && engine.getVoiceInfo(index).pressure == 0.0f,
                  "expression routing fixture starts from neutral member pressure");
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
                // Channel 14 is currently a lower member despite its captured
                // upper routing. Ordinary own-channel expression now retains
                // released targets; held recipients still take the new value.
                f.send(14, 0.5f); f.verify(index, tail ? 0.25f : 0.5f, 3000, "retained upper own channel");
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

// The routing preview is checked through the existing public router so this
// same test file can be compiled against the pre-fix Engine and header. Its
// oracle is the real dispatcher in an independently copied Engine, not a
// second implementation of the allocator or scanner.
static_assert(std::is_copy_constructible<Engine>::value, "dispatch oracle needs an Engine snapshot");

ParamValues scanRoutingParams()
{
    auto p = quiet();
    p[P_sourcePerformance] = p[P_voiceMode] = p[P_kasScanTiming] = 1.0f;
    p[P_sustainMode] = p[P_masterSustain] = 0.0f;
    return p;
}

int routingHeld(const Engine& e, int note = -1, int channel = -1)
{
    int held = 0;
    for (int i = 0; i < 8; ++i)
    {
        const auto v = e.getVoiceInfo(i);
        held += v.held && (note < 0 || v.note == note) && (channel < 0 || v.channel == channel);
    }
    return held;
}

bool sameRoutingVoices(const Engine& a, const Engine& b)
{
    for (int i = 0; i < 8; ++i)
    {
        const auto x = a.getVoiceInfo(i), y = b.getVoiceInfo(i);
        if (x.active != y.active || x.note != y.note || x.channel != y.channel
            || x.velocity != y.velocity || x.pressure != y.pressure || x.bend != y.bend
            || x.ribbonRatio != y.ribbonRatio || x.held != y.held || x.pedalHeld != y.pedalHeld
            || x.touchA != y.touchA || x.touchB != y.touchB || x.touchC != y.touchC)
            return false;
    }
    return a.hasFreeCircuit() == b.hasFreeCircuit();
}

bool routesToFirst(Engine& e, Engine& empty)
{
    return &SixteenVoiceRouter::noteOnTarget(e, empty) == &e;
}

bool routingMatchesDispatch(Engine& e, Engine& empty, int* projectedFull = nullptr)
{
    const bool physicallyFree = e.hasFreeCircuit();
    auto dispatched = std::make_unique<Engine>(e);
    // Every fixture is 48 kHz. Forty-eight host frames pass one complete
    // nominal scan regardless of the current phase or oversampling factor.
    render(*dispatched, 48);
    const bool expected = physicallyFree && dispatched->hasFreeCircuit();
    if (projectedFull != nullptr && physicallyFree && !dispatched->hasFreeCircuit())
        ++*projectedFull;
    return routesToFirst(e, empty) == expected;
}

void routingFill(Engine& e, int count, int base = 40, int channel = 1)
{
    for (int n = 0; n < count; ++n) e.noteOn(base + n, 0.75f, channel);
    render(e, 48);
}

void testSixteenVoiceScanChords(const Engine& prototype)
{
    const std::array<int, 16> ascending {{40, 41, 42, 43, 44, 45, 46, 47,
                                         48, 49, 50, 51, 52, 53, 54, 55}};
    const std::array<int, 16> scrambled {{76, 43, 65, 38, 70, 54, 61, 47,
                                         59, 35, 80, 51, 68, 41, 73, 56}};
    for (const bool keyboard : {false, true})
        for (const auto& notes : {ascending, scrambled})
        {
            auto a = std::make_unique<Engine>(prototype), b = std::make_unique<Engine>(prototype);
            auto silent = std::make_unique<Engine>(prototype);
            bool destinations = true;
            for (int k = 0; k < 16; ++k)
            {
                auto& target = SixteenVoiceRouter::noteOnTarget(*a, *b);
                destinations &= &target == (k < 8 ? a.get() : b.get());
                if (keyboard) target.noteOnFromKeyboard(notes[static_cast<std::size_t>(k)], 0.8f);
                else target.noteOn(notes[static_cast<std::size_t>(k)], 0.8f);
            }
            const std::string label = std::string("scan chord ") + (keyboard ? "keyboard " : "host ")
                + (notes == ascending ? "ascending: " : "scrambled: ");
            check(destinations, label + "the first eight arrivals choose A and the next eight B");
            check(a->hasFreeCircuit() && b->hasFreeCircuit() && routingHeld(*a) == 0 && routingHeld(*b) == 0,
                  label + "physical capacity and live gates remain untouched before dispatch");
            const auto silence = render(*silent, 47);
            check(render(*a, 47) == silence && render(*b, 47) == silence
                      && routingHeld(*a) == 0 && routingHeld(*b) == 0,
                  label + "no allocator onset or audio change before the one-ms boundary");
            render(*a, 1); render(*b, 1);
            check(routingHeld(*a) == 8 && routingHeld(*b) == 8,
                  label + "sixteen gates open on the 48th host frame");
            bool sorted = true;
            for (int side = 0; side < 2; ++side)
            {
                std::array<int, 8> expected;
                std::copy_n(notes.begin() + side * 8, 8, expected.begin());
                std::sort(expected.begin(), expected.end());
                const auto& engine = side == 0 ? *a : *b;
                for (int v = 0; v < 8; ++v)
                    sorted &= engine.getVoiceInfo(v).note == expected[static_cast<std::size_t>(v)]
                           && engine.getVoiceInfo(v).velocity == 0.8f;
            }
            check(sorted, label + "each instrument retains its own pitch-sorted scan and strike velocity");
            check(&SixteenVoiceRouter::noteOnTarget(*a, *b) == a.get(),
                  label + "both physically full instruments retain first-instrument fallback");
        }

    auto a = std::make_unique<Engine>(prototype), b = std::make_unique<Engine>(prototype);
    for (int n = 0; n < 8; ++n) { a->noteOn(40 + n, 0.7f); b->noteOn(60 + n, 0.7f); }
    check(&SixteenVoiceRouter::noteOnTarget(*a, *b) == a.get(),
          "both projected-full instruments retain first-instrument fallback before dispatch");
    render(*a, 48); render(*b, 48);
    a->noteOff(47);
    b->allNotesOff(false);
    check(!routesToFirst(*a, *b) && routingMatchesDispatch(*a, *b),
          "a queued release cannot make a physically full instrument claim a new strike");
    SixteenVoiceRouter::noteOnTarget(*a, *b).noteOn(30, 0.9f);
    render(*a, 48); render(*b, 48);
    check(routingHeld(*a) == 7 && routingHeld(*b, 30) == 1 && b->getVoiceInfo(0).velocity == 0.9f,
          "physical-free upper bound sends the lower key to a real circuit with its original touch");
}

void testScanRoutingLifecycles(const Engine& prototype, Engine& empty)
{
    auto e = std::make_unique<Engine>(prototype);
    routingFill(*e, 6, 20);
    e->noteOnFromKeyboard(60, 0.6f, 2);
    e->noteOn(60, 0.9f, 2);
    e->noteOffFromKeyboard(60, 2);
    check(!routesToFirst(*e, empty) && routingMatchesDispatch(*e, empty),
          "queued UI release cannot release the newest host-owned duplicate");
    e->noteOff(60, 2);
    check(routesToFirst(*e, empty) && routingMatchesDispatch(*e, empty),
          "queued host release frees only the newest matching duplicate");
    render(*e, 48);
    check(routingHeld(*e, 60, 2) == 1 && routingHeld(*e) == 7,
          "older keyboard duplicate survives the host release");
    e->noteOffFromKeyboard(60, 2); render(*e, 48);
    check(routingHeld(*e, 60, 2) == 0 && routingHeld(*e) == 6,
          "the remaining keyboard-owned duplicate releases normally");

    *e = prototype; routingFill(*e, 6, 20);
    e->noteOn(60, 0.6f, 2); e->noteOn(60, 0.8f, 3); e->noteOff(60, 4);
    check(!routesToFirst(*e, empty) && routingMatchesDispatch(*e, empty),
          "unmatched-channel note-off does not create projected capacity");
    e->noteOff(60, 2);
    check(routesToFirst(*e, empty) && routingMatchesDispatch(*e, empty),
          "equal pitches on distinct channels retain separate projected ownership");

    *e = prototype; routingFill(*e, 7, 50);
    e->noteOn(20, 0.8f); e->noteOn(21, 0.8f); e->noteOn(22, 0.8f);
    e->noteOff(21); e->noteOff(56);
    check(!routesToFirst(*e, empty) && routingMatchesDispatch(*e, empty),
          "queued waiter cancellation and lowest-key promotion refill the released circuit");
    render(*e, 48);
    bool promoted = false;
    for (int v = 0; v < 8; ++v)
    {
        const auto info = e->getVoiceInfo(v);
        promoted |= info.held && info.note == 22 && info.velocity == 0.0f;
    }
    check(promoted && routingHeld(*e, 21) == 0 && routingHeld(*e) == 8,
          "cancelled waiter stays absent and surviving waiter has zero initial touch");

    *e = prototype; routingFill(*e, 8);
    e->noteOn(40, 0.8f); render(*e, 48);
    e->noteOff(40); render(*e, 48);
    check(routingHeld(*e, 40) == 1 && !routesToFirst(*e, empty),
          "same-note waiting key is cancelled before its existing held voice");
    e->noteOff(40); render(*e, 48);
    check(routingHeld(*e, 40) == 0 && routesToFirst(*e, empty),
          "a second release frees the existing voice after waiter cancellation");

    // A release group created under unison remains an onset's ownership even
    // after switching to sixteen-voice mode before its deferred release.
    *e = prototype;
    auto p = e->getParams(); p[P_voiceMode] = 2.0f; e->setParams(p);
    routingFill(*e, 4, 60);
    p[P_voiceMode] = 1.0f; e->setParams(p);
    e->noteOff(63); render(*e, 48);
    check(routingHeld(*e) == 6, "mode change preserves both circuits of the original unison release group");
    e->noteOn(70, 0.7f); e->noteOn(71, 0.7f);
    check(!routesToFirst(*e, empty) && routingMatchesDispatch(*e, empty),
          "projection resumes from capacity released by an existing unison group");

    // Keep the old group's release queued while querying: releasing both
    // circuits leaves seven occupied after the three new onsets, whereas
    // releasing only one would incorrectly project all eight as occupied.
    *e = prototype;
    p = e->getParams(); p[P_voiceMode] = 2.0f; e->setParams(p);
    routingFill(*e, 3, 60);
    check(routingHeld(*e) == 6 && routingHeld(*e, 62) == 2,
          "projected release-group fixture starts with three x2-unison onsets");
    p[P_voiceMode] = 1.0f; e->setParams(p);
    e->noteOff(62);
    for (int note : {70, 71, 72}) e->noteOn(note, 0.7f);
    check(routingMatchesDispatch(*e, empty) && routesToFirst(*e, empty),
          "queued old-unison release frees its entire group in the routing projection");
    render(*e, 48);
    check(routingHeld(*e) == 7 && routingHeld(*e, 62) == 0
              && routingHeld(*e, 70) == 1 && routingHeld(*e, 71) == 1 && routingHeld(*e, 72) == 1,
          "real dispatch confirms seven occupied circuits after the queued group release and three onsets");
}

void testScanRoutingQueueBounds(const Engine& prototype, Engine& empty)
{
    for (const int events : {63, 64, 65, 129, 130})
    {
        auto e = std::make_unique<Engine>(prototype);
        for (int i = 0; i < events; ++i)
        {
            if (i % 2 == 0) e->noteOn(60, 0.8f);
            else e->noteOff(60);
        }
        const std::string label = "scan cancellation queue with " + std::to_string(events) + " events: ";
        check(routingMatchesDispatch(*e, empty) && routesToFirst(*e, empty),
              label + "no phantom full reservation across capacity/overflow boundaries");
        check(routingHeld(*e) == (events == 65 || events == 129 ? 1 : 0),
              label + "only an overflow onset can dispatch before rendering");
        render(*e, 48);
        check(routingHeld(*e) == events % 2, label + "real dispatch preserves every short-note cancellation");
    }
    auto e = std::make_unique<Engine>(prototype);
    render(*e, 17);
    for (int i = 0; i < 64; ++i) e->noteOn(20 + i, 0.7f);
    check(e->hasFreeCircuit() && !routesToFirst(*e, empty) && routingMatchesDispatch(*e, empty),
          "64 pending onsets are projected without consuming or eagerly flushing them");
    e->noteOff(20); // Event 65 flushes the 64 onsets, then releases immediately.
    check(routingHeld(*e) == 8 && routingHeld(*e, 28) == 1 && !routesToFirst(*e, empty),
          "event 65 keeps live flush-then-release ordering and waiting promotion");
    e->allNotesOff(false);
    e->noteOn(100, 0.6f);
    render(*e, 30);
    check(routingHeld(*e) == 0, "overflow and panic preserve the pre-existing scan phase before frame 48");
    render(*e, 1);
    check(routingHeld(*e, 100) == 1, "overflow does not reset the scanner clock");

    *e = prototype;
    routingFill(*e, 8, 0, 1);
    for (int n = 8; n < 48; ++n) e->noteOn(n, 0.7f, 2);
    render(*e, 48);
    e->allNotesOff(false, 1);
    bool waitingBound = true;
    for (int n = 8; n < 40; ++n)
    {
        waitingBound &= routingHeld(*e, n, 2) == 1;
        e->noteOff(n, 2); render(*e, 48);
    }
    check(waitingBound && routingHeld(*e) == 0,
          "existing 32-waiter bound retains keys 8..39 and drops later excess keys");
}

void testScanRoutingPedalsAndMapping(const Engine& prototype, Engine& empty)
{
    for (const bool mpe : {false, true})
        for (const int sustain : {0, 1})
            for (const int channel : {2, 8, 15})
                for (const int pedal : {1, 8, 16, channel})
                {
                    auto e = std::make_unique<Engine>(prototype);
                    auto p = e->getParams(); p[P_mpeOn] = mpe ? 1.0f : 0.0f;
                    p[P_sustainMode] = static_cast<float>(sustain); e->setParams(p);
                    if (mpe)
                        for (const int master : {1, 16})
                        {
                            e->controlChange(101, 0.0f, master);
                            e->controlChange(100, 6.0f / 127.0f, master);
                            e->controlChange(6, 3.0f / 127.0f, master);
                        }
                    routingFill(*e, 6, 20);
                    e->noteOn(60, 0.8f, channel); e->noteOn(61, 0.8f, channel);
                    e->noteOff(60, channel);
                    e->controlChange(64, 1.0f, pedal); // State at dispatch, not at enqueue.
                    const bool inherited = pedal == channel || (mpe && ((channel == 2 && pedal == 1)
                                                                              || (channel == 15 && pedal == 16)));
                    const std::string label = "routing pedal mpe=" + std::to_string(mpe)
                        + " sustain=" + std::to_string(sustain) + " note=" + std::to_string(channel)
                        + " pedal=" + std::to_string(pedal);
                    check(routesToFirst(*e, empty) == !inherited && routingMatchesDispatch(*e, empty),
                          label + " matches own-channel and negotiated-zone hold");
                    e->controlChange(64, 0.0f, pedal);
                    check(routesToFirst(*e, empty) && routingMatchesDispatch(*e, empty),
                          label + " latest pedal-up removes projected hold before the scan");
                }

    for (const int mode : {1, 2})
    {
        auto e = std::make_unique<Engine>(prototype);
        auto p = e->getParams(); p[P_tuningMode] = static_cast<float>(mode); e->setParams(p);
        tn::TuningTable map; map.mapped[60] = false;
        const auto setMap = [&] { if (mode == 1) e->setScalaTuning(map); else e->setMtsTuning(map); };
        setMap();
        for (int n = 60; n < 68; ++n) e->noteOn(n, 0.8f);
        check(routesToFirst(*e, empty) && routingMatchesDispatch(*e, empty),
              "unmapped queued onset consumes no projected circuit in tuning mode " + std::to_string(mode));
        map.mapped[60] = true; setMap();
        check(!routesToFirst(*e, empty) && routingMatchesDispatch(*e, empty),
              "mapping change before dispatch updates projected occupancy in tuning mode " + std::to_string(mode));
        render(*e, 48);
        e->noteOn(80, 0.8f); render(*e, 48);
        map.mapped[80] = false; setMap();
        e->noteOff(60); render(*e, 48);
        bool excludedWaiterSounds = false;
        for (int v = 0; v < 8; ++v)
        {
            const auto voice = e->getVoiceInfo(v);
            excludedWaiterSounds |= voice.held && voice.note == 80;
        }
        check(!excludedWaiterSounds && routesToFirst(*e, empty) && routingMatchesDispatch(*e, empty),
              "excluded waiting key leaves a free circuit in tuning mode " + std::to_string(mode));
        // A mapped queued strike takes that free circuit; projection must not
        // let the retained excluded waiter steal it during replay.
        e->noteOn(81, 0.8f);
        check(!routesToFirst(*e, empty) && routingMatchesDispatch(*e, empty),
              "mapped queued strike fills circuit while excluded waiter remains");
    }
}

void testScanRoutingTransitions(const Engine& prototype, Engine& empty)
{
    auto e = std::make_unique<Engine>(prototype);
    for (int n = 40; n < 48; ++n) e->noteOn(n, 0.7f, n % 2 + 2);
    check(!routesToFirst(*e, empty), "eight queued notes occupy projected capacity before channel panic");
    e->allNotesOff(true, 2);
    check(routesToFirst(*e, empty) && routingMatchesDispatch(*e, empty),
          "channel release panic discards only that channel's pending events");
    e->allNotesOff(false, 3); render(*e, 48);
    check(routingHeld(*e) == 0, "channel hard panic removes the remaining pending events");
    for (int n = 40; n < 48; ++n) e->noteOn(n, 0.7f);
    e->reset();
    check(routesToFirst(*e, empty) && routingMatchesDispatch(*e, empty) && routingHeld(*e) == 0,
          "reset clears pending reservations and live allocator state");

    // Deliberate limit: disabling scan preserves the published distinction.
    // Host notes are immediate; keyboard notes stay behind an existing queue.
    for (const bool keyboard : {false, true})
    {
        *e = prototype;
        for (int n = 40; n < 48; ++n) e->noteOnFromKeyboard(n, 0.7f);
        auto p = e->getParams(); p[P_kasScanTiming] = 0.0f; e->setParams(p);
        check(!e->hasDeferredNoteTiming() && routesToFirst(*e, empty),
              "scan-disable routing retains physical-only capacity with old UI events pending");
        if (keyboard) e->noteOnFromKeyboard(90, 0.9f);
        else e->noteOn(90, 0.9f);
        check(routingHeld(*e, 90) == (keyboard ? 0 : 1),
              "scan-disable keeps the existing keyboard-deferred versus host-immediate distinction");
        render(*e, 1);
        check(routingHeld(*e) == 8 && routingHeld(*e, 90) == (keyboard ? 0 : 1),
              "mixed disabled-scan/UI congestion remains the explicitly unchanged behavior");
    }

    // Paths outside sixteen-voice active direct scanning remain physical-only,
    // including existing queued events surviving an ARP transition.
    for (const int control : {0, 1, 2, 3, 4, 5, 6, 7})
    {
        *e = prototype;
        auto p = e->getParams();
        if (control == 0) p[P_sourcePerformance] = 0.0f; // Legacy, scan requested.
        if (control == 1) p[P_kasScanTiming] = 0.0f;
        if (control >= 2 && control <= 5) p[P_voiceMode] = static_cast<float>(control == 2 ? 0 : control - 1);
        if (control == 6) p[P_arpOn] = 1.0f;
        if (control == 7) p[P_parityMode] = 1.0f;
        e->setParams(p);
        for (int n = 40; n < 48; ++n) e->noteOn(n, 0.7f);
        check(routesToFirst(*e, empty) == e->hasFreeCircuit(),
              "non-preview control retains physical-only routing " + std::to_string(control));
    }
    *e = prototype;
    for (int n = 40; n < 48; ++n) e->noteOn(n, 0.7f);
    auto p = e->getParams(); p[P_arpOn] = 1.0f; e->setParams(p);
    check(routesToFirst(*e, empty) == e->hasFreeCircuit(),
          "ARP enable with a pending queue does not use direct-scan projection");
}

void testScanRoutingQueryPurity(const Engine& prototype, Engine& empty)
{
    auto queried = std::make_unique<Engine>(prototype), untouched = std::make_unique<Engine>(prototype);
    bool beforeEqual = true, audioEqual = true, afterEqual = true;
    for (int round = 0; round < 24; ++round)
    {
        for (auto* e : {queried.get(), untouched.get()})
        {
            const int note = 40 + round % 12, channel = 1 + round % 3;
            e->noteOnFromKeyboard(note, 0.6f, channel);
            e->noteOn(note, 0.9f, channel);
            if (round % 2 == 0) e->noteOffFromKeyboard(note, channel);
            if (round % 3 == 0) e->controlChange(64, round % 6 == 0 ? 1.0f : 0.0f, channel);
            if (round % 4 == 0) e->noteOff(40 + (round + 7) % 12, channel);
            if (round == 12) e->allNotesOff(false, 2);
        }
        for (int repeat = 0; repeat < 37; ++repeat) (void) routesToFirst(*queried, empty);
        beforeEqual &= sameRoutingVoices(*queried, *untouched);
        const int frames = std::array<int, 5> {{1, 17, 30, 48, 73}}[static_cast<std::size_t>(round % 5)];
        audioEqual &= render(*queried, frames) == render(*untouched, frames);
        afterEqual &= sameRoutingVoices(*queried, *untouched);
    }
    check(beforeEqual, "repeated routing queries leave live voice and physical-capacity telemetry unchanged");
    check(audioEqual, "repeated routing queries leave subsequent PCM bit-identical across scan boundaries");
    check(afterEqual, "repeated routing queries preserve allocator, ownership, pedal and touch continuation");
}

void testScanRoutingRandomOracle(const Engine& prototype, Engine& empty)
{
    int projectedFull = 0, comparisons = 0, mismatches = 0;
    for (std::uint32_t seed = 1; seed <= 24; ++seed)
    {
        auto e = std::make_unique<Engine>(prototype);
        auto p = e->getParams(); p[P_sustainMode] = static_cast<float>(seed % 2);
        p[P_mpeOn] = static_cast<float>((seed / 2) % 2); p[P_tuningMode] = static_cast<float>(1 + seed % 2);
        e->setParams(p);
        routingFill(*e, static_cast<int>(seed % 8), 40);
        std::uint32_t rng = 0x80C580u ^ seed;
        const auto random = [&]() { rng = rng * 1664525u + 1013904223u; return rng; };
        bool matches = true;
        tn::TuningTable map;
        for (int step = 0; step < 48; ++step)
        {
            const auto value = random();
            const int note = 40 + static_cast<int>((value >> 8) % 24);
            const int channel = std::array<int, 5> {{1, 2, 8, 15, 16}}[(value >> 16) % 5];
            const int action = static_cast<int>((value >> 24) % 16);
            if (action < 5) e->noteOn(note, 0.75f, channel);
            else if (action < 8) e->noteOnFromKeyboard(note, 0.5f, channel);
            else if (action < 10) e->noteOff(note, channel);
            else if (action < 12) e->noteOffFromKeyboard(note, channel);
            else if (action == 12) e->controlChange(64, (value & 1u) ? 1.0f : 0.0f, channel);
            else if (action == 13)
            {
                map.mapped[static_cast<std::size_t>(note)] = (value & 1u) != 0;
                e->setScalaTuning(map); e->setMtsTuning(map);
            }
            else if (action == 14) e->allNotesOff((value & 1u) != 0, channel);
            else render(*e, 1 + static_cast<int>((value >> 4) % 47));
            const bool match = routingMatchesDispatch(*e, empty, &projectedFull);
            matches &= match; mismatches += !match; ++comparisons;
        }
        check(matches, "independent real-dispatch capacity oracle, deterministic transcript " + std::to_string(seed));
    }
    check(projectedFull > 20, "random oracle exercises physically-free but projected-full states");
    std::printf("Scan routing dispatch oracle: %d comparisons, %d projected-full states, %d mismatches\n",
                comparisons, projectedFull, mismatches);
}

void testSixteenVoiceScanRouting()
{
    auto prototype = std::make_unique<Engine>();
    prototype->prepare(48000.0); prototype->setParams(scanRoutingParams());
    auto empty = std::make_unique<Engine>(*prototype);
    testSixteenVoiceScanChords(*prototype);
    testScanRoutingLifecycles(*prototype, *empty);
    testScanRoutingQueueBounds(*prototype, *empty);
    testScanRoutingPedalsAndMapping(*prototype, *empty);
    testScanRoutingTransitions(*prototype, *empty);
    testScanRoutingQueryPurity(*prototype, *empty);
    testScanRoutingRandomOracle(*prototype, *empty);
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
    testArpResetParameterSync();
    testArpLiveSequenceEdits();
    testWaitingTuningAdmission();
    testUnmappedSustainAdmission();
    testMtsNonRealtimeAdmission();
    testMtsEmptyChannelMask();
    testMtsChannelRecipients();
    testMtsChannelState();
    testMtsChannelLifecycle();
    testMtsChannelMappingRoutes();
    testArpeggiatorTuningMap();
    testMpe();
    testMpePedalZoneBoundaries();
    testMpeExpressionZoneBoundaries();
    testMpeNrpnIsolation();
    testMpeZeroMemberConfiguration();
    testVoiceModes();
    testSixteenVoiceScanRouting();
    testRibbonCc();
    std::printf("Tuning / MIDI / voice modes: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
