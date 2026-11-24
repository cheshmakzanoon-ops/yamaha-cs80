#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
int main(int argc,char**argv){
 if(argc!=2)return 2;std::filesystem::create_directories(argv[1]);
 for(const char*name:{"Stage Keys","Soft Keys","Electric Piano I","Reed Piano"}){
  auto&bank=cs80::programLibrary();auto it=std::find_if(bank.begin(),bank.end(),[&](auto&p){return p.name==name;});if(it==bank.end())return 3;
  int note=std::string(name).find("Keys")!=std::string::npos?36:60;
  for(int mode=0;mode<3;mode++){
   auto e=std::make_unique<cs80::Engine>();e->prepare(48000);e->setParams(it->values);e->noteOn(note,.85f,1);
   std::vector<float>a(3*48000*2);bool off=false;
   for(int at=0;at<3*48000;at+=48){
    if(at==4800&&mode==1)e->controlChange(64,1,1);
    if(at==9600&&mode!=2){e->noteOff(note,1);off=true;}
    if(at==96000){if(mode==1)e->controlChange(64,0,1);if(!off)e->noteOff(note,1);}
    e->render(a.data()+at*2,48);
   }
   std::string n=name;std::replace(n.begin(),n.end(),' ','-');
   auto file=std::filesystem::path(argv[1])/(n+"-m"+std::to_string(mode)+".f32");
   std::ofstream f(file,std::ios::binary);f.write(reinterpret_cast<char*>(a.data()),a.size()*sizeof(float));if(!f)return 4;
   std::cout<<name<<','<<note<<','<<mode<<'\n';
  }
 }
}
