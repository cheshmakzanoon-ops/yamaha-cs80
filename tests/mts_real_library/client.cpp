#include "plugin/MtsEspClient.h"
#include <iostream>
#include <string>
#include <cmath>
#include <stdexcept>
using namespace cs80;
void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}
bool equal(const tuning::TuningTable&a,const tuning::TuningTable&b){return a.pitch==b.pitch&&a.mapped==b.mapped;}
bool equal(const MtsEspClient::ChannelTables&a,const MtsEspClient::ChannelTables&b){for(int c=0;c<16;c++)if(!equal(a[c],b[c]))return false;return true;}
bool near(float a,double b){return std::abs(a-b)<.00003;}
int main(){int failed=0; {MtsEspClient client; tuning::TuningTable table; MtsEspClient::ChannelTables bank;std::cout<<"READY"<<std::endl;
 std::string s; while(std::getline(std::cin,s)) {if(s=="quit")break;
 try{
 if(s=="initial" || s=="update" || s=="reconnect") {double off=s=="initial"?.25:s=="update"?.5:.75;
 check(client.hasMaster(),"master visible");check(client.masterScaleName()=="Real IPC qualification","name sanitization");check(client.fillTable(table),"global fill");
 for(int n=0;n<128;n++){check(near(table.pitch[n],n+off),"global pitch");check(table.mapped[n]==(n!=60),"global filter");}
 check(client.fillChannelTables(bank,true),"independent fill");for(auto&t:bank)check(equal(t,table),"independent global match");
 check(client.fillChannelTables(bank,false),"channel fill after global");for(int c=0;c<16;c++)for(int n=0;n<128;n++){check(near(bank[c].pitch[n],n+off+1+c*.1),"channel pitch");check(bank[c].mapped[n]==!(c==3&&n==61),"channel filter");}
 }
 else if(s=="badglobal"){auto old=table;check(!client.fillTable(table),"invalid global rejected");check(equal(old,table),"global unchanged");auto before=bank;check(!client.fillChannelTables(bank,true),"invalid independent rejected");check(equal(before,bank),"independent bank unchanged");}
 else if(s=="filterbadglobal"){check(client.fillTable(table),"filtered bad global accepted");check(table.pitch[127]==127&&!table.mapped[127],"filtered bad global safe");}
 else if(s=="badchannel"){auto before=bank;check(!client.fillChannelTables(bank,false),"invalid late channel rejected");check(equal(before,bank),"entire bank unchanged");}
 else if(s=="filterbadchannel"){check(client.fillChannelTables(bank,false),"filtered bad channel accepted");check(bank[15].pitch[127]==127&&!bank[15].mapped[127],"filtered bad channel safe");}
 else if(s=="disconnect"){check(!client.hasMaster(),"disconnect visible");check(client.masterScaleName().empty(),"disconnected name empty");auto old=table;auto before=bank;check(!client.fillTable(table)&&equal(old,table),"disconnected global unchanged");check(!client.fillChannelTables(bank,false)&&equal(before,bank),"disconnected bank unchanged");}
 else check(false,"unknown command");
 std::cout<<"PASS "<<s<<std::endl;
 }catch(const std::exception&e){failed=1;std::cout<<"FAIL "<<s<<" "<<e.what()<<std::endl;}
 }
 } std::cout<<"EXIT"<<std::endl;return failed;}
