// Wiring tests exercise the actual audit parser and render function. This is
// not an independent analog oracle and never substitutes for the 885-row gate.
#define main cs80AuditMainForRevisionTests
#include "../src/tools/engine_audit.cpp"
#undef main
#include <cstdio>
#include <bit>

namespace {
int revisionChecks=0,revisionFailures=0;
void auditCheck(bool ok,const char* what) {
    ++revisionChecks;if(!ok){++revisionFailures;std::fprintf(stderr,"FAIL: %s\n",what);}
}
bool auditSamePcm(const std::vector<float>& a,const std::vector<float>& b) {
    if(a.size()!=b.size())return false;
    for(std::size_t i=0;i<a.size();++i)
        if(std::bit_cast<std::uint32_t>(a[i])!=std::bit_cast<std::uint32_t>(b[i]))return false;
    return true;
}
struct BufferedFlushFailure : std::stringbuf { int sync() override { return -1; } };
int invoke(std::vector<std::string> arguments) {
    arguments.insert(arguments.begin(),"cs80_engine_audit");std::vector<char*> argv;
    for(auto& value:arguments)argv.push_back(value.data());
    return cs80AuditMainForRevisionTests(static_cast<int>(argv.size()),argv.data());
}
std::string read(const fs::path& path) {
    std::ifstream input(path);return {std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
}
Rendered directExpected(const Setup& setup,FactoryToneRevision revision) {
    Engine engine;engine.prepare(48000);auto values=setup.values;values[P_oversampling]=0;
    auto frame=ControlFrame::programmer(values,setup.sources);frame.factoryToneRevision=revision;
    auditCheck(engine.setControlFrame(frame),"direct explicit revision fixture accepted");engine.reset();
    engine.noteOn(60,.8f,1);Rendered result;result.l.resize(24000);result.r.resize(24000);
    std::array<float,128> block{};
    for(int offset=0;offset<24000;offset+=64) {
        const int count=std::min(64,24000-offset);auditCheck(engine.setControlFrame(frame),"direct frame repeated");
        engine.render(block.data(),count);
        for(int i=0;i<count;++i){result.l[offset+i]=block[2*i];result.r[offset+i]=block[2*i+1];}
    }
    return result;
}
}
int main() {
    auditCheck(!auditSamePcm({0.f},{-0.f}),"audit bit-exact PCM oracle distinguishes signed zero");
    BufferedFlushFailure sink;std::ostream stream(&sink);bool refused=false;
    try { writeCandidatePolicy(stream); } catch(const std::runtime_error&) { refused=true; }
    auditCheck(!sink.str().empty(),"sidecar test accepts buffered bytes before sync failure");
    auditCheck(refused,"candidate sidecar writer rejects delayed buffered-write failure");
    const auto root=fs::temp_directory_path()/ ("cs80-audit-revision-"+std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root);
    const auto args=[&](const char* name) {return std::vector<std::string>{"--out",(root/name).string(),
        "--suite","latency","--rates","48000","--oversampling","eco"};};
    Setup setup=tabSetups(true).front();
    setup.values[P_drift]=setup.values[P_voiceDispersion]=setup.values[P_analogNoise]=0;
    const std::vector<Event> events{{0,true,60,.8f}};
    auditCheck(invoke(args("default"))==0,"historical default audit command accepted");
    auditCheck(!fs::exists(root/"default"/"candidate-policy.json"),"default output has no candidate sidecar");
    const auto defaultAudio=render(setup,48000,events,.5);
    auto frozen=args("frozen");frozen.insert(frozen.end(),{"--factory-tone-revision","frozen-v1"});
    auditCheck(invoke(frozen)==0,"explicit frozen audit option accepted");
    auditCheck(!fs::exists(root/"frozen"/"candidate-policy.json"),"explicit frozen output has no candidate sidecar");
    auditCheck(read(root/"default"/"latency.csv")==read(root/"frozen"/"latency.csv"),
               "default and explicit V1 preserve exact audit CSV contents");
    const auto frozenAudio=render(setup,48000,events,.5);
    auditCheck(auditSamePcm(defaultAudio.l,frozenAudio.l)&&auditSamePcm(defaultAudio.r,frozenAudio.r),"default and explicit frozen audit PCM exact");
    auto candidate=args("candidate");candidate.insert(candidate.end(),{"--factory-tone-revision","public-amended-v2"});
    auditCheck(invoke(candidate)==0,"explicit candidate audit option accepted");
    const auto sidecar=read(root/"candidate"/"candidate-policy.json");
    for(const char* token:{"\"schema_version\": 1","\"factory_tone_revision\": 2",
                          "\"policy_id\": \"public-amended-marks-v2\"","\"policy_hash\": \"832eee6f44f5cfcc\"",
                          "\"default_changed\": false","\"qualified\": false","\"hardware_measured\": false"})
        auditCheck(sidecar.find(token)!=std::string::npos,"candidate sidecar records exact policy and qualification boundary");
    const auto actual=render(setup,48000,events,.5),expected=directExpected(setup,FactoryToneRevision::publicAmendedV2);
    auditCheck(auditSamePcm(actual.l,expected.l)&&auditSamePcm(actual.r,expected.r),"actual audit render propagates V2 after programmer builder");
    auditCheck(actual.l!=frozenAudio.l||actual.r!=frozenAudio.r,"candidate flag changes affected actual audit PCM");
    auditCheck(programSetups(false).size()==222&&programSetups(true).size()==222
        &&tabSetups(false).size()==11&&tabSetups(true).size()==11,"original program and hardware-tab inventories unchanged");
    for(const char* invalid:{"unknown","0","v2"}) {
        auto bad=args(invalid);bad.insert(bad.end(),{"--factory-tone-revision",invalid});
        auditCheck(invoke(bad)==2&&!fs::exists(root/invalid),"unknown audit revision rejects before creating output");
    }
    auditCheck(invoke({"--out",(root/"cpu-rejected").string(),"--suite","cpu","--rates","48000",
                      "--factory-tone-revision","public-amended-v2"})==2&&!fs::exists(root/"cpu-rejected"),
               "candidate cannot replace original registered CPU workload or create an output");
    fs::remove_all(root);
    std::printf("Public tone audit revision: %d checks, %d failures; no full885/default qualification\n",revisionChecks,revisionFailures);
    return revisionFailures?1:0;
}
