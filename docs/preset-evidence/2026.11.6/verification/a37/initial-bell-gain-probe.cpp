// Independent metric audit: a uniform gain change must not impersonate a material.
#define main preceding_bell_material_suite_main
#include "tests/BellMaterialPresetTests.cpp"
#undef main
Audio scaled(Audio a, double gain) { for(auto& x:a.samples)x=static_cast<float>(x*gain);return a; }
int main() {
    for(const auto& entry:entries) {
        const auto& p=preset(entry.name);auto control=p.values;
        control[P_body1Material]=control[P_body2Material]=body::defaultMaterial;
        const auto a=render(p.values,60,.85f,1.6),b=render(control,60,.85f,1.6);
        const double begin=entry.material==body::gong?.65:.025,end=entry.material==body::gong?1.5:.25;
        const double frequency=261.6255653005986*entry.footage*entry.identityRatio;
        const auto metric=[&](const Audio& audio){return tone(audio,begin,end,frequency)/audio.rms(begin,end);};
        const double actual=db(metric(a)/metric(b));
        const double gainOnly=db(metric(a)/metric(scaled(a,3.7)));
        const double rescaled=db(metric(scaled(a,.125))/metric(scaled(b,3.7)));
        check(actual>9.0,std::string(entry.name)+": actual material passes existing mode threshold");
        check(std::abs(gainOnly)<1.e-4,std::string(entry.name)+": pure gain cannot pass mode threshold");
        check(std::abs(rescaled-actual)<.01,std::string(entry.name)+": independent gains preserve normalized mode difference");
        std::printf("GAIN %s actual_db %.12g pure_gain_db %.12g rescaled_error_db %.12g\n",entry.name,actual,gainOnly,std::abs(rescaled-actual));
    }
    std::printf("A37 bells: %d checks, %d failures\n",checks,failures);return failures?1:0;
}
