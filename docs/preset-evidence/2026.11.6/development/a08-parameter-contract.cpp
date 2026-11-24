#include "Params.h"
#include <bit>
#include <cstdint>
#include <cstdio>
int main() {
for (int i=0;i<cs80::P_numParams;++i) {
const auto& d=cs80::paramDef(i);
std::printf("%d\t%s\t%s\t%s\t%s\t%08x\t%08x\t%08x\t%08x",i,cs80::paramId(i).c_str(),d.name,d.unit,d.group,
std::bit_cast<unsigned>(d.min),std::bit_cast<unsigned>(d.max),std::bit_cast<unsigned>(d.def),std::bit_cast<unsigned>(d.skew));
for(int k=0;k<=256;++k) {const float x=(float)k/256.f; const float v=cs80::denormaliseParam(i,x);
std::printf("\t%08x:%08x",std::bit_cast<unsigned>(v),std::bit_cast<unsigned>(cs80::normaliseParam(i,v)));}
std::puts("");
}
}