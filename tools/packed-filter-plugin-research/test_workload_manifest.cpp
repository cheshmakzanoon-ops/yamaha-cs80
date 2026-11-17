#include "workload_manifest.hpp"
#include <iostream>
#include <tuple>
using namespace packed_filter_workload;
namespace {
void check(bool value) { if(!value)throw std::runtime_error("workload fixture assertion"); }
template<class F> void rejects(F f) {bool rejected=false;try {f();}catch(const std::invalid_argument&){rejected=true;}check(rejected);}
double value(const std::vector<Control>& p,const std::string& name) {for(const auto& c:p)if(c.name==name)return c.value;throw std::runtime_error("missing fixture control");}
}
int main() {
    const auto grid=cases();check(grid.size()==108);
    std::set<std::tuple<Policy,std::uint32_t,Mode,std::uint32_t,std::uint32_t>> unique;
    std::set<std::vector<std::uint8_t>> encoded;
    for(auto c:grid) {
        check(unique.insert({c.policy,c.rate,c.mode,c.voices,c.block}).second);
        auto p=controls(c);check(p.size()==cs80::P_numParams+2);
        check(value(p,"source_performance")== (c.policy==Policy::legacy?0.0:1.0));
        check(value(p,"parity_mode")== (c.policy==Policy::pure?1.0:0.0));
        check(value(p,"voice_mode")== (c.voices==16?0.25:0.0));
        check(value(p,"volume")==double(0.11f));
        for(const auto* name:{"drift","voice_dispersion","analog_noise","tuning_mode"})check(value(p,name)==0);
        check(value(p,"hardware_source_1")==1&&value(p,"hardware_source_2")==1);
        check(value(p,"oversampling")==0.5);
        auto defaults=cs80::defaultValues();
        const std::set<int> overrides={cs80::P_sourcePerformance,cs80::P_parityMode,cs80::P_voiceMode,cs80::P_drift,cs80::P_voiceDispersion,cs80::P_analogNoise,cs80::P_volume,cs80::P_tuningMode};
        for(int i=0;i<cs80::P_numParams;++i) {
            check(p[i].name==cs80::paramId(i));
            if(!overrides.count(i)) {
                const auto& d=cs80::paramDef(i);float expected=std::clamp((defaults[i]-d.min)/(d.max-d.min),0.0f,1.0f);
                if(d.skew!=1.0f)expected=std::pow(expected,d.skew);
                check(p[i].value==double(expected));
            }
        }
        Metadata metadata{std::uint32_t(p.size()),{}};for(const auto& x:p)metadata.ids.push_back(x.id);
        bind_native_ids(c,metadata);
        auto ev=events(c);check(ev.size()==2*c.voices);
        for(std::uint32_t n=0;n<c.voices;++n) {
            const auto& on=ev[n];const auto& off=ev[n+c.voices];
            check(on.on&&!off.on&&on.sample==17+n&&off.sample==3072+n);
            check(on.bus==0&&on.channel==0&&off.bus==0&&off.channel==0);
            check(on.pitch==48+int(n)&&on.note_id==on.pitch&&off.pitch==on.pitch&&off.note_id==on.note_id);
            check(on.velocity==0.75f&&off.velocity==0&&off.sample<frames);
        }
        auto data=encode_draft(c);check(data==encode_draft(c));check(encoded.insert(data).second);
    }
    check(parameter_id("voice_mode")==632422544u);check(parameter_id("hardware_source_1")==1629768068u);
    check(parameter_id("Aa")==parameter_id("BB"));
    rejects([]{validate_controls({{"Aa",parameter_id("Aa"),0},{"BB",parameter_id("BB"),0}});});
    auto p=controls(grid.front());Metadata m{std::uint32_t(p.size()),{}};for(const auto& c:p)m.ids.push_back(c.id);
    auto missing=m;missing.ids.pop_back();--missing.declared_count;rejects([&]{bind_native_ids(grid.front(),missing);});
    auto duplicate=m;duplicate.ids.push_back(duplicate.ids.front());++duplicate.declared_count;rejects([&]{bind_native_ids(grid.front(),duplicate);});
    auto duplicate_extra=m;duplicate_extra.ids.insert(duplicate_extra.ids.end(),{0xffffffffu,0xffffffffu});duplicate_extra.declared_count+=2;rejects([&]{bind_native_ids(grid.front(),duplicate_extra);});
    auto count=m;++count.declared_count;rejects([&]{bind_native_ids(grid.front(),count);});
    auto changed=p;changed.front().id^=1;rejects([&]{validate_controls(changed);});
    auto nan=p;nan.front().value=std::numeric_limits<double>::quiet_NaN();rejects([&]{validate_controls(nan);});
    auto duplicate_control=p;duplicate_control.push_back(p.front());rejects([&]{validate_controls(duplicate_control);});
    rejects([]{parameter_id("");});rejects([]{parameter_id("\xff");});
    for(auto invalid:{Case{Policy(3),48000,Mode::realtime,8,64},Case{Policy::legacy,1,Mode::realtime,8,64},Case{Policy::legacy,48000,Mode(2),8,64},Case{Policy::legacy,48000,Mode::realtime,2,64},Case{Policy::legacy,48000,Mode::realtime,8,63}})rejects([&]{encode_draft(invalid);});
    std::vector<std::uint8_t> bytes;u32(bytes,0x12345678);u64(bytes,0x0102030405060708ull);
    check(bytes==std::vector<std::uint8_t>({0x78,0x56,0x34,0x12,8,7,6,5,4,3,2,1}));
    std::cout<<"PASS: 108 unique draft cases; complete controls, native-ID rejection and deterministic event/encoding fixtures; no native runtime\n";
}
