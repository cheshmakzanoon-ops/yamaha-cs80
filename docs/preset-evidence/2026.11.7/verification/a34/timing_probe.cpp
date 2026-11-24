// A34 evidence only: exact source timing functions, untouched factory vectors.
#include "core/FactoryPresets.h"
#include "core/BodyArticulation.h"
#include "core/ToleranceModel.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace cs80;

int main()
{
    std::puts("index,name,category,rank,capture_active,sounding_path,filter_span,body_model,authored_v_a,authored_f_a,cli_default_hold,amp_min_v08,amp_max_v08,filter_min,filter_max,amp_min_v1,amp_max_v1,amp_max_v0,amp_card0_v08,filter_card0");
    const auto& bank = programLibrary();
    for (size_t i = 0; i < bank.size(); ++i)
    {
        const auto& p = bank[i]; const auto& v = p.values;
        // Every emitted program is a neutral-macro, ordinary panel recipe.
        if (v[P_sourcePerformance] != 1 || v[P_parityMode] != 0) return 2;
        for (int macro = P_macro1; macro <= P_macro4; ++macro) if (v[macro] != .5f) return 3;
        double attack = 0;
        for (int r = 0; r < 2; ++r)
            if ((r == 0 && v[P_mix] < .99f) || (r == 1 && v[P_mix] > .01f))
                attack = std::max({attack, double(v[chParam(r, CP_vA)]), double(v[chParam(r, CP_fA)])});
        const double hold = std::clamp(attack + 1.5, 3.0, 10.0);
        for (int r = 0; r < 2; ++r)
        {
            const auto c = [&](int local) { return v[chParam(r, local)]; };
            const bool active = r == 0 ? v[P_mix] < .99f : v[P_mix] > .01f;
            const int model = std::lround(v[bodyParam(r, 0)]);
            const float bodyMix = v[bodyParam(r, 1)];
            const bool source = (model > 0 && bodyMix > 0) || c(CP_sine) > 0
                || (c(CP_vcfLevel) > 0 && (c(CP_saw) >= .5f || c(CP_pulse) >= .5f || c(CP_noise) > 0));
            const bool sounding = active && source && (c(CP_vLevel) + c(CP_initLevel) + c(CP_afterLevel) > 0);
            const bool filterSpan = c(CP_fIl) != 0 || c(CP_fAl) != 0;
            float aMin = 1e9f, aMax = 0, fMin = 1e9f, fMax = 0, oneMin = 1e9f, oneMax = 0, zeroMax = 0, a0 = 0, f0 = 0;
            const float scale = v[P_voiceDispersion] / tolerance::kDefaultDispersion;
            // Engine has eight voice cards and two rank cards per voice; drawCard(voice*2+rank).
            // Sounding note is irrelevant for attack: body::captureArticulation's attack depends on velocity/mix only.
            for (int voice = 0; voice < 8; ++voice)
            {
                const auto t = tolerance::ratios(tolerance::drawCard(voice * 2 + r), scale);
                const auto amplitude = [&](float velocity) {
                    const auto b = body::captureArticulation(model, 60.f, velocity, bodyMix);
                    return std::max(.001f, c(CP_vA) * b.attack * 1.f * t.ampEnvTime);
                };
                const float a = amplitude(.8f), f = std::max(.001f, c(CP_fA) * 1.f * t.filterEnvTime);
                if (voice == 0) { a0 = a; f0 = f; }
                aMin = std::min(aMin, a); aMax = std::max(aMax, a);
                fMin = std::min(fMin, f); fMax = std::max(fMax, f);
                oneMin = std::min(oneMin, amplitude(1)); oneMax = std::max(oneMax, amplitude(1));
                zeroMax = std::max(zeroMax, amplitude(0));
            }
            std::printf("%zu,\"%s\",\"%s\",%d,%d,%d,%d,%d,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g\n",
                i,p.name.c_str(),p.category.c_str(),r+1,active,sounding,filterSpan,model,c(CP_vA),c(CP_fA),hold,
                aMin,aMax,fMin,fMax,oneMin,oneMax,zeroMax,a0,f0);
        }
    }
}
