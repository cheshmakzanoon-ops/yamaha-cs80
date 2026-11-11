// Bounded offline conditional IG00151 diagnostics; never runs the audio engine.
#include "reference/IG00151Reference.h"
#include "core/Vca.h"
#include "tools/TruthInputSha256.h"
#include <array>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <locale>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
namespace ref = cs80::reference::ig00151;
namespace fs = std::filesystem;
namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
double decimal(std::string_view s) {
    std::size_t i=0, digits=0;
    if (i<s.size() && (s[i]=='+' || s[i]=='-')) ++i;
    const auto digit=[&](std::size_t j){return j<s.size() && s[j]>='0' && s[j]<='9';};
    while (digit(i)) { ++i; ++digits; }
    if (i<s.size() && s[i]=='.') { ++i; while (digit(i)) { ++i; ++digits; } }
    require(digits>0,"Expected ASCII decimal");
    if (i<s.size() && (s[i]=='e' || s[i]=='E')) {
        ++i; if (i<s.size() && (s[i]=='+' || s[i]=='-')) ++i;
        auto begin=i; while (digit(i)) ++i;
        require(i>begin,"Missing exponent digits");
    }
    require(i==s.size(),"Invalid decimal");
    if (s.front()=='+') s.remove_prefix(1);
    double result=0;
    auto parsed=std::from_chars(s.data(),s.data()+s.size(),result,std::chars_format::general);
    require(parsed.ec==std::errc{} && parsed.ptr==s.data()+s.size() && std::isfinite(result),"Unrepresentable decimal");
    return result;
}
int integer(std::string_view s, int low, int high) {
    require(!s.empty(),"Missing integer");
    for (char c:s) require(c>='0' && c<='9',"Expected unsigned integer digits");
    int value=0; auto p=std::from_chars(s.data(),s.data()+s.size(),value);
    require(p.ec==std::errc{} && p.ptr==s.data()+s.size() && value>=low && value<=high,"Integer outside bounds");
    return value;
}
std::string snapshot(const fs::path& path) {
    constexpr std::size_t limit=64*1024*1024;
    require(fs::is_regular_file(path),"Input must be a regular file");
    require(fs::file_size(path)<=limit,"Input exceeds 64 MiB");
    std::ifstream file(path,std::ios::binary); require(bool(file),"Cannot open input");
    std::string bytes; std::array<char,8192> buffer{};
    while(file) { file.read(buffer.data(),buffer.size()); auto n=static_cast<std::size_t>(file.gcount());
        require(bytes.size()+n<=limit,"Input exceeds 64 MiB"); bytes.append(buffer.data(),n); }
    require(file.eof() && !file.bad(),"Input read failed"); return bytes;
}
std::vector<ref::Drive> parse(const std::string& bytes,int count) {
    std::size_t pos=0;
    auto line=[&]() {
        auto end=bytes.find('\n',pos);
        require(end!=std::string::npos && end-pos<=256,"Missing newline or line exceeds 256 bytes");
        std::string_view value(bytes.data()+pos,end-pos); pos=end+1;
        if (!value.empty() && value.back()=='\r') value.remove_suffix(1);
        return value;
    };
    require(line()=="sample,input,lc,ei,envelope","Expected header sample,input,lc,ei,envelope");
    std::vector<ref::Drive> drives; drives.reserve(count);
    for (int n=0;n<count;++n) {
        auto row=line(); std::array<std::string_view,5> columns{};
        for (int c=0;c<4;++c) { auto comma=row.find(','); require(comma!=std::string_view::npos,"Missing CSV column");
            columns[c]=row.substr(0,comma); row.remove_prefix(comma+1); }
        columns[4]=row;
        require(columns[0]==std::to_string(n),"Expected canonical contiguous sample index");
        ref::Drive d{decimal(columns[1]),decimal(columns[2]),decimal(columns[3]),decimal(columns[4])};
        require(std::abs(d.input)<=8 && std::abs(d.lc)<=8 && std::abs(d.ei)<=8
                && d.envelope>=0 && d.envelope<=1,"Drive outside bounds");
        drives.push_back(d);
    }
    require(pos==bytes.size(),"Extra CSV rows"); return drives;
}
std::vector<ref::State> render(const std::vector<ref::Drive>& d, int rate,int steps,double initial) {
    std::vector<ref::State> trace; trace.reserve(d.size()); ref::State s{initial,0}; trace.push_back(s);
    const double h=1.0/(rate*static_cast<double>(steps));
    for(std::size_t i=1;i<d.size();++i) {
        for(int j=0;j<steps;++j) {
            s=ref::rk4(s,ref::interpolate(d[i-1],d[i],double(j)/steps),
                      ref::interpolate(d[i-1],d[i],double(j+1)/steps),h);
            require(std::isfinite(s.li) && std::isfinite(s.coupling),"Nonfinite solver state");
        }
        require(std::isfinite(ref::output(s,d[i])),"Nonfinite solver output"); trace.push_back(s);
    }
    return trace;
}
void close(std::ofstream& file) { file.flush(); require(bool(file),"Output write failed"); file.close(); require(bool(file),"Output close failed"); }
}
int main(int argc,char** argv) {
    fs::path owned;
    try {
        if(argc==2 && std::string_view(argv[1])=="--help") {
            std::cout<<"Conditional IG00151 model, not hardware truth.\n"
                     <<"Required: --input FILE --rate HZ --samples N --out NEW_DIR\n"
                     <<"Optional: --steps N (1..128, default 8), --initial-li X (default first LC)\n"
                     <<"--tolerance X (absolute h/h2 guard, default .001); --compare-realtime true\n"
                     <<"CSV: sample,input,lc,ei,envelope; N endpoints / N-1 piecewise-linear intervals.\n"
                     <<"Rate 8000..384000; duration <=10s; combined steps <=10000000.\n"
                     <<"Input/LC/EI/initial LI [-8,8], envelope [0,1]. Coupling starts at zero.\n";
            return 0;
        }
        std::map<std::string,std::string> args;
        for(int i=1;i<argc;i+=2) {
            const std::string key=argv[i];
            require(key=="--input" || key=="--rate" || key=="--samples" || key=="--out" || key=="--steps" || key=="--initial-li" || key=="--tolerance" || key=="--compare-realtime","Unknown option");
            require(i+1<argc,"Missing option value"); require(args.emplace(key,argv[i+1]).second,"Duplicate option");
        }
        for(auto key:{"--input","--rate","--samples","--out"}) require(args.contains(key) && !args[key].empty(),"Missing required option");
        const int rate=integer(args.at("--rate"),8000,384000), count=integer(args.at("--samples"),2,3840001);
        const int steps=args.contains("--steps") ? integer(args.at("--steps"),1,128) : 8;
        require(count-1<=10*rate,"Duration exceeds ten seconds");
        require(static_cast<long long>(count-1)*steps*3<=10000000,"Combined step budget exceeds ten million");
        const double tolerance=args.contains("--tolerance") ? decimal(args.at("--tolerance")) : .001;
        require(tolerance>0 && tolerance<=1,"Tolerance must be in (0,1]");
        const bool compare=args.contains("--compare-realtime");
        require(!compare || args.at("--compare-realtime")=="true","Comparison flag value must be true");
        fs::path out=args.at("--out");
        require(!fs::exists(fs::symlink_status(out)),"Output already exists (including symlink)");
        auto bytes=snapshot(args.at("--input")); auto drives=parse(bytes,count);
        double initial=args.contains("--initial-li") ? decimal(args.at("--initial-li")) : drives.front().lc;
        require(std::abs(initial)<=8,"Initial LI outside bounds");
        require(!compare || (initial==drives.front().lc && drives.front().envelope==0),"Comparison requires initial LI equal first LC and first envelope zero");
        const auto coarse=render(drives,rate,steps,initial), fine=render(drives,rate,2*steps,initial);
        double liDifference=0, couplingDifference=0, outputDifference=0;
        for(int n=0;n<count;++n) {
            liDifference=std::max(liDifference,std::abs(coarse[n].li-fine[n].li));
            couplingDifference=std::max(couplingDifference,std::abs(coarse[n].coupling-fine[n].coupling));
            outputDifference=std::max(outputDifference,std::abs(ref::output(coarse[n],drives[n])-ref::output(fine[n],drives[n])));
        }
        require(std::max({liDifference,couplingDifference,outputDifference})<=tolerance,"Time refinement exceeds tolerance; increase steps");
        std::vector<std::array<double,2>> realtime;
        double rtLiDifference=0, rtOutputDifference=0;
        if(compare) {
            cs80::Ig00151Vca vca; vca.prepare(rate); vca.reset(static_cast<float>(initial));
            for(int n=0;n<count;++n) {
                auto d=drives[n];
                const double value=vca.process(static_cast<float>(d.input),static_cast<float>(d.lc),
                                               static_cast<float>(d.ei),static_cast<float>(d.envelope));
                require(std::isfinite(value) && std::isfinite(vca.levelNode()),"Nonfinite production diagnostic");
                realtime.push_back({vca.levelNode(),value});
                rtLiDifference=std::max(rtLiDifference,std::abs(realtime.back()[0]-fine[n].li));
                rtOutputDifference=std::max(rtOutputDifference,std::abs(value-ref::output(fine[n],d)));
            }
        }
        require(fs::create_directory(out),"Cannot create output directory"); owned=out;
        std::ofstream copy(out/"stimulus.csv",std::ios::binary); require(bool(copy),"Cannot create stimulus");
        copy.write(bytes.data(),static_cast<std::streamsize>(bytes.size())); close(copy);
        std::ofstream trace(out/"trace.csv",std::ios::binary); require(bool(trace),"Cannot create trace");
        trace.imbue(std::locale::classic()); trace<<std::setprecision(17);
        trace<<"sample,time,input,lc,ei,envelope,li,gain,coupling,output";
        if(compare) trace<<",realtime_li,realtime_output";
        trace<<'\n';
        for(int n=0;n<count;++n) { auto d=drives[n]; auto s=fine[n];
            trace<<n<<','<<double(n)/rate<<','<<d.input<<','<<d.lc<<','<<d.ei<<','<<d.envelope<<','
                 <<s.li<<','<<ref::gain(s,d)<<','<<s.coupling<<','<<ref::output(s,d);
            if(compare) trace<<','<<realtime[n][0]<<','<<realtime[n][1];
            trace<<'\n'; }
        close(trace);
        std::ofstream manifest(out/"manifest.json",std::ios::binary); require(bool(manifest),"Cannot create manifest");
        manifest.imbue(std::locale::classic()); manifest<<std::setprecision(17);
        manifest<<"{\n  \"schema\": \"cs80-ig00151-reference-v1\",\n"
                <<"  \"scope\": \"conditional inferred VCA stage; not hardware or whole-note truth\",\n"
                <<"  \"forcing\": \"piecewise linear endpoints, no normalization\",\n"
                <<"  \"units\": {\"input_lc_li_output_coupling\": \"uncalibrated engine units\", \"ei\": \"octaves\", \"envelope_gain\": \"unitless\"},\n"
                <<"  \"assumptions\": \"documented LI branches/capacitor; inferred ideal-source tau, multiplier, offset/feedthrough and next-stage AC coupling\",\n"
                <<"  \"constant_convention\": \"independently declared float offset/feedthrough literals promoted to double; double time constants\",\n"
                <<"  \"initialization\": \"explicit LI or first LC; zero coupling; instantaneous output at t=0\",\n"
                <<"  \"solver\": \"double RK4, h and h/2; fine trace exported\",\n"
                <<"  \"refinement_meaning\": \"raw maximum endpoint differences, not rigorous bounds or hardware acceptance gate\",\n"
                <<"  \"absolute_refinement_tolerance\": "<<tolerance<<",\n"
                <<"  \"compare_realtime\": "<<(compare ? "true" : "false")<<",\n"
                <<"  \"comparison_clock\": \"same-rate endpoint calls; t0 envelope zero; PWL reference versus endpoint production, not a parity gate\",\n"
                <<"  \"max_abs_realtime_difference\": "
                ;
        if(compare) manifest<<"{\"li\": "<<rtLiDifference<<", \"output\": "<<rtOutputDifference<<"}";
        else manifest<<"null";
        manifest<<",\n"
                <<"  \"rate\": "<<rate<<", \"samples\": "<<count<<", \"intervals\": "<<count-1<<",\n"
                <<"  \"duration_seconds\": "<<double(count-1)/rate<<", \"coarse_steps_per_interval\": "<<steps<<",\n"
                <<"  \"fine_steps_per_interval\": "<<2*steps<<", \"initial_li\": "<<initial<<",\n"
                <<"  \"li_seconds\": "<<ref::liSeconds<<", \"ac_seconds\": "<<ref::acSeconds<<",\n"
                <<"  \"input_offset\": "<<ref::offset<<", \"feedthrough\": "<<ref::feedthrough<<",\n"
                <<"  \"max_abs_h_h2_difference\": {\"li\": "<<liDifference<<", \"coupling\": "<<couplingDifference<<", \"output\": "<<outputDifference<<"},\n"
                <<"  \"stimulus_bytes\": "<<bytes.size()<<", \"stimulus_sha256\": \""<<cs80::tools::inputSha256(bytes)<<"\",\n"
                <<"  \"source_sha256\": {\"renderer\": \""<<VCA_RENDER_SHA<<"\", \"reference\": \""<<VCA_REFERENCE_SHA
                <<"\", \"hash_helper\": \""<<VCA_HASH_SHA<<"\", \"declared_core_model\": \""<<VCA_CORE_SHA
                <<"\", \"model_document\": \""<<VCA_MODEL_SHA<<"\", \"workflow_document\": \""<<VCA_DOCUMENT_SHA<<"\"}\n}\n";
        close(manifest);
        std::cout<<"Wrote conditional VCA reference; h/h2 output difference "<<outputDifference<<"\n";
        return 0;
    } catch(const std::exception& e) {
        if(!owned.empty()) { std::error_code ignored;
            for(auto name:{"stimulus.csv","trace.csv","manifest.json"}) fs::remove(owned/name,ignored);
            fs::remove(owned,ignored); }
        std::cerr<<"vca_reference: "<<e.what()<<'\n'; return 1;
    }
}
