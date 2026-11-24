#pragma once

namespace cs80::body
{

// Separate, append-only controls preserve both established model and variant
// selectors, including their normalized automation ranges. Material is an
// optional next-note specialization: zero and incompatible pairs retain the
// existing model without changing its output.
enum Material : int
{
    defaultMaterial = 0, tubular = 1, gong = 2, temple = 3, agogo = 4,
    musicBox = 5, kalimba = 6, banjo = 7, koto = 8, harp = 9
};
inline constexpr int maxMaterial = harp;

inline constexpr const char* materialName(int material) noexcept
{
    switch (material)
    {
        case tubular: return "Tubular bell";
        case gong: return "Gong plate";
        case temple: return "Temple bell";
        case agogo: return "Agogo bell";
        case musicBox: return "Music box comb";
        case kalimba: return "Kalimba tine";
        case banjo: return "Banjo membrane";
        case koto: return "Koto bridge";
        case harp: return "Harp soundboard";
        default: return "Standard";
    }
}

inline constexpr int compatibleMaterial(int model, int material) noexcept
{
    // Frozen model IDs: Tine=2, Nylon=6, Steel=7, Bell=16, Celesta=19.
    // Keep the metadata header independent of the signal generator.
    if ((model == 16 && material >= tubular && material <= agogo)
        || (model == 19 && material == musicBox)
        || (model == 2 && material == kalimba)
        || (model == 7 && (material == banjo || material == koto))
        || (model == 6 && material == harp))
        return material;
    return defaultMaterial;
}

} // namespace cs80::body
