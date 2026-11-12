// Bounded supported-input renderer only. Historical broad host bodies excluded.
#define CS80_SUPPORTED_SUB_AUTOMATION_ONLY 1
#include "Vst3AutomationTests.cpp"
#include "Vst3Frozen89Roster.h"
#include "Vst3Frozen86Roster.h"
#include <fstream>
namespace {
using NamedCurves = std::vector<std::pair<int, Curve>>;
int renderCount = 0;
std::vector<float> lfoRender(Module& module, const cs80::ParamValues& parameters,
    const NamedCurves& curves, int block, int source, int secondSource,
    bool flatten = false, bool nullChanges = false)
{
    ++renderCount;
    Instance instance(module, parameters, source, secondSource);
    std::vector<float> output;
    output.reserve(8192);
    for (int start = 0; start < 4096; start += block) {
        const int count = std::min(block, 4096 - start);
        Changes changes;
        for (const auto& [parameter, curve] : curves) {
            const double initial = normalized(parameter, parameters[parameter]);
            Curve points;
            if (!flatten)
                for (const auto& point : curve)
                    if (point.first >= start && point.first < start + count)
                        points.emplace_back(point.first - start, point.second);
            if (points.empty() || points.back().first != count - 1)
                points.emplace_back(flatten ? 0 : count - 1, absoluteValue(curve, initial, start + count - 1));
            changes.add(parameterId(cs80::paramId(parameter)), std::move(points));
        }
        instance.process(start, count, nullChanges ? nullptr : &changes, output, parameters[cs80::P_voiceMode] > .5f);
    }
    return output;
}
bool exact(const std::vector<float>& a, const std::vector<float>& b)
{
    (void) difference(a, b); // checks sizes and every sample's finiteness
    return std::memcmp(a.data(), b.data(), a.size() * sizeof(float)) == 0;
}
void capture(const std::filesystem::path& path, const std::vector<float>& audio)
{
    (void) difference(audio, audio);
    std::ofstream file(path, std::ios::binary);
    require(bool(file), "capture open");
    file.write(reinterpret_cast<const char*>(audio.data()), std::streamsize(audio.size() * sizeof(float)));
    require(bool(file), "complete capture write");
}
}
int main(int argc, char** argv)
{
    try {
        require(argc == 7 || argc == 8, "usage: lfo-rate-test MODULE HOST_CONTRACT RATE_CONTRACT OUTPUT RATE MODE [--compat-only]");
        const bool compat = argc == 8;
        if (compat) require(std::string(argv[7]) == "--compat-only", "explicit compatibility mode");
        const std::string rate(argv[5]), mode(argv[6]);
        require(rate == "44100" || rate == "48000" || rate == "96000", "supported rate");
        require(mode == "realtime" || mode == "offline", "supported mode");
        testSampleRate = std::stod(rate); testProcessMode = mode == "offline" ? kOffline : kRealtime;
        const ws1::Tolerances host(argv[2], "host_automation"), limits(argv[3], "host_automation_lfo_rate");
        require(cs80::kSampleAccurateParameters.size() >= 86, "86-lane base available");
        require(std::equal(cs80::kFrozen86Roster.begin(), cs80::kFrozen86Roster.end(),
                           cs80::kSampleAccurateParameters.begin()), "literal86 prefix unchanged");
        constexpr std::array rates {int(cs80::P_lfo1Rate), int(cs80::P_lfo2Rate), int(cs80::P_lfo3Rate)};
        constexpr std::array depths {int(cs80::P_lfo1Depth), int(cs80::P_lfo2Depth), int(cs80::P_lfo3Depth)};
        constexpr std::array destinations {int(cs80::P_lfo1Dest), int(cs80::P_lfo2Dest), int(cs80::P_lfo3Dest)};
        const std::array<std::string, 3> ids {"lfo1_rate", "lfo2_rate", "lfo3_rate"};
        for (size_t i = 0; i < rates.size(); ++i)
            require(cs80::paramId(rates[i]) == ids[i], "literal LFO rate parameter IDs");
        require(limits["key_parameter_count"] == 89, "89-lane contract");
        std::cout << "compiled_lanes=" << cs80::kSampleAccurateParameters.size() << "\n";
        if (!compat) {
            require(cs80::kSampleAccurateParameters.size() >= 89, "89-lane prefix available");
            require(std::equal(cs80::kFrozen89Roster.begin(), cs80::kFrozen89Roster.end(),
                               cs80::kSampleAccurateParameters.begin()), "literal89 prefix unchanged");
            require(std::equal(rates.begin(), rates.end(), cs80::kSampleAccurateParameters.begin() + 86),
                    "rate1/2/3 at86/87/88 exactly");
        }
        std::filesystem::create_directories(argv[4]);
        const std::filesystem::path out(argv[4]);
        Module module(argv[1]);
        auto parameters = cs80::defaultValues();
        parameters[cs80::P_sourcePerformance] = 1;
        parameters[cs80::P_parityMode] = 0;
        parameters[cs80::P_drift] = parameters[cs80::P_voiceDispersion] = parameters[cs80::P_analogNoise] = 0;
        parameters[cs80::P_kbdLevelLow] = 0;
        parameters[cs80::P_kbdLevelHigh] = 0;
        parameters[cs80::P_kbdBrillianceLow] = parameters[cs80::P_kbdBrillianceHigh] = 0;
        parameters[cs80::P_portamento] = 0;
        parameters[cs80::P_initBend] = 0;
        parameters[cs80::P_ribbonCcEnable] = 0;
        parameters[cs80::P_sustainMode] = 0;
        parameters[cs80::P_glideMode] = 0;
        parameters[cs80::P_glideAssign] = 0;
        parameters[cs80::P_footswitchPlugged] = parameters[cs80::P_footswitchPressed] = 0;
        parameters[cs80::P_sustainAssign] = 0;
        parameters[cs80::P_macro1] = parameters[cs80::P_macro2]
            = parameters[cs80::P_macro3] = parameters[cs80::P_macro4] = .5f;
        parameters[cs80::P_arpOn] = parameters[cs80::P_kasScanTiming] = 0;
        parameters[cs80::P_lfo1Depth] = parameters[cs80::P_lfo2Depth] = parameters[cs80::P_lfo3Depth] = 0;
        for (int i = cs80::P_mod1Amount; i <= cs80::P_mod16Amount; ++i) parameters[i] = 0;
        int scenarios = 0;
        parameters[cs80::P_mix] = .5f;
        parameters[cs80::P_masterSustain] = 0;
        parameters[cs80::P_keyRelease] = .5f;
        parameters[cs80::P_ribbon] = 0;
        // Neutral motion macro above yields Engine envTimeScale == 1; dispersion is zero.
        parameters[cs80::P_mpeOn] = parameters[cs80::P_pressureCurve] = parameters[cs80::P_pressureThreshold] = 0;
        for (int rank : {0, 1}) {
            parameters[cs80::chParam(rank, cs80::CP_vLevel)] = .35f;
            parameters[cs80::chParam(rank, cs80::CP_vA)] = .001f;
            parameters[cs80::chParam(rank, cs80::CP_vD)] = .002f;
            parameters[cs80::chParam(rank, cs80::CP_vS)] = .7f;
            parameters[cs80::chParam(rank, cs80::CP_vR)] = .5f;
            parameters[cs80::chParam(rank, cs80::CP_fA)] = .001f;
            parameters[cs80::chParam(rank, cs80::CP_fD)] = .002f;
            parameters[cs80::chParam(rank, cs80::CP_fAl)] = 0;
            parameters[cs80::chParam(rank, cs80::CP_fIl)] = 0;
            parameters[cs80::chParam(rank, cs80::CP_fR)] = .30f;
            parameters[cs80::chParam(rank, cs80::CP_lpfCutoff)] = 4000;
            parameters[cs80::chParam(rank, cs80::CP_hpfCutoff)] = 25;
            parameters[cs80::chParam(rank, cs80::CP_lpfRes)] = parameters[cs80::chParam(rank, cs80::CP_hpfRes)] = 0;
            parameters[cs80::chParam(rank, cs80::CP_vcfLevel)] = .78f;
            parameters[cs80::chParam(rank, cs80::CP_pulse)] = 1;
            parameters[cs80::chParam(rank, cs80::CP_saw)] = parameters[cs80::chParam(rank, cs80::CP_sine)] = parameters[cs80::chParam(rank, cs80::CP_noise)] = 0;
            parameters[cs80::chParam(rank, cs80::CP_pw)] = .5f;
            parameters[cs80::chParam(rank, cs80::CP_pwmSpeed)] = 3;
            parameters[cs80::chParam(rank, cs80::CP_pwmDepth)] = 0;
            for (int local : {cs80::CP_initBrill, cs80::CP_initLevel, cs80::CP_afterBrill, cs80::CP_afterLevel})
                parameters[cs80::chParam(rank, local)] = 0;
        }

        // Pin deterministic compatibility SUB oscillator: sine=0, 8 Hz, no external input.
        parameters[cs80::P_subOriginalMode] = 0;
        parameters[cs80::P_subWave] = 0;
        parameters[cs80::P_subSpeed] = 8;
        parameters[cs80::P_subExtLevel] = 0;
        parameters[cs80::P_subVco] = parameters[cs80::P_subVcf] = parameters[cs80::P_subVca] = 0;
        parameters[cs80::P_subAtSpeed] = parameters[cs80::P_subAtVco] = parameters[cs80::P_subAtVcf] = 0;

        // Explicitly neutralize all other modulation and effects.
        parameters[cs80::P_modWheelSub] = parameters[cs80::P_modWheelVcf] = 0;
        parameters[cs80::P_ringAmount] = parameters[cs80::P_chorusOn] = 0;
        parameters[cs80::P_fx1Mix] = parameters[cs80::P_fx2Mix] = parameters[cs80::P_fx3Mix] = 0;
        parameters[cs80::P_fx1Type] = parameters[cs80::P_fx2Type] = parameters[cs80::P_fx3Type] = 0;
        parameters[cs80::P_velocityCurve] = 0;
        parameters[cs80::P_lfo1Rate] = 8; parameters[cs80::P_lfo2Rate] = 11; parameters[cs80::P_lfo3Rate] = 14;
        parameters[cs80::P_lfo1Wave] = parameters[cs80::P_lfo2Wave] = parameters[cs80::P_lfo3Wave] = 0;
        for (int route : {0, 1, 2, 3})
        for (int capacity : {8, 16})
        for (int source : {13, 0}) {
            auto p = parameters;
            p[cs80::P_voiceMode] = capacity == 16 ? 1.f : 0.f;
            NamedCurves curves, constant;
            for (size_t i = 0; i < rates.size(); ++i) {
                const bool active = route == 3 || route == int(i);
                p[depths[i]] = active ? .65f : 0.f;
                p[destinations[i]] = active ? float(i + 1) : 0.f; // pitch/filter/amp
                if (!active) continue;
                const double initial = normalized(rates[i], p[rates[i]]);
                curves.push_back({rates[i], {{0, initial}, {1023, initial}, {1535, normalized(rates[i], 16.f)},
                                            {2047, normalized(rates[i], 4.f)}, {3071, normalized(rates[i], 18.f)}, {4095, normalized(rates[i], 7.f)}}});
                constant.push_back({rates[i], {{0, initial}, {4095, initial}}});
            }
            const std::string id = "lfo-route" + std::to_string(route) + "-capacity" + std::to_string(capacity)
                + "-sources" + std::to_string(source) + "-13";
            const auto neutral = lfoRender(module, p, {}, 256, source, 13);
            check(exact(neutral, lfoRender(module, p, {}, 256, source, 13, false, true)), "empty/null noqueue byte-identical");
            capture(out / (id + "-noqueue.pcm"), neutral);
            NamedCurves preceding;
            for (int parameter : {int(cs80::P_volume), int(cs80::P_masterTune), int(cs80::P_tuningRef), cs80::chParam(0, cs80::CP_pan), int(cs80::P_lfo1Depth), int(cs80::P_lfo2Depth), int(cs80::P_lfo3Depth)}) {
                const double initial = normalized(parameter, p[parameter]);
                preceding.push_back({parameter, {{0, initial}, {1023, initial}, {1535, .4}, {4095, .6}}});
            }
            capture(out / (id + "-preceding86.pcm"), lfoRender(module, p, preceding, 127, source, 13));
            auto legacy = p; legacy[cs80::P_sourcePerformance] = 0;
            const auto legacyActual = lfoRender(module, legacy, curves, 256, source, 13);
            check(exact(legacyActual, lfoRender(module, legacy, curves, 256, source, 13, true)),
                  "Legacy rate offsets match block-start final-point flattening exactly");
            capture(out / (id + "-legacy.pcm"), legacyActual);
            auto pure = p; pure[cs80::P_parityMode] = 1;
            const auto pureActual = lfoRender(module, pure, curves, 256, source, 13);
            check(exact(pureActual, lfoRender(module, pure, constant, 256, source, 13)), "PURE rate automation inert");
            capture(out / (id + "-pure.pcm"), pureActual);
            if (!compat) {
                const auto scalar = lfoRender(module, p, curves, 1, source, 13);
                double error = 0;
                for (int block : {64, 127, 256})
                    error = std::max(error, difference(scalar, lfoRender(module, p, curves, block, source, 13)));
                check(error <= host["audio_max_abs_error"], "LFO rate queues match scalar reference");
                const auto fixed = lfoRender(module, p, constant, 1, source, 13);
                (void) difference(scalar, fixed);
                require(scalar.size() == 8192, "complete stereo capture");
                check(std::memcmp(scalar.data(), fixed.data(), 2048 * sizeof(float)) == 0, "initial1024frame bytehold");
                const std::vector<float> changed(scalar.begin() + 2048, scalar.begin() + 6144);
                const std::vector<float> held(fixed.begin() + 2048, fixed.begin() + 6144);
                const double effect = difference(changed, held);
                check(effect > limits["minimum_held_effect"], "positive held destination effect");
                auto off = p;
                for (int destination : destinations) off[destination] = 0;
                check(exact(lfoRender(module, off, curves, 256, source, 13),
                            lfoRender(module, off, constant, 256, source, 13)), "destination-off amount automation byte-inert");
                auto zero = p;
                for (int depth : depths) zero[depth] = 0;
                check(exact(lfoRender(module, zero, curves, 256, source, 13),
                            lfoRender(module, zero, constant, 256, source, 13)), "zero-depth rate automation byte-inert");
                std::cout << id << " notes=" << (capacity == 16 ? 16 : 2)
                          << " max_error=" << error << " held_effect=" << effect << '\n';
            }
            ++scenarios;
        }
        std::cout << "scenarios=" << scenarios << " renders=" << renderCount << " captures=" << scenarios * 4
                  << " failures=" << failures << " mode=" << (compat ? "compat-only" : "full") << '\n';
        return failures ? 1 : 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 2; }
}
