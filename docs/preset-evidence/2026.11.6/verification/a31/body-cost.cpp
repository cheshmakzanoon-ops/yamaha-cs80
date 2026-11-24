#include "InstrumentBody.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <new>

static bool tracking = false;
static unsigned allocations = 0;
void* operator new(std::size_t n) { if (tracking) ++allocations; if (auto p=std::malloc(n)) return p; throw std::bad_alloc(); }
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }
void operator delete[](void* p,std::size_t) noexcept { std::free(p); }

using cs80::body::InstrumentBody;
constexpr int bodies=16, frames=12000, repetitions=7, starts=512;
volatile double observable=0;
struct Case {const char* name; int model,material;};
constexpr Case cases[] = {
 {"piano",1,0},{"flute",10,0},{"clarinet",11,0},{"oboe",12,0},
 {"bassoon",13,0},{"brass",14,0},{"steel",7,0},{"nylon",6,0},{"bass",8,0},
 {"tubular",16,1},{"gong",16,2},{"temple",16,3},{"agogo",16,4},
 {"music-box",19,5},{"kalimba",2,6},{"banjo",7,7},{"koto",7,8},{"harp",6,9}
};
void start(InstrumentBody& b, const Case& c, int i, int seed) {
 b.noteOn(c.model,float(48+(i%8)*4),.8f,.55f,1.2f,unsigned(12001+seed),1.0f,0
#ifdef A31_MATERIALS
 ,c.material
#endif
 );
}
double now() {return double(std::clock())/CLOCKS_PER_SEC;}
int main() {
 std::printf("# sizeof_body=%zu internal_rate=96000 output_rate=48000 bodies=%d frames=%d repetitions=%d starts=%d clock=process_cpu\n",sizeof(InstrumentBody),bodies,frames,repetitions,starts);
 std::puts("name,model,material,workload,rep,cpu_seconds,units,allocations,checksum");
 std::array<InstrumentBody,bodies> b;
 std::array<float,frames> movement{};
 for(int i=0;i<frames;++i) movement[i]=float(std::exp2(.25*std::sin(2*3.141592653589793*i/frames)));
 std::array<float,bodies> hz{};
 for(int i=0;i<bodies;++i) {b[i].prepare(96000,48000);hz[i]=float(440*std::exp2((48+(i%8)*4-69)/12.));}
 for(const auto& c:cases) {
  for(int mode=0;mode<3;++mode) for(int rep=-1;rep<repetitions;++rep) {
   for(int i=0;i<bodies;++i)start(b[i],c,i,i);
   allocations=0;tracking=true; double sum=0; const double begin=now();
   if(mode==2) {
    for(int n=0;n<starts;++n)for(int i=0;i<bodies;++i){start(b[i],c,i,n*17+i);sum+=b[i].tick(hz[i],.55f,.2f);}
   } else {
    for(int n=0;n<frames;++n)for(int i=0;i<bodies;++i)sum+=b[i].tick(mode==1?hz[i]*movement[n]:hz[i],.55f,.2f);
   }
   const double elapsed=now()-begin; tracking=false;observable+=sum;
   if(rep>=0)std::printf("%s,%d,%d,%s,%d,%.9f,%d,%u,%.12g\n",c.name,c.model,c.material,mode==2?"note-on":mode==1?"moving":"steady",rep,elapsed,(mode==2?starts:frames)*bodies,allocations,sum);
   if(allocations || !std::isfinite(sum))return 2;
  }
 }
}
