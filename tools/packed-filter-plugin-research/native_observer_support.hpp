#pragma once
// Preparation-only support. No entry point, module loading, or experiment launch.
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <type_traits>
#include <array>
#include <exception>
#include <vector>

namespace cs80_observer {
inline void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
class Bytes {
public:
    std::vector<std::uint8_t> data;
    void u32(std::uint32_t v) { for (int n=0;n<4;++n) data.push_back(static_cast<std::uint8_t>(v>>(8*n))); }
    void u64(std::uint64_t v) { for (int n=0;n<8;++n) data.push_back(static_cast<std::uint8_t>(v>>(8*n))); }
    void raw(const void* p,std::size_t n) {
        require(p || n==0,"null byte span");
        if(n) { const auto* b=static_cast<const std::uint8_t*>(p);data.insert(data.end(),b,b+n); }
    }
    void blob(const void* p,std::size_t n) { u64(n);raw(p,n); }
    void text(std::string_view s) { blob(s.data(),s.size()); }
    void f64(double v) { require(std::isfinite(v),"nonfinite numeric observation");u64(std::bit_cast<std::uint64_t>(v)); }
    void pcm(float v) { require(std::isfinite(v),"nonfinite PCM");u32(std::bit_cast<std::uint32_t>(v)); }
    template<class T,std::size_t N> void terminated(const T (&s)[N]) {
        std::size_t n=0;while(n<N && s[n]!=0)++n;
        require(n<N,"unterminated SDK text");u64(n);
        for(std::size_t i=0;i<n;++i) {
            const auto c=static_cast<std::make_unsigned_t<T>>(s[i]);
            for(std::size_t k=0;k<sizeof(T);++k)data.push_back(static_cast<std::uint8_t>(c>>(8*k)));
        }
    }
};

class StateBuffer {
public:
    static constexpr std::size_t limit=64u*1024u*1024u;
    std::vector<std::uint8_t> bytes;
    std::size_t position=0;
    std::size_t read(void* dest,std::size_t n) {
        require(dest || !n,"null state read");require(position<=bytes.size(),"invalid state cursor");
        n=std::min(n,bytes.size()-position);
        if(n) { std::memcpy(dest,bytes.data()+position,n); }
        position+=n;return n;
    }
    void write(const void* source,std::size_t n) {
        require(source || !n,"null state write");require(position<=limit && n<=limit-position,"state size bound");
        bytes.resize(std::max(bytes.size(),position+n));
        if(n) { std::memcpy(bytes.data()+position,source,n); }
        position+=n;
    }
    void seek(std::int64_t offset,int mode) {
        require(mode>=0 && mode<=2,"invalid seek mode");
        const auto base=static_cast<std::int64_t>(mode==0?0:mode==1?position:bytes.size());
        require(offset>=-base && offset<=static_cast<std::int64_t>(bytes.size())-base,"state seek bounds");
        position=static_cast<std::size_t>(base+offset);
    }
};

struct CloseResult { std::string operation; bool ok=false; std::exception_ptr error; };
// Registration may allocate. Once close begins, all action/result slots already
// exist; no name/error/result allocation can abandon the remaining cleanup.
// Destruction is best-effort only, not evidence of successful checked close.
class CheckedClose {
    std::array<std::function<bool()>,32> actions{};
    bool attempted=false, accepted=false;
public:
    std::array<CloseResult,32> results{};
    std::size_t resultCount=0;
    CheckedClose()=default;
    CheckedClose(const CheckedClose&)=delete;
    CheckedClose& operator=(const CheckedClose&)=delete;
    void add(std::string name,std::function<bool()> action) {
        require(!attempted,"close already attempted");
        require(resultCount<actions.size(),"close operation bound");
        require(!name.empty() && bool(action),"close operation required");
        require(std::none_of(results.begin(),results.begin()+resultCount,[&](const auto& r){return r.operation==name;}),"duplicate close operation");
        results[resultCount].operation=std::move(name);
        actions[resultCount]=std::move(action);
        ++resultCount;
    }
    void close() {
        require(!attempted,"close retry forbidden");attempted=true;bool ok=true;
        for(std::size_t i=0;i<resultCount;++i) {
            auto& r=results[i];
            try { r.ok=actions[i](); } catch(...) { r.error=std::current_exception(); }
            ok=ok && r.ok;
        }
        accepted=ok;require(ok,"checked close failed");
    }
    bool successful() const { return attempted && accepted; }
    ~CheckedClose() { if(!attempted) { try { close(); } catch(...) {} } }
};
} // namespace cs80_observer
