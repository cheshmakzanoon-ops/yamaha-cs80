#include "reference/KeyboardContour.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <limits>
using namespace cs80::reference::keyboard_contour;
namespace
{
int checks=0, failures=0;
void check(bool ok, const char* label)
{
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL: %s\n",label); }
}
void near(double a,double b,const char* label)
{
    check(std::isfinite(a) && std::abs(a-b)<2e-11,label);
}
template<class F> void rejects(F f)
{
    bool caught=false;
    try { f(); } catch(const std::invalid_argument&) { caught=true; }
    check(caught,"invalid physical input rejected");
}
Inputs stateInput(int q)
{
    Inputs p;
    p.q1=(q&4)!=0; p.q2=(q&2)!=0; p.q3=(q&1)!=0;
    return p;
}
// Independent 12-node conductance solve, not the reference's closed-form ramps.
// Each adjacent tap is joined by 1k; endpoint gate enables follow gate expressions.
std::array<double,12> nodal(const Inputs& p,double o,double r)
{
    std::array<std::array<double,13>,12> a {};
    for(int j=0;j<11;++j)
    {
        a[j][j]+=.001; a[j+1][j+1]+=.001;
        a[j][j+1]-=.001; a[j+1][j]-=.001;
    }
    const auto fixed=[&](int j,double v) { a[j].fill(0); a[j][j]=1; a[j][12]=v; };
    const auto resistor=[&](int j,double ohms) { a[j][j]+=1/ohms; a[j][12]+=r/ohms; };
    const bool d=(!p.q1&&!p.q2&&!p.q3)||(!p.q2&&p.q3);
    if(d) fixed(11,o); else fixed(0,o);
    if(!p.q1&&p.q2&&!p.q3) fixed(11,r);
    if(p.q1&&p.q2&&!p.q3) resistor(11,22000+p.vr1Ohms);
    if(!p.q1&&!p.q2&&p.q3) fixed(0,r);
    if(p.q1&&!p.q2&&p.q3) resistor(0,20000+p.vr2Ohms);
    for(int j=0;j<12;++j)
    {
        int pivot=j;
        for(int k=j+1;k<12;++k) if(std::abs(a[k][j])>std::abs(a[pivot][j])) pivot=k;
        check(std::abs(a[pivot][j])>1e-14,"nodal system nonsingular");
        std::swap(a[j],a[pivot]);
        const double scale=a[j][j];
        for(int k=j;k<13;++k) a[j][k]/=scale;
        for(int row=0;row<12;++row) if(row!=j)
        {
            const double g=a[row][j];
            for(int k=j;k<13;++k) a[row][k]-=g*a[j][k];
        }
    }
    std::array<double,12> v {};
    for(int j=0;j<12;++j) v[j]=a[j][12];
    for(int j=1;j<11;++j) near((v[j]-v[j-1])/1000+(v[j]-v[j+1])/1000,0,"interior KCL");
    return v;
}
}
int main()
{
    const double r=referenceVolts;
    for(int q=0;q<8;++q) for(int oc=0;oc<6;++oc) for(int m=0;m<12;++m)
    {
        auto p=stateInput(q); p.selectedOc=oc; p.noteTap=m;
        p.vr1Ohms=2136.0; p.vr2Ohms=4136.0;
        near(evaluate(p).outputVolts,0,"neutral all raw states/OC/taps");
    }
    near(loadedWiperVolts(0),0,"center ground");
    near(loadedWiperVolts(1),15,"positive rail");
    near(loadedWiperVolts(-1),-15,"negative rail");
    near(loadedWiperVolts(.5),7.5*56/57.25,"loaded midpoint");
    for(double x : {-1.0,-.5,-.125,0.0,.125,.5,1.0})
    {
        near(loadedWiperVolts(-x),-loadedWiperVolts(x),"electrical odd polarity");
        // Independently write KCL between the two half-track endpoints and 56k load.
        const double a=std::abs(x);
        if(a>0 && a<1)
        {
            const double w=loadedWiperVolts(x);
            near((w-std::copysign(15.0,x))/(5000*(1-a))+w/(5000*a)+w/56000,0,"pot KCL");
        }
    }
    for(double lo : {-1.0,-.5,0.0,.5,1.0}) for(double hi : {-1.0,0.0,1.0})
    for(double trim : {0.0,2500.0,5000.0}) for(int q=0;q<8;++q) for(int oc=0;oc<6;++oc)
    {
        auto p=stateInput(q); p.selectedOc=oc; p.lowTravel=lo; p.highTravel=hi;
        p.vr1Ohms=trim; p.vr2Ohms=5000-trim;
        const auto out=evaluate(p);
        const auto nodes=nodal(p,out.ooVolts,r);
        near((out.lowReferenceVolts-r)/15000+out.lowWiperVolts/56000,0,"LOW summer KCL");
        near((out.highReferenceVolts-r)/15000+out.highWiperVolts/56000,0,"HIGH summer KCL");
        near((out.octaveVolts[2]-out.octaveVolts[1])/1500+(out.octaveVolts[2]-r)/3000,0,"LOW octave KCL");
        near((out.octaveVolts[4]-r)/3000+(out.octaveVolts[4]-out.octaveVolts[5])/1500,0,"HIGH octave KCL");
        for(int m=0;m<12;++m)
        {
            p.noteTap=m; const auto v=evaluate(p);
            near(v.sampledVolts,nodes[m],"independent endpoint nodal solve");
            near(v.outputVolts,r-nodes[m],"detector reference subtraction");
            // Permit roundoff at ideal rails, including fused operations on ARM.
            check(v.lowReferenceVolts>=-2e-11 && v.lowReferenceVolts<=2*r+2e-11,"LOW reference bounds");
            check(v.highReferenceVolts>=-2e-11 && v.highReferenceVolts<=2*r+2e-11,"HIGH reference bounds");
        }
    }
    // SUPPLIED states consistent with calibration; these tests do not prove key mapping.
    auto low=stateInput(6); low.selectedOc=1; low.noteTap=11; low.lowTravel=-1;
    auto high=stateInput(5); high.selectedOc=5; high.noteTap=0; high.highTravel=1;
    constexpr double target=2.76;
    low.vr1Ohms=(target*33000-r*22000)/(r-target);
    high.vr2Ohms=(target*31000-r*20000)/(r-target);
    near(low.vr1Ohms,2136.286201022144,"calibration-consistent VR1");
    near(high.vr2Ohms,4136.286201022134,"calibration-consistent VR2");
    near(evaluate(low).outputVolts,-target,"supplied C2-like fixture");
    near(evaluate(high).outputVolts,target,"supplied C5#-like fixture");
    low.vr1Ohms=0; near(evaluate(low).outputVolts,-r*22/33,"VR1 minimum");
    low.vr1Ohms=5000; near(evaluate(low).outputVolts,-r*27/38,"VR1 maximum");
    high.vr2Ohms=0; near(evaluate(high).outputVolts,r*20/31,"VR2 minimum");
    high.vr2Ohms=5000; near(evaluate(high).outputVolts,r*25/36,"VR2 maximum");
    for(double bad : {-1.01,1.01,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()})
    {
        rejects([&]{loadedWiperVolts(bad);});
        rejects([&]{Inputs p; p.highTravel=bad; evaluate(p);});
    }
    for(double bad : {-1.0,5000.01,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()})
    {
        rejects([&]{Inputs p; p.vr1Ohms=bad; evaluate(p);});
        rejects([&]{Inputs p; p.vr2Ohms=bad; evaluate(p);});
    }
    for(int bad : {-1,6}) rejects([&]{Inputs p; p.selectedOc=bad; evaluate(p);});
    for(int bad : {-1,12}) rejects([&]{Inputs p; p.noteTap=bad; evaluate(p);});
    std::printf("%d checks, %d failures\n",checks,failures);
    return failures ? 1 : 0;
}
