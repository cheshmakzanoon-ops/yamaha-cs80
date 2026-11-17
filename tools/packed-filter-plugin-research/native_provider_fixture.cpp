#include "native_provider.hpp"
#include <fstream>
#include <iostream>
using namespace cs80_provider;
void put(std::vector<std::uint8_t>& b,std::size_t at,U value,std::size_t n){for(std::size_t i=0;i<n;++i)b[at+i]=static_cast<std::uint8_t>(value>>(i*8));}
std::vector<std::uint8_t> elf(){std::vector<std::uint8_t>b(8192);b[0]=0x7f;b[1]='E';b[2]='L';b[3]='F';b[4]=2;b[5]=1;b[6]=1;put(b,16,3,2);put(b,18,62,2);put(b,20,1,4);put(b,32,64,8);put(b,52,64,2);put(b,54,56,2);put(b,56,1,2);put(b,64,1,4);put(b,68,5,4);put(b,72,4096,8);put(b,80,4096,8);put(b,96,4096,8);put(b,104,8192,8);put(b,112,4096,8);return b;}
int main(int argc,char** argv){try{require(argc==2,"case");std::string name=argv[1];auto bytes=elf();std::string maps="101000-102000 r-xp 00001000 08:01 42  /ignored name\n";Stamp stamp{static_cast<U>(makedev(8,1)),42,8192};U base=0x100000;std::array<U,3> symbols{0x101000,0x101080,0x101fff};bool bad=true;
 if(name=="valid"||name=="aslr"||name=="stale-path-not-covered")bad=false;
 if(name=="aslr"){base=0x200000;symbols={0x201000,0x201080,0x201fff};maps="201000-202000 r-xp 00001000 08:01 42\n";}
 else if(name=="self-maps"){std::ifstream in("/proc/self/maps");std::string text((std::istreambuf_iterator<char>(in)),{});require(!parseMaps(text).empty(),"real self maps parsed");return 0;}
 else if(name=="elf-short")bytes.resize(63);
 else if(name=="elf-class")bytes[4]=1;
 else if(name=="elf-endian")bytes[5]=2;
 else if(name=="elf-ident-version")bytes[6]=2;
 else if(name=="elf-version")put(bytes,20,2,4);
 else if(name=="elf-machine")put(bytes,18,183,2);
 else if(name=="elf-type")put(bytes,16,2,2);
 else if(name=="elf-header-size")put(bytes,52,63,2);
 else if(name=="elf-entry-size")put(bytes,54,55,2);
 else if(name=="elf-count")put(bytes,56,65535,2);
 else if(name=="elf-ph-overflow")put(bytes,32,~U{0}-20,8);
 else if(name=="elf-section-extended")put(bytes,40,4096,8);
 else if(name=="elf-section-entry"){put(bytes,40,4096,8);put(bytes,60,1,2);put(bytes,58,63,2);}
 else if(name=="elf-section-overflow"){put(bytes,40,~U{0}-5,8);put(bytes,60,1,2);put(bytes,58,64,2);}
 else if(name=="elf-load-flags")put(bytes,68,9,4);
 else if(name=="elf-filesz")put(bytes,96,9000,8);
 else if(name=="elf-memory")put(bytes,104,2,8);
 else if(name=="elf-address-overflow")put(bytes,80,~U{0}-2,8);
 else if(name=="elf-align")put(bytes,112,3,8);
 else if(name=="elf-congruence")put(bytes,80,4097,8);
 else if(name=="elf-nonexec")put(bytes,68,4,4);
 else if(name=="elf-overlap"){put(bytes,56,2,2);std::copy_n(bytes.begin()+64,56,bytes.begin()+120);}
 else if(name=="maps-truncated")maps.pop_back();
 else if(name=="maps-empty")maps.clear();
 else if(name=="maps-offset-overflow")maps="101000-102000 r-xp fffffffffffff000 08:01 42\n";
 else if(name=="maps-unpaged")maps="101001-102000 r-xp 00001000 08:01 42\n";
 else if(name=="maps-inode-suffix")maps="101000-102000 r-xp 00001000 08:01 42x\n";
 else if(name=="maps-perms")maps="101000-102000 r-zp 00001000 08:01 42\n";
 else if(name=="maps-device")maps="101000-102000 r-xp 00001000 zz:01 42\n";
 else if(name=="maps-overflow")maps="101000-102000 r-xp 00001000 08:01 18446744073709551616\n";
 else if(name=="maps-overlap")maps+=maps;
 else if(name=="maps-reverse")maps="102000-101000 r-xp 00001000 08:01 42\n";
 else if(name=="maps-line-bound")maps+=std::string(16385,'x')+'\n';
 else if(name=="maps-byte-bound")maps=std::string(4*1024*1024+1,'x')+'\n';
 else if(name=="wrong-inode")stamp.inode=43;
 else if(name=="wrong-device")stamp.device=static_cast<U>(makedev(8,2));
 else if(name=="wrong-size")stamp.size=8193;
 else if(name=="wrong-base")base+=4096;
 else if(name=="wrong-offset")maps="101000-102000 r-xp 00000000 08:01 42\n";
 else if(name=="nonexec-map")maps="101000-102000 r--p 00001000 08:01 42\n";
 else if(name=="bss-export")symbols[0]=0x102000;
 else if(name=="before-base")symbols[0]=0x100;
 bool rejected=false;std::array<U,3> result{};
 try{auto layout=parseElf(bytes);auto parsed=parseMaps(maps);if(name=="preexisting")rejectExistingInode(parsed,stamp);else if(name=="stale-path-not-covered"){auto stale=parsed;stale[0].inode=99;rejectExistingInode(stale,stamp);}result=checkExportOffsets(layout,parsed,stamp,base,symbols);}catch(const std::exception&){rejected=true;}
 require(rejected==bad,"expected structural acceptance/rejection");if(!bad)require(result==std::array<U,3>{4096,4224,8191},"independent expected ELF offsets");return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
