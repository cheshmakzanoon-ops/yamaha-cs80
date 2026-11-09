#include "reference/ProgrammerControlIdentity.h"
#include "core/MemoryPanels.h"
#include <array>
#include <cstdio>
#include <string_view>
#include <limits>

int main()
{
    using namespace cs80;
    namespace id = reference::programmer_identity;
    // Explicit physical-row oracle, independent of channel-enum numeric order.
    constexpr std::array<std::string_view, 26> expected {{
        "pwm_speed", "pwm_depth", "pw", "pulse", "saw", "noise",
        "hpf_cutoff", "hpf_res", "lpf_cutoff", "lpf_res",
        "f_il", "f_al", "f_a", "f_d", "f_r", "vcf_level", "sine",
        "v_a", "v_d", "v_s", "v_r", "v_level",
        "init_brill", "init_level", "after_brill", "after_level"
    }};
    std::array<bool, CP_count> seen {};
    int failures = 0;
    auto check = [&](bool value) { if (!value) ++failures; };
    for (int row = 1; row <= 26; ++row)
    {
        const int channel = id::channelParamForOutput(row);
        check(channel >= 0 && channel < CP_count);
        if (channel < 0 || channel >= CP_count) continue;
        check(!seen[static_cast<size_t>(channel)]);
        seen[static_cast<size_t>(channel)] = true;
        check(kChannelDefs[channel].id == expected[static_cast<size_t>(row - 1)]);
        check(id::outputForChannelParam(channel) == row);
        check(memory_panels::isSwitchRow(row) == (channel == CP_pulse || channel == CP_saw));
    }
    for (int channel = 0; channel < CP_count; ++channel)
    {
        const bool excluded = channel == CP_feet || channel == CP_pan || channel == CP_hpfOn || channel == CP_lpfOn;
        check(seen[static_cast<size_t>(channel)] != excluded);
        if (excluded) check(id::outputForChannelParam(channel) == -1);
    }
    for (int invalid : {std::numeric_limits<int>::min(), -1, 0, 27, std::numeric_limits<int>::max()})
        check(id::channelParamForOutput(invalid) == -1);
    for (int invalid : {std::numeric_limits<int>::min(), -1, static_cast<int>(CP_count), std::numeric_limits<int>::max()})
        check(id::outputForChannelParam(invalid) == -1);
    // Identity is shared by both ranks; electrical touch receivers are not.
    check(source_factory::touchReceiverIds[0][1] != source_factory::touchReceiverIds[1][1]);
    check(source_factory::touchReceiverIds[0][3] != source_factory::touchReceiverIds[1][3]);
    static_assert(id::channelParamForOutput(1) == CP_pwmSpeed);
    static_assert(id::outputForChannelParam(CP_saw) == 5);
    std::printf("26 physical row identities and exclusions: %d failures\n", failures);
    return failures != 0;
}
