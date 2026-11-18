#pragma once
namespace cs80 {
class ReconstructedOscillator;
struct WscControls;
extern "C" void wsc_census_sample_begin() noexcept;
extern "C" void wsc_census_sample_end() noexcept;
extern "C" void wsc_census_lane(int) noexcept;
extern "C" void wsc_census_pre(const ReconstructedOscillator&, double, const WscControls&) noexcept;
extern "C" void wsc_census_flag(unsigned, unsigned) noexcept;
extern "C" void wsc_census_span(double, double, bool) noexcept;
extern "C" void wsc_census_post(const ReconstructedOscillator&) noexcept;
}
