#define LANE_COST_DIAGNOSTIC 1
#include "retained-cost.cpp"
extern "C" void bbd_census_block(unsigned) noexcept;
extern "C" void bbd_census_finish() noexcept;
static void privateBbd(lane_cost::Diagnostic& d,const cs80::BbdChorus& b){
 d.put(b.sr);d.put(b.trim);d.put(b.do1Phase);d.put(b.do2Phase);d.put(b.clockPeriods);d.put(b.mask);d.put(b.newest);d.put(b.cursors);d.put(b.weights);
 auto lp=[&](const auto& l){d.put(l.a1);d.put(l.a2);d.put(l.a3);d.put(l.ic1);d.put(l.ic2);};auto pole=[&](const auto& l){d.put(l.a);d.put(l.b);d.put(l.z);};lp(b.inputLp);for(const auto& c:b.chains){lp(c.reconLp);pole(c.couplingHp);pole(c.interstageHp);}for(const auto& n:b.noise)d.put(n.state);
 d.put(b.samples.size());for(const auto& x:b.samples)d.put(x);d.put(b.stamps.size());for(const auto& x:b.stamps)d.put(x);
}
int main(int argc,char**argv){using namespace lane_cost;if(argc!=3)return 2;int scene=-1;for(int i=0;i<4;++i)if(!std::strcmp(argv[1],labels[i]))scene=i;if(scene<0)return 2;
 if(fesetenv(FE_DFL_ENV))return 3;_mm_setcsr(0x9fc0U);errno=0;Work work(scene);Diagnostic d(argv[2]);
 for(unsigned b=0;b<385;++b){bbd_census_block(b);const auto before=fp();work.step(b);const auto after=fp();d.observe(work,b,before,after);fenv_t saved{};if(fegetenv(&saved))return 4;const int err=errno;privateBbd(d,work.engine->bbdChorus);if(fesetenv(&saved))return 4;errno=err;if(!same(after,fp()))return 5;}
 bbd_census_finish();return 0;
}
