// Engine-private: no retained result or physical state. The cutoff solver's
// same-invocation certificate says all eight complete drive representations
// are equal within each rank; the two ranks need not match each other.
#if defined(__linux__) && defined(__x86_64__) && defined(__GNUC__) && __GNUC__==14 && __GNUC_MINOR__==2 && defined(__SSE2_MATH__) && !defined(__clang__) \
    && !defined(__INTEL_COMPILER) && !defined(__INTEL_LLVM_COMPILER) && !defined(__NVCOMPILER) && !defined(__PGI) && !defined(__ibmxl__) && !defined(_MSC_VER) \
    && defined(__OPTIMIZE__) && !defined(__OPTIMIZE_SIZE__) && !defined(__SSE3__) && !defined(__AVX__) && !defined(__ROUNDING_MATH__) && !defined(__SUPPORT_SNAN__) \
    && !defined(__FMA__) && !defined(__FMA4__) && !defined(__AVX512F__) && !defined(__FP_FAST_FMA) && !defined(__FP_FAST_FMAF) && !defined(__FAST_MATH__) \
    && !defined(__ASSOCIATIVE_MATH__) && !defined(__RECIPROCAL_MATH__) && !defined(__NO_SIGNED_ZEROS__) && !defined(__NO_TRAPPING_MATH__) \
    && (!defined(__FINITE_MATH_ONLY__) || !__FINITE_MATH_ONLY__) && defined(__FLT_EVAL_METHOD__) && __FLT_EVAL_METHOD__==0 \
    && !defined(CS80_DISABLE_UNIFORM_CUTOFF) && !defined(CS80_DISABLE_UNIFORM_RECEIVERS)
#define CS80_DETAIL_UNIFORM_RECEIVERS 1
namespace
{
static inline bool uniformReceiverControls() noexcept
{
    // Both cutoff gates just checked MXCSR. Require the ordinary masked,
    // round-to-nearest controls of both units at the transaction itself.
    unsigned short cw;
    asm volatile ("fnstcw %0" : "=m"(cw) :: "memory");
    return (_mm_getcsr() & 0x7f80u) == 0x1f80u && (cw & 0x0c3fu) == 0x003fu;
}

[[gnu::noipa]] bool evaluateUniformReceivers(const std::array<source_factory::RankBoundary, 2>& boundaries,
                                            double masterRes, const source_factory::BothRankDrives& drives,
                                            source_factory::SharedReadout& result) noexcept
{
    namespace sf = source_factory;
    if (!std::isfinite(masterRes) || !sf::valid(boundaries[0]) || !sf::valid(boundaries[1]))
        return false;

    // One solved value per rank feeds eight identical receivers. Stage
    // only those distinct values so a rejection on rank II still preserves
    // the caller's prior complete result. Expand them at final publication.
    struct UniformRank {
        std::array<double, 2> qBus, volts, q;
        double levelBus, pin, gain;
    };
    std::array<UniformRank, 2> ranks;
    std::uint32_t saturatedVoices = 0;
    const double r = std::clamp(masterRes, 0.0, 1.0);
    const bool masterSaturated = r != masterRes;
    const double masterVolts = 10.0 * (1.0 - r);
    for (std::size_t rank = 0; rank < ranks.size(); ++rank)
    {
        auto& next = ranks[rank];
        const auto& b = boundaries[rank];
        sf::VoiceDrives drive;
        bool saturated = false;
        if (!sf::sanitizeDrive(drives[rank][0], drive, saturated))
            return false;
        if (saturated)
            saturatedVoices |= ((1u << sf::voiceCount) - 1) << (rank * sf::voiceCount);

        const double touch = b.initialLevelVolts * drive.velocity + b.afterLevelVolts * drive.pressure;
        double sum = 0.0;
        for (int voice = 0; voice < sf::voiceCount; ++voice)
            sum += touch; // Preserve the scalar sequence, including signed zero.

        next.qBus = {sf::loadedQBus(b.qHp, masterVolts), sf::loadedQBus(b.qLp, masterVolts)};
        next.levelBus = sf::loadedLevelBus(b.level, sum);
        for (std::size_t side = 0; side < 2; ++side)
        {
            next.volts[side] = 0.5 * (next.qBus[side] + masterVolts);
            next.q[side] = sf::qFromVolts(next.volts[side]);
        }
        next.pin = sf::levelPinPerLoadedBus * next.levelBus + sf::levelPinPerTouch * touch;
        next.gain = next.pin / sf::levelUnitVolts;
    }
    for (std::size_t rank = 0; rank < ranks.size(); ++rank)
    {
        const auto& next = ranks[rank];
        result.qBusVolts[rank] = next.qBus;
        result.levelBusVolts[rank] = next.levelBus;
        result.qVolts[rank].fill(next.volts);
        result.q[rank].fill(next.q);
        result.levelPinVolts[rank].fill(next.pin);
        result.levelGain[rank].fill(next.gain);
    }
    result.saturatedVoices = saturatedVoices;
    result.masterResVolts = masterVolts;
    result.masterResSaturated = masterSaturated;
    return true;
}
} // namespace
#endif
