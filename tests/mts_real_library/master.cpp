#include "Master/libMTSMaster.h"
#include <iostream>
#include <cmath>
#include <string>
bool owned=false;
void setup(double offset) {
 MTS_ClearNoteFilter();
 double f[128]; for(int n=0;n<128;n++) f[n]=440*std::exp2((n-69+offset)/12.);
 MTS_SetNoteTunings(f); MTS_SetScaleName("Real IPC\nqualification");
 MTS_FilterNote(true,60,-1);
 for(int c=0;c<16;c++) { MTS_ClearNoteFilterMultiChannel(c); MTS_SetMultiChannel(true,c); for(int n=0;n<128;n++) f[n]=440*std::exp2((n-69+offset+1+c*.1)/12.); MTS_SetMultiChannelNoteTunings(f,c); }
 MTS_FilterNoteMultiChannel(true,61,3);
}
int main(){
 if(!MTS_HasIPC() || !MTS_CanRegisterMaster() || MTS_GetNumClients()!=0) { std::cout<<"BLOCKED IPC="<<MTS_HasIPC()<<" canRegister="<<MTS_CanRegisterMaster()<<" clients="<<MTS_GetNumClients()<<std::endl;return 2; }
 MTS_RegisterMaster();owned=true; setup(.25);std::cout<<"READY IPC=1 clients=0"<<std::endl;
 std::string s; while(std::getline(std::cin,s)) {
  if(s=="quit") break;
  if(s=="clients") {std::cout<<"CLIENTS "<<MTS_GetNumClients()<<std::endl;continue;}
  if(s=="update") setup(.5);
  else if(s=="badglobal") MTS_SetNoteTuning(-1,127);
  else if(s=="filterbadglobal") MTS_FilterNote(true,127,-1);
  else if(s=="badchannel") MTS_SetMultiChannelNoteTuning(-1,127,15);
  else if(s=="filterbadchannel") MTS_FilterNoteMultiChannel(true,127,15);
  else if(s=="disconnect") {MTS_DeregisterMaster();owned=false;}
  else if(s=="reconnect") {if(!MTS_CanRegisterMaster() || MTS_GetNumClients()!=1) {std::cout<<"BLOCKED reconnect"<<std::endl;break;} MTS_RegisterMaster();owned=true;setup(.75);}
  else {std::cout<<"UNKNOWN"<<std::endl;break;}
  std::cout<<"OK "<<s<<std::endl;
 }
 if(owned) MTS_DeregisterMaster();
 std::cout<<"EXIT"<<std::endl;
}
