#include "core/Ig00156RootFallback.h"
#include "counted-helper.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cstdio>
#include <bit>
#include <cstdint>
int main(int argc,char**argv){if(argc!=2)return 2;std::ifstream f(argv[1]);std::string l;unsigned n=0;puts("clock,sample,additional_equation_evaluations");while(getline(f,l)){if(l.rfind("CAP,",0))continue;std::stringstream ss(l);std::vector<std::string>r;std::string t;while(getline(ss,t,','))r.push_back(t);if(r.size()!=50)return 2;if(r[2]=="0")continue;std::vector<double>v;for(int j=6;j<50;++j)v.push_back(strtod(r[j].c_str(),nullptr));cs80::ig00156_detail::RootPoint a{};cs80::counted_root::RootPoint b{};cs80::counted_root::evaluations=0;bool x=cs80::ig00156_detail::bracketedRoot(v[16],v[17],v[18],v[19],v[20],v[21],a),y=cs80::counted_root::bracketedRoot(v[16],v[17],v[18],v[19],v[20],v[21],b);if(!x||!y)return 3;
for(auto pair:{std::pair{a.bp,b.bp},std::pair{a.hp,b.hp},std::pair{a.t1,b.t1},std::pair{a.t2,b.t2},std::pair{a.residual,b.residual}})if(std::bit_cast<std::uint64_t>(pair.first)!=std::bit_cast<std::uint64_t>(pair.second))return 4;
printf("%s,%s,%u\n",r[2].c_str(),r[5].c_str(),cs80::counted_root::evaluations);++n;}return n==512?0:2;}
