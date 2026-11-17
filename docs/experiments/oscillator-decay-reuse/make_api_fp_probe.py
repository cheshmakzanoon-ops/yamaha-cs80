from pathlib import Path
r=Path(__file__).resolve().parent;s=(r/'tests/OscillatorProfileCacheTests.cpp').read_text()
s=s.replace('#include <limits>','#include <limits>\n#include <cfenv>\n#include <cerrno>\n#include <xmmintrin.h>')
needle='void tick(Candidate& a,Baseline& b,const Controls& c) {'
helper='''unsigned fpMode=0;int fpSeed=0;unsigned long fpPairs=0;
struct FpStatus {unsigned mxcsr;unsigned short sw,cw;int error;};
FpStatus fpStatus(){FpStatus f{};f.mxcsr=_mm_getcsr();asm volatile("fnstsw %0":"=am"(f.sw));asm volatile("fnstcw %0":"=m"(f.cw));f.error=errno;return f;}
bool fpEqual(const FpStatus& a,const FpStatus& b){return a.mxcsr==b.mxcsr && (a.sw&0x7f)==(b.sw&0x7f) && a.cw==b.cw && a.error==b.error;}
// Every pair receives a newly clean or newly seeded environment. Flags from
// setup/control generation/previous ticks cannot mask a missing exception.
struct FpPair {
 std::fenv_t saved{};unsigned mxcsr;int savedErrno;FpStatus a{};const char* label;bool restored=false;
 explicit FpPair(const char* name):mxcsr(_mm_getcsr()),savedErrno(errno),label(name){check(std::fegetenv(&saved)==0,"save caller FPenv");seed();}
 void seed(){check(std::fesetenv(FE_DFL_ENV)==0,"seed default FPenv");_mm_setcsr((_mm_getcsr()&~0x8040u)|fpMode);if(fpSeed==1)std::feraiseexcept(FE_INVALID);if(fpSeed==2)std::feraiseexcept(FE_INEXACT|FE_UNDERFLOW);errno=fpSeed?ERANGE:17;}
 void between(){a=fpStatus();seed();}
 void finish(){const auto b=fpStatus();restore();check(fpEqual(a,b),label);++fpPairs;}
 void restore(){if(!restored){std::fesetenv(&saved);_mm_setcsr(mxcsr);errno=savedErrno;restored=true;}}
 ~FpPair(){restore();}
};
template<class A,class B> bool pairedPrepare(A&& a,B&& b){FpPair fp("prepare FP/status/errno");const bool x=a();fp.between();const bool y=b();fp.finish();check(x==y,"prepare results match");return x&&y;}
'''
s=s.replace(needle,helper+needle)
s=s.replace('    const auto x=a.tick(', '    FpPair fp("tick FP/status/errno");\n    const auto x=a.tick(').replace('    const auto y=b.tick(','    fp.between();const auto y=b.tick(').replace('    check(output(x,y),','    fp.finish();check(output(x,y),')
s=s.replace('Candidate a;Baseline b;','FpPair constructor("constructor FP/status/errno");Candidate a;constructor.between();Baseline b;constructor.finish();')
s=s.replace('Candidate ac(a);Baseline bc(b);','FpPair copy("copy constructor FP/status/errno");Candidate ac(a);copy.between();Baseline bc(b);copy.finish();')
s=s.replace('Candidate assigned;Baseline assignedBaseline;','FpPair assignedCtor("assignment destination constructor FP/status/errno");Candidate assigned;assignedCtor.between();Baseline assignedBaseline;assignedCtor.finish();')
s=s.replace('a.prepare(rate,pa)&&b.prepare(rate,pb)','pairedPrepare([&]{return a.prepare(rate,pa);},[&]{return b.prepare(rate,pb);})')
s=s.replace('assigned.prepare(48000.,qa)&&assignedBaseline.prepare(48000.,qb)','pairedPrepare([&]{return assigned.prepare(48000.,qa);},[&]{return assignedBaseline.prepare(48000.,qb);})')
s=s.replace('!a.prepare(invalid,badA)&&!b.prepare(invalid,profile<cs80_osc_profile_baseline::OscillatorNominalProfile>(0))','!pairedPrepare([&]{return a.prepare(invalid,badA);},[&]{return b.prepare(invalid,profile<cs80_osc_profile_baseline::OscillatorNominalProfile>(0));})')
s=s.replace('!a.prepare(rate,invalidA)&&!b.prepare(rate,invalidB)','!pairedPrepare([&]{return a.prepare(rate,invalidA);},[&]{return b.prepare(rate,invalidB);})')
s=s.replace('a.prepare(rate,changedA)&&b.prepare(rate,changedB)','pairedPrepare([&]{return a.prepare(rate,changedA);},[&]{return b.prepare(rate,changedB);})')
s=s.replace('!a.prepare(192000.,pa)&&!b.prepare(192000.,pb)','!pairedPrepare([&]{return a.prepare(192000.,pa);},[&]{return b.prepare(192000.,pb);})')
for a,b,label in [('a.clearDiagnosticFlags()','b.clearDiagnosticFlags()','clear diagnostics'),('assigned=ac','assignedBaseline=bc','copy assignment'),('assigned=assigned','assignedBaseline=assignedBaseline','self assignment'),('a.restoreState(sa)','b.restoreState(sb)','restore state'),('a.restoreIdleState(sa)','b.restoreIdleState(sb)','restore idle'),('a.reset(phase,321.)','b.reset(phase,321.)','reset'),('a.reset(.37,432.)','b.reset(.37,432.)','unprepared reset'),('ac=a','bc=b','unprepared assignment')]:
 old=a+';'+b+';';new='{FpPair fp("'+label+' FP/status/errno");'+a+';fp.between();'+b+';fp.finish();}'
 assert old in s,old;s=s.replace(old,new)
s=s.replace('                const double ia=a.advanceIdle(', '                FpPair idle("advance idle FP/status/errno");const double ia=a.advanceIdle(').replace('                const double ib=b.advanceIdle(', '                idle.between();const double ib=b.advanceIdle(').replace('                check(equal(ia,ib)', '                idle.finish();check(equal(ia,ib)')
s=s.replace('check(equal(a.advanceIdle(.0001,440.,{}),b.advanceIdle(.0001,440.,{}))&&complete(a,b),','FpPair idle("unprepared idle FP/status/errno");const auto ia=a.advanceIdle(.0001,440.,{});idle.between();const auto ib=b.advanceIdle(.0001,440.,{});idle.finish();check(equal(ia,ib)&&complete(a,b),')
old='int main(){matrix();poolAndAllocation();std::printf("Oscillator profile cache: %d checks, %d failures, %zu compared ticks\\n",checks,failures,comparedTicks);return failures?1:0;}'
new='int main(){std::fenv_t original;std::fegetenv(&original);const auto originalMxcsr=_mm_getcsr();const int originalErrno=errno;for(unsigned mode:{0u,0x8040u})for(int seeded:{0,1,2}){fpMode=mode;fpSeed=seeded;matrix();poolAndAllocation();}std::fesetenv(&original);_mm_setcsr(originalMxcsr);errno=originalErrno;std::printf("Oscillator API FP: %d checks, %d failures, %zu compared ticks, %lu paired FP calls\\n",checks,failures,comparedTicks,fpPairs);return failures?1:0;}'
assert old in s;s=s.replace(old,new)
(r/'tests/OscillatorDecayApiFpTests.cpp').write_text(s)
