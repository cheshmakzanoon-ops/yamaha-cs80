// A27 diagnostic only. This does not deserialize a host/plugin state file.
// It checks the completed ParamValues passed to the current core boundary.
#include "core/Engine.h"
#include "core/FactoryPresets.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <limits>
#include <memory>

int main()
{
    using namespace cs80;
    static_assert(P_numParams == 204 && P_body1Model == 190 && P_body1Size == 198
                  && P_body1Variant == 200 && P_body1Material == 202);
    int checks = 0, failures = 0;
    const auto check = [&](bool ok) { ++checks; if (!ok) ++failures; };
    const ParamValues defaults;
    const auto& bank = programLibrary();
    check(bank.size() == 523 && bank[43].name == "Init Patch" && bank[522].name == "Init Circuit");
    constexpr std::array<float, 14> neutral {0, 0, .5f, 1, 0, 0, .5f, 1, 1, 1, 0, 0, 0, 0};
    for (int program : {43, 522})
        for (int k = 0; k < 14; ++k)
            check(bank[program].values[P_body1Model + k] == neutral[k]);

    // Deliberately edited historical controls, not a new catalogue program.
    auto saved = bank[43].values;
    saved[P_volume] = .413f;
    saved[chParam(0, CP_lpfCutoff)] = 1234.5f;
    constexpr std::array<float, 14> authored {1, .83f, .31f, 2.17f, 2, .47f, .72f, .63f,
                                            1.35f, .82f, 3, 4, 1, 6};
    for (int k = 0; k < 14; ++k) saved[P_body1Model + k] = authored[k];
    std::printf("{\n  \"scope\": \"completed values at core boundary; no host deserialization\",\n");
    std::printf("  \"parameter_count\": %d,\n  \"prefix_cases\": [\n", P_numParams);
    int caseNumber = 0;
    for (int stored : {190, 198, 200, 202, 204})
    {
        ParamValues completed;
        std::copy_n(saved.v.begin(), stored, completed.v.begin());
        auto engine = std::make_unique<Engine>();
        engine->setParams(saved); // Populate the receiving core with stale later fields.
        engine->setParams(completed);
        bool preserved = true, neutralMissing = true;
        for (int i = 0; i < stored; ++i)
            preserved = preserved && engine->getParams()[i] == saved[i];
        for (int i = stored; i < P_numParams; ++i)
            neutralMissing = neutralMissing && engine->getParams()[i] == defaults[i];
        check(preserved); check(neutralMissing);
        check(engine->getParams()[P_volume] != bank[43].values[P_volume]);
        std::printf("    %s{\"stored_fields\": %d, \"saved_values_exact\": %s, \"omitted_fields_default\": %s}",
                    caseNumber++ ? "," : "", stored, preserved ? "true" : "false", neutralMissing ? "true" : "false");
        std::puts("");
    }
    auto engine = std::make_unique<Engine>();
    for (float invalid : {std::numeric_limits<float>::quiet_NaN(),
                          std::numeric_limits<float>::infinity(),
                          -std::numeric_limits<float>::infinity()})
    {
        auto values = saved;
        for (int i = P_body1Model; i < P_numParams; ++i) values[i] = invalid;
        engine->setParams(values);
        for (int i = P_body1Model; i < P_numParams; ++i)
            check(engine->getParams()[i] == defaults[i]);
    }
    for (bool high : {false, true})
    {
        auto values = saved;
        for (int i = P_body1Model; i < P_numParams; ++i)
            values[i] = high ? paramDef(i).max + 1.f : paramDef(i).min - 1.f;
        engine->setParams(values);
        for (int i = P_body1Model; i < P_numParams; ++i)
            check(engine->getParams()[i] == (high ? paramDef(i).max : paramDef(i).min));
    }
    std::printf("  ],\n  \"nonfinite_body_checks\": 42,\n  \"body_range_checks\": 28,\n");
    std::printf("  \"checks\": %d,\n  \"failures\": %d\n}\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
