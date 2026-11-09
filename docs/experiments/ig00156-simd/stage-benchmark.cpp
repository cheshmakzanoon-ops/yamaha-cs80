#include "core/Ig00156Batch.h"
#include <chrono>
#include <cstdio>
#include <cmath>
#include <cstdint>
#include <array>
#include <cstdlib>
#include <ctime>
using namespace cs80;
volatile double sink;
int main(int argc,char**argv){
  int frames=argc>1?std::atoi(argv[1]):500000;
  const auto kernel=Ig00156Batch::selectKernel();
  std::fprintf(stderr,"kernel=%d,frames=%d\n",kernel!=nullptr,frames);
  std::puts("network,mode,repeat,batch,wall_seconds,cpu_seconds,accelerated_groups,checksum");
  for(int network=0;network<3;++network)for(int mode=0;mode<3;++mode)for(int repeat=0;repeat<5;++repeat)for(int reverse=0;reverse<2;++reverse){
    const bool batch=(repeat%2)==reverse;
    std::array<Ig00156Stage,8> s;
    for(int i=0;i<8;++i){s[i]=Ig00156Stage(network==0|| (network==2&&i%2==0)?kIg00156HighPassNetwork:kIg00156LowPassNetwork);s[i].set(300.f+900.f*i,0.75f+0.5f*i,96000.f);}
    double total=0;std::uint64_t used=0;const auto cpuBegin=std::clock();auto begin=std::chrono::steady_clock::now();
    for(int n=0;n<frames;++n){
      if(mode)for(int i=0;i<8;++i)s[i].set(mode==1?300.f+900.f*i:800.f+float((n*43+i*1297)%18000),0.5f+float((n*7+i*271)%5000)/1000.f,96000.f);
      for(int group=0;group<2;++group){
        Ig00156Batch::Stages p;Ig00156Batch::Inputs x;Ig00156Batch::Outputs y;
        for(int i=0;i<4;++i){int k=group*4+i;p[i]=&s[k];x[i]=float((n*13+k*137)%2000-1000)*0.001f;}
        if(batch)used+=processIg00156Four(p,x,y,kernel);else for(int i=0;i<4;++i)y[i]=p[i]->process(x[i]);
        for(auto a:y)total+=a.lp;
      }
    }
    double sec=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();sink=total;const double cpu=double(std::clock()-cpuBegin)/CLOCKS_PER_SEC;
    std::printf("%d,%d,%d,%d,%.9f,%.9f,%llu,%.17g\n",network,mode,repeat,batch,sec,cpu,(unsigned long long)used,total);
  }
}
