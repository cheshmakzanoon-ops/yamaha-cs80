// Frozen TapeEchoFx from main b5e0f1f271f0d189750233f7fde6c6cbf30b71d5.
// Keep the original control evaluation for exact-output/cache regression tests.
// Shared Eq and DelayLine primitives are unchanged by this optimisation.
#pragma once
#include "core/FxModels.h"

namespace cs80::fx::test
{
class UncachedTapeEcho
{
public:
    static constexpr double kWowHz = 0.55, kWowDepth = 0.0018, kFlutterHz = 6.1, kFlutterDepth = 0.0004;
    static constexpr double kMaxSeconds = 1.2;
    // Keep the playback head moving forward during large time increases. Leaving 10 %
    // of the nominal speed also gives the wow/flutter modulation ample headroom.
    static constexpr double kMaxDelayIncrease = 0.9; // seconds of delay per elapsed second

    static double delaySeconds(float a) noexcept { return 0.06 * std::pow(20.0, (double) clampf(a, 0.0f, 1.0f)); }
    static double tailSeconds(float b) noexcept { return 0.3 * std::pow(8.0 / 0.3, (double) clampf(b, 0.0f, 1.0f)); }

    void prepare(double fs)
    {
        sr = fs;
        for (auto& c : channels)
        {
            c.line.prepare((float) (kMaxSeconds + 0.02), (float) fs);
            c.preEmphasis = Eq::highShelf(fs, 3000.0, 6.0);
            c.deEmphasis = Eq::highShelf(fs, 3000.0, -6.0);
            c.headLoss = Eq::lowPass(fs, 7000.0, 0.6);
            c.loopHighPass = Eq::highPass(fs, 50.0, 0.6);
            c.headBump = Eq::peak(fs, 90.0, 1.0, 1.5);
        }
        glide = 1.0 - std::exp(-1.0 / (0.25 * fs));
        reset();
    }

    void reset()
    {
        for (int i = 0; i < 2; ++i)
        {
            auto& c = channels[(size_t) i];
            c.line.reset();
            for (auto* e : { &c.preEmphasis, &c.deEmphasis, &c.headLoss, &c.loopHighPass, &c.headBump })
                e->reset();
            c.playback = 0.0f;
            c.wow = i == 0 ? 0.0 : 0.37;
            c.flutter = i == 0 ? 0.0 : 0.61;
        }
        time = -1.0;
    }

    StereoSample process(float l, float r, float a, float b) noexcept
    {
        const double target = delaySeconds(a);
        // A delay tap reads at t - D(t). An unconstrained 250 ms glide can increase D
        // faster than real time (4.56 s/s for 60 ms -> 1.2 s), replaying old audio in
        // reverse. Limit only the positive slope; small changes and faster playback
        // retain the existing one-pole glide, and delay memory is never discarded.
        time = time < 0.0 ? target : time + std::min(glide * (target - time), kMaxDelayIncrease / sr);
        const float g = (float) std::min(0.95, std::pow(10.0, -3.0 * time / tailSeconds(b)));
        const float in[2] = { l, r };
        const float cross[2] = { channels[1].playback, channels[0].playback }; // ping-pong
        StereoSample out;
        for (int i = 0; i < 2; ++i)
        {
            auto& c = channels[(size_t) i];
            c.wow += kWowHz / sr;
            c.flutter += kFlutterHz / sr;
            c.wow -= std::floor(c.wow);
            c.flutter -= std::floor(c.flutter);
            const double D = time - transport(kWowDepth, kWowHz, c.wow, time) - transport(kFlutterDepth, kFlutterHz, c.flutter, time);
            float record = c.preEmphasis.process(in[i] + g * cross[i]);
            record = std::tanh(0.9f * record) / 0.9f;
            c.line.write(record);
            float y = c.line.readHermite(D * sr);
            y = c.loopHighPass.process(c.deEmphasis.process(c.headLoss.process(y)));
            c.playback = y;
            (i == 0 ? out.l : out.r) = c.headBump.process(y) * trim;
        }
        return out;
    }

    void setTrim(float linear) noexcept { trim = linear; }

private:
    // (A / w) [cos(w (t - T)) - cos(w t)] with w t = 2 pi phase.
    static double transport(double depth, double hz, double phase, double T) noexcept
    {
        const double w = 6.283185307179586 * hz;
        const double theta = 6.283185307179586 * phase;
        return depth / w * (std::cos(theta - w * T) - std::cos(theta));
    }
    struct Channel
    {
        DelayLine line;
        Eq preEmphasis, deEmphasis, headLoss, loopHighPass, headBump;
        float playback = 0.0f;
        double wow = 0.0, flutter = 0.0;
    };
    std::array<Channel, 2> channels;
    double sr = 96000.0, glide = 0.0, time = -1.0;
    float trim = 1.0f;
};

} // namespace cs80::fx::test
