// A26 documentation-only diagnostic. Reuses the existing LibraryAudit measure
// and its existing limits; it neither calls its calibration entry point nor
// introduces an acoustic-realism criterion for original electrical tone tabs.
#define main cs80_full_library_audit_not_run
#include "tests/LibraryAuditTests.cpp"
#undef main

#include <bit>
#include <fstream>
#include <iomanip>

int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    const std::string output = argv[1];
    std::ofstream columns(output + "/source-columns.csv");
    columns << "host_index,rank,selector,board,column,name,route,unknown_mask,inferred_mask,unknown_count,hardware_measured\n";
    for (int selector = 0; selector < 11; ++selector)
        for (int rank = 0; rank < 2; ++rank)
        {
            source_factory::ToneControls tone;
            if (!source_factory::compileTone(rank, selector, tone)) return 3;
            const auto& a = *source_factory::address(rank, selector);
            const auto& p = tone.provenance;
            columns << 2 * selector + rank << ',' << rank << ',' << selector << ','
                    << a.board << ',' << a.column << ',' << a.name << ','
                    << (p.unknownCells == 0 ? "source_complete" : "source_inferred") << ','
                    << p.unknownCells << ',' << p.inferredCells << ','
                    << std::popcount(p.unknownCells) << ',' << p.hardwareMeasured << '\n';
        }
    std::ofstream stacks(output + "/representative-stacks.csv");
    stacks << "name,rate,phrase_peak_dbfs,loudest_50ms_rms_dbfs,eight_voice_peak_dbfs,finite,source_performance,existing_limits_pass\n";
    stacks << std::setprecision(10);
    int failures = 0;
    for (const char* name : {"Tone I: String 1", "Tone II: String 4",
                             "Tone I: E. Piano", "Tone II: Harpsichord 2",
                             "Tone II: Organ 2", "Tone II: Guitar 2"})
    {
        const auto& bank = programLibrary();
        const auto p = std::find_if(bank.begin(), bank.end(),
            [name](const auto& value) { return value.name == name; });
        if (p == bank.end()) return 4;
        const auto level = measure(*p);
        const double phrase = dbfs(level.phrasePeak), rms = dbfs(level.windowRms);
        const double stress = dbfs(level.stressPeak);
        const bool passed = level.finite && level.circuit
            && phrase <= kMaxPhrasePeakDbfs && stress < kMaxStressPeakDbfs
            && rms >= kMinWindowRmsDbfs
            && rms >= kLevelWindowLowDbfs && rms <= kLevelWindowHighDbfs;
        failures += !passed;
        stacks << name << ',' << kSampleRate << ',' << phrase << ',' << rms << ','
               << stress << ',' << level.finite << ',' << level.circuit << ',' << passed << '\n';
    }
    std::printf("A26: 22 source columns; 6 representative existing stack contracts; %d failures\n", failures);
    return failures ? 1 : 0;
}
