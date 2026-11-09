#include "core/ReconstructedOscillator.h"
__attribute__((noinline)) double oldKnee(double a,double aSlope,int side) noexcept {
 for(int k=1;k<=4;++k){const double knot=.25*double(k);if(std::abs(a-knot)<2e-11)a=knot+std::copysign(2e-11,aSlope*double(side));}
 return a;
}
__attribute__((noinline)) double newKnee(double a,double aSlope,int side) noexcept { return cs80::oscillator_detail::orientShaperKnee(a,aSlope,side); }
