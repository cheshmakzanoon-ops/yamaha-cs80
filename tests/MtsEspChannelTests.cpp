// Actual adapter, precomputed deterministic external API. No live-service claim.
#include "plugin/MtsEspClient.h"
#include "vendor/mts-esp/Client/libMTSClient.h"
#include <cmath>
#include <cfenv>
#include <bit>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <chrono>
struct MTSClient { bool channelFiltering=false; };
namespace {
using Bank=cs80::MtsEspClient::ChannelTables;
bool master=true, distinct=true;
int badChannel=-2,badNote=-1,disconnectAt=-1,frequencyCalls=0,filterCalls=0;
bool filterBad=false;
double badHz=0;
double frequencies[16][128]{};
int checks=0,failures=0;
void check(bool ok,const char* text) {++checks;if(!ok){++failures;std::cerr<<"FAIL "<<text<<'\n';}}
bool equal(const Bank&a,const Bank&b) {for(int c=0;c<16;++c)if(std::memcmp(a[c].pitch.data(),b[c].pitch.data(),128*sizeof(float)) || std::memcmp(a[c].mapped.data(),b[c].mapped.data(),128*sizeof(bool)))return false;return true;}
void resetCounts(){frequencyCalls=filterCalls=0;}
}
extern "C" {
MTSClient* MTS_RegisterClient(){return new MTSClient;}
void MTS_DeregisterClient(MTSClient*c){delete c;}
bool MTS_HasMaster(MTSClient*){return master;}
double MTS_NoteToFrequency(MTSClient*c,char note,signed char channel){
 const int n=static_cast<unsigned char>(note);++frequencyCalls;
 if(frequencyCalls==disconnectAt)master=false;
 if(channel==badChannel && n==badNote)return badHz;
 return frequencies[distinct && channel>=0 && c->channelFiltering ? channel : 0][n];
}
bool MTS_ShouldFilterNote(MTSClient*c,char note,signed char channel){
 ++filterCalls;c->channelFiltering=channel>=0;
 return (filterBad && channel==badChannel && note==badNote) || (note==23 && (channel==1 || channel==15));
}
}
int main(int argc,char**argv){
 for(int c=0;c<16;++c)for(int n=0;n<128;++n)frequencies[c][n]=440.*std::exp2((n+(c+1)*.03125-69.)/12.);
 cs80::MtsEspClient client;Bank bank;
 if(argc==2 && std::strcmp(argv[1],"--benchmark")==0){
  cs80::tuning::TuningTable global; unsigned witness=0;
  for(int mode=0;mode<3;++mode)for(int trial=0;trial<9;++trial)for(int order=0;order<2;++order){
   const bool channel=(order+trial)%2;distinct=mode==2;
   const auto start=std::chrono::steady_clock::now();
   for(int i=0;i<1000;++i) witness+=channel ? client.fillChannelTables(bank,mode==0) : client.fillTable(global);
   const auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count();
   std::cout<<mode<<','<<trial<<','<<order<<','<<channel<<','<<ns<<'\n';
  }
  return witness==54000 ? 0:1;
 }
 auto before=bank;master=false;
 check(!client.fillChannelTables(bank) && equal(bank,before),"no master preserves bank");master=true;
 cs80::tuning::TuningTable global;
 check(client.fillTable(global),"global query primes prior API mode");resetCounts();
 check(client.fillChannelTables(bank),"global to channel transition accepted");
 check(frequencyCalls==2048 && filterCalls==2049,"full bank query counts include one capability primer");
 for(int c=0;c<16;++c)for(int n=0;n<128;++n){
  check(std::abs(bank[c].pitch[n]-(n+(c+1)*.03125f))<1e-4f,"each channel retains its own pitch");
  check(std::bit_cast<std::uint32_t>(bank[c].pitch[n])==std::bit_cast<std::uint32_t>(cs80::tuning::TuningTable::pitchFromHz(frequencies[c][n])),"pitch exactly equals direct conversion");
  check(bank[c].mapped[n]!=(n==23 && (c==1 || c==15)),"each channel retains its own filtering");
 }
 before=bank;
 for(int c : {0,7,15})for(int n : {0,64,127})for(double hz : {0.,-1.,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity(),1e-300,1e300}){
  badChannel=c;badNote=n;badHz=hz;
  check(!client.fillChannelTables(bank) && equal(bank,before),"invalid mapped frequency rejects complete bank");
 }
 badChannel=0;badNote=64;badHz=std::numeric_limits<double>::quiet_NaN();filterBad=true;
 check(client.fillChannelTables(bank),"filtered invalid frequency accepted with finite fallback");
 check(bank[0].pitch[64]==64.f && !bank[0].mapped[64] && std::abs(bank[1].pitch[64]-64.0625f)<1e-4f,"invalid filtered channel does not poison next conversion");
 badChannel=-2;filterBad=false;before=bank;resetCounts();disconnectAt=2048;
 check(!client.fillChannelTables(bank) && equal(bank,before),"late observed disconnect rejects all staged channels");
 master=true;disconnectAt=-1;check(client.fillChannelTables(bank),"reconnect restores bank");
 distinct=false;check(client.fillChannelTables(bank),"equal frequencies still query independent mappings");
 check(bank[0].pitch[23]==bank[1].pitch[23] && bank[0].mapped[23] && !bank[1].mapped[23],"conversion reuse never reuses filter flags");
 distinct=true;resetCounts();check(client.fillChannelTables(bank,true),"independent MPE mode accepted");
 check(frequencyCalls==128 && filterCalls==128,"MPE retains global query cost");
 for(int c=1;c<16;++c)check(bank[c].pitch==bank[0].pitch && bank[c].mapped==bank[0].mapped,"global result broadcasts to every channel");
 check(client.fillChannelTables(bank) && std::abs(bank[15].pitch[64]-64.5f)<1e-4f,"return from MPE reestablishes channel-aware capability");
 const int originalRounding=std::fegetround();
 for(int rounding : {FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO}) {
  check(std::fesetround(rounding)==0,"rounding mode selected");
  for(int trial=0;trial<16;++trial) {
   for(int n=0;n<128;++n)for(int c=0;c<16;++c)
    frequencies[c][n]=(c>0 && (c+trial)%3!=0) ? frequencies[c-1][n]
       : std::ldexp(1.0+double((n*17+c*31+trial*13)%1024)/1024.,(n*7+c*11+trial)%180-90);
   check(client.fillChannelTables(bank),"mixed repeated frequencies accepted");
   for(int c=0;c<16;++c)for(int n=0;n<128;++n) {
    volatile double hz=frequencies[c][n];
    check(std::bit_cast<std::uint32_t>(bank[c].pitch[n])==std::bit_cast<std::uint32_t>(cs80::tuning::TuningTable::pitchFromHz(hz)),"mixed repeated conversion remains bit exact");
   }
  }
 }
 check(std::fesetround(originalRounding)==0,"rounding mode restored");
 for(double hz : {double(std::numeric_limits<float>::denorm_min()),double((std::numeric_limits<float>::max)())/65536.}) {
  for(auto& channel:frequencies)for(double& value:channel)value=hz;
  check(client.fillChannelTables(bank),"equal accepted boundary frequencies admitted");
  for(int c=0;c<16;++c)for(int n=0;n<128;++n)
   check(std::bit_cast<std::uint32_t>(bank[c].pitch[n])==std::bit_cast<std::uint32_t>(cs80::tuning::TuningTable::pitchFromHz(hz)),"boundary reuse remains bit exact");
  badChannel=1;badNote=64;badHz=std::numeric_limits<double>::quiet_NaN();filterBad=true;
  check(client.fillChannelTables(bank),"filtered invalid between equal valid frequencies accepted");
  check(bank[1].pitch[64]==64.f && !bank[1].mapped[64] && bank[0].pitch[64]==bank[2].pitch[64],"filtered invalid neither poisons nor clears safe reuse");
  badChannel=-2;filterBad=false;
 }
 std::cout<<"MTS-ESP channel adapter: "<<checks<<" checks, "<<failures<<" failures\n";
 return failures?1:0;
}
