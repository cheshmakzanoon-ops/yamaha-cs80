#include "reference/MemorySparseCompiler.h"
#include "experimental/MemorySparseNetwork.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <string>

namespace sp = cs80::experimental::memory_sparse;
namespace ref = cs80::reference::memory_panel;
namespace
{
template<class T> void materialize(T& object)
{
#if defined(__GNUC__) || defined(__clang__)
    asm volatile("" : "+m"(object) : : "memory");
#else
#error "This opt-in timing probe requires whole-object GCC/Clang materialization"
#endif
}
ref::ReceiverDrives drive(int n)
{
    ref::ReceiverDrives out; out.masterResVolts = (n % 17) * (10.0 / 16.0);
    for (int voice = 0; voice < 8; ++voice)
    {
        auto& v = out.voices[static_cast<std::size_t>(voice)];
        v.envelopeVolts = 5.0 * std::sin(0.013 * n + 0.2 * voice);
        v.velocity = (voice + 1) / 8.0;
        v.pressure = 0.5 + 0.5 * std::sin(0.007 * n + 0.1 * voice);
        v.keyboardCutoffVolts = (voice - 3.5) * 0.3;
        v.keyboardLevelVolts = voice * 0.2;
    }
    return out;
}
}
int main(int argc, char** argv)
{
    if (argc != 2 && argc != 4) { std::fprintf(stderr,"usage: memory-sparse-probe output.csv [mode0..4 trial0..6]\n"); return 2; }
    int selectedMode = -1, selectedTrial = -1;
    if (argc == 4)
    {
        char* end = nullptr;
        const long mode = std::strtol(argv[2],&end,10);
        if (end == argv[2] || *end != '\0' || mode < 0 || mode > 4) return 2;
        const long trial = std::strtol(argv[3],&end,10);
        if (end == argv[3] || *end != '\0' || trial < 0 || trial > 6) return 2;
        selectedMode = static_cast<int>(mode); selectedTrial = static_cast<int>(trial);
    }
    std::ofstream out(argv[1]); if (!out) return 2;
    out << std::setprecision(12) << "scenario,pot_ohms,shape,trial,iterations,mean_ns,failures,max_newton_steps,max_node_bound_V,mean_process_cpu_ns\n";
    int errors = 0;
    for (int fixture = 0; fixture < 3; ++fixture)
    {
        ref::Setup source; source.selector = fixture == 2 ? 12 : 11;
        source.electrical.potOhms = fixture == 0 ? 50000.0 : 100000.0;
        const int shape = fixture == 1 ? 4 : 5;
        for (std::size_t bank = 0; bank < 3; ++bank) for (std::size_t row = 0; row < 26; ++row)
            source.banks[bank][row] = shape == 4 ? 1.0 : ((row * 7 + bank * 3 + 2) % 9) / 8.0;
        sp::Prepared prepared; if (!ref::compileSparse(source,44,prepared)) return 1;
        constexpr int sequence = 128;
        std::array<ref::ReceiverDrives,sequence> physical;
        std::array<sp::Inputs,sequence> inputs;
        std::array<ref::ReceiverInjection,sequence> injection;
        for (int i = 0; i < sequence; ++i)
        {
            physical[static_cast<std::size_t>(i)] = drive(i);
            if (!ref::sparseInputs(prepared,physical[static_cast<std::size_t>(i)],inputs[static_cast<std::size_t>(i)])) return 1;
            injection[static_cast<std::size_t>(i)] = ref::receiverInjection(physical[static_cast<std::size_t>(i)]);
        }
        sp::State base; if (!sp::update(prepared,inputs[0],nullptr,base).accepted()) return 1;
        for (int trial = 0; trial < 7; ++trial) for (int position = 0; position < 5; ++position)
        {
            const int mode = trial % 2 ? 4-position : position;
            if (selectedMode >= 0 && (mode != selectedMode || trial != selectedTrial)) continue;
            const int count = 128; // Identical input indices for paired scenario comparisons.
            const char* names[] {"dense_cold_full_nodes", "sparse_cold_certified", "sparse_warm_certified", "residual_certificate_only", "fixed_coefficient_compile"};
            sp::State state = base; int failed = 0, steps = 0; double bound = 0.0;
            const auto cpuStart = std::clock();
            const auto start = std::chrono::steady_clock::now();
            for (int i = 0; i < count; ++i)
            {
                const auto index = static_cast<std::size_t>(i % sequence);
                if (mode == 0)
                {
                    auto setup = source;
                    for (std::size_t r = 0; r < 26; ++r) setup.injectedAmps[r] += injection[index].constantAmps[r];
                    ref::NodeSnapshot nodes;
                    auto result = ref::solve(setup,nullptr,&injection[index].feedback,&nodes);
                    materialize(nodes); materialize(result);
                    failed += !result.converged || !nodes.valid;
                    steps = std::max(steps,result.iterations);
                }
                else if (mode == 1 || mode == 2)
                {
                    auto result = sp::update(prepared,inputs[index],mode == 1 ? nullptr : &state,state);
                    materialize(state); materialize(result);
                    failed += !result.accepted(); steps = std::max(steps,result.iterations);
                    bound = std::max(bound,result.certificate.nodeErrorUpperVolts);
                }
                else if (mode == 3)
                {
                    auto certificate = sp::certify(prepared,inputs[0],base,1e-8);
                    materialize(certificate); failed += !certificate.accepted();
                    bound = std::max(bound,certificate.nodeErrorUpperVolts);
                }
                else
                {
                    sp::Prepared graph; const bool good = ref::compileSparse(source,44,graph);
                    materialize(graph); failed += !good;
                }
            }
            const double ns = std::chrono::duration<double,std::nano>(std::chrono::steady_clock::now()-start).count()/count;
            const auto cpuEnd = std::clock();
            if (cpuStart == static_cast<std::clock_t>(-1) || cpuEnd < cpuStart) return 2;
            const double cpuNs = static_cast<double>(cpuEnd - cpuStart) * (1e9 / CLOCKS_PER_SEC) / count;
            out << names[mode] << ',' << source.electrical.potOhms << ',' << shape << ',' << trial << ',' << count << ',' << ns << ',' << failed << ',' << steps << ',' << bound << ',' << cpuNs << '\n';
            errors += failed;
        }
    }
    std::printf("Sparse timing: full-node outputs materialized; %d failed evaluations. Shared-machine wall time, not WCET or whole-instrument CPU.\n",errors);
    return errors == 0 ? 0 : 1;
}
