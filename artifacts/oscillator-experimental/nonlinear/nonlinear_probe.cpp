#include "ReconstructedOscillator.h"
#include "Dsp.h"
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
struct Case {const char* name;bool hp,lp;float hpc,lpc,q;};
struct Path {cs80::Svf hp,lp;cs80::Decimator dec;};
int main(){constexpr int N=16384; const Case cases[]={
 {"bypass",false,false,18,18000,.707f},
 {"hp18_lp18000_q0707",true,true,18,18000,.707f},
 {"hp1000_lp12000_q3",true,true,1000,12000,3},
 {"hp5000_lp18000_q5",true,true,5000,18000,5},
 {"hp15000_lp18000_q5",true,true,15000,18000,5},
 {"hp18_lp30000_q0707",true,true,18,30000,.707f},
 {"hponly15000_q5",true,false,15000,18000,5},
 {"lponly18000_q5",false,true,18,18000,5},
 {"hp5000_lp18000_q9_api_stress",true,true,5000,18000,9}};
 std::ofstream report("/tmp/oscillator-nonlinear-review/results.csv");report<<"fs,target,f,blend,case,residual_db,peak_difference,reference_ac_rms,reference_mean\n";
 double worst=-1e9;int index=0;
 for(double fs:{88200.,96000.})for(double target:{1046.,4186.,8372.,16744.,23000.}){
  int bin=int(std::round(target*N/fs));double f=fs*bin/N;int warm=int(std::ceil(2*fs/N))*N;
  std::vector<std::array<double,3>> ref(N),actual(N);
  std::string base="/tmp/oscillator-nonlinear-review/ref-"+std::to_string(int(fs))+"-"+std::to_string(int(target))+".bin";
  std::ifstream input(base,std::ios::binary);input.read(reinterpret_cast<char*>(ref.data()),N*sizeof(ref[0]));if(!input){std::cerr<<"badref "<<base;return 2;}
  cs80::ReconstructedOscillator o;o.prepare(fs,{});o.reset(0.,f);for(int n=0;n<warm;++n)o.tick(f);
  for(auto& row:actual){auto v=o.tick(f);row={v.sawPin5,v.pulsePin7,v.sinePin2};}
  for(int blend=1;blend<8;++blend)for(const auto& c:cases){
   std::array<Path,2> paths;for(auto& p:paths){p.hp.set(c.hpc,c.q,float(fs));p.lp.set(c.lpc,c.q,float(fs));p.dec.prepare(float(fs));}
   std::vector<std::array<float,2>> outs(N/2);
   for(int n=0;n<warm+N;++n){int k=n%N;for(int which=0;which<2;++which){auto& p=paths[which];const auto& v=which?ref[k]:actual[k];
    float x=float(((blend&1?v[0]:0.)+(blend&2?v[1]:0.))/1.75)*.9f;
    if(c.hp)x=p.hp.process(x).hp;if(c.lp)x=p.lp.process(x).lp;
    x+=float((blend&4?v[2]:0.)/1.75);p.dec.push(x);
    if(n>=warm&&(n&1))outs[(n-warm)/2][which]=p.dec.output();}}
   double err=0.,pwr=0.,mean=0.,peak=0.;for(auto y:outs){double d=double(y[0])-y[1];err+=d*d;pwr+=double(y[1])*y[1];mean+=y[1];peak=std::max(peak,std::abs(d));}
   mean/=outs.size();pwr=pwr/outs.size()-mean*mean;err/=outs.size();double db=10*std::log10(std::max(err,1e-300)/std::max(pwr,1e-300));
   report<<fs<<','<<target<<','<<f<<','<<blend<<','<<c.name<<','<<db<<','<<peak<<','<<std::sqrt(std::max(0.,pwr))<<','<<mean<<'\n';
   if(db>worst&&pwr>1e-8){worst=db;std::ofstream raw("/tmp/oscillator-nonlinear-review/worst.bin",std::ios::binary);raw.write(reinterpret_cast<char*>(outs.data()),outs.size()*sizeof(outs[0]));std::ofstream meta("/tmp/oscillator-nonlinear-review/worst.txt");meta<<fs<<' '<<target<<' '<<f<<' '<<bin<<' '<<blend<<' '<<c.name<<' '<<db<<' '<<std::sqrt(pwr)<<'\n';}
   ++index;
  }
  report.flush();std::cout<<"fs="<<fs<<" target="<<target<<" cases="<<index<<" worst_nonzero_reference_residual="<<worst<<"dB\n"<<std::flush;
 }
}
