// CA-031: CC88 applies to the next Note On OR Note Off on the same channel.
// Specification summary: https://midi.org/high-resolution-velocity-prefix
// No processor, GUI, audio device, or MIDI device is instantiated here.
#include "core/MpeZones.h"
#include <cstdio>

namespace
{
int checks = 0, failures = 0;
void check(bool condition, const char* message)
{
    ++checks;
    if (!condition)
    {
        if (failures < 10) std::fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}
float sevenBit(int velocity) { return static_cast<float>(velocity) * (1.0f / 127.0f); }
}

int main()
{
    cs80::HighResolutionVelocity velocity;
    for (int channel = 1; channel <= 16; ++channel)
    {
        for (int prefix = 0; prefix < 128; ++prefix)
        {
            for (int strike = 1; strike < 128; ++strike)
            {
                velocity.controlChange(channel, prefix);
                velocity.noteOff(channel);
                check(velocity.noteOnVelocity(channel, strike) == sevenBit(strike),
                      "release consumes prefix before next strike");

                velocity.controlChange(channel, prefix);
                const float expected = static_cast<float>(strike * 128 + prefix) / 16383.0f;
                check(velocity.noteOnVelocity(channel, strike) == expected,
                      "positive Note On retains exact existing high-resolution arithmetic");
                check(velocity.noteOnVelocity(channel, strike) == sevenBit(strike),
                      "Note On consumes prefix exactly once");
            }
        }
    }

    for (int releasedChannel = 1; releasedChannel <= 16; ++releasedChannel)
    {
        for (int channel = 1; channel <= 16; ++channel)
            velocity.controlChange(channel, channel * 7);
        velocity.noteOff(releasedChannel);
        velocity.noteOff(releasedChannel); // repeated release cannot affect other channels
        for (int channel = 1; channel <= 16; ++channel)
        {
            const float expected = channel == releasedChannel ? sevenBit(80)
                : static_cast<float>(80 * 128 + channel * 7) / 16383.0f;
            check(velocity.noteOnVelocity(channel, 80) == expected,
                  "release consumes only its own channel prefix");
        }
    }

    for (int invalidChannel : {-999, -1, 0, 17, 999})
    {
        for (int channel = 1; channel <= 16; ++channel)
            velocity.controlChange(channel, 127);
        velocity.noteOff(invalidChannel);
        for (int channel = 1; channel <= 16; ++channel)
            check(velocity.noteOnVelocity(channel, 64) == (64 * 128 + 127) / 16383.0f,
                  "invalid release channel leaves valid channels alone");
    }
    for (int channel = 1; channel <= 16; ++channel)
        velocity.controlChange(channel, 127);
    velocity.reset();
    for (int channel = 1; channel <= 16; ++channel)
    {
        velocity.noteOff(channel);
        check(velocity.noteOnVelocity(channel, 64) == sevenBit(64),
              "reset and unprefixed releases preserve normal velocity");
        velocity.controlChange(channel, 16);
        velocity.controlChange(channel, 32);
        check(velocity.noteOnVelocity(channel, 64) == (64 * 128 + 32) / 16383.0f,
              "latest prefix wins before a strike");
    }
    std::printf("High-resolution velocity: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
