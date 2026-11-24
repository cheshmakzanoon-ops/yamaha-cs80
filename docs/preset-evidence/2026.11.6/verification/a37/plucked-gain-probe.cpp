// Independent metric audit: a uniform gain change must not impersonate a material.
#define main preceding_plucked_material_suite_main
#include "tests/PluckedMaterialPresetTests.cpp"
#undef main
Audio scaled(Audio a, double gain) { for (auto& x:a.data) x=static_cast<float>(x*gain); return a; }
int main() {
    for(const char* name:{"Harp","Koto","Banjo","Kalimba"}) {
        const auto& p=preset(name); auto control=p;
        control.values[P_body1Material]=control.values[P_body2Material]=body::defaultMaterial;
        const auto a=render(p,60,.85f,.4), b=render(control,60,.85f,.4);
        const bool tine=std::string(name)=="Kalimba"; const double f=261.6255653005986;
        const double actual=shapeDistance(a,b,f,tine);
        const double gainOnly=shapeDistance(a,scaled(a,3.7),f,tine);
        const double rescaled=shapeDistance(scaled(a,.125),scaled(b,3.7),f,tine);
        check(actual>.12,std::string(name)+": actual material passes existing identity threshold");
        check(gainOnly<1.e-6,std::string(name)+": pure gain control cannot pass identity threshold");
        check(std::abs(rescaled-actual)<1.e-6,std::string(name)+": independent gains preserve identity distance");
        std::printf("GAIN %s actual %.12g pure_gain %.12g rescaled_error %.12g\n",name,actual,gainOnly,std::abs(rescaled-actual));
    }
    std::printf("A37 plucked: %d checks, %d failures\n",checks,failures);return failures?1:0;
}
