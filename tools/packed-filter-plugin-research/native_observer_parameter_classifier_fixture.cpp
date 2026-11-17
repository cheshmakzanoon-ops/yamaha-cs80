#include "native_observer_parameter_classifier.hpp"
#include <iostream>
#include <iomanip>
using namespace cs80_observer;
int main(int argc,char** argv){try{
 auto expected=sourceParameterExpectations();
 if(argc==2&&std::string(argv[1])=="export"){
  std::cout<<std::setprecision(17);for(const auto& e:expected){const auto& p=e.info;std::cout<<p.id<<'\t'<<int(e.role)<<'\t'<<source_parameter_detail::text(p.title)<<'\t'<<source_parameter_detail::text(p.shortTitle)<<'\t'<<source_parameter_detail::text(p.units)<<'\t'<<p.stepCount<<'\t'<<p.defaultNormalizedValue<<'\t'<<p.unitId<<'\t'<<p.flags<<'\n';}return 0;
 }
 ConnectedParameterRoster roster;roster.declaredCount=2378;for(const auto& e:expected){roster.ids.push_back(e.info.id);roster.parameters.push_back({e.info,e.info.defaultNormalizedValue});}
 const packed_filter_workload::Case c{packed_filter_workload::Policy::legacy,48000,packed_filter_workload::Mode::realtime,8,256};
 std::string mode=argc==2?argv[1]:"valid";bool rejects=mode!="valid"&&mode!="reorder"&&mode!="workload-current";
 if(mode=="missing"){roster.parameters.pop_back();roster.ids.pop_back();--roster.declaredCount;}
 else if(mode=="extra"){roster.parameters.push_back(roster.parameters.back());roster.ids.push_back(roster.ids.back());++roster.declaredCount;}
 else if(mode=="duplicate"){roster.parameters[1]=roster.parameters[0];roster.ids[1]=roster.ids[0];}
 else if(mode=="unknown"){roster.parameters[1].info.id=7;roster.ids[1]=7;}
 else if(mode=="id-vector")roster.ids[0]=7;
 else if(mode=="reorder"){std::reverse(roster.ids.begin(),roster.ids.end());std::reverse(roster.parameters.begin(),roster.parameters.end());}
 else if(mode=="workload-current")for(std::size_t i=0;i<192;++i)roster.parameters[i].current=0.25;
 else if(mode=="nan-current")roster.parameters[0].current=std::numeric_limits<double>::quiet_NaN();
 else if(mode=="negative-zero-default")roster.parameters[296].info.defaultNormalizedValue=-0.0;
 else if(mode=="negative-zero-residual")roster.parameters[296].current=-0.0;
 else if(mode=="residual-current")roster.parameters[192].current=0.25;
 else if(mode!="valid"){
  auto colon=mode.find(':');require(colon!=std::string::npos,"unknown test mode");auto index=std::stoul(mode.substr(colon+1));auto field=mode.substr(0,colon);auto& p=roster.parameters.at(index).info;
  if(field=="title")++p.title[0];else if(field=="short")++p.shortTitle[0];else if(field=="units")++p.units[0];else if(field=="steps")++p.stepCount;else if(field=="default")p.defaultNormalizedValue=std::nextafter(p.defaultNormalizedValue,1.5);else if(field=="unit")++p.unitId;else if(field=="flags")++p.flags;else if(field=="unterminated")std::fill(std::begin(p.title),std::end(p.title),'x');else throw std::runtime_error("unknown mutation");
 }
 bool rejected=false;try{auto b=classifySourceAwarePanel(roster,c);require(b.writableIds.size()==192&&b.residual.size()==2186&&b.panelIds[0]==1629768068&&b.panelIds[1]==1629768069,"binding result");}catch(const std::invalid_argument&){rejected=true;}catch(const std::runtime_error&){rejected=true;}
 if(rejected!=rejects)throw std::runtime_error("unexpected classification outcome: "+mode);
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
