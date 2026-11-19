// Additional discrete restore/edge oracle. Counts, does not measure time.
#define main retained_direct_cases_not_run
#include "direct.cpp"
#undef main
static uint64_t seed=0x2ee951cc321ULL;
static uint64_t rng(){seed=seed*6364136223846793005ULL+1442695040888963407ULL;return seed;}
static double unit(){return double(rng()>>11)*0x1p-53;}
int main(){
 const unsigned mx0=_mm_getcsr();unsigned short cw0;asm volatile("fnstcw %0":"=m"(cw0));
 initFp(0x1fa0);S s;C c;
 for(int k=0;k<8192;++k) {
  const double f=16.+(2048.-16.)*unit(), thr=(unit()*2.-1.)*3.99;
  const bool saw=rng()&1,pulse=rng()&2;
  s.reset(.37,f);c.reset(.37,f);
  auto a=s.getState();auto o=c.getState();
  const double q=(k%16==0)?4.*f/96000.+(int(k%3)-1)*1e-10:unit();
  const double ramp=3.05/(1.-.00003*261.6);
  const double pin=ramp*q;
  double u0,w0;const double jitter=(int(k%3)-1)*2.e-11;
  switch((k/16)%5){
   case 0: {const double level[]={-1.525,0.,1.525,thr}; u0=level[k%4]+jitter;w0=(2.*unit()-1.)*3.; break;}
   case 1: {const int knot=int(rng()%8);w0=.4*(knot<4?knot-4:knot-3)+jitter;u0=unit()*3.-1.5;break;}
   case 2: u0=(2.*unit()-1.)*7.;w0=(2.*unit()-1.)*7.;break;
   case 3: u0=std::copysign(1.e-20,unit()-.5);w0=std::copysign(1.e-22,unit()-.5);break;
   default:u0=(2.*unit()-1.)*1.5;w0=(2.*unit()-1.)*1.8;break;
  }
  double tri=3.2*std::min(1.,std::abs(u0)/1.525);
  a.charge=o.charge=q;a.resetRemainingSeconds=o.resetRemainingSeconds=0.;
  a.inputBaselineVolts=o.inputBaselineVolts=pin-u0;a.triangleBaselineVolts=o.triangleBaselineVolts=tri-w0;
  a.heldFrequencyHz=o.heldFrequencyHz=f;a.heldThresholdVolts=o.heldThresholdVolts=thr;
  a.sawEnabled=o.sawEnabled=saw;a.pulseEnabled=o.pulseEnabled=pulse;
  a.residualCursor=o.residualCursor=rng()%16;
  for(unsigned i=0;i<3;++i)for(unsigned j=0;j<16;++j)a.residual[i][j]=o.residual[i][j]=(rng()&1)?unit()-.5:0.;
  // Adversarial residual/elapsed inputs, without allowing an illegal cursor.
  if(k%37==0){uint64_t bc=uint64_t(0x7ff0000000000015ULL);a.residual[k%3][(a.residualCursor+2)%16]=o.residual[k%3][(o.residualCursor+2)%16]=std::bit_cast<double>(bc);}
  if(k%41==0)a.residual[k%3][(a.residualCursor+2)%16]=o.residual[k%3][(o.residualCursor+2)%16]=std::numeric_limits<double>::denorm_min();
  if(k%127==0)a.elapsedSeconds=o.elapsedSeconds=std::bit_cast<double>(uint64_t(0x7ff0000000000032ULL));
  s.restoreState(a);c.restoreState(o);
  for(int j=0;j<12;++j){
   unsigned mx=(k%2?0x9fe0:0x1fa0);unsigned short cw=0x37f;
   if((k%19)==0)mx=0x1fc0;if(k%23==0)mx=0x9f80;if(k%29==0)cw=0x077f;
   const double t=(k%11==0&&j==5)?-thr:thr;
   cs80::WscControls sc{t,saw,pulse};cs80_candidate::WscControls cc{t,saw,pulse};
   initFp(mx,cw);auto x=s.tick(f,sc);auto fx=flags();initFp(mx,cw);auto y=c.tick(f,cc);auto fy=flags();
   ck(out(x,y),"restored pins",k,j);ck(fx==fy,"restored status/errno",k,j);compare(s,c,k,j);++pairs;
  }
 }
#ifdef CS80_WSC_HORIZON_VERIFY
 ck(C::horizonFalseWitnesses==0,"stock no-root independent witness",-1,0);
#endif
 std::printf("restored_scenarios=8192 pairs=%lld checks=%lld failures=%lld admitted=%llu\n",pairs,checks,failures,(unsigned long long)C::horizonFast);
 _mm_setcsr(mx0);asm volatile("fldcw %0"::"m"(cw0));return failures?1:0;
}
