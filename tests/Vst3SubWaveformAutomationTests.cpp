// Only the supported-input native host/renderer is reused. The historical
// boundary/fuzz suite and its main are not compiled into this executable.
#define CS80_SUPPORTED_SUB_AUTOMATION_ONLY 1
#include "Vst3AutomationTests.cpp"

int main(int argc, char** argv)
{
    try
    {
        require(argc == 5, "usage: sub-waveform-test MODULE ORIGINAL_CONTRACT 44100|48000|96000 realtime|offline");
        const std::string rate(argv[3]), mode(argv[4]);
        require(rate == "44100" || rate == "48000" || rate == "96000", "supported sample rate");
        require(mode == "realtime" || mode == "offline", "supported process mode");
        testSampleRate = std::stod(rate);
        testProcessMode = mode == "offline" ? kOffline : kRealtime;
        const ws1::Tolerances limits(argv[2], "host_automation");
        require(cs80::kSampleAccurateParameters.size() >= 24, "unchanged 24-lane capability prefix");
        require(cs80::kSampleAccurateParameters[20] == cs80::P_subOriginalSpeed
                && cs80::kSampleAccurateParameters[21] == cs80::P_subVco
                && cs80::kSampleAccurateParameters[22] == cs80::P_subVcf
                && cs80::kSampleAccurateParameters[23] == cs80::P_subVca,
                "existing continuous SUB lane order");
        std::cout << "rate=" << testSampleRate << " mode=" << mode << '\n';
        Module module(argv[1]);
        auto parameters = cs80::defaultValues();
        parameters[cs80::P_sourcePerformance] = 1;
        parameters[cs80::P_parityMode] = 0;
        parameters[cs80::P_drift] = parameters[cs80::P_voiceDispersion] = parameters[cs80::P_analogNoise] = 0;
        parameters[cs80::P_subOriginalMode] = 1;
        parameters[cs80::P_subOriginalSpeed] = 12;
        parameters[cs80::P_subVco] = .3f;
        parameters[cs80::P_subVcf] = .4f;
        parameters[cs80::P_subVca] = .5f;
        Curves curves;
        curves[20] = {{511, .15}, {1023, .7}, {1024, .25}, {2047, .8}, {3071, .2}, {4095, .6}};
        curves[21] = {{511, .7}, {1023, .2}, {1024, .65}, {2047, .3}, {3071, .8}, {4095, .4}};
        curves[22] = {{511, .2}, {1023, .8}, {1024, .35}, {2047, .65}, {3071, .25}, {4095, .7}};
        curves[23] = {{511, .6}, {1023, .3}, {1024, .75}, {2047, .2}, {3071, .65}, {4095, .35}};

        int scenarios = 0;
        // Static original functions: falling saw, rising saw, comparator square.
        // Function selection happens only in initialization, never in a queue.
        for (int function : {1, 2, 3})
            for (int voices : {8, 16})
                for (const auto sources : {std::array{13, 13}, std::array{13, 0}, std::array{11, 13}})
                {
                    auto p = parameters;
                    p[cs80::P_subOriginalFunction] = float(function);
                    p[cs80::P_voiceMode] = voices == 16 ? 1.f : 0.f;
                    const auto waveformRender = [&](const cs80::ParamValues& values, int block,
                                                    bool automate = true, bool flatten = false,
                                                    bool nullChanges = false)
                    {
                        return render(module, values, curves, block, automate, flatten,
                                      sources[0], nullChanges, sources[1]);
                    };
                    const auto scalar = waveformRender(p, 1);
                    auto sine = p;
                    sine[cs80::P_subOriginalFunction] = 0;
                    const double functionEffect = difference(scalar, waveformRender(sine, 1));
                    check(functionEffect > .00001, "static SUB function differs from sine under identical queues");
                    double error = 0;
                    for (int block : {7, 64, 256, 1024, 4096})
                        error = std::max(error, difference(scalar, waveformRender(p, block)));
                    check(error <= limits["audio_max_abs_error"],
                          "static SUB waveform queues match per-sample reference across partitions");
                    const auto neutral = waveformRender(p, 256, false);
                    const double effect = difference(scalar, neutral);
                    check(effect > .00001, "static SUB waveform modulation positive PCM witness");
                    check(neutral == waveformRender(p, 256, false, false, true),
                          "static SUB waveform empty/no queue unchanged");
                    for (int policy : {0, 2})
                    {
                        auto protectedP = p;
                        protectedP[cs80::P_sourcePerformance] = policy == 0 ? 0.f : 1.f;
                        protectedP[cs80::P_parityMode] = policy == 2 ? 1.f : 0.f;
                        check(waveformRender(protectedP, 256) == waveformRender(protectedP, 256, true, true),
                              "static SUB waveform Legacy/PURE retain flattened native queues");
                    }
                    ++scenarios;
                    std::cout << "waveform=" << function << " voices=" << voices
                              << " sources=" << sources[0] << ',' << sources[1]
                              << " max_error=" << error << " effect=" << effect
                              << " function_effect=" << functionEffect << '\n';
                }
        std::cout << scenarios << " native SUB waveform scenarios, " << failures << " failures\n";
        return failures ? 1 : 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "ERROR: " << e.what() << '\n';
        return 2;
    }
}
