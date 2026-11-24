// Detect nonidentical seed response, without imposing an unsupported perceptual threshold.
#include "core/FactoryPresets.h"
#include "core/InstrumentBody.h"
#include <vector>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>
using namespace cs80;
std::vector<double> capture(const FactoryPreset&p,unsigned age){body::InstrumentBody b;b.prepare(96000,48000);b.noteOn(int(p.values[bodyParam(0,0)]),60,.7f,p.values[bodyParam(0,2)],p.values[bodyParam(0,3)],age*0x9e3779b9u,p.values[bodySizeParam(0)],int(p.values[bodyVariantParam(0)]),int(p.values[bodyMaterialParam(0)]));std::vector<double>a;for(int i=0;i<19200;++i)a.push_back(b.tick(261.625565f,p.values[bodyParam(0,2)]));return a;}
double difference(const std::vector<double>&a,const std::vector<double>&b){double aa=0,bb=0,ab=0;for(size_t i=0;i<a.size();++i){aa+=a[i]*a[i];bb+=b[i]*b[i];ab+=a[i]*b[i];}return std::sqrt(std::max(0.,2.-2.*ab/std::sqrt(aa*bb)));}
int main(){int checks=0,failures=0;std::puts("name,min_normalized_seed_difference,max_normalized_seed_difference,fixed_seed_identical");for(const char*name:{"Analog Piano","Electric Piano I","Banjo","Kalimba","Tubular Bells"})for(const auto&p:programLibrary())if(p.name==name){auto a=capture(p,1),same=capture(p,1);bool exact=a==same;double mn=10,mx=0;for(unsigned age=2;age<=8;++age){auto b=capture(p,age);double d=difference(a,b);mn=std::min(mn,d);mx=std::max(mx,d);}checks+=2;failures+=!exact;failures+=!(mn>1e-12);std::printf("%s,%.12g,%.12g,%d\n",name,mn,mx,exact);}
std::printf("CHECKS %d FAILURES %d\n",checks,failures);return failures?1:0;}
