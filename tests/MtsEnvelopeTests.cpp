// Supported MTS message-envelope validation, not bank/channel implementation.
#include "core/Tuning.h"
#include <array>
#include <bit>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace tn = cs80::tuning;
namespace {
int checks=0, failures=0;
void check(bool ok,const std::string& label) {
    ++checks; if(!ok){++failures;std::cerr<<"FAIL: "<<label<<'\n';}
}
tn::TuningTable sentinel() {
    tn::TuningTable t;
    for(std::size_t n=0;n<128;++n){t.pitch[n]=(float)n+.125f;t.mapped[n]=n%3!=0;}
    return t;
}
bool exact(const tn::TuningTable& a,const tn::TuningTable& b) {
    for(std::size_t n=0;n<128;++n)
        if(std::bit_cast<std::uint32_t>(a.pitch[n])!=std::bit_cast<std::uint32_t>(b.pitch[n])
            ||a.mapped[n]!=b.mapped[n])return false;
    return true;
}
struct Fixture {const char* name;int sub;bool realtime;};
std::vector<std::uint8_t> message(const Fixture& f) {
    std::vector<std::uint8_t> d{0xF0,(std::uint8_t)(f.realtime?0x7F:0x7E),0x7F,8,(std::uint8_t)f.sub};
    if(f.sub==1) {
        d.push_back(0);
        for(int i=0;i<16;++i)d.push_back(' ');
        for(int n=0;n<128;++n) {
            if(n==126)d.insert(d.end(),{0x7F,0x7F,0x7F});
            else d.insert(d.end(),{(std::uint8_t)n,0x20,0});
        }
        d.push_back(0); // Original01 checksum may be ignored per printed6.
    } else if(f.sub==2||f.sub==7) {
        if(f.sub==7)d.push_back(0);
        d.insert(d.end(),{0,2,60,61,0x40,0,69,0x7F,0x7F,0x7F});
    } else {
        d.insert(d.end(),{3,0x7F,0x7F}); // all16 channels, reserved bits zero
        for(int pc=0;pc<12;++pc) {
            if(f.sub==8)d.push_back(pc==0?74:64);
            else d.insert(d.end(),{(std::uint8_t)(pc==0?0x60:0x40),0});
        }
    }
    d.push_back(0xF7);return d;
}
tn::TuningTable expected(const Fixture& f) {
    auto t=sentinel();
    if(f.sub==1) {
        for(std::size_t n=0;n<128;++n)if(n!=126){t.pitch[n]=(float)n+.25f;t.mapped[n]=true;}
    } else if(f.sub==2||f.sub==7) {
        t.pitch[60]=61.5f;t.mapped[60]=true;
    } else {
        for(std::size_t n=0;n<128;++n){t.pitch[n]=(float)n+(n%12==0?(f.sub==8?.1f:.5f):0.f);t.mapped[n]=true;}
    }
    return t;
}
void rejects(const std::vector<std::uint8_t>& d,const std::string& label,int device=0x7F) {
    auto t=sentinel();
    check(!tn::applyMtsSysEx(d.data(),(int)d.size(),t,device),label+" rejected");
    check(exact(t,sentinel()),label+" preserves all128 pitches/mapped bits");
}
void accepts(const std::vector<std::uint8_t>& d,const tn::TuningTable& wanted,const std::string& label,int device=0x7F) {
    auto t=sentinel();
    check(tn::applyMtsSysEx(d.data(),(int)d.size(),t,device),label+" accepted");
    check(exact(t,wanted),label+" exact independent128-entry oracle");
}
}
int main() {
    const Fixture fixtures[]={{"bulk01",1,false},{"single02",2,true},
        {"bank07-real",7,true},{"bank07-nonreal",7,false},
        {"octave08-real",8,true},{"octave08-nonreal",8,false},
        {"octave09-real",9,true},{"octave09-nonreal",9,false}};
    for(const auto& f:fixtures) {
        const auto d=message(f);const std::string label=f.name;
        accepts(d,expected(f),label+" valid broadcast");
        auto directed=d;directed[2]=5;
        accepts(directed,expected(f),label+" valid directed device",5);
        rejects(directed,label+" wrong device",0x10);
        for(std::size_t at=1;at+1<d.size();++at) {
            auto bad=d;bad[at]|=0x80;
            rejects(bad,label+" high-bit byte"+std::to_string(at));
        }
        for(std::size_t prefix=1;prefix+1<d.size();++prefix) {
            std::vector<std::uint8_t> shortMessage(d.begin(),d.begin()+prefix);
            shortMessage.push_back(0xF7);
            rejects(shortMessage,label+" framed prefix"+std::to_string(prefix));
        }
        for(int extra=1;extra<=4;++extra) {
            auto longMessage=d;longMessage.insert(longMessage.end()-1,extra,0);
            rejects(longMessage,label+" excess payload"+std::to_string(extra));
        }
        auto badStart=d;badStart.front()=0;
        auto badEnd=d;badEnd.back()=0;
        rejects(badStart,label+" missing F0");rejects(badEnd,label+" missing F7");
        if(f.sub==8||f.sub==9) {
            for(int bit=2;bit<=6;++bit) {
                auto reserved=d;reserved[5]|=(std::uint8_t)(1<<bit);
                rejects(reserved,label+" reserved mask bit"+std::to_string(bit));
            }
            auto zero=d;
            for(std::size_t n=8;n+1<zero.size();++n)zero[n]=f.sub==8?64:((n-8)%2==0?64:0);
            accepts(zero,tn::TuningTable(),label+" zero-offset message");
        } else {
            auto noChange=d;
            if(f.sub==1) {
                for(int n=0;n<128;++n)for(int byte=0;byte<3;++byte)noChange[22+3*n+byte]=0x7F;
                for(std::uint8_t checksum:{(std::uint8_t)0,(std::uint8_t)1,(std::uint8_t)0x7F}) {
                    auto tolerant=d;tolerant[tolerant.size()-2]=checksum;
                    accepts(tolerant,expected(f),"original01 permitted checksum tolerance"+std::to_string(checksum));
                }
            } else {
                const int header=f.sub==2?7:8;
                for(int n=0;n<2;++n)for(int byte=0;byte<3;++byte)noChange[header+4*n+1+byte]=0x7F;
                for(int count:{128,255}) {
                    std::vector<std::uint8_t> bad(d.begin(),d.begin()+header);
                    bad[header-1]=(std::uint8_t)count;
                    for(int n=0;n<count;++n)bad.insert(bad.end(),{(std::uint8_t)(n%128),60,0,0});
                    bad.push_back(0xF7);
                    rejects(bad,label+" structurally complete high-bit count"+std::to_string(count));
                }
            }
            auto unchanged=sentinel();
            check(!tn::applyMtsSysEx(noChange.data(),(int)noChange.size(),unchanged),label+" all no-change reports no update");
            check(exact(unchanged,sentinel()),label+" all no-change preserves128 entries");
        }
    }
    auto untouched=sentinel();
    check(!tn::applyMtsSysEx(nullptr,408,untouched),"null MTS pointer rejected");
    check(exact(untouched,sentinel()),"null MTS pointer preserves128 entries");
    std::cout<<checks<<" MTS envelope checks, "<<failures<<" failures\n";
    return failures?1:0;
}
