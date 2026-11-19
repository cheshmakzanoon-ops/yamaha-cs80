#include "gen/stock.h"
#define cs80 cs80_c
#include "gen/candidate.h"
#undef cs80
#include <cstdio>
int main(){using A=cs80::ReconstructedOscillator;using B=cs80_c::ReconstructedOscillator;std::printf("sizeof %zu %zu align %zu %zu\n",sizeof(A),sizeof(B),alignof(A),alignof(B));return sizeof(A)!=sizeof(B)||alignof(A)!=alignof(B);}
