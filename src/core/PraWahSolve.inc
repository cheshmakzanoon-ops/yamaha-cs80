// Shared mono/stereo PRA capacitor/node solve. Preserve each scalar expression
// and update order: Signal is double or two independent packed doubles.
        std::array<Signal,4> j;
        for (std::size_t k=0;k<j.size();++k)
            j[k]=-g[k]*caps[k].voltage-caps[k].current;

        constexpr double R=10000.0, r=470.0;
        const auto aP=1.0/(1.0+rt*g[1]); const auto bP=-rt*j[1]*aP;
        const auto aQ=1.0/(1.0+R*g[2]); const auto bQ=-R*j[2]*aQ;
        const auto bN=1.0-2.0*aP; const auto b0=2.0*bP;
        const auto cN=(2.0*aQ-1.0)*bN; const auto c0=(2.0*aQ-1.0)*b0+2.0*bQ;
        const double dN=2.0/R+(1.0-cN)/r;
        const auto nX=(1.0/R)/dN; const auto n0=(c0/r)/dN;
        const double sourceDenominator=1.0+profile.sourceOhms*g[0];
        const auto yI=g[0]/sourceDenominator; const auto jSource=j[0]/sourceDenominator;
        out.x=(yI*u+jSource+n0/R)/(yI+(2.0-nX)/R+profile.dryLoadSiemens);
        out.n=nX*out.x+n0;
        out.a=-out.n;
        out.p=aP*out.a+bP;
        out.b=2.0*out.p-out.a;
        out.q=aQ*out.b+bQ;
        out.c=2.0*out.q-out.b;
        out.d=(g[3]*out.c-j[3])/(g[3]+2.0/R);
        out.y=-(profile.vr2Ohms/R)*out.d;
        const Signal inputCurrent=yI*(u-out.x)+jSource;
        const std::array<Signal,4> voltage {
            u-profile.sourceOhms*inputCurrent-out.x, out.p, out.q, out.d-out.c
        };
        for (std::size_t k=0;k<caps.size();++k) {
            caps[k].voltage=voltage[k];
            caps[k].current=g[k]*voltage[k]+j[k];
        }
