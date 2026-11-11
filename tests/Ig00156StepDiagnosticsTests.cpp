#include "reference/IG00156Network.h"
#include <iostream>
#include <limits>

int main() {
    using namespace cs80::reference::ig00156;
    StageNetwork hp{75000, 33000, 1e-6}, lp{150000, 33000, 1e-6};
    StageControl control{1e-5, 1e-5, 1e-6, 2.5};
    int failures = 0;
    auto check = [&](bool ok, const char* name) { if (!ok) { ++failures; std::cerr << name << '\n'; } };
    NonlinearPair checked(hp, control, lp, control, 1, 1), original(hp, control, lp, control, 1, 1);
    for (int i = 0; i < 1000; ++i) {
        double u0 = std::sin(i * .01), u1 = std::sin((i + 1) * .01);
        auto result = checked.stepChecked(u0, u1, 1./1536000);
        original.step(u0, u1, 1./1536000);
        check(result.finite && result.converged && result.residual <= 1e-14 * result.scale, "certified fine step");
        check(checked.state() == original.state(), "unchanged original state arithmetic");
    }
    NonlinearPair exhausted(hp, control, lp, control, 1, 1);
    auto result = exhausted.stepChecked(1, -1, .001);
    check(result.finite && !result.converged && result.iterations == 50, "finite exhausted Newton must not be certified");
    NonlinearPair nonfinite(hp, control, lp, control, 1, 1);
    result = nonfinite.stepChecked(0, std::numeric_limits<double>::quiet_NaN(), 1./1536000);
    check(!result.finite && !result.converged, "nonfinite Newton must not be certified");
    std::cout << "Step diagnostics: " << failures << " failures\n";
    return failures ? 1 : 0;
}
