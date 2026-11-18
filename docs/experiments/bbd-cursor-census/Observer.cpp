#include <bit>
#include <cstdint>
#include <cstdio>
struct Counts { unsigned long long passes{},taps{},same{},eligible{},bad{},hist[7]{},ordinal[7]{}; } counts[5];
static unsigned bucket=1,ordinal=0,hits=0;static const void* owner;static long long lastCursor;static std::uint32_t last[7]{};static unsigned lastCsr;static unsigned short lastCw;static bool lastOkay=false;
static void inc(unsigned long long Counts::*m){++(counts[0].*m);++(counts[bucket].*m);}
static void fault(){inc(&Counts::bad);}
extern "C" void bbd_census_block(unsigned b) noexcept {bucket=b<8?1:b<256?2:b<384?3:4;}
extern "C" void bbd_census_begin(const void* p) noexcept {if(ordinal!=0)fault();owner=p;hits=0;lastOkay=false;inc(&Counts::passes);}
extern "C" void bbd_census_tap(const void* p,long long cursor,float y0,float y1,float y2,float y3,float c1,float c2,float c3) noexcept {
 if(p!=owner||ordinal>=7){fault();return;}inc(&Counts::taps);
 unsigned csr;unsigned short cw;asm volatile("stmxcsr %0;fnstcw %1":"=m"(csr),"=m"(cw)::"memory");csr&=0xffc0U;
 std::uint32_t bits[7]={std::bit_cast<std::uint32_t>(y0),std::bit_cast<std::uint32_t>(y1),std::bit_cast<std::uint32_t>(y2),std::bit_cast<std::uint32_t>(y3),std::bit_cast<std::uint32_t>(c1),std::bit_cast<std::uint32_t>(c2),std::bit_cast<std::uint32_t>(c3)};
 bool okay=cw==0x037fU&&(csr==0x1f80U||csr==0x9fc0U),equal=true;
 for(unsigned i=0;i<7;++i){const unsigned exponent=bits[i]&0x7f800000U;okay &= exponent!=0x7f800000U&&(exponent!=0||(bits[i]&0x7fffffffU)==0);equal &= bits[i]==last[i];}
 if(ordinal&&cursor==lastCursor){inc(&Counts::same);if(okay&&lastOkay&&equal&&csr==lastCsr&&cw==lastCw){inc(&Counts::eligible);++hits;++counts[0].ordinal[ordinal];++counts[bucket].ordinal[ordinal];}}
 for(unsigned i=0;i<7;++i)last[i]=bits[i];lastCursor=cursor;lastOkay=okay;lastCsr=csr;lastCw=cw;
 if(++ordinal==7){++counts[0].hist[hits];++counts[bucket].hist[hits];ordinal=0;}
}
extern "C" void bbd_census_finish() noexcept {
 if(ordinal)fault();const char* names[]={"all","startup","middle","steady","continuation"};
 std::printf("{\"buckets\":{");
 for(unsigned b=0;b<5;++b){const auto& c=counts[b];std::printf("%s\"%s\":{\"passes\":%llu,\"taps\":%llu,\"adjacent_same_cursor\":%llu,\"eligible_rebuild_reuses\":%llu,\"bad\":%llu,\"pass_hit_histogram\":[",b?",":"",names[b],c.passes,c.taps,c.same,c.eligible,c.bad);for(unsigned k=0;k<7;++k)std::printf("%s%llu",k?",":"",c.hist[k]);std::printf("],\"per_tap_hits\":[");for(unsigned k=0;k<7;++k)std::printf("%s%llu",k?",":"",c.ordinal[k]);std::printf("]}");}std::printf("},\"shared_tap_passes_not_per_chain\":true,\"downstream_chains\":2}\n");
}
