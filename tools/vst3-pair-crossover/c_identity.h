// Separate binary: retain the Q clock brackets, never its stock-only authorization switch.
#pragma once
#define initialize unused_q_only_initializer_in_c_image
#include "q_clocks.h"
#undef initialize
namespace qclock {
inline void initialize(int lane,const std::string& scope,const struct stat& loaded) {
    static constexpr const char* stock="0ed90e446f1faa6153b0e2f76f074b7da62201be49f76fc9183375e9ff20edf2";
    static constexpr const char* fir="74f2507db4a7042a23a96b9a8c2249cc2f76d1f7f417ebb4838498d7f0578895";
    req(!std::getenv("CS80_CROSSOVER_Q_SEAL"),"a Q seal cannot launch the C worker");
    const auto* seal=std::getenv("CS80_DISTINCT_C_SEAL");
    if(!seal)return; // exact same binary supports isolated and mixed-pair dry admission
    const std::string h=seal;
    req(h.size()==64 && h.find_first_not_of("0123456789abcdef")==std::string::npos &&
        scope=="full" && (lane==0||lane==1),"invalid C-stage authorization");
    const auto* kind=std::getenv("CS80_DISTINCT_C_PAIR");
    const auto* alane=std::getenv("CS80_DISTINCT_C_BASE_LANE");
    const auto* identity=std::getenv("CS80_DISTINCT_C_IMAGE");
    req(kind&&alane&&identity&&(std::string(kind)=="S"||std::string(kind)=="C")&&
        (std::string(alane)=="0"||std::string(alane)=="1"),"invalid C/S physical mapping");
    const char* expected=std::string(kind)=="C" && lane!=(*alane-'0') ? fir : stock;
    req(std::string(identity)==std::to_string(loaded.st_dev)+":"+std::to_string(loaded.st_ino)+":"+
        std::to_string(loaded.st_size)+":"+expected,"C-stage loaded wrong physical image for lane/role");
    enabled=true;
}
}
