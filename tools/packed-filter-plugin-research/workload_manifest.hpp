#pragma once
// Pure proposed workload data. No SDK, module loading, audio processing or flush.
#include "core/Params.h"
#include <bit>
#include <cstdint>
#include <limits>
#include <set>
#include <stdexcept>
#include <vector>

namespace packed_filter_workload {
inline constexpr const char* params_sha256="eb5b73f144f83ff386865b0abb726f767eb8f4a0b651662442c4d03ccf8a2ac2";
inline constexpr const char* fixture_sha256="dcc9bc824420ceb801d5047660ff4857a9ab4adda9b4762d0bd8df09aba08981";
inline void require(bool yes,const char* message) { if(!yes) throw std::invalid_argument(message); }
enum class Policy : std::uint32_t { legacy=0,circuit=1,pure=2 };
// Private manifest encoding, NOT SDK ProcessModes: SDK kOffline is 2 and
// SDK value 1 means kPrefetch. A future bridge must explicitly switch-map modes.
enum class Mode : std::uint32_t { realtime=0,offline=1 };
struct Case { Policy policy; std::uint32_t rate; Mode mode; std::uint32_t voices,block; };
inline void validate(Case c) {
    require(c.policy==Policy::legacy||c.policy==Policy::circuit||c.policy==Policy::pure,"unknown policy");
    require(c.mode==Mode::realtime||c.mode==Mode::offline,"unknown mode");
    require(c.rate==44100||c.rate==48000||c.rate==96000,"unknown rate");
    require(c.voices==8||c.voices==16,"unknown note cohort");
    require(c.block==64||c.block==256||c.block==1024,"unknown block");
}
inline std::vector<Case> cases() {
    std::vector<Case> result;
    for(auto policy:{Policy::legacy,Policy::circuit,Policy::pure})
        for(std::uint32_t rate:{44100,48000,96000}) for(auto mode:{Mode::realtime,Mode::offline})
            for(std::uint32_t voices:{8,16}) for(std::uint32_t block:{64,256,1024})
                result.push_back({policy,rate,mode,voices,block});
    return result;
}
inline std::uint32_t parameter_id(const std::string& name) {
    require(!name.empty(),"empty parameter name");std::uint32_t hash=0;
    for(unsigned char c:name) { require(c<128,"only frozen ASCII parameter names supported"); hash=hash*31u+c; }
    return hash&0x7fffffffu;
}
inline float normalized(int index,float physical) {
    require(index>=0&&index<cs80::P_numParams&&std::isfinite(physical),"invalid physical parameter");
    const auto& d=cs80::paramDef(index);
    float proportion=std::clamp((physical-d.min)/(d.max-d.min),0.0f,1.0f);
    if(d.skew!=1.0f) proportion=std::pow(proportion,d.skew);
    return proportion;
}
struct Control { std::string name; std::uint32_t id; double value; };
inline void validate_controls(const std::vector<Control>& controls) {
    require(!controls.empty(),"empty controls");std::set<std::string> names;std::set<std::uint32_t> ids;
    for(const auto& c:controls) {
        require(names.insert(c.name).second&&ids.insert(c.id).second,"duplicate/colliding generated controls");
        require(parameter_id(c.name)==c.id,"generated ID differs from frozen hash");
        require(std::isfinite(c.value)&&c.value>=0&&c.value<=1,"invalid normalized value");
    }
}
inline std::vector<Control> controls(Case c) {
    validate(c);auto p=cs80::defaultValues();
    p[cs80::P_sourcePerformance]=c.policy==Policy::legacy?0.0f:1.0f;
    p[cs80::P_parityMode]=c.policy==Policy::pure?1.0f:0.0f;
    p[cs80::P_voiceMode]=c.voices==16?1.0f:0.0f;
    p[cs80::P_drift]=p[cs80::P_voiceDispersion]=p[cs80::P_analogNoise]=0.0f;
    p[cs80::P_volume]=0.11f;p[cs80::P_tuningMode]=0.0f;
    std::vector<Control> result;
    for(int i=0;i<cs80::P_numParams;++i) {
        auto name=cs80::paramId(i);result.push_back({name,parameter_id(name),double(normalized(i,p[i]))});
    }
    for(const auto* name:{"hardware_source_1","hardware_source_2"}) result.push_back({name,parameter_id(name),1.0});
    validate_controls(result);return result;
}
struct Metadata { std::uint32_t declared_count; std::vector<std::uint32_t> ids; };
// The complete native metadata must be independently captured/authenticated.
// Passing this check alone does not prove it came from any native module.
inline void bind_native_ids(Case c,const Metadata& native) {
    const auto expected=controls(c);
    require(native.declared_count>0&&native.ids.size()==native.declared_count,"native metadata count mismatch");
    const std::set<std::uint32_t> ids(native.ids.begin(),native.ids.end());
    require(ids.size()==native.ids.size(),"duplicate native metadata IDs");
    for(const auto& control:expected) require(ids.count(control.id)==1,"generated ID absent from native metadata");
}
inline constexpr std::uint32_t frames=4096;
struct Event { std::uint32_t sample; bool on; std::int32_t bus,channel,pitch,note_id; float velocity; };
inline std::vector<Event> events(Case c) {
    validate(c);std::vector<Event> result;
    for(std::uint32_t n=0;n<c.voices;++n) result.push_back({17+n,true,0,0,int(48+n),int(48+n),0.75f});
    for(std::uint32_t n=0;n<c.voices;++n) result.push_back({3072+n,false,0,0,int(48+n),int(48+n),0.0f});
    return result;
}
inline void u32(std::vector<std::uint8_t>& out,std::uint32_t value) { for(int i=0;i<4;++i)out.push_back(std::uint8_t(value>>(i*8))); }
inline void u64(std::vector<std::uint8_t>& out,std::uint64_t value) { for(int i=0;i<8;++i)out.push_back(std::uint8_t(value>>(i*8))); }
inline void string(std::vector<std::uint8_t>& out,const std::string& value) { require(value.size()<=UINT32_MAX,"string length");u32(out,std::uint32_t(value.size()));out.insert(out.end(),value.begin(),value.end()); }
// Canonical fieldwise little-endian draft encoding. Does NOT authorize or choose
// the legal SDK phase for applying controls (including any zero-frame process).
inline std::vector<std::uint8_t> encode_draft(Case c) {
    static_assert(sizeof(float)==4&&sizeof(double)==8&&std::numeric_limits<float>::is_iec559&&std::numeric_limits<double>::is_iec559);
    validate(c);auto p=controls(c);auto e=events(c);std::vector<std::uint8_t> out;
    string(out,"packed-filter-workload-draft-v1");string(out,params_sha256);string(out,fixture_sha256);
    u32(out,std::uint32_t(c.policy));u32(out,c.rate);u32(out,std::uint32_t(c.mode));u32(out,c.voices);u32(out,c.block);u32(out,frames);
    u32(out,std::uint32_t(p.size()));for(const auto& control:p) {string(out,control.name);u32(out,control.id);u64(out,std::bit_cast<std::uint64_t>(control.value));}
    u32(out,std::uint32_t(e.size()));for(const auto& event:e) {
        u32(out,event.sample);u32(out,event.on?1:0);u32(out,std::uint32_t(event.bus));u32(out,std::uint32_t(event.channel));
        u32(out,std::uint32_t(event.pitch));u32(out,std::uint32_t(event.note_id));u32(out,std::bit_cast<std::uint32_t>(event.velocity));
    }
    return out;
}
}
