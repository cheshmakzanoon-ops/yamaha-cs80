#include "tools/FloatWav.h"
#include <iostream>
#include <string>
#include <vector>
// Isolated helper contract probe; same owned-file cleanup contract as both CLIs.
int main(int argc, char** argv) {
    if(argc != 3) return 2;
    std::vector<std::filesystem::path> owned;
    try {
        const std::string mode=argv[2];
        auto sample=[&](std::size_t i) -> double {
            if(mode=="nonfinite") return std::numeric_limits<double>::infinity();
            if(mode=="overflow") return std::numeric_limits<double>::max();
            const double values[]={0.,-0.,.25,-.25,2.,-2.,1.00000001};
            return values[i%7];
        };
        cs80::tools::writeFloatWav(argv[1],mode=="size" ? std::size_t{0xffffffff} : 700,
                                 mode=="rate" ? 0 : 48000,1,owned,sample);
        return 0;
    } catch(const std::exception& e) {
        for(const auto& path:owned) { std::error_code ignored; std::filesystem::remove(path,ignored); }
        std::cerr<<e.what()<<'\n'; return 1;
    }
}
