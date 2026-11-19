// Test host only. Linux/x86-64 own-thread statistical PC sampling.
// No unwind, clock reads, I/O, allocation or symbolization in the handler.
#pragma once
#if !defined(__linux__) || !defined(__x86_64__)
#error "This diagnostic needs Linux x86-64 ucontext, thread CPU timers and executable ELF mappings"
#endif
#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <dlfcn.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <link.h>
#include <map>
#include <pthread.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <ucontext.h>
#include <unistd.h>
#include <vector>

namespace pcs {
using Word = std::uintptr_t;
enum Region : unsigned { outside, calibration, callback, wakeFirst };
constexpr unsigned capacity = 32768;
struct Sample { std::atomic<Word> pc{}; std::atomic<unsigned> region{}; };
static_assert(std::atomic<Word>::is_always_lock_free && std::atomic<unsigned>::is_always_lock_free);
inline std::array<Sample, capacity> samples{};
inline std::atomic<unsigned> region{outside}, used{}, outsideCount{}, overflow{}, overrun{}, foreign{};
inline int cookie;
inline void mark(Region r) { region.store(r, std::memory_order_relaxed); std::atomic_signal_fence(std::memory_order_seq_cst); }
inline void handler(int, siginfo_t* info, void* ctx) noexcept {
    if (info->si_code != SI_TIMER || info->si_value.sival_ptr != &cookie) {
        foreign.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    if (info->si_overrun > 0) overrun.fetch_add(unsigned(info->si_overrun), std::memory_order_relaxed);
    const unsigned r = region.load(std::memory_order_relaxed);
    if (r == outside) { outsideCount.fetch_add(1, std::memory_order_relaxed); return; }
    const unsigned i = used.load(std::memory_order_relaxed);
    if (i == capacity) { overflow.fetch_add(1, std::memory_order_relaxed); return; }
    samples[i].region.store(r, std::memory_order_relaxed);
    samples[i].pc.store(Word(static_cast<ucontext_t*>(ctx)->uc_mcontext.gregs[REG_RIP]), std::memory_order_relaxed);
    used.store(i + 1, std::memory_order_relaxed);
}
inline void ck(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }

// Dedicated short-lived host process; this signal must otherwise be unused.
// Unlike a process-wide ITIMER_PROF, CPU time accrues *and* the signal is
// delivered on the thread that calls IAudioProcessor::process.
class Sampler {
    int signo = SIGRTMIN + 5;
    struct sigaction previous{};
    timer_t timer{};
    bool active = false;
public:
    explicit Sampler(unsigned periodUs) {
        if (!periodUs) return; // zero-instrument control: no signals or timer at all
        sigset_t mask{}, pending{};
        ck(signo <= SIGRTMAX && pthread_sigmask(SIG_SETMASK, nullptr, &mask) == 0 && !sigismember(&mask, signo)
           && sigpending(&pending) == 0 && !sigismember(&pending, signo), "sampling signal unavailable");
        ck(sigaction(signo, nullptr, &previous) == 0 && previous.sa_handler == SIG_DFL, "sampling signal occupied");
        struct sigaction action{}; action.sa_sigaction = handler; action.sa_flags = SA_SIGINFO | SA_RESTART;
        sigemptyset(&action.sa_mask);
        ck(sigaction(signo, &action, nullptr) == 0, "cannot install sampling handler");
        sigevent ev{}; ev.sigev_notify = SIGEV_THREAD_ID; ev.sigev_signo = signo;
        ev._sigev_un._tid = gettid(); ev.sigev_value.sival_ptr = &cookie;
        if (timer_create(CLOCK_THREAD_CPUTIME_ID, &ev, &timer) != 0) {
            sigaction(signo, &previous, nullptr); throw std::runtime_error("cannot create callback-thread CPU timer");
        }
        itimerspec time{}; time.it_value = time.it_interval = {time_t(periodUs / 1000000), long(periodUs % 1000000) * 1000L};
        if (timer_settime(timer, 0, &time, nullptr) != 0) {
            timer_delete(timer); sigaction(signo, &previous, nullptr); throw std::runtime_error("cannot arm callback-thread CPU timer");
        }
        active = true;
    }
    Sampler(const Sampler&) = delete;
    void stop() {
        if (!active) return;
        mark(outside);
        sigset_t one{}, old{}; sigemptyset(&one); sigaddset(&one, signo);
        ck(pthread_sigmask(SIG_BLOCK, &one, &old) == 0, "cannot block sampler");
        ck(timer_delete(timer) == 0, "cannot stop sampler");
        timespec zero{}; while (sigtimedwait(&one, nullptr, &zero) >= 0 || errno == EINTR) {}
        ck(errno == EAGAIN && sigaction(signo, &previous, nullptr) == 0 &&
           pthread_sigmask(SIG_SETMASK, &old, nullptr) == 0, "cannot restore signal disposition");
        active = false;
    }
    ~Sampler() { if (active) { try { stop(); } catch (...) { std::terminate(); } } }
};

struct Range { Word begin, end, bias; std::string name; };
struct Images { Word pluginEntry{}; std::string pluginBuildId; std::vector<Range> ranges; };
inline int readImage(dl_phdr_info* info, std::size_t, void* ptr) {
    auto& out = *static_cast<Images*>(ptr);
    bool isPlugin = false;
    const Word base = info->dlpi_addr;
    for (unsigned i=0;i<info->dlpi_phnum;++i) {
        const auto& h = info->dlpi_phdr[i];
        if (h.p_type == PT_LOAD && (h.p_flags & PF_X) && base+h.p_vaddr <= out.pluginEntry &&
            out.pluginEntry < base+h.p_vaddr+h.p_memsz) isPlugin = true;
    }
    const std::string name = isPlugin ? "@plugin" : !*info->dlpi_name ? "@host" : std::filesystem::path(info->dlpi_name).filename().string();
    for (unsigned i=0;i<info->dlpi_phnum;++i) {
        const auto& h=info->dlpi_phdr[i];
        if (h.p_type == PT_LOAD && (h.p_flags & PF_X)) out.ranges.push_back({base+h.p_vaddr, base+h.p_vaddr+h.p_memsz, base, name});
        if (!isPlugin || h.p_type != PT_NOTE) continue;
        const auto* p = reinterpret_cast<const unsigned char*>(base+h.p_vaddr);
        const auto* end = p+h.p_filesz;
        while (std::size_t(end-p) >= sizeof(ElfW(Nhdr))) {
            ElfW(Nhdr) note{}; std::memcpy(&note,p,sizeof(note)); p+=sizeof(note);
            const auto namesz=(std::size_t(note.n_namesz)+3)&~std::size_t(3);
            const auto descsz=(std::size_t(note.n_descsz)+3)&~std::size_t(3);
            if (namesz > std::size_t(end-p) || descsz > std::size_t(end-p)-namesz) break;
            if (note.n_type == NT_GNU_BUILD_ID && note.n_namesz == 4 && std::memcmp(p,"GNU",4)==0) {
                std::ostringstream id;
                for(unsigned j=0;j<note.n_descsz;++j) id<<std::hex<<std::setfill('0')<<std::setw(2)<<unsigned(p[namesz+j]);
                out.pluginBuildId=id.str();
            }
            p+=namesz+descsz;
        }
    }
    return 0;
}
inline void write(const std::filesystem::path& dir, void* entry) {
    Images images{reinterpret_cast<Word>(entry),{},{}};
    ck(entry && dl_iterate_phdr(readImage,&images)==0 && !images.pluginBuildId.empty(), "could not identify loaded module build ID");
    std::map<std::tuple<unsigned,std::string,Word>,unsigned> counts;
    for (unsigned i=0;i<used.load();++i) {
        const Word pc=samples[i].pc.load(); auto it=std::find_if(images.ranges.begin(),images.ranges.end(),
                                          [pc](const Range& r){return pc>=r.begin && pc<r.end;});
        if(it==images.ranges.end()) ++counts[{samples[i].region.load(),"@unknown",0}];
        else ++counts[{samples[i].region.load(),it->name,pc-it->bias}]; // ELF vaddr, no ASLR addresses
    }
    std::ofstream rows(dir/"pcs.tsv"); rows<<"region\tmodule\telf_vaddr\tcount\n";
    for(const auto& [key,n]:counts) {
        const auto& [r,image,vaddr]=key;
        rows<<r<<'\t'<<image<<'\t'<<std::hex<<vaddr<<std::dec<<'\t'<<n<<'\n';
    }
    std::ofstream meta(dir/"sampler.json");
    meta<<"{\"plugin_build_id\":\""<<images.pluginBuildId<<"\",\"recorded\":"<<used.load()
        <<",\"outside\":"<<outsideCount.load()<<",\"dropped\":"<<overflow.load()
        <<",\"timer_overruns\":"<<overrun.load()<<",\"foreign_signals\":"<<foreign.load()<<"}\n";
    rows.close();meta.close();
    ck(bool(rows)&&bool(meta)&&overflow.load()==0&&foreign.load()==0, "invalid or incomplete PC capture");
}
} // namespace pcs
