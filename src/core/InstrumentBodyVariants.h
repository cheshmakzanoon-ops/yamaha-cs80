#pragma once

namespace cs80::body
{

// Separate, append-only choices keep the published model selector's 0..19
// range and normalized host automation unchanged. A variant is captured by
// the next accepted note; incompatible model/variant pairs sound Standard.
enum Variant : int { standard = 0, xylophone = 1, steelpan = 2, choirAh = 3, choirOo = 4 };
inline constexpr int maxVariant = choirOo;

inline constexpr const char* variantName(int variant) noexcept
{
    switch (variant)
    {
        case xylophone: return "Xylophone bar";
        case steelpan: return "Steelpan shell";
        case choirAh: return "Choir ah";
        case choirOo: return "Choir oo";
        default: return "Standard";
    }
}

inline constexpr int compatibleVariant(int model, int variant) noexcept
{
    // Published model IDs: Organ=15, Vibraphone=17 and Marimba=18. Keep this small
    // metadata header independent of the audio generator; its caller can
    // statically assert those established IDs against the Model enum.
    if ((model == 18 && variant == xylophone)
        || (model == 17 && variant == steelpan)
        || (model == 15 && (variant == choirAh || variant == choirOo)))
        return variant;
    return standard;
}

} // namespace cs80::body
