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

    // As in the public scalar oracle, publish only after both ranks finish.
    sf::SharedReadout next;
    const double r = std::clamp(masterRes, 0.0, 1.0);
    next.masterResSaturated = r != masterRes;
    next.masterResVolts = 10.0 * (1.0 - r);
    for (std::size_t rank = 0; rank < 2; ++rank)
    {
        const auto& b = boundaries[rank];
        sf::VoiceDrives drive;
        bool saturated = false;
        if (!sf::sanitizeDrive(drives[rank][0], drive, saturated))
            return false;
        if (saturated)
            next.saturatedVoices |= ((1u << sf::voiceCount) - 1) << (rank * sf::voiceCount);

        const double touch = b.initialLevelVolts * drive.velocity + b.afterLevelVolts * drive.pressure;
        double sum = 0.0;
        for (int voice = 0; voice < sf::voiceCount; ++voice)
            sum += touch; // Preserve the scalar sequence, including signed zero.

        next.qBusVolts[rank] = {sf::loadedQBus(b.qHp, next.masterResVolts),
                                sf::loadedQBus(b.qLp, next.masterResVolts)};
        next.levelBusVolts[rank] = sf::loadedLevelBus(b.level, sum);
        std::array<double, 2> volts, q;
        for (std::size_t side = 0; side < 2; ++side)
        {
            volts[side] = 0.5 * (next.qBusVolts[rank][side] + next.masterResVolts);
            q[side] = sf::qFromVolts(volts[side]);
        }
        const double pin = sf::levelPinPerLoadedBus * next.levelBusVolts[rank] + sf::levelPinPerTouch * touch;
        const double gain = pin / sf::levelUnitVolts;
        next.qVolts[rank].fill(volts);
        next.q[rank].fill(q);
        next.levelPinVolts[rank].fill(pin);
        next.levelGain[rank].fill(gain);
    }
    result = next;
    return true;
}
} // namespace
#endif
