#include "core/FactoryPresets.h"
#include <bit>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <iostream>
using namespace cs80;
void u64(std::ostream& f, std::uint64_t n) { for (int i=0;i<8;++i) f.put(static_cast<char>((n>>(i*8))&255)); }
void str(std::ostream& f, const std::string& s) { u64(f,s.size()); f.write(s.data(),s.size()); }
int main(int argc,char** argv) {
 if(argc!=2) return 2;
 auto out=std::filesystem::path(argv[1]); std::filesystem::create_directories(out);
 std::ofstream b(out/"legacy.bin",std::ios::binary); const auto& bank=legacyLibrary();
 u64(b,bank.size()); u64(b,P_numParams);
 for(const auto& p:bank) {
  str(b,p.name);str(b,p.category);str(b,p.description);u64(b,static_cast<int>(p.library));
  u64(b,p.tags.size());for(const auto& tag:p.tags)str(b,tag);
  for(float value:p.values.v) {auto bits=std::bit_cast<std::uint32_t>(value); for(int i=0;i<4;++i)b.put(static_cast<char>((bits>>(i*8))&255));}
 }
 std::ofstream m(out/"midi.bin",std::ios::binary);
 for(int msb=0;msb<3;++msb)for(int lsb=0;lsb<128;++lsb)for(int pc=0;pc<128;++pc)u64(m,static_cast<std::int64_t>(programIndexForMidi(msb,lsb,pc)));
 int memory=0; for(int local=0;local<CP_count;++local) if(local!=CP_feet&&local!=CP_pan&&local!=CP_hpfOn&&local!=CP_lpfOn)++memory;
 std::cout<<"{\"legacy_count\":"<<bank.size()<<",\"curated_count\":"<<curatedProgramCount()<<",\"host_program_count\":"<<programLibrary().size()<<",\"core_parameters\":"<<P_numParams<<",\"memory_controls\":"<<memory<<",\"host_parameter_count_by_layout\":"<<(P_numParams+2+4*memory)<<",\"midi_cases\":49152}\n";
 return b&&m?0:1;
}
