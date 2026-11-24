// Attacks subtract a cloned same-state no-strike continuation. Raw gaps are bounded, not claimed silent.
#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <vector>
using namespace cs80;
constexpr int rate=48000;
struct Result {std::vector<std::vector<float>> attacks;std::vector<float> all;double quiet=0,peak=0;bool finite=true;int activeAfterPanic=0;};
const FactoryPreset& find(const char* name){for(const auto& p:programLibrary())if(p.name==name)return p;throw std::runtime_error(name);}
Result run(const FactoryPreset& p,bool irregular){auto e=std::make_unique<Engine>();e->prepare(rate);e->setParams(p.values);Result r;int part=0;const int blocks[]={17,53,251,32,127};
 auto render=[&](int count){std::vector<float> a(2*count);for(int at=0;at<count;){int n=std::min(count-at,irregular?blocks[(part++)%5]:128);e->render(a.data()+2*at,n);at+=n;}for(float v:a){r.peak=std::max(r.peak,std::abs(double(v)));r.finite=r.finite&&std::isfinite(v);}r.all.insert(r.all.end(),a.begin(),a.end());return a;};
 render(4800);
 for(int strike=0;strike<8;++strike){auto silent=std::make_unique<Engine>(*e);std::vector<float> continuation(38400);for(int at=0;at<19200;at+=128)silent->render(continuation.data()+2*at,std::min(128,19200-at));e->noteOn(60,.7f,1);auto held=render(19200);for(size_t i=0;i<held.size();++i)held[i]-=continuation[i];r.attacks.emplace_back(held.begin()+2*240,held.begin()+2*4800);e->noteOff(60,1);render(7200);e->allNotesOff(false);auto gap=render(4800);r.activeAfterPanic=std::max(r.activeAfterPanic,e->activeVoices());for(auto i=gap.end()-1024;i!=gap.end();++i)r.quiet=std::max(r.quiet,std::abs(double(*i)));}return r;}
double distance(const std::vector<float>&a,const std::vector<float>&b){double aa=0,bb=0,ab=0;for(size_t i=0;i<a.size();++i){aa+=double(a[i])*a[i];bb+=double(b[i])*b[i];ab+=double(a[i])*b[i];}return std::sqrt(std::max(0.,2.-2.*ab/std::sqrt(aa*bb)));}
int main(){int failures=0,checks=0;auto check=[&](bool good){++checks;if(!good)++failures;};std::puts("name,min_normalized_onset_distance,max_normalized_onset_distance,max_gap_sample,max_sample,replay_max_difference,partition_max_difference,active_after_panic");
 for(const char* name:{"Analog Piano","Electric Piano I","Banjo","Kalimba","Tubular Bells"}){auto a=run(find(name),false),b=run(find(name),false),c=run(find(name),true);double minimum=10,maximum=0,replay=0,partition=0;for(size_t i=1;i<a.attacks.size();++i){auto d=distance(a.attacks[i-1],a.attacks[i]);minimum=std::min(minimum,d);maximum=std::max(maximum,d);}for(size_t i=0;i<a.all.size();++i){replay=std::max(replay,std::abs(double(a.all[i])-b.all[i]));partition=std::max(partition,std::abs(double(a.all[i])-c.all[i]));}check(a.finite&&b.finite&&c.finite);check(a.peak<1);check(a.quiet<1e-4);check(a.activeAfterPanic==0);check(replay==0);check(partition<1e-7);check(minimum>.001);std::printf("%s,%.12g,%.12g,%.12g,%.12g,%.12g,%.12g,%d\n",name,minimum,maximum,a.quiet,a.peak,replay,partition,a.activeAfterPanic);}
 std::printf("CHECKS %d FAILURES %d\n",checks,failures);return failures?1:0;}
