namespace vector_witness {
struct Group {
 std::uint64_t attempts=0,successes=0,rejections=0,known=0,scalar=0,bad=0,ownerChecks=0;
 std::array<std::uint64_t,16> finishMasks{};
 std::array<std::uint64_t,4> bindings{};
};
struct Span {
 std::uint64_t renders=0,frames=0,selections=0,selected=0,bad=0;
 std::array<std::uint64_t,4> preparations{},valid{};
};
std::array<std::array<Group,4>,2> groups{};
std::array<Span,2> spans{};
const cs80::Engine* engine=nullptr;
unsigned group=0,remaining=0,knownMask=0;
bool attempted=false;
const cs80::Engine* renderEngine=nullptr;
unsigned renderPrepared=0,renderSelected=0;
void reset() noexcept {groups={};spans={};engine=nullptr;group=remaining=knownMask=0;attempted=false;renderEngine=nullptr;renderPrepared=renderSelected=0;}
}
extern "C" void wsc_vector_context(const cs80::Engine*e,unsigned group) noexcept {
 using namespace vector_witness;if(census::phase<0)return;
 if(group>3){++groups[unsigned(census::phase)][0].bad;return;}
 if(remaining)++groups[unsigned(census::phase)][group].bad;
 engine=e;vector_witness::group=group;
}
extern "C" void wsc_vector_event(unsigned event,unsigned mask,unsigned binding) noexcept {
 using namespace vector_witness;if(census::phase<0)return;auto&g=groups[unsigned(census::phase)][group];
 if(!engine||mask==0||mask>15||binding>3){++g.bad;return;}
 if(event==0){if(remaining)++g.bad;remaining=mask;knownMask=0;attempted=false;++g.finishMasks[mask];++g.bindings[binding];}
 else if(event==1){if(mask!=15||binding!=3||remaining!=mask||attempted)++g.bad;++g.attempts;attempted=true;}
 else if(event==2){if(!attempted||mask!=15||remaining!=mask)++g.bad;++g.successes;knownMask=mask;}
 else if(event==3){if(!attempted||mask!=15||remaining!=mask)++g.bad;++g.rejections;}
 else ++g.bad;
}
extern "C" void wsc_vector_owners(const cs80::ReconstructedOscillator*a,const cs80::ReconstructedOscillator*b,const cs80::ReconstructedOscillator*c,const cs80::ReconstructedOscillator*d,const cs80::ReconstructedOscillator*p,const cs80::ReconstructedOscillator*q,const cs80::ReconstructedOscillator*r,const cs80::ReconstructedOscillator*s) noexcept {
 using namespace vector_witness;if(census::phase<0)return;auto&g=groups[unsigned(census::phase)][group];++g.ownerChecks;
 if(!engine){++g.bad;return;}const auto&v=inspect::get(*engine,inspect::Voices{});
 const std::array<const void*,4>actual{a,b,c,d},prepared{p,q,r,s};
 for(unsigned i=0;i<4;++i){const auto rank=group*4+i;const void*expected=&v[rank/2].ranks[rank%2].wsc;if(actual[i]!=expected||prepared[i]!=expected)++g.bad;}
}
extern "C" void wsc_vector_commit(unsigned ordinal,const void*owner,unsigned known) noexcept {
 using namespace vector_witness;if(census::phase<0)return;auto&g=groups[unsigned(census::phase)][group];
 if(!engine||ordinal>3||known>1){++g.bad;return;}const auto bit=1u<<ordinal;const auto&v=inspect::get(*engine,inspect::Voices{});const auto rank=group*4+ordinal;
 if(!(remaining&bit)||bool(knownMask&bit)!=bool(known)||owner!=&v[rank/2].ranks[rank%2].wsc)++g.bad;
 if(known)++g.known;else ++g.scalar;remaining&=~bit;knownMask&=~bit;
}
extern "C" void wsc_vector_render(const cs80::Engine*e,int frames) noexcept {
 using namespace vector_witness;if(census::phase<0)return;auto&s=spans[unsigned(census::phase)];
 if(remaining||frames<=0)++s.bad;++s.renders;s.frames+=unsigned(frames);renderEngine=e;renderPrepared=renderSelected=0;
}
extern "C" void wsc_vector_selection(const cs80::Engine*e,unsigned selected) noexcept {
 using namespace vector_witness;if(census::phase<0)return;auto&s=spans[unsigned(census::phase)];
 if(e!=renderEngine||renderSelected||selected>1)++s.bad;renderSelected=1;++s.selections;s.selected+=selected;
}
extern "C" void wsc_vector_prepare(const cs80::Engine*e,unsigned group,unsigned valid,const void*a,const void*b,const void*c,const void*d) noexcept {
 using namespace vector_witness;if(census::phase<0)return;auto&s=spans[unsigned(census::phase)];
 if(e!=renderEngine||!renderSelected||group>3||valid>1){++s.bad;return;}
 if(renderPrepared&(1u<<group))++s.bad;renderPrepared|=1u<<group;++s.preparations[group];s.valid[group]+=valid;
 const auto&v=inspect::get(*e,inspect::Voices{});const std::array<const void*,4>owners{a,b,c,d};
 for(unsigned i=0;i<4;++i){const auto rank=group*4+i;if(owners[i]!=&v[rank/2].ranks[rank%2].wsc)++s.bad;}
}
void vectorReport(){
 using namespace vector_witness;
 for(unsigned w=0;w<2;++w){
  const auto&s=spans[w];std::printf("VECTOR_SPAN,%s,%llu,%llu,%llu,%llu,%llu",w?"late":"onset",(unsigned long long)s.renders,(unsigned long long)s.frames,(unsigned long long)s.selections,(unsigned long long)s.selected,(unsigned long long)s.bad);
  for(auto n:s.preparations)std::printf(",%llu",(unsigned long long)n);for(auto n:s.valid)std::printf(",%llu",(unsigned long long)n);std::puts("");
  for(unsigned i=0;i<4;++i){const auto&g=groups[w][i];std::printf("VECTOR_GROUP,%s,%u,%llu,%llu,%llu,%llu,%llu,%llu,%llu",w?"late":"onset",i,(unsigned long long)g.attempts,(unsigned long long)g.successes,(unsigned long long)g.rejections,(unsigned long long)g.known,(unsigned long long)g.scalar,(unsigned long long)g.bad,(unsigned long long)g.ownerChecks);
   for(auto n:g.finishMasks)std::printf(",%llu",(unsigned long long)n);for(auto n:g.bindings)std::printf(",%llu",(unsigned long long)n);std::puts("");}
 }
}
