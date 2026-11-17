#pragma once
// Pure source-derived schema matching. No native calls and no write permission
// for any residual parameter. See companion STATUS for source/provenance scope.
#include "native_observer_sdk.hpp"
#include "workload_manifest.hpp"
#include <map>
#include <set>
namespace cs80_observer {
enum class SourceParameterRole { Workload, Memory, Bypass, Program, Midi };
struct SourceParameterExpectation { ParameterInfo info{}; SourceParameterRole role; };
struct SourceAwareBinding {
 std::vector<ParamID> writableIds;
 std::vector<std::pair<ParamID,double>> residual;
 std::array<ParamID,2> panelIds{};
};
namespace source_parameter_detail {
inline ParamID hash(const std::string& text){return packed_filter_workload::parameter_id(text);}
inline void text(TChar (&out)[128],const std::string& value){
 require(value.size()<128,"source parameter text bound");std::fill(std::begin(out),std::end(out),0);
 for(std::size_t i=0;i<value.size();++i){require(static_cast<unsigned char>(value[i])<128,"source parameter ASCII");out[i]=static_cast<TChar>(value[i]);}
}
inline std::string text(const TChar (&in)[128]){
 std::string result;for(std::size_t i=0;i<128;++i){if(!in[i])return result;require(in[i]>0 && in[i]<128,"parameter metadata ASCII");result.push_back(static_cast<char>(in[i]));}
 throw std::invalid_argument("unterminated parameter metadata");
}
inline double normalized(const cs80::ParamDef& d){
 // JUCE NormalisableRange<float>::convertTo0to1, non-symmetric skew,
 // zero interval. Both intermediates and pow overload are float, not double.
 float proportion=std::clamp((d.def-d.min)/(d.max-d.min),0.0f,1.0f);
 return double(d.skew==1.0f?proportion:std::pow(proportion,d.skew));
}
inline SourceParameterExpectation make(ParamID id,const std::string& title,const std::string& units,int32 steps,double value,int32 unit,int32 flags,SourceParameterRole role,bool shortName=true){
 SourceParameterExpectation r{};r.role=role;r.info.id=id;text(r.info.title,title);text(r.info.shortTitle,shortName?title.substr(0,8):"");text(r.info.units,units);r.info.stepCount=steps;r.info.defaultNormalizedValue=value;r.info.unitId=unit;r.info.flags=flags;return r;
}
inline int32 group(int index){
 using namespace cs80;
 if(index<P_numGlobals){int run=0;std::string previous;for(int i=0;i<=index;++i){std::string now=paramDef(i).group;if(i==0||now!=previous)++run;previous=now;}return static_cast<int32>(hash("global_"+std::to_string(run)));}
 if(index<P_legacyNumParams)return static_cast<int32>(hash(index<P_ch2Start?"channel_1":"channel_2"));
 const char* name=nullptr;
 if(index>=P_subAtVcf&&index<=P_subExtLevel)name="sub_osc";
 else if(index>=P_kbdBrillianceLow&&index<=P_kbdLevelHigh)name="keyboard";
 else if(index>=P_footswitchPlugged&&index<=P_footMode)name="foot";
 else if(index==P_outputLevel)name="output_switch";
 else if(index==P_parityMode||index==P_sourcePerformance)name="engine";
 else if(index==P_driftModel)name="ws1_voice";
 else if(index==P_oversampling||index==P_outputJack)name="ws2_engine_io";
 return name?static_cast<int32>(hash(name)):0;
}
}
inline std::vector<SourceParameterExpectation> sourceParameterExpectations(){
 using namespace source_parameter_detail;using namespace cs80;
 require(P_numParams==190 && CP_count==30,"source core parameter dimensions");
 std::vector<SourceParameterExpectation> result;
 for(int i=0;i<P_numParams;++i){const auto& d=paramDef(i);auto title=std::string(d.name);std::string units=d.unit;int32 steps=0;double value=normalized(d);
  switch(i){
   case P_subOriginalMode:title="Original SUB mode";steps=1;value=1;break;
   case P_subOriginalFunction:title="Original SUB function";steps=5;value=0;break;
   case P_footswitchPlugged:case P_footswitchPressed:case P_sustainAssign:case P_glideAssign:steps=1;value=d.def>=0.5f?1:0;break;
   case P_footMode:title="Foot controller mode";steps=1;value=0;break;
   case P_outputLevel:title="Output HIGH (on) / LOW (off)";steps=1;value=1;break;
   case P_parityMode:title="PURE CS-80";steps=1;value=0;break;
   case P_sourcePerformance:title="Engine";steps=1;value=1;break;
   case P_driftModel:title="Drift model";steps=1;value=0;break;
   case P_oversampling:title="Oversampling quality";steps=2;value=double(float(static_cast<int>(d.def))/2.0f);break;
   case P_outputJack:title="Listening output";steps=2;value=0;break;
   default:break;
  }
  if(steps)units.clear();
  result.push_back(make(hash(paramId(i)),title,units,steps,value,group(i),1,SourceParameterRole::Workload));
 }
 for(int ch=0;ch<2;++ch)result.push_back(make(hash("hardware_source_"+std::to_string(ch+1)),ch==0?"Channel I source":"Channel II source","",13,1,static_cast<int32>(hash("programmer")),1,SourceParameterRole::Workload));
 for(int bank=0;bank<4;++bank)for(int local=0;local<CP_count;++local){
  if(local==CP_feet||local==CP_pan||local==CP_hpfOn||local==CP_lpfOn)continue;
  const auto& d=kChannelDefs[local];bool boolean=local==CP_saw||local==CP_pulse;auto prefix="memory_"+std::to_string(bank+1);
  result.push_back(make(hash(prefix+"_"+d.id),"Memory "+std::to_string(bank+1)+" "+d.group+" "+d.name,boolean?"":d.unit,boolean?1:0,boolean?(d.def>=0.5f?1:0):normalized(d),static_cast<int32>(hash(prefix)),1,SourceParameterRole::Memory));
 }
 // Source programLibrary() construction, never inferred from native metadata.
 constexpr std::size_t programCount=523; // independently compiled source probe in tests
 result.push_back(make(0x62797073,"Bypass","",1,0,0,65537,SourceParameterRole::Bypass));
 result.push_back(make(0x70727374,"Program","",static_cast<int32>(programCount-1),0,0,32769,SourceParameterRole::Program));
 for(int ch=0;ch<16;++ch)for(int cc=0;cc<130;++cc)result.push_back(make(0x6d636d00+ch*130+cc,"MIDI CC "+std::to_string(ch)+"|"+std::to_string(cc),"",0,0,0,0,SourceParameterRole::Midi,false));
 require(result.size()==2378,"source full parameter count");std::set<ParamID> ids;for(const auto& r:result)require(r.info.id!=kNoParamId&&ids.insert(r.info.id).second,"source parameter collision");return result;
}
inline SourceAwareBinding classifySourceAwarePanel(const ConnectedParameterRoster& roster,packed_filter_workload::Case c){
 using namespace source_parameter_detail;packed_filter_workload::validate(c);
 require(roster.declaredCount==2378 && roster.parameters.size()==2378 && roster.ids.size()==2378,"source-aware complete native roster");
 const auto expected=sourceParameterExpectations();std::map<ParamID,const SourceParameterExpectation*> byId;for(const auto& e:expected)byId.emplace(e.info.id,&e);
 SourceAwareBinding binding;binding.panelIds={hash("hardware_source_1"),hash("hardware_source_2")};std::set<ParamID> seen;
 for(std::size_t i=0;i<roster.parameters.size();++i){const auto& p=roster.parameters[i];const auto& a=p.info;
  require(a.id==roster.ids[i]&&seen.insert(a.id).second,"source-aware native ID consistency/duplicate");auto found=byId.find(a.id);require(found!=byId.end(),"source-aware unknown parameter");const auto& e=*found->second;const auto& b=e.info;
  require(text(a.title)==text(b.title)&&text(a.shortTitle)==text(b.shortTitle)&&text(a.units)==text(b.units)&&a.stepCount==b.stepCount&&std::bit_cast<std::uint64_t>(a.defaultNormalizedValue)==std::bit_cast<std::uint64_t>(b.defaultNormalizedValue)&&a.unitId==b.unitId&&a.flags==b.flags,"source-aware parameter metadata mismatch");
  require(std::isfinite(p.current)&&p.current>=0&&p.current<=1,"source-aware normalized current");
  if(e.role!=SourceParameterRole::Workload){require(std::bit_cast<std::uint64_t>(p.current)==std::bit_cast<std::uint64_t>(b.defaultNormalizedValue),"source-aware residual baseline mismatch");binding.residual.emplace_back(a.id,p.current);}
 }
 const auto controls=packed_filter_workload::controls(c);for(const auto& q:controls){require(seen.count(q.id)==1&&byId.at(q.id)->role==SourceParameterRole::Workload,"source-aware workload identity");binding.writableIds.push_back(q.id);}
 require(binding.writableIds.size()==192 && binding.residual.size()==2186,"source-aware role partition");
 for(auto id:binding.panelIds){auto it=std::find_if(controls.begin(),controls.end(),[&](const auto& q){return q.id==id;});require(it!=controls.end()&&it->value==1.0,"source-aware panel selector control");}
 return binding;
}
}
