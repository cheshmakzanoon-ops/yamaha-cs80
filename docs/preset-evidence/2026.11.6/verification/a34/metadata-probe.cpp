#include "Params.h"
#include <bit>
#include <cstdint>
#include <cstdio>
int main()
{
    for (int i=0;i<cs80::P_numParams;++i)
    {
        const auto& d=cs80::paramDef(i);
        std::printf("%d|%s|%s|%s|%s|%08x|%08x|%08x|%08x", i,cs80::paramId(i).c_str(),d.name,d.unit,d.group,
            std::bit_cast<std::uint32_t>(d.min),std::bit_cast<std::uint32_t>(d.max),
            std::bit_cast<std::uint32_t>(d.def),std::bit_cast<std::uint32_t>(d.skew));
        for(int k=0;k<=512;++k)
        {
            const float n=static_cast<float>(k)/512.0f;
            const float p=cs80::denormaliseParam(i,n);
            std::printf("|%08x:%08x",std::bit_cast<std::uint32_t>(p),std::bit_cast<std::uint32_t>(cs80::normaliseParam(i,p)));
        }
        std::puts("");
    }
}
