// Source-derived topology, synthetic nominal boundaries, NOT hardware parity.
#include "core/SourceFactoryTones.h"
#include "reference/LoadedControlRank.h"
#include "reference/ResonanceControl.h"
#include "reference/LevelControl.h"

#include <iostream>
#include <limits>
#include <random>
#include <type_traits>
#include <vector>

namespace sf = cs80::source_factory;
namespace hw = cs80::hardware;
namespace fh = cs80::factory_hypotheses;
namespace lc = cs80::reference::loaded_control;
namespace rq = cs80::reference::resonance;
namespace lv = cs80::reference::level;
namespace {
int checks = 0, failures = 0;
void check(bool value, const char* message) {
    ++checks;
    if (!value) { if (failures < 25) std::cerr << message << '\n'; ++failures; }
}
void near(double a, double b, const char* message, double tolerance = 3e-11) {
    check(std::isfinite(a) && std::isfinite(b)
        && std::abs(a-b) <= tolerance * (1.0 + std::abs(a) + std::abs(b)), message);
}
constexpr std::uint32_t bit(int output) { return 1u << (output - 1); }
constexpr std::array<std::array<std::uint32_t, 11>, 2> excluded {{
    {{bit(20)|bit(25),0,bit(25),0,bit(22)|bit(26),bit(22)|bit(25)|bit(26),
       bit(26),bit(9),bit(22),0,bit(20)}},
    {{bit(20)|bit(25),bit(14)|bit(20),bit(20),0,bit(23)|bit(26),bit(22)|bit(26),
       bit(26),0,bit(22)|bit(25)|bit(26),0,0}}
}};
constexpr std::array<std::array<const char*, 11>, 2> names {{
    {{"String 1","String 3","Brass 1","Flute","E. Piano","Clavichord 1",
       "Harpsichord 1","Organ 1","Guitar 1","Funky 1","Funky 3"}},
    {{"String 2","String 4","Brass 2","Brass 3","Bass","Clavichord 2",
       "Harpsichord 2","Organ 2","Guitar 2","Funky 2","Funky 4"}}
}};
std::array<sf::ToneControls, 7> completeTones() {
    std::array<sf::ToneControls, 7> tones;
    std::size_t index = 0;
    for (int rank = 0; rank < 2; ++rank) for (int selector = 0; selector < 11; ++selector) {
        sf::ToneControls tone;
        if (sf::unknownMask(*sf::address(rank,selector)) == 0 && sf::compileTone(rank, selector, tone)) {
            if (index < tones.size()) tones[index] = tone;
            ++index;
        }
    }
    check(index == 7, "Exactly seven first-increment columns compile");
    return tones;
}
std::array<sf::ToneControls,22> allTones() {
    std::array<sf::ToneControls,22> tones;
    for(int rank=0;rank<2;++rank)for(int selector=0;selector<11;++selector)
        check(sf::compileTone(rank,selector,tones[rank*11+selector]),"every canonical tone compiles");
    return tones;
}
// Deliberately independent literal accepted-source traversal, formula and sort.
double independentMedian(int board,int output,std::size_t* count=nullptr) {
    std::vector<double> values;values.reserve(11);
    const int first=board<53?51:53;
    for(int b=first;b<=first+1;++b)for(char c='A';c<=(b==first?'F':'E');++c) {
        const auto branch=hw::findBranch(b,c,output);
        if(branch.kind==hw::BranchKind::unknown)continue;
        values.push_back(branch.kind==hw::BranchKind::direct?1.0
            :branch.kind==hw::BranchKind::open?0.0:10000.0/(10000.0+branch.resistanceOhms()));
    }
    std::sort(values.begin(),values.end());
    const auto n=values.size();
    if(count)*count=n;
    return n%2?values[n/2]:(values[n/2-1]+values[n/2])*.5;
}
void sourceIdentity() {
    check(completeTones().size()==7,"original classified subset retained");
    int known = 0, unknown = 0, complete = 0;
    for (int rank = 0; rank < 2; ++rank) for (int selector = 0; selector < 11; ++selector) {
        const auto* a = sf::address(rank, selector);
        check(a != nullptr, "canonical selector exists");
        const int board = 51 + 2 * rank + (selector >= 6 ? 1 : 0);
        const char column = static_cast<char>('A' + (selector >= 6 ? selector - 6 : selector));
        check(a->board == board && a->column == column && a->name == names[rank][selector],
              "independent canonical board/column/name mapping");
        check(sf::addressByName(rank, a->name) == a && sf::addressByName(rank, a->sourceName) == a,
              "UI/source aliases map to same source identity");
        check(sf::unknownMask(*a) == excluded[rank][selector], "exact source exclusion addresses");
        complete += excluded[rank][selector] == 0;
        for (int output = 1; output <= 26; ++output) {
            const auto b = hw::findBranch(board,column,output);
            if (b.kind == hw::BranchKind::unknown) ++unknown; else ++known;
            check((b.kind == hw::BranchKind::unknown) == ((excluded[rank][selector] & bit(output)) != 0),
                  "unknown never promoted to open/direct/resistor");
            const auto local = hw::findLocalShunt(board,column,output);
            check(local == (output == 4 || output == 5 || board == 52 || board == 54
                            ? hw::LocalShunt::none : hw::LocalShunt::tenK),
                  "local shunt evidence remains board-specific");
        }
        sf::ToneControls tone;
        tone.source.selector = 1234;
        const bool success = sf::compileTone(rank, selector, tone);
        check(success, "all22 nominal source columns compile through classified/explicit inferred paths");
        check(tone.provenance.knownCells == (sf::allOutputsMask & ~excluded[rank][selector])
              && tone.provenance.inferredCells == excluded[rank][selector]
              && tone.provenance.unknownCells == excluded[rank][selector],
              "inference never erases original evidence exclusions");
        if (!success) check(tone.source.selector == 1234, "failed compile keeps previous complete output");
        check(sf::route(rank,selector,false) == sf::Route::legacy_bridge, "isolation required for activation");
        check(sf::route(rank,selector,true) == (excluded[rank][selector] == 0 ? sf::Route::source_complete : sf::Route::source_inferred),
              "source-inferred route distinct from source-complete");
        check(sf::isSourceDriven(sf::route(rank,selector,true))
              && !sf::isSourceDriven(sf::route(rank,selector,false)), "central source-driven activation helper respects isolation");
    }
    check(known == 547 && unknown == 25 && complete == 7, "547 classifications /25 unknown /7 complete immutable");
    check(sf::sourceFingerprint == 0xa95212770767733cull, "pinned canonical source fingerprint");
    check(sf::addressByName(0,"Electric Piano") == sf::address(0,4), "Electric Piano source alias");
    check(sf::addressByName(0,"unknown") == nullptr && sf::addressByName(2,"Flute") == nullptr,
          "invalid names/ranks are not guessed");
    check(sf::address(-1,0) == nullptr && sf::address(2,0) == nullptr && sf::address(0,11) == nullptr,
          "address lookup bounded before indexing");
    for (int r = -2; r <= 3; ++r) for (int s = -3; s <= 16; ++s) {
        sf::ToneControls t;
        if (r < 0 || r > 1 || s < 0 || s > 13) {
            check(sf::route(r,s,true) == sf::Route::invalid, "invalid restored route explicitly rejected");
            check(!sf::compileTone(r,s,t), "invalid compile selector safely rejected");
        } else if (s >= 11) check(sf::route(r,s,true) == sf::Route::panel_or_memory, "memory/PANEL not factory");
    }
    check(hw::findBranch(52,'F',1).kind == hw::BranchKind::unknown
          && hw::findBranch(54,'F',1).kind == hw::BranchKind::unknown, "no extra unused board columns");
    check(sf::touchReceiverIds[1][1] == "TRG4:ILII" && sf::touchReceiverIds[1][3] == "TRG5:ALII",
          "rank-II touch receivers not mirrored rank-I outputs");
}
void scalarLaws() {
    sf::SourceEquivalent source {123,456};
    check(!sf::analogEquivalent(hw::unknownBranch,source) && source.volts == 123,
          "unknown equivalent rejected transactionally");
    check(sf::analogEquivalent(hw::openBranch,source), "open equivalent valid");
    near(source.volts,0,"open E=0"); near(source.ohms,10000,"open retains assembled shunt");
    check(sf::analogEquivalent(hw::directBranch,source), "direct equivalent valid");
    near(source.volts,10,"ideal conducting diode explicit10V"); near(source.ohms,0,"direct stiff nominal source");
    for (auto b : {hw::ohms(750), hw::ohms(510), hw::ohms(390), hw::kiloOhms(18)}) {
        check(sf::analogEquivalent(b,source), "resistor equivalent valid");
        const double r = b.resistanceOhms();
        near(source.volts,10.0*10000.0/(10000.0+r),"single shunt source voltage");
        near(source.ohms,r*10000.0/(r+10000.0),"source impedance retains ohms units");
        // Independent literal source+10k+receiver KCL, NOT double-dividing a loaded bus.
        for (double load : {39000.0,220000.0,560000.0}) {
            const double v = source.volts * load / (load+source.ohms);
            near((10-v)/r,v/10000.0+v/load,"literal source+shunt+receiver current budget",1e-13);
        }
    }
    check(hw::findBranch(53,'A',9).resistanceOhms() == 750.0
          && hw::findBranch(52,'D',13).resistanceOhms() == 510.0
          && hw::findBranch(52,'D',25).resistanceOhms() == 390.0, "source small resistors never k-ohms");
    for (auto b : {hw::ohms(0),hw::ohms(-1),hw::ohms(std::numeric_limits<double>::infinity()),
                   hw::ohms(std::numeric_limits<double>::quiet_NaN()),
                   hw::Branch{hw::BranchKind::resistor,1,static_cast<hw::ResistanceUnit>(9)},
                   hw::Branch{static_cast<hw::BranchKind>(9),0,hw::ResistanceUnit::ohms}})
        check(!sf::analogEquivalent(b,source), "invalid branch cannot publish equivalent");
    bool enabled = true;
    check(sf::switchEnabled(hw::openBranch,enabled) && !enabled,"open switch off without shunt");
    check(sf::switchEnabled(hw::directBranch,enabled) && enabled,"direct switch on without shunt");
    check(!sf::switchEnabled(hw::ohms(1000),enabled),"switch resistor needs explicit new model");
    near(sf::attackSeconds(0),1,"attack slow endpoint"); near(sf::attackSeconds(10),.001,"attack fast endpoint");
    near(sf::decayReleaseSeconds(0),10,"decay/release slow endpoint");
    near(sf::decayReleaseSeconds(10),.01,"decay/release fast endpoint");
    near(sf::qFromVolts(0),5,"Q zero-V endpoint"); near(sf::qFromVolts(10),.5,"Q ten-V endpoint");
    for (int step=0; step<100; ++step) {
        const double v=step*.1;
        check(sf::attackSeconds(v)>sf::attackSeconds(v+.1),"higher timing volts always shorten attack");
        check(sf::decayReleaseSeconds(v)>sf::decayReleaseSeconds(v+.1),"higher timing volts shorten decay/release");
        near(sf::releaseSeconds(v,4.7,true),std::max(sf::decayReleaseSeconds(v),sf::decayReleaseSeconds(4.7)),
             "release selector minCV equals maxTime");
        near(sf::releaseSeconds(v,4.7,false),sf::decayReleaseSeconds(v),"sustain gate off keeps individual release");
    }
    for (double z : {-100.0,-3.0,-.1,0.0,.1,3.0,100.0})
        near(sf::envelopeStateFromVolts(sf::envelopeVoltsFromState(z)),z,"state/volts inverse without modifying z");
    for (const auto& t : allTones()) {
        const auto mask=excluded[t.source.rank][t.source.selector];
        check(t.compiled && t.provenance.knownCells == (sf::allOutputsMask&~mask)
              && t.provenance.unknownCells == mask && t.provenance.inferredCells == mask
              && !t.provenance.hardwareMeasured,"honest classified/inferred provenance");
        check((t.provenance.bridgeMask & sf::pulseWidthPanelProxy) != 0
              && (t.provenance.bridgeMask & sf::settledDcNoCapacitorState) != 0,"retained bridges always reported");
        check(t.analogShuntMask == (sf::allOutputsMask & ~(bit(4)|bit(5))),"no switch shunts, one every analog row");
        check(t.filterEnvelope.initialVolts <= 0 && t.filterEnvelope.attackVolts >= 0
              && t.filterEnvelope.sustainVolts == 0,"filter voltage target polarity and grounded sustain");
        check(t.amplitudeEnvelope.initialRatio == 0 && t.amplitudeEnvelope.attackRatio == 1
              && t.amplitudeEnvelope.sustainRatio >= 0 && t.amplitudeEnvelope.sustainRatio <= 1,
              "dimensionless amplitude target bounds");
        check(t.filterEnvelope.attackSeconds >= .001 && t.filterEnvelope.attackSeconds <= 1
              && t.filterEnvelope.decaySeconds >= .01 && t.filterEnvelope.decaySeconds <= 10
              && t.filterEnvelope.releaseSeconds >= .01 && t.filterEnvelope.releaseSeconds <= 10,
              "IG00152 nominal timing bounds");
        check(t.amplitudeEnvelope.attackSeconds >= .001 && t.amplitudeEnvelope.attackSeconds <= 1
              && t.amplitudeEnvelope.decaySeconds >= .01 && t.amplitudeEnvelope.decaySeconds <= 10
              && t.amplitudeEnvelope.releaseSeconds >= .01 && t.amplitudeEnvelope.releaseSeconds <= 10,
              "IG00159 nominal timing bounds");
        check(t.noiseGain == 0 && t.filterPathGain == 1,"all noise open and VCF gain direct");
        check(t.pwmRateHz >= .05 && t.pwmRateHz <= 20 && t.pwmDepth >= 0 && t.pwmDepth <= 1,
              "separate PWM rate/depth bounds");
        near(t.wsc.pulseThresholdVolts,1.525*(1-2*t.dutyProxy),"named threshold sign/duty bridge shared with oscillator");
        for (std::size_t i=0;i<26;++i) {
            check(static_cast<std::size_t>(sf::roles[i]) == i && !sf::boundaryLawIds[i].empty(),
                  "all26 roles have stable destination and explicit law identity");
            check(std::isfinite(t.busVolts[i]) && t.busVolts[i] >= 0 && t.busVolts[i] <= 10,
                  "nominal zero-state bus readout finite and bounded");
        }
    }
}
// Independent literal18-node network:16 chip pins +2 real cutoff buses. Source
// resistor and ONE 10k shunt are stamped separately (no production equivalents).
std::array<long double,18> literalCutoff(const sf::ToneControls& t, const sf::RankDrives& drives) {
    constexpr int n=18;
    std::array<std::array<long double,n>,n> a{};
    std::array<long double,n> rhs{}, out{};
    const auto branch=[&](int i,int j,long double ohms) {
        const long double g=1/ohms;
        a[i][i]+=g; a[j][j]+=g; a[i][j]-=g; a[j][i]-=g;
    };
    const auto endpoint=[&](int i,long double ohms,long double volts) {
        a[i][i]+=1/ohms; rhs[i]+=volts/ohms;
    };
    for (int voice=0;voice<8;++voice) for (int side=0;side<2;++side) {
        const int node=voice*2+side;
        const bool hp=side==0;
        const auto grade=hp?t.hpGrade:t.lpGrade;
        // literal values duplicated deliberately, independent from production helper
        const double shunt=grade==sf::Grade::A?(hp?2700:3300):grade==sf::Grade::B?(hp?2200:2700):(hp?1800:2200);
        endpoint(node,shunt,0); endpoint(node,175000,0); // centered trim includes25k source
        endpoint(node,hp?120000:39000,0); // KC
        endpoint(node,hp?120000:39000,0); // TF
        endpoint(node,hp?120000:39000,t.afterBrillianceVolts*drives[voice].pressure);
        endpoint(node,hp?120000:39000,t.initialBrillianceVolts*drives[voice].velocity);
        endpoint(node,hp?100000:47000,drives[voice].filterEnvelopeZ*5.0L/3.0L);
        endpoint(node,hp?120000:39000,0); // shared SUB ideal zero
        endpoint(node,hp?680000:220000,0); // shared BR ideal zero
        branch(node,16+side,39000);
    }
    for (int side=0;side<2;++side) {
        const int node=16+side;
        const auto b=hw::findBranch(t.source.board,t.source.column,side==0?7:9);
        if(b.kind==hw::BranchKind::unknown) {
            const int output=side==0?7:9;
            const double x=(t.provenance.hypothesisOverrideMask & bit(output)) != 0
                ?t.hypothesisExcitations[output-1]:independentMedian(t.source.board,output);
            const double volts=10*x,ohms=10000*(1-x);
            // Hypothesis is already a source+single-shunt equivalent. Do not
            // stamp a second10k or fabricate a classified source resistor.
            if(ohms==0){a[node].fill(0);a[node][node]=1;rhs[node]=volts;}
            else endpoint(node,ohms,volts);
        } else {
            endpoint(node,10000,0);
            if (b.kind==hw::BranchKind::resistor) endpoint(node,b.resistanceOhms(),10);
            else if (b.kind==hw::BranchKind::direct) {a[node].fill(0);a[node][node]=1;rhs[node]=10;}
            else check(b.kind==hw::BranchKind::open,"literal oracle requires classified or tagged inferred source");
        }
    }
    for (int k=0;k<n;++k) {
        int pivot=k;
        for (int i=k+1;i<n;++i) if (std::abs(a[i][k])>std::abs(a[pivot][k])) pivot=i;
        std::swap(a[k],a[pivot]);std::swap(rhs[k],rhs[pivot]);
        for (int i=k+1;i<n;++i) {
            const long double f=a[i][k]/a[k][k];
            for (int j=k;j<n;++j) a[i][j]-=f*a[k][j];
            rhs[i]-=f*rhs[k];
        }
    }
    for (int i=n-1;i>=0;--i) {
        long double v=rhs[i];
        for (int j=i+1;j<n;++j) v-=a[i][j]*out[j];
        out[i]=v/a[i][i];
    }
    return out;
}
void cutoffOracles() {
    std::mt19937 rng(8156);
    std::uniform_real_distribution<double> uni(0,1);
    for (const auto& base : allTones())
      for (auto hg : {sf::Grade::A,sf::Grade::B,sf::Grade::C})
       for (auto lg : {sf::Grade::A,sf::Grade::B,sf::Grade::C}) {
        sf::ToneControls t;
        check(sf::compileTone(base.source.rank,base.source.selector,t,hg,lg),"all9 grade pair fixtures compile");
        for (int trial=0;trial<8;++trial) {
            sf::RankDrives drives{};
            for (auto& d:drives) d={6*uni(rng)-3,uni(rng),uni(rng)};
            sf::CutoffReadout actual;
            check(sf::evaluateCutoff(t,drives,actual),"finite dynamic cutoff evaluation");
            const auto expected=literalCutoff(t,drives);
            lc::RankInputs reference;
            for (int i=0;i<8;++i) {
                auto& v=reference.voices[i];
                v.local={lc::Source{0,0},lc::Source{0,0},lc::Source{t.afterBrillianceVolts*drives[i].pressure,0},
                    lc::Source{t.initialBrillianceVolts*drives[i].velocity,0},
                    lc::Source{sf::envelopeVoltsFromState(drives[i].filterEnvelopeZ),0}};
                v.hpTrim={0,25000};v.lpTrim={0,25000};v.hpReceiver={0,0};v.lpReceiver={0,0};
                v.hpGrade=hg==sf::Grade::A?lc::Grade::A:hg==sf::Grade::B?lc::Grade::B:lc::Grade::C;
                v.lpGrade=lg==sf::Grade::A?lc::Grade::A:lg==sf::Grade::B?lc::Grade::B:lc::Grade::C;
            }
            reference.buses={lc::Source{0,0},lc::Source{0,0},
                lc::Source{t.sources[6].volts,t.sources[6].ohms},lc::Source{t.sources[8].volts,t.sources[8].ohms}};
            const auto referenceOut=lc::solveRank(reference);
            for (std::size_t voice=0;voice<8;++voice) for (std::size_t side=0;side<2;++side) {
                near(actual.pinVolts[voice][side],static_cast<double>(expected[voice*2+side]),"literal source/shunt full-rank pin oracle");
                near(actual.pinVolts[voice][side],referenceOut.pinVolts[voice*2+side],"independent reference16 cutoff pins");
                const auto& m=t.cutoff[side];
                near(m.effectiveCvPerPinVolt*m.pinPerLoadedBus,1,"effective slope wrt loaded bus not source E");
                near(actual.effectiveCv[voice][side],m.effectiveCvOffset+(actual.pinVolts[voice][side]-m.referencePinVolts)/m.pinPerLoadedBus,
                     "pin versus effective CV domains remain distinct");
            }
            for (int side=0;side<2;++side) {
                const double bus=actual.loadedBusVolts[side];
                near(bus,static_cast<double>(expected[16+side]),"literal assembled source terminal");
                near(bus,referenceOut.busTerminalVolts[2+side],"reference common bus terminal");
                double receiver=0;
                for (int voice=0;voice<8;++voice) receiver+=(bus-actual.pinVolts[voice][side])/39000.0;
                const auto b=hw::findBranch(t.source.board,t.source.column,side==0?7:9);
                if (b.kind==hw::BranchKind::resistor)
                    near((10-bus)/b.resistanceOhms(),receiver+bus/10000,"source and receiver KCL budget",1e-13);
                if (b.kind==hw::BranchKind::open)
                    near(0,receiver+bus/10000,"open bus can be back-driven by real receiver sources",1e-13);
            }
        }
        sf::RankDrives drive{};sf::CutoffReadout zero, changed;
        check(sf::evaluateCutoff(t,drive,zero),"zero cutoff fixture");
        drive[0].filterEnvelopeZ=1;
        check(sf::evaluateCutoff(t,drive,changed),"envelope impulse cutoff fixture");
        check(changed.pinVolts[0][0]>zero.pinVolts[0][0] && changed.pinVolts[0][1]>zero.pinVolts[0][1],
              "positive envelope drives both cutoff receivers positively");
        check(std::abs((changed.effectiveCv[0][0]-zero.effectiveCv[0][0])
                      -(changed.effectiveCv[0][1]-zero.effectiveCv[0][1]))>.01,
              "HP envelope never copied from LP or generic halfLP");
        if (t.sources[6].ohms>0) check(changed.pinVolts[7][0]>zero.pinVolts[7][0],"eighth HP receiver follows finite shared bus");
        if (t.sources[8].ohms>0) check(changed.pinVolts[7][1]>zero.pinVolts[7][1],"eighth LP receiver follows finite shared bus");
    }
}
void sharedOracles() {
    std::mt19937 rng(151159);
    std::uniform_real_distribution<double> uni(0,1);
    const auto tones=allTones();
    for (std::size_t first=0;first<tones.size();++first) for (std::size_t second=0;second<tones.size();++second)
      for (int trial=0;trial<3;++trial) {
        std::array<sf::RankBoundary,2> boundaries{tones[first].boundary,tones[second].boundary};
        if (trial==2) check(sf::bridgeBoundary(.17,.83,.6,.2,.9,boundaries[1]),"mixed-source proxy valid");
        sf::BothRankDrives drives{};
        for (auto& rank:drives) for (auto& d:rank) d={6*uni(rng)-3,uni(rng),uni(rng)};
        const double master=uni(rng);
        sf::SharedReadout actual;
        check(sf::evaluateShared(boundaries,master,drives,actual),"all32 Q/all16 LC evaluation");
        rq::Inputs qInput;lv::Inputs lInput;
        for (auto& r:qInput.receivers) r={0,0};
        for (auto& r:lInput.receivers) r={0,0};
        for (auto& s:lInput.sources) s={0,0}; // one AM and one perkey KB explicitly stiff zero
        qInput.sources[4]={10*(1-master),0};
        for (int rank=0;rank<2;++rank) {
            const auto& b=boundaries[rank];
            qInput.sources[rank*2]={b.qHp.volts,b.qHp.ohms};
            qInput.sources[rank*2+1]={b.qLp.volts,b.qLp.ohms};
            lInput.sources[1+rank]={b.level.volts,b.level.ohms};
            for (int voice=0;voice<8;++voice) {
                const int node=rank*8+voice;
                lInput.sources[11+node]={b.initialLevelVolts*drives[rank][voice].velocity,0};
                lInput.sources[27+node]={b.afterLevelVolts*drives[rank][voice].pressure,0};
            }
        }
        const auto qExpected=rq::solve(qInput);const auto lExpected=lv::solve(lInput);
        for (int rank=0;rank<2;++rank) {
            for (int side=0;side<2;++side)
                near(actual.qBusVolts[rank][side],qExpected.busVolts[rank*2+side],"all4 rank-side Q buses with shared RES");
            near(actual.levelBusVolts[rank],lExpected.busVolts[1+rank],"finite loaded LV source");
            for (int voice=0;voice<8;++voice) {
                const int levelNode=rank*8+voice;
                near(actual.levelPinVolts[rank][voice],lExpected.nodeVolts[levelNode],"all16 LC pins include shared AM/perkey KB");
                near(actual.levelGain[rank][voice],lExpected.nodeVolts[levelNode]/(30.0/31.0),"linear LC scale no hidden normalization");
                const double pin=actual.levelPinVolts[rank][voice];
                const double ti=lInput.sources[11+levelNode].volts,ta=lInput.sources[27+levelNode].volts;
                near(pin/180000+(pin-actual.levelBusVolts[rank])/560000+(3*pin-ti-ta)/270000,
                     0,"independent LC KCL explicit omitted ports",1e-13);
                for (int side=0;side<2;++side) {
                    const int node=rank*16+side*8+voice;
                    near(actual.qVolts[rank][voice][side],qExpected.nodeVolts[node],"all32 Q receiver oracle");
                    const double vq=actual.qVolts[rank][voice][side];
                    near((vq-actual.qBusVolts[rank][side])/220000+(vq-actual.masterResVolts)/220000,
                         0,"independent Q receiver current budget",1e-13);
                    near(actual.q[rank][voice][side],1/(.2+.18*vq),"single Q damping adapter not old mixer twice");
                }
            }
        }
    }
    sf::RankBoundary b;check(sf::bridgeBoundary(1,1,1,1,1,b),"full scale bridge");
    sf::BothRankDrives drives{};sf::SharedReadout out;
    check(sf::evaluateShared({b,b},1,drives,out),"shared ideal anchor");
    near(sf::levelUnitVolts,30.0/31.0,"documented nominal LV10 unit scale");
    near(out.levelGain[0][0],1,"LV10 other drives0 gain one");
    near(out.q[0][0][0],5,"rank+master max resonance Q5");
    for(auto& rank:drives)for(auto& d:rank)d={0,1,1};
    check(sf::evaluateShared({b,b},1,drives,out),"full touch anchor");
    near(out.levelGain[0][0],1+2*560.0/270.0,"touch gains not silently clamped to unity");
    check(out.levelGain[0][0]>5,"headroom inference retains actual linear graph scale");
}
void hypothesisManifest() {
    check(!fh::acceptedExcitation(hw::unknownBranch),"unknown cannot become a zero median vote");
    struct MedianAnchor { int rank,output,count;double x; };
    const MedianAnchor anchors[] = {
        {0,9,10,.5*(10000.0/15100.0+10000.0/14300.0)}, {0,20,9,.5},
        {0,22,8,1},{0,25,8,0},{0,26,8,1},
        {1,14,10,.5*(10000.0/22000.0+10000.0/18200.0)},
        {1,20,8,.5*(10000.0/53000.0+10000.0/49000.0)},
        {1,22,9,1},{1,23,10,.3125},{1,25,9,0},{1,26,7,1}
    };
    check(fh::version==1 && sf::modelVersion==2 && fh::cells.size()==25,"separate explicit overlay and model versions");
    check(fh::fingerprint==0xe5f877a025fc52c6ull,"independently pinned manifest/basis fingerprint");
    for(const auto& cell:fh::cells) {
        const auto& a=cell.address;
        check(fh::find(a)==&cell,"address lookup not inferred fixture order");
        check(hw::findBranch(a.board,a.column,a.output).kind==hw::BranchKind::unknown,
              "every inferred entry remains unknown in evidence");
        check(cell.policy==fh::Policy::sameRankOutputMedian && !cell.sourceExclusionReason.empty(),
              "every hypothesis has explicit policy and original reason");
        check(cell.minimumExcitation==0 && cell.maximumExcitation==1,"full nominal model-family domain, not sample spread");
        std::size_t accepted=0;
        const double expected=independentMedian(a.board,a.output,&accepted);
        check(accepted==cell.basisCount,"basis includes every classified same-rank/output cell");
        near(cell.nominalExcitation,expected,"independent accepted-only median",1e-15);
        near(cell.nominalDriveVolts,10*expected,"median equivalent E, not recovered voltage");
        near(cell.nominalSourceOhms,10000*(1-expected),"median equivalent Rth, not recovered resistor");
        bool anchorFound=false;
        for(const auto& anchor:anchors)if(anchor.rank==fh::rankOf(a.board) && anchor.output==a.output) {
            anchorFound=true;check(static_cast<int>(accepted)==anchor.count,"frozen accepted-cell count");
            near(expected,anchor.x,"literal independent median anchor",1e-15);
        }
        check(anchorFound,"every unknown rank/output group is accounted for");
        std::array<bool,11> seen{};
        for(std::size_t i=0;i<cell.basisCount;++i) {
            const auto& b=cell.basis[i];
            const auto raw=hw::findBranch(b.address.board,b.address.column,b.address.output);
            check(fh::rankOf(b.address.board)==fh::rankOf(a.board) && b.address.output==a.output,
                  "no neighboring output or other-rank borrowing");
            check(raw.kind!=hw::BranchKind::unknown && raw.kind==b.classifiedBranch.kind
                  && raw.sourceUnit==b.classifiedBranch.sourceUnit && raw.sourceValue==b.classifiedBranch.sourceValue,
                  "basis preserves raw accepted classification/unit/value");
            const int selector=(b.address.board==52 || b.address.board==54?6:0)+b.address.column-'A';
            check(selector>=0 && selector<11 && !seen[static_cast<std::size_t>(selector)],"no duplicate basis votes");
            seen[static_cast<std::size_t>(selector)]=true;
            const double x=raw.kind==hw::BranchKind::open?0:raw.kind==hw::BranchKind::direct?1
                :10000/(10000+raw.resistanceOhms());
            near(b.excitation,x,"basis excitation preserves explicit branch meaning");
        }
        check(cell.visibleAlternativeCount>0 && cell.visibleAlternativeCount<=2,"visible readings separately retained");
        for(std::size_t i=0;i<cell.visibleAlternativeCount;++i) {
            const auto& alternative=cell.visibleAlternatives[i];
            check(alternative.excitation>=0 && alternative.excitation<=1 && !alternative.label.empty(),
                  "visible alternatives are labeled finite hypotheses, not median votes");
        }
    }
    check(fh::find(53,'B',14)->sourceExclusionReason.find("readability")!=std::string_view::npos,
          "String4 decay is readability uncertainty, not a settled revision");
    check(fh::find(51,'B',20)==nullptr && fh::find(52,'F',20)==nullptr
          && fh::find(99,'A',20)==nullptr,"overlay never invents accepted/invalid addresses");
    for(const auto& t:allTones()) {
        std::uint32_t inferred=0;
        for(int output=1;output<=26;++output) {
            const auto i=static_cast<std::size_t>(output-1);
            if((excluded[t.source.rank][t.source.selector]&bit(output))!=0) {
                inferred|=bit(output);
                const auto* cell=fh::find(t.source.board,t.source.column,output);
                check(t.hypotheses[i]==cell,"compiled tone retains complete per-cell basis pointer");
                near(t.sources[i].volts,cell->nominalDriveVolts,"nominal compile uses frozen inferred E");
                near(t.sources[i].ohms,cell->nominalSourceOhms,"nominal compile uses frozen inferred Rth");
                near(t.hypothesisExcitations[i],cell->nominalExcitation,"nominal excitation trace retained even zero/one");
            } else check(t.hypotheses[i]==nullptr,"classified control not relabeled inferred");
        }
        check(inferred==t.provenance.inferredCells && t.provenance.hypothesisOverrideMask==0,
              "nominal per-cell mask and non-sensitivity metadata");
        check((t.provenance.knownCells&t.provenance.inferredCells)==0
              && (t.provenance.knownCells|t.provenance.inferredCells)==sf::allOutputsMask,
              "all26 usable controls with disjoint evidence/inference masks");
        check(t.provenance.hypothesisVersion==(inferred?fh::version:0)
              && t.provenance.hypothesisHash==(inferred?fh::fingerprint:0),
              "only inferred tones claim applied overlay provenance");
        sf::ToneControls again;check(sf::compileTone(t.source.rank,t.source.selector,again),"deterministic repeat compile");
        check(again.busVolts==t.busVolts && again.hypothesisExcitations==t.hypothesisExcitations,
              "frozen overlay is deterministic, not runtime sampling/fitting");
    }
}
void sensitivityOracle(const sf::ToneControls& tone) {
    for(int pattern=0;pattern<3;++pattern) {
        sf::BothRankDrives drives{};
        for(int rank=0;rank<2;++rank)for(int voice=0;voice<8;++voice)
            drives[rank][voice]={pattern==0?0.0:-2.7+.7*voice,
                                pattern==0?0.0:.1*(voice+1),pattern==2?.11*voice:0.0};
        sf::CutoffReadout cutoff;
        check(sf::evaluateCutoff(tone,drives[0],cutoff),"sensitivity cutoff finite");
        const auto literal=literalCutoff(tone,drives[0]);
        for(int voice=0;voice<8;++voice)for(int side=0;side<2;++side)
            near(cutoff.pinVolts[voice][side],static_cast<double>(literal[voice*2+side]),
                 "overlaid cutoff versus independent literal nodal assembly");
        sf::RankBoundary other;check(sf::bridgeBoundary(.2,.8,.6,.3,.7,other),"sensitivity mixed boundary");
        const std::array<sf::RankBoundary,2> boundaries{tone.boundary,other};
        sf::SharedReadout shared;check(sf::evaluateShared(boundaries,.37,drives,shared),"sensitivity shared readout finite");
        lv::Inputs level;rq::Inputs resonance;
        for(auto& source:level.sources)source={0,0};
        for(auto& receiver:level.receivers)receiver={0,0};
        for(auto& receiver:resonance.receivers)receiver={0,0};
        resonance.sources[4]={6.3,0};
        for(int rank=0;rank<2;++rank) {
            const auto& b=boundaries[rank];
            resonance.sources[rank*2]={b.qHp.volts,b.qHp.ohms};
            resonance.sources[rank*2+1]={b.qLp.volts,b.qLp.ohms};
            level.sources[1+rank]={b.level.volts,b.level.ohms};
            for(int voice=0;voice<8;++voice) {
                const int node=rank*8+voice;
                level.sources[11+node]={b.initialLevelVolts*drives[rank][voice].velocity,0};
                level.sources[27+node]={b.afterLevelVolts*drives[rank][voice].pressure,0};
            }
        }
        const auto expectedLevel=lv::solve(level);const auto expectedQ=rq::solve(resonance);
        for(int rank=0;rank<2;++rank)for(int voice=0;voice<8;++voice) {
            near(shared.levelPinVolts[rank][voice],expectedLevel.nodeVolts[rank*8+voice],"overlaid LC16 reference");
            check(std::isfinite(shared.levelGain[rank][voice]),"sensitivity gain is finite without normalization");
            for(int side=0;side<2;++side)
                near(shared.qVolts[rank][voice][side],expectedQ.nodeVolts[rank*16+side*8+voice],"overlaid Q32 reference");
        }
    }
}
void inferenceSensitivity() {
    for(const auto& cell:fh::cells) {
        const int rank=fh::rankOf(cell.address.board);
        const int selector=(cell.address.board==52 || cell.address.board==54?6:0)+cell.address.column-'A';
        sf::ToneControls nominal;check(sf::compileTone(rank,selector,nominal),"sensitivity nominal compile");
        const auto compile=[&](double excitation,sf::ToneControls& out) {
            sf::InferenceScenario scenario;scenario.count=1;scenario.overrides[0]={cell.address.output,excitation};
            check(sf::compileToneForSensitivity(rank,selector,scenario,out),"explicit single-address sensitivity compile");
            check(out.provenance.hypothesisOverrideMask==bit(cell.address.output)
                  && out.provenance.inferredCells==nominal.provenance.inferredCells
                  && out.provenance.unknownCells==nominal.provenance.unknownCells,
                  "alternative retains classification and records exact override");
            for(int output=1;output<=26;++output) {
                const auto i=static_cast<std::size_t>(output-1);
                if(output!=cell.address.output)
                    check(out.sources[i].volts==nominal.sources[i].volts && out.sources[i].ohms==nominal.sources[i].ohms,
                          "single-address alternative cannot overwrite unrelated source drives");
                else {near(out.sources[i].volts,10*excitation,"explicit sensitivity excitation E");
                      near(out.sources[i].ohms,10000*(1-excitation),"explicit sensitivity excitation Rth");}
            }
            sensitivityOracle(out);
        };
        sf::ToneControls low,high;compile(0,low);compile(1,high);
        for(std::size_t i=0;i<cell.visibleAlternativeCount;++i) {
            sf::ToneControls alternative;compile(cell.visibleAlternatives[i].excitation,alternative);
        }
        if(cell.address.output==14) {
            near(low.filterEnvelope.decaySeconds,10,"String4 decay low endpoint10s");
            near(high.filterEnvelope.decaySeconds,.01,"String4 decay high endpoint10ms");
        }
        if(cell.address.output==20) {
            near(low.amplitudeEnvelope.sustainRatio,0,"inferred sustain zero remains usable inference");
            near(high.amplitudeEnvelope.sustainRatio,1,"inferred sustain high endpoint");
        }
        sf::RankDrives zeroTouch{};sf::CutoffReadout lowCutoff,highCutoff;
        check(sf::evaluateCutoff(low,zeroTouch,lowCutoff) && sf::evaluateCutoff(high,zeroTouch,highCutoff),
              "endpoint cutoff comparison valid");
        if(cell.address.output==23 || cell.address.output==25)
            check(lowCutoff.effectiveCv==highCutoff.effectiveCv,"zero touch removes inferred brilliance contribution");
        if(cell.address.output==9)for(int voice=0;voice<8;++voice) {
            check(lowCutoff.effectiveCv[voice][0]==highCutoff.effectiveCv[voice][0],"LP-only hypothesis leaves HP unchanged");
            check(highCutoff.effectiveCv[voice][1]>lowCutoff.effectiveCv[voice][1],"Organ1 LP endpoint response monotone");
        }
        if(cell.address.output==23 || cell.address.output==25) {
            for(auto& drive:zeroTouch) {
                drive.velocity=cell.address.output==23?1:0;
                drive.pressure=cell.address.output==25?1:0;
            }
            check(sf::evaluateCutoff(low,zeroTouch,lowCutoff) && sf::evaluateCutoff(high,zeroTouch,highCutoff),
                  "nonzero touch endpoint comparison valid");
            for(int voice=0;voice<8;++voice)for(int side=0;side<2;++side)
                check(highCutoff.effectiveCv[voice][side]>lowCutoff.effectiveCv[voice][side],"inferred touch brightness monotone");
        }
        if(cell.address.output==22 || cell.address.output==26) {
            sf::BothRankDrives zero{};sf::SharedReadout l,h;
            check(sf::evaluateShared({low.boundary,low.boundary},.37,zero,l)
                  && sf::evaluateShared({high.boundary,high.boundary},.37,zero,h),"zero-pressure level endpoint comparison");
            if(cell.address.output==26)check(l.levelGain==h.levelGain,"pressurezero removes inferred after-level contribution");
            if(cell.address.output==22)check(h.levelGain[0][0]>l.levelGain[0][0],"inferred LV monotone at zero other drives");
            for(auto& rankDrives:zero)for(auto& drive:rankDrives)drive.pressure=1;
            check(sf::evaluateShared({low.boundary,low.boundary},.37,zero,l)
                  && sf::evaluateShared({high.boundary,high.boundary},.37,zero,h),"pressure endpoint level comparison");
            check(h.levelGain[0][0]>l.levelGain[0][0],"inferred level/after-level endpoint positive");
        }
        // Compile after alternatives must still use the immutable nominal policy.
        sf::ToneControls again;check(sf::compileTone(rank,selector,again),"nominal after alternatives");
        check(again.busVolts==nominal.busVolts && again.provenance.hypothesisOverrideMask==0,
              "sensitivity does not mutate/freeze a different production tone");
    }
    for(const auto& nominal:allTones())for(double endpoint:{0.0,1.0}) {
        sf::InferenceScenario scenario;
        for(int output=1;output<=26;++output)if((nominal.provenance.inferredCells&bit(output))!=0)
            scenario.overrides[scenario.count++]={output,endpoint};
        sf::ToneControls joint;
        check(sf::compileToneForSensitivity(nominal.source.rank,nominal.source.selector,scenario,joint),"joint endpoint scenario");
        check(joint.provenance.hypothesisOverrideMask==nominal.provenance.inferredCells,"joint alternatives tagged per address");
        sensitivityOracle(joint);
    }
    sf::ToneControls target;target.source.selector=1234;
    const auto reject=[&](sf::InferenceScenario scenario) {
        check(!sf::compileToneForSensitivity(0,0,scenario,target) && target.source.selector==1234,
              "bad sensitivity scenario rejected transactionally");
    };
    sf::InferenceScenario scenario;scenario.count=27;reject(scenario);
    scenario={};scenario.count=1;
    for(int output:{0,27,1,22}) {scenario.overrides[0]={output,.5};reject(scenario);}
    for(double x:{-1.0,1.1,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()}) {
        scenario.overrides[0]={20,x};reject(scenario);
    }
    scenario={};scenario.count=2;scenario.overrides[0]={20,0};scenario.overrides[1]={20,1};reject(scenario);
}
void sevenToneV1NumericalRegression() {
    //16632 V1 numbers captured before V2 edits (all7 x9 grades, every source,
    //scalar/map plus dynamic cutoff/Q/LC readouts). Quantization1e-9 permits
    //libm last-bit portability; separate same-compiler hexfloat corpus matched
    //V1 and V2 byte-for-byte. This fixture does not hash provenance/version.
    std::uint64_t hash=14695981039346656037ull;std::size_t count=0;
    const auto val=[&](double x) {
        auto bits=static_cast<std::uint64_t>(std::llround(x*1e9));++count;
        for(int byte=0;byte<8;++byte){hash^=bits&255u;hash*=1099511628211ull;bits>>=8;}
    };
    const auto source=[&](sf::SourceEquivalent x){val(x.volts);val(x.ohms);};
    const int ids[7][2]={{0,1},{0,3},{0,9},{1,3},{1,7},{1,9},{1,10}};
    for(const auto& id:ids)for(auto hg:{sf::Grade::A,sf::Grade::B,sf::Grade::C})
      for(auto lg:{sf::Grade::A,sf::Grade::B,sf::Grade::C}) {
        sf::ToneControls t;check(sf::compileTone(id[0],id[1],t,hg,lg),"V1 seven-tone regression compile");
        for(auto x:t.sources)source(x);
        for(auto x:t.busVolts)val(x);
        val(t.wsc.sawEnabled);val(t.wsc.pulseEnabled);val(t.wsc.pulseThresholdVolts);
        for(auto x:{t.dutyProxy,t.pwmRateHz,t.pwmDepth,t.noiseGain,t.filterPathGain,t.sinePathGain})val(x);
        auto f=t.filterEnvelope;
        for(auto x:{f.initialVolts,f.attackVolts,f.sustainVolts,f.attackSeconds,f.decaySeconds,f.releaseSeconds})val(x);
        auto a=t.amplitudeEnvelope;
        for(auto x:{a.initialRatio,a.attackRatio,a.sustainRatio,a.attackSeconds,a.decaySeconds,a.releaseSeconds})val(x);
        val(t.initialBrillianceVolts);val(t.afterBrillianceVolts);
        auto b=t.boundary;source(b.qHp);source(b.qLp);source(b.level);val(b.initialLevelVolts);val(b.afterLevelVolts);
        for(auto m:t.cutoff)for(auto x:{m.pinPerLoadedBus,m.pinPerTouch,m.pinPerEnvelopeVolt,m.busBaseVolts,
            m.busPerTouchSum,m.busPerEnvelopeVoltSum,m.effectiveCvPerPinVolt,m.referencePinVolts,m.effectiveCvOffset})val(x);
        sf::BothRankDrives d;
        for(int r=0;r<2;++r)for(int v=0;v<8;++v)d[r][v]={-2.7+.7*v,.1*(v+1),.11*v};
        sf::CutoffReadout c;check(sf::evaluateCutoff(t,d[0],c),"V1 cutoff readout regression");
        for(int v=0;v<8;++v)for(int side=0;side<2;++side){val(c.pinVolts[v][side]);val(c.effectiveCv[v][side]);}
        for(auto x:c.loadedBusVolts)val(x);
        sf::SharedReadout out;check(sf::evaluateShared({t.boundary,t.boundary},.37,d,out),"V1 shared readout regression");
        for(int r=0;r<2;++r){for(int v=0;v<8;++v){for(int side=0;side<2;++side){val(out.qVolts[r][v][side]);val(out.q[r][v][side]);}
            val(out.levelPinVolts[r][v]);val(out.levelGain[r][v]);}for(auto x:out.qBusVolts[r])val(x);val(out.levelBusVolts[r]);}
        val(out.masterResVolts);
    }
    check(count==16632 && hash==0x0704e37fcac8c19dull,"frozen seven-tone V1 numerical corpus unchanged");
}

void safetyAndIdempotence() {
    static_assert(std::is_trivially_copyable_v<sf::ToneControls>);
    static_assert(noexcept(sf::compileTone(0,1,std::declval<sf::ToneControls&>())));
    static_assert(noexcept(sf::evaluateCutoff(std::declval<const sf::ToneControls&>(),
        std::declval<const sf::RankDrives&>(),std::declval<sf::CutoffReadout&>())));
    static_assert(noexcept(sf::evaluateShared(std::declval<const std::array<sf::RankBoundary,2>&>(),0,
        std::declval<const sf::BothRankDrives&>(),std::declval<sf::SharedReadout&>())));
    sf::ToneControls t;check(sf::compileTone(0,1,t),"idempotent source fixture");
    sf::RankDrives drives{};sf::CutoffReadout a,b;
    check(sf::evaluateCutoff(t,drives,a) && sf::evaluateCutoff(t,drives,b),"repeated evaluation valid");
    check(a.effectiveCv==b.effectiveCv && a.pinVolts==b.pinVolts,"same-selection repeated evaluation bit stable");
    drives[1].filterEnvelopeZ=std::numeric_limits<double>::quiet_NaN();
    check(!sf::evaluateCutoff(t,drives,b) && b.effectiveCv==a.effectiveCv,"NaN drive retains previous readout");
    drives[1]={100,-2,4};check(sf::evaluateCutoff(t,drives,b),"finite out-of-domain readout sanitizes");
    check(b.saturatedVoices==(1u<<1) && drives[1].filterEnvelopeZ==100,"saturation reported without stored z mutation");
    sf::BothRankDrives both{};sf::SharedReadout x,y;
    const std::array<sf::RankBoundary,2> boundaries{t.boundary,t.boundary};
    check(sf::evaluateShared(boundaries,.5,both,x),"shared baseline valid");y=x;
    check(!sf::evaluateShared(boundaries,std::numeric_limits<double>::infinity(),both,y)
          && y.levelGain==x.levelGain,"invalid master cannot overwrite valid frame");
    auto invalid=boundaries;invalid[1].level.ohms=-1;
    check(!sf::evaluateShared(invalid,.5,both,y) && y.levelGain==x.levelGain,"invalid source boundary rejected atomically");
    check(!sf::compileTone(0,1,t,static_cast<sf::Grade>(9),sf::Grade::B),"invalid grade rejected without throwing");
    sf::ToneControls uncompiled;
    check(!sf::evaluateCutoff(uncompiled,{},b),"uncompiled tone cannot render zeros as a source tone");
    t.provenance.sourceHash^=1;
    check(!sf::evaluateCutoff(t,{},b),"stale/mismatched model provenance rejected");
    near(sf::alignment.referenceKeyVolts,.125,"physical lowest C1 key anchor, not MIDI60");
    check(sf::alignment.midiNote==36 && sf::alignment.feetRatio==4,"alignment C1 and two-foot interpretation explicit");
    const double f0=440*std::exp2((36.0-69)/12)*4;
    near(125*.125*std::exp2(sf::alignment.hpEffectiveCv)/f0,.9936652495,"frozen full-square HP alignment",1e-8);
    near(125*.125*std::exp2(sf::alignment.lpEffectiveCv)/f0,1.0062787580,"frozen full-square LP alignment",1e-8);
}
} // namespace
int main() {
    sourceIdentity(); scalarLaws(); cutoffOracles(); sharedOracles();
    hypothesisManifest(); inferenceSensitivity(); sevenToneV1NumericalRegression(); safetyAndIdempotence();
    std::cout<<checks<<" source factory nominal-model checks; "<<failures<<" failures\n";
    return failures==0?0:1;
}
