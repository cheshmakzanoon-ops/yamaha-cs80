// Reuse the unchanged shipped bank audit and thresholds for the four edited patches.
// Compile with -std=c++20 -O2 -Isrc, then link the candidate libcs80_core.a and pthread.
#define main completeBankAuditMain
#include "../../../../../tests/LibraryAuditTests.cpp"
#undef main
int main()
{
    int bad=0;
    for(const char* name:{"Harp","Koto","Banjo","Kalimba"})
    {
        auto found=std::find_if(curatedLibrary().begin(),curatedLibrary().end(),[name](const auto& p){return p.name==name;});
        if(found==curatedLibrary().end())return 2;
        const auto x=measure(*found);
        const bool pass=x.finite&&x.circuit&&dbfs(x.phrasePeak)<=kMaxPhrasePeakDbfs&&dbfs(x.stressPeak)<kMaxStressPeakDbfs&&dbfs(x.windowRms)>=kLevelWindowLowDbfs&&dbfs(x.windowRms)<=kLevelWindowHighDbfs;
        bad+=!pass;
        std::printf("%s phrase_peak=%.6f window_rms=%.6f stress_peak=%.6f pass=%d\n",name,dbfs(x.phrasePeak),dbfs(x.windowRms),dbfs(x.stressPeak),pass);
    }
    return bad?1:0;
}
