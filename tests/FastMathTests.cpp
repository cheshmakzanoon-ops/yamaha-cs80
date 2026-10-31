// Bounded-error approximations of src/core/FastMath.h against the standard
// library on dense grids. The bounds are the ones stated in the header.
#include "core/FastMath.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

int main()
{
    double worstTan = 0.0;
    for (int i = 1; i <= 2000000; ++i)
    {
        const double x = 1.45 * i / 2000000.0;
        worstTan = std::max(worstTan, std::abs(cs80::fastmath::tan(x) / std::tan(x) - 1.0));
    }
    std::printf("fast tan on (0, 1.45]: worst relative error %.3g (bound 1e-10)\n", worstTan);
    if (worstTan > 1e-10)
    {
        std::printf("FAIL: tan bound\n");
        return 1;
    }
    return 0;
}
