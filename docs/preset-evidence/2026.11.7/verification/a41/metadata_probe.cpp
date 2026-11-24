// Read-only metadata/articulation accounting. This produces no audio.
#include "core/BodyArticulation.h"
#include <cstdio>

int main()
{
    using namespace cs80::body;
    std::printf("{\"models\":[");
    for (int model = off; model <= maxModel; ++model)
        std::printf("%s{\"id\":%d,\"name\":\"%s\"}", model ? "," : "", model, modelName(model));
    std::printf("],\"materials\":[");
    for (int material = defaultMaterial; material <= maxMaterial; ++material)
    {
        std::printf("%s{\"id\":%d,\"name\":\"%s\",\"effective_by_model\":[",
                    material ? "," : "", material, materialName(material));
        for (int model = off; model <= maxModel; ++model)
            std::printf("%s%d", model ? "," : "", compatibleMaterial(model, material));
        std::printf("]}");
    }
    std::printf("],\"variants\":[");
    for (int variant = standard; variant <= maxVariant; ++variant)
    {
        std::printf("%s{\"id\":%d,\"name\":\"%s\",\"effective_by_model\":[",
                    variant ? "," : "", variant, variantName(variant));
        for (int model = off; model <= maxModel; ++model)
            std::printf("%s%d", model ? "," : "", compatibleVariant(model, variant));
        std::printf("]}");
    }
    std::printf("],\"articulation\":[");
    bool first = true;
    for (int model = off; model <= maxModel; ++model)
        for (float note : {36.0f, 48.0f, 60.0f, 72.0f})
            for (float velocity : {0.25f, 0.85f})
                for (float mix : {0.0f, 0.5f, 1.0f})
                {
                    const auto a = captureArticulation(model, note, velocity, mix);
                    std::printf("%s{\"model\":%d,\"sounding_note\":%.0f,\"velocity\":%.9g,"
                                "\"body_mix\":%.9g,\"attack\":%.9g,\"decay\":%.9g,\"release\":%.9g}",
                                first ? "" : ",", model, note, velocity, mix,
                                a.attack, a.decay, a.release);
                    first = false;
                }
    std::printf("]}\n");
}
