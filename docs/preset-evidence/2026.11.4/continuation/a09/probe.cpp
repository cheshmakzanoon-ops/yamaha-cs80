#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include <algorithm>
#include <bit>
#include <cstdint>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
constexpr double pi=3.14159265358979323846;
double db(double x) {return 20*std::log10(std::max(x,1e-15));}
double rms(const std::vector<float>& a, double start,double end) {int first=int(start*48000),last=int(end*48000);double sum=0;for(int i=first;i<last;i++) sum+=(double(a[2*i])*a[2*i]+double(a[2*i+1])*a[2*i+1])/2;return std::sqrt(sum/(last-first));}
double tone(const std::vector<float>&a,double f,double start,double end) {int first=int(start*48000),last=int(end*48000);double sr=0,si=0,sw=0;for(int i=first;i<last;i++){double w=.5-.5*std::cos(2*pi*(i-first)/(last-first-1));double s=(a[2*i]+a[2*i+1])*.5;sr+=s*w*std::cos(2*pi*f*i/48000);si+=s*w*std::sin(2*pi*f*i/48000);sw+=w;}return 2*std::hypot(sr,si)/sw;}
std::uint64_t audioHash(const std::vector<float>& a) { std::uint64_t h=14695981039346656037ull;for(float s:a) { auto b=std::bit_cast<std::uint32_t>(s);for(int i=0;i<4;++i) {h^=b&255;h*=1099511628211ull;b>>=8;}}return h;}
int main(int argc,char** argv){const bool parityOnly=argc>1;std::cout<<std::setprecision(17)<<"index,name,note,velocity,peak_dbfs,rms_025_250_dbfs,rms_300_800_dbfs,fund_025_250_dbfs,upper_to_fund_db,audio_fnv1a64\n";const auto& b=cs80::programLibrary();int matched=0;for(size_t idx=0;idx<b.size();idx++){const auto&p=b[idx];if(p.name!="Electric Piano I"&&p.name!="Electric Piano II"&&p.name!="Tine Piano"&&p.name!="Reed Piano"&&p.name!="Analog Piano")continue;matched++;for(int n:{36,60,84})for(float v:{.15f,.3f,.5f,.7f,.9f,1.f}){if(parityOnly && v!=1.f && p.name!="Reed Piano" && p.name!="Analog Piano")continue;auto e=std::make_unique<cs80::Engine>();e->prepare(48000);e->setParams(p.values);e->noteOn(n,v,1);std::vector<float>a(48000*2);for(int at=0;at<48000;at+=128)e->render(a.data()+2*at,std::min(128,48000-at));double peak=0;for(float s:a)peak=std::max(peak,std::abs(double(s)));double hz=440*std::exp2((n-69.)/12);double fund=tone(a,hz,.025,.25);double upper=0;for(int h=2;h<=8;h++){double x=tone(a,hz*h,.025,.25);upper+=x*x;}std::cout<<idx<<','<<p.name<<','<<n<<','<<v<<','<<db(peak)<<','<<db(rms(a,.025,.25))<<','<<db(rms(a,.3,.8))<<','<<db(fund)<<','<<db(std::sqrt(upper)/fund)<<','<<audioHash(a)<<std::endl;}}std::cerr<<"matched "<<matched<<" presets\n";return matched==5?0:1;}
