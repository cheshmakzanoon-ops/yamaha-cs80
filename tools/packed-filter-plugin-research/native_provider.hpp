#pragma once
// Pure structural Linux/x86_64 checks. NOT verified loaded-provider authority.
#include "native_observer_support.hpp"
#include <charconv>
#include <span>
#include <sys/sysmacros.h>
namespace cs80_provider {
using cs80_observer::require;
using U=std::uint64_t;
inline U add(U a,U b){require(b<=std::numeric_limits<U>::max()-a,"provider arithmetic overflow");return a+b;}
struct Stamp {U device,inode,size;};
struct Mapping {U begin,end,offset,major,minor,inode;bool executable;};
struct Segment {U offset,address,fileSize,memorySize;};
struct Layout {U fileSize;std::vector<Segment> executable;};
inline U number(std::string_view s,int base){U v=0;require(!s.empty(),"empty numeric field");const auto [p,e]=std::from_chars(s.data(),s.data()+s.size(),v,base);require(e==std::errc{} && p==s.data()+s.size(),"invalid/overflow numeric field");return v;}
inline std::vector<Mapping> parseMaps(std::string_view text){
    require(!text.empty() && text.size()<=4*1024*1024 && text.back()=='\n',"bounded complete maps text required");
    std::vector<Mapping> out;std::size_t pos=0;
    while(pos<text.size()){
        require(out.size()<32768,"maps count bound");auto end=text.find('\n',pos);require(end!=text.npos && end>pos && end-pos<=16384,"maps line bound");auto line=text.substr(pos,end-pos);pos=end+1;
        std::array<std::string_view,5> field{};std::size_t cursor=0;
        for(auto& f:field){while(cursor<line.size() && line[cursor]==' ')++cursor;const auto begin=cursor;while(cursor<line.size() && line[cursor]!=' ')++cursor;f=line.substr(begin,cursor-begin);require(!f.empty(),"missing maps field");}
        const auto dash=field[0].find('-'),colon=field[3].find(':');require(dash!=field[0].npos && colon!=field[3].npos,"maps separators");
        const auto p=field[1];require(p.size()==4 && (p[0]=='r'||p[0]=='-') && (p[1]=='w'||p[1]=='-') && (p[2]=='x'||p[2]=='-') && (p[3]=='p'||p[3]=='s'),"maps permissions");
        Mapping m{number(field[0].substr(0,dash),16),number(field[0].substr(dash+1),16),number(field[2],16),number(field[3].substr(0,colon),16),number(field[3].substr(colon+1),16),number(field[4],10),p[2]=='x'};
        require(m.begin<m.end && m.begin%4096==0 && m.end%4096==0 && m.offset%4096==0,"maps ordered page ranges");add(m.offset,m.end-m.begin);
        require(out.empty() || out.back().end<=m.begin,"maps overlap/order");out.push_back(m);
    }
    return out;
}
inline Layout parseElf(std::span<const std::uint8_t> b){
    require(b.size()>=64 && b.size()<=256*1024*1024,"bounded ELF bytes");
    const auto read=[&](std::size_t at,std::size_t n){require(at<=b.size() && n<=b.size()-at,"truncated ELF field");U v=0;for(std::size_t i=0;i<n;++i)v|=U(b[at+i])<<(i*8);return v;};
    require(b[0]==0x7f && b[1]=='E' && b[2]=='L' && b[3]=='F' && b[4]==2 && b[5]==1 && b[6]==1,"ELF64 little-endian version1");
    require(read(16,2)==3 && read(18,2)==62 && read(20,4)==1 && read(52,2)==64 && read(54,2)==56,"exact x86_64 ET_DYN header");
    const auto ph=read(32,8),count=read(56,2);require(count>0 && count<=128 && ph>=64 && add(ph,count*56)<=b.size(),"program table bounds/extended counts unsupported");
    const auto sections=read(40,8),sectionSize=read(58,2),sectionCount=read(60,2),names=read(62,2);
    require(sectionCount!=0xffff && names!=0xffff,"extended section counts unsupported");
    if(sections)require(sectionCount>0 && sectionSize==64 && add(sections,sectionCount*64)<=b.size() && names<sectionCount,"section table bounds/extended forms unsupported");
    else require(sectionCount==0 && names==0 && (sectionSize==0 || sectionSize==64),"absent section table metadata");
    Layout layout{b.size(),{}};
    for(U i=0;i<count;++i){const auto at=static_cast<std::size_t>(ph+i*56);if(read(at,4)!=1)continue;
        const auto flags=read(at+4,4),offset=read(at+8,8),address=read(at+16,8),files=read(at+32,8),memory=read(at+40,8),align=read(at+48,8);
        require((flags&~U{7})==0,"unknown PT_LOAD flags");
        require(files<=memory && add(offset,files)<=b.size(),"ELF file range");add(address,memory);
        require(align<=1 || ((align&(align-1))==0 && address%align==offset%align),"ELF segment alignment");
        if(flags&1){require(files>0,"empty executable segment");for(const auto& prior:layout.executable)require(add(address,memory)<=prior.address || add(prior.address,prior.memorySize)<=address,"ambiguous executable segment overlap");layout.executable.push_back({offset,address,files,memory});}
    }
    require(!layout.executable.empty(),"executable PT_LOAD required");return layout;
}
inline bool sameFile(const Mapping& m,const Stamp& s){return m.inode!=0 && m.inode==s.inode && m.major==static_cast<U>(::major(static_cast<dev_t>(s.device))) && m.minor==static_cast<U>(::minor(static_cast<dev_t>(s.device)));}
inline void rejectExistingInode(const std::vector<Mapping>& maps,const Stamp& stamp){require(stamp.inode!=0 && stamp.size>0,"nonzero structural file stamp");for(const auto& m:maps)require(!sameFile(m,stamp),"target inode already mapped");}
// Canonical path/provider preload exclusion is a SEPARATE mandatory gate. A stale
// mapping of a replaced pathname has another inode and passes this inode check.
inline std::array<U,3> checkExportOffsets(const Layout& elf,const std::vector<Mapping>& maps,const Stamp& stamp,U base,const std::array<U,3>& exports){
    require(stamp.inode && stamp.size==elf.fileSize && base%4096==0,"structural stamp/base mismatch");std::array<U,3> offsets{};
    for(std::size_t i=0;i<exports.size();++i){const auto address=exports[i];require(address>=base,"export before load base");const auto relative=address-base;const Segment* seg=nullptr;
        for(const auto& s:elf.executable)if(relative>=s.address && relative-s.address<s.fileSize){require(!seg,"ambiguous export segment");seg=&s;}
        require(seg,"export not in executable file-backed segment");const Mapping* map=nullptr;
        for(const auto& m:maps)if(address>=m.begin && address<m.end){require(!map,"ambiguous export mapping");map=&m;}
        require(map && map->executable && sameFile(*map,stamp),"export mapped provider differs");
        const auto expected=add(seg->offset,relative-seg->address),observed=add(map->offset,address-map->begin);require(expected==observed && observed<stamp.size,"export file offset differs");offsets[i]=observed;
    }
    return offsets;
}
}
