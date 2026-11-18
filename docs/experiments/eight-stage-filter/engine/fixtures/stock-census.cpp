#include "fixture.inc"
using namespace cs80;
struct Count {uint64_t calls{}, accepted{}, pairs{}, admitted{}, distinct{}, inputsDiffer{}, futureNoop{}, badOrder{};};
Count count; Engine* current; Ig00156Batch::Kernel realKernel;
std::array<Ig00156Stage*,4> prev{};bool prevOK{},prevNoop{};int prevType=-1,prevFirst=-1;
uint32_t prevInput{};bool prevDiff{};
uint32_t bits(float f){return std::bit_cast<uint32_t>(f);}
uint32_t qkey(float f){auto u=bits(f);if((u>>31)||u<bits(.35f))return bits(.35f);return u>bits(9.f)?bits(9.f):u;}
int find(Ig00156Stage* p,int& type){for(int i=0;i<16;++i){auto&r=current->voices[i/2].ranks[i%2];if(p==&r.hpStage){type=0;return i;}if(p==&r.lpStage){type=1;return i;}}return -1;}
bool futureNoop(int type,int first){for(int i=first+4;i<first+8;++i){auto&r=current->voices[i/2].ranks[i%2];auto&s=type==0?r.hpStage:r.lpStage;auto&c=r.idle;float cut=type==0?c.hpCutoff:c.lpCutoff,q=type==0?c.hpQ:c.lpQ,clock=type==0?r.idleRefs.hpRate:r.idleRefs.lpRate;if(bits(cut)!=bits(s.lastCutoff)||qkey(q)!=bits(s.lastQ)||bits(s.lastRate)!=bits(96000.f)||s.lastUnwarped||bits(clock)!=bits(96000.f))return false;}return true;}
bool observe(const Ig00156Batch::Stages& s,const Ig00156Batch::Inputs&i,Ig00156Batch::Outputs&o) noexcept {
 ++count.calls;int type=-1,first=find(s[0],type);bool order=first>=0;
 for(int n=0;n<4;++n){int t=-1;order &=find(s[n],t)==first+n&&t==type;}
 if(!order)++count.badOrder;
 bool noOp=order&&first%8==0&&futureNoop(type,first);bool diff=false;for(int n=1;n<4;++n)diff|=bits(i[n])!=bits(i[0]);
 const bool ok=realKernel(s,i,o);count.accepted+=ok;
 if(order&&first%8==4){++count.pairs;bool dis=true;for(auto*a:prev)for(auto*b:s)dis&=a!=b;bool adjacent=prevType==type&&prevFirst==first-4;count.distinct+=adjacent&&dis;count.admitted+=adjacent&&dis&&prevOK&&ok;count.inputsDiffer+=adjacent&&(diff||prevDiff||prevInput!=bits(i[0]));count.futureNoop+=adjacent&&prevNoop;}
 prev=s;prevOK=ok;prevNoop=noOp;prevType=type;prevFirst=first;prevInput=bits(i[0]);prevDiff=diff;return ok;
}
int main(int argc,char**argv){if(argc!=4)return 2;ScopedDenormals fp;int scene=std::atoi(argv[1]);bool observed=std::atoi(argv[2]);lane_cost::Work w(scene);current=w.engine.get();realKernel=current->filterBatchKernel;if(!realKernel)return 3;if(observed)current->filterBatchKernel=observe;FILE*f=std::fopen(argv[3],"wb");if(!f)return 4;
 for(unsigned b=0;b<1041;++b){if(b==1024){count={};prevType=-1;prevFirst=-1;}auto before=lane_cost::fp();w.step(b);auto after=lane_cost::fp();std::fwrite(w.pcm.data(),sizeof(float),w.pcm.size(),f);for(auto x:{before.csr,after.csr,unsigned(before.cw),unsigned(after.cw),unsigned(before.sw),unsigned(after.sw),unsigned(before.error),unsigned(after.error)})std::fwrite(&x,4,1,f);if(b==1039)std::printf("scene %d calls %llu accepted %llu pairs %llu admitted %llu distinct %llu inputs_differ %llu future_noop %llu bad_order %llu\n",scene,(unsigned long long)count.calls,(unsigned long long)count.accepted,(unsigned long long)count.pairs,(unsigned long long)count.admitted,(unsigned long long)count.distinct,(unsigned long long)count.inputsDiffer,(unsigned long long)count.futureNoop,(unsigned long long)count.badOrder);}
 std::fclose(f);
}
