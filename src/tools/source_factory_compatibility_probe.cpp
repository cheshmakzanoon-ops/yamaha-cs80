// Same-toolchain V1/V2 numeric regression, NOT hardware evidence. This fixture
// intentionally never selects an unresolved V1 source. Model-version metadata
// changes by design and is not written into the numeric corpus.
#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <vector>
using namespace cs80;
namespace {
FILE* file = nullptr;
template<class T> bool write(const T& x) { return std::fwrite(&x, sizeof(x), 1, file) == 1; }
bool capture(const Engine& e, const std::vector<float>& audio)
{
    for (float x : audio) if (!std::isfinite(x) || !write(x)) return false;
    if (!write(e.activeVoices()) || !write(e.getRingFrequency()) || !write(e.getRingEnvelope())
        || !write(e.getSubOscillatorValue()) || !write(e.getSubOscillatorRate())) return false;
    for (int voice = 0; voice < 8; ++voice) for (int rank = 0; rank < 2; ++rank)
    {
        auto a = e.getRankAudioState(voice, rank); auto s = e.getRankEnvelopeState(voice, rank); auto i = e.getRankInfo(voice, rank);
        for (float x : {a.phase,a.hpf.ic1,a.hpf.ic2,a.lpf.ic1,a.lpf.ic2,s.amplitude.value,s.amplitude.elapsed,
             s.filter.value,s.filter.elapsed,i.hpfHz,i.lpfHz,i.hpfQ,i.lpfQ,i.oscillatorHz,i.keyVolts})
            if (!std::isfinite(x) || !write(x)) return false;
        if (!write(s.amplitude.stage) || !write(s.filter.stage)) return false;
    }
    for (int rank = 0; rank < 2; ++rank)
    {
        const auto c = e.getSourceCutoffReadout(rank);
        for (int v = 0; v < 8; ++v) for (int side = 0; side < 2; ++side)
            if (!write(c.pinVolts[v][side]) || !write(c.effectiveCv[v][side])) return false;
        for (double x : c.loadedBusVolts) if (!write(x)) return false;
    }
    const auto shared = e.getSourceSharedReadout();
    for (int rank = 0; rank < 2; ++rank) for (int v = 0; v < 8; ++v)
    {
        for (int side = 0; side < 2; ++side)
            if (!write(shared.qVolts[rank][v][side]) || !write(shared.q[rank][v][side])) return false;
        if (!write(shared.levelPinVolts[rank][v]) || !write(shared.levelGain[rank][v])) return false;
    }
    return true;
}
}
int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    file = std::fopen(argv[1], "wb"); if (!file) return 3;
    constexpr std::array<std::array<int, 2>, 7> selections {{{1,13},{3,13},{9,13},{13,3},{13,7},{13,9},{13,10}}};
    constexpr std::array<int,8> onsetNotes {21,36,48,55,60,67,84,108};
    std::vector<float> audio(254), external(127);
    for (double rate : {44100.,48000.,96000.,192000.}) for (auto selection : selections)
    {
        Engine e; e.prepare(rate); ControlFrame frame; frame.hardwareSources = selection;
        auto& p = frame.effectiveValues; p[P_parityMode] = 1; p[P_masterSustain] = 0;
        p[P_chorusOn] = 1; p[P_ringAmount] = .2f; p[P_subVcf] = .1f; p[P_subVca] = .1f;
        if (!e.setControlFrame(frame)) return 4;
        auto notes = onsetNotes;
        for (int block = 0; block < 96; ++block)
        {
            const float u = static_cast<float>(block % 13) / 12;
            p[P_brilliance] = (u - .5f) * .3f; p[P_resonance] = .1f + .7f * u;
            p[P_footMode] = (block / 11) % 2; p[P_outputLevel] = (block / 7) % 2;
            p[P_outputMode] = (block / 5) % 2; p[P_subExtLevel] = .1f;
            // PANEL/memory handoffs and pairs use complete V1 sources only.
            frame.hardwareSources = selection;
            if (block % 12 == 8) frame.hardwareSources = {13,13};
            if (block % 12 == 9) frame.hardwareSources = {11,12};
            if (block % 12 == 10) frame.hardwareSources = {9,10};
            for (int rank = 0; rank < 2; ++rank)
                if (frame.hardwareSources[rank] < 11) applyHardwareTone(p,rank,frame.hardwareSources[rank]);
            if (!e.setControlFrame(frame) || !e.setControlFrame(frame)) return 5;
            if (block < 8) e.noteOn(notes[block], .125f * (block + 1), 1 + block % 2);
            if (block % 8 == 7)
            {
                int v = (block / 8) % 8;
                e.noteOff(notes[v], 1 + v % 2); notes[v] = 36 + (block * 7) % 60;
            }
            if (block > 8 && block % 8 == 0) { int v = ((block - 1) / 8) % 8; e.noteOn(notes[v], .7f, 1 + v % 2); }
            if (block == 40) e.controlChange(120,0,2);
            if (block == 56) e.allNotesOff(false);
            if (block == 64) for (int v = 0; v < 8; ++v) e.noteOn(notes[v], .6f, 1 + v % 2);
            if (block == 80) e.allNotesOff();
            e.channelPressure(1,u); e.setExpression(.2f + .8f * u); e.setRibbon((u - .5f) * .4f);
            for (int n = 0; n < 127; ++n) external[n] = .2f * std::sin(static_cast<float>(n + block * 127) * .02f);
            e.render(audio.data(),127,external.data()); if (!capture(e,audio)) return 6;
        }
        e.prepare(rate); if (!e.setControlFrame(frame)) return 7;
        e.noteOn(60,.8f); e.render(audio.data(),127); if (!capture(e,audio)) return 8;
    }
    return std::fclose(file) == 0 ? 0 : 9;
}
