// Matched-header full Engine probe: actual Agogo parameters except explicit HQ.
#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>
using namespace cs80;
int main(int argc,char**argv) {
 if(argc!=2)return 2;
 std::filesystem::create_directories(argv[1]);
 for(const char* name:{"Agogo","Gong"}) {
  const auto& bank=curatedLibrary();auto it=std::find_if(bank.begin(),bank.end(),[&](auto&p){return p.name==name;});
  if(it==bank.end())return 3;
  for(int mode=0;mode<3;++mode) {
   if(mode<2 && std::string(name)!="Agogo")continue;
   auto p=it->values;if(mode==1)p[P_oversampling]=2;
   auto e=std::make_unique<Engine>();e->prepare(48000);e->setParams(p);
   if(mode==2)for(int n:{48,55,60,64,67,72})e->noteOn(n,1,1);else e->noteOn(60,.85f,1);
   std::vector<float> pcm(2*48000*3);
   for(int at=0;at<48000*3;at+=128)e->render(pcm.data()+2*at,128);
   const auto path=std::filesystem::path(argv[1])/(std::string(name)+"_"+std::to_string(mode)+".f32");
   std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<char*>(pcm.data()),pcm.size()*sizeof(float));
   std::cout<<name<<","<<mode<<",oversampling="<<e->getOversamplingFactor()<<"\n";
  }
 }
}
