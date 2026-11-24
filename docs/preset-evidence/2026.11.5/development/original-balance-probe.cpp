#define main audit_main
#include "/workspace/scratch/c98fb40a9529/yamaha-cs80/tests/LibraryAuditTests.cpp"
#undef main
int main() {
    for (const auto& original : originalLibrary()) {
        if (original.name == "Distant Oceans") {
            for (auto pair : { std::pair<float,float>{420.f,380.f}, {320.f,300.f}, {260.f,260.f} }) {
                auto p=original;p.values[chParam(0,CP_hpfCutoff)]=pair.first;p.values[chParam(1,CP_hpfCutoff)]=pair.second;
                const auto x=measure(p);
                std::printf("Distant HP %.0f/%.0f window %.6f peak %.6f stress %.6f\n",pair.first,pair.second,dbfs(x.windowRms),dbfs(x.phrasePeak),dbfs(x.stressPeak));
            }
        }
        if (original.name == "Slow Sequences 166") {
            for (auto pair : { std::pair<float,float>{24.f,.13f}, {20.f,.08f}, {20.f,.05f} }) {
                auto p=original;p.values[chParam(0,CP_hpfCutoff)]=pair.first;p.values[P_mix]=pair.second;
                const auto x=measure(p);
                std::printf("Slow Seq HP %.0f mix %.2f window %.6f peak %.6f stress %.6f\n",pair.first,pair.second,dbfs(x.windowRms),dbfs(x.phrasePeak),dbfs(x.stressPeak));
            }
        }
    }
}
