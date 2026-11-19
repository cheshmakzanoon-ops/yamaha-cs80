// Included in a *separate* executable. No candidate treatment is supported.
#pragma once
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <time.h>
#include <sys/resource.h>
#include <sys/stat.h>

namespace qclock {
struct Clock {timespec w0{},c0{},c1{},w1{};};
struct Region {rusage ru{};timespec wall{};};
inline bool enabled=false;
inline std::array<Clock,576> real{};
inline std::array<Clock,128> empty{};
inline unsigned count=0,emptyCount=0;
inline Region before{};
inline void req(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
inline std::uint64_t ns(timespec t) {
    req(t.tv_sec>=0 && t.tv_nsec>=0 && t.tv_nsec<1000000000L,"invalid clock endpoint");
    return std::uint64_t(t.tv_sec)*1000000000ULL+std::uint64_t(t.tv_nsec);
}
inline void initialize(int lane,const std::string& scope,const struct stat& loaded) {
    const auto* seal=std::getenv("CS80_CROSSOVER_Q_SEAL");
    if(!seal) return; // Supports a complete dry rehearsal with this exact measured-build image.
    const std::string h=seal;
    req(h.size()==64 && h.find_first_not_of("0123456789abcdef")==std::string::npos &&
        scope=="full" && (lane==0||lane==1),"invalid Q scope/seal");
    const auto* identity=std::getenv("CS80_CROSSOVER_Q_STOCK");
    req(identity && std::string(identity)==std::to_string(loaded.st_dev)+":"+std::to_string(loaded.st_ino)+":"+
        std::to_string(loaded.st_size)+":0ed90e446f1faa6153b0e2f76f074b7da62201be49f76fc9183375e9ff20edf2",
        "Q may load only its pinned physical stock image");
    enabled=true;
}
inline Region rc(){
    Region r;req(getrusage(RUSAGE_THREAD,&r.ru)==0 && clock_gettime(CLOCK_MONOTONIC,&r.wall)==0,"scene resource envelope");
    return r;
}
inline void beginScene(){
    count=emptyCount=0;
    if(enabled)before=rc();
}
inline void emptyBrackets() {
    if(!enabled)return;
    req(count==real.size() && emptyCount==0,"empty brackets must follow complete scene window");
    for(auto& x:empty){
        const int a=clock_gettime(CLOCK_MONOTONIC,&x.w0),b=clock_gettime(CLOCK_THREAD_CPUTIME_ID,&x.c0);
        asm volatile("" ::: "memory");
        const int c=clock_gettime(CLOCK_THREAD_CPUTIME_ID,&x.c1),d=clock_gettime(CLOCK_MONOTONIC,&x.w1);
        req(a==0&&b==0&&c==0&&d==0,"empty clock failure");++emptyCount;
    }
}
inline void finishScene(const std::filesystem::path& dir) {
    if(!enabled)return;
    auto after=rc();
    req(count==real.size() && emptyCount==empty.size(),"incomplete Q timing arrays");
    std::ofstream raw(dir/"clocks.csv"),emptyOut(dir/"empty.csv"),r(dir/"resources.csv");
    const auto dump=[](auto& file,const auto& values) {
        file<<"index,cpu0,cpu1,wall0,wall1\n";
        for(std::size_t i=0;i<values.size();++i){
            const auto& x=values[i];file<<i<<','<<ns(x.c0)<<','<<ns(x.c1)<<','<<ns(x.w0)<<','<<ns(x.w1)<<'\n';
        }
        file.close();req(bool(file),"Q raw clock write failed");
    };
    dump(raw,real);dump(emptyOut,empty);
    r<<"wall0,wall1,voluntary_switches,involuntary_switches,minor_faults,major_faults\n"
     <<ns(before.wall)<<','<<ns(after.wall)<<','<<after.ru.ru_nvcsw-before.ru.ru_nvcsw<<','
     <<after.ru.ru_nivcsw-before.ru.ru_nivcsw<<','<<after.ru.ru_minflt-before.ru.ru_minflt<<','
     <<after.ru.ru_majflt-before.ru.ru_majflt<<'\n';
    r.close();req(bool(r),"Q resource write failed");
}
}
