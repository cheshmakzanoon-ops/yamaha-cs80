#include "core/Tuning.h"
#include "core/Dsp.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <string>

namespace {
int checks=0, failures=0;
void check(bool ok,const std::string& why) {
    ++checks; if(!ok) {++failures;std::cerr<<"FAIL: "<<why<<'\n';}
}
cs80::tuning::Scale scale(const std::string& text) {
    cs80::tuning::Scale s;
    check(cs80::tuning::parseScale(text,s).empty(),"fixture syntax is valid Scala");
    return s;
}
void rejected(const char* name,const cs80::tuning::Scale& s,const cs80::tuning::KeyboardMapping& m) {
    cs80::tuning::TuningTable out;
    for(int n=0;n<128;++n) {out.pitch[n]=n+.125f;out.mapped[n]=(n%3)!=0;}
    const auto before=out;
    const auto error=cs80::tuning::buildTable(s,m,out);
    check(!error.empty(),std::string(name)+" rejects unrenderable nominal float frequency");
    check(out.pitch==before.pitch&&out.mapped==before.mapped,std::string(name)+" leaves caller table unchanged");
    if(error.empty()) {
        int invalid=0;
        for(float p:out.pitch) {const float hz=cs80::midiToHz(p);invalid+=!std::isfinite(hz)||!(hz>0);}
        std::cout<<"accepted-invalid diagnostic "<<name<<": "<<invalid<<" nonpositive/nonfinite core frequencies\n";
    }
}
void accepted(const char* name,const std::string& text,std::ofstream& raw,bool equal=false) {
    const auto s=scale(text); const auto m=cs80::tuning::defaultMapping(s);
    cs80::tuning::TuningTable out;
    const auto error=cs80::tuning::buildTable(s,m,out);
    check(error.empty(),std::string(name)+" remains accepted");
    bool safe=true,exact=true;
    for(int n=0;n<128;++n) {
        const float hz=cs80::midiToHz(out.pitch[n]);
        safe=safe&&std::isfinite(hz)&&hz>0&&out.mapped[n];
        exact=exact&&std::abs(out.pitch[n]-n)<=1e-5f;
    }
    check(safe,std::string(name)+" uses finite positive actual core conversions");
    if(equal)check(exact,"12TET remains equal tempered");
    raw.write(reinterpret_cast<const char*>(out.pitch.data()),sizeof(float)*out.pitch.size());
}
}
int main(int argc,char** argv) {
    if(argc!=2)return 2;
    std::ofstream raw(argv[1],std::ios::binary);
    if(!raw)return 3;
    for(const char* period:{"2400.0","3600.0"}) {
        const auto s=scale(std::string("extreme repeated degree\n1\n")+period+"\n");
        rejected(period,s,cs80::tuning::defaultMapping(s));
    }
    const auto tet=scale("12TET\n12\n100.0\n200.0\n300.0\n400.0\n500.0\n600.0\n700.0\n800.0\n900.0\n1000.0\n1100.0\n1200.0\n");
    cs80::tuning::KeyboardMapping tiny;
    check(cs80::tuning::parseKeyboardMapping("0\n0\n127\n60\n69\n0."+std::string(43,'0')+"1\n0\n",tiny).empty(),
          "finite tiny reference is valid existing KBM syntax");
    rejected("tiny reference",tet,tiny);
    accepted("12TET","12TET\n12\n100.0\n200.0\n300.0\n400.0\n500.0\n600.0\n700.0\n800.0\n900.0\n1000.0\n1100.0\n1200.0\n",raw,true);
    accepted("just five","just\n5\n9/8\n5/4\n3/2\n5/3\n2/1\n",raw);
    accepted("non-octave","tritave\n3\n600.0\n1200.0\n3/1\n",raw);
    accepted("negative interior","negative degree is allowed\n3\n-5.0\n700.0\n1200.0\n",raw);
    accepted("nonmonotonic interior","unsorted degrees are allowed\n3\n700.0\n200.0\n1200.0\n",raw);
    std::cout<<checks<<" Scala float-frequency checks, "<<failures<<" failures\n";
    return failures?1:0;
}
