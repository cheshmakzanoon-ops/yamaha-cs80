// Direct public-FilterEnv oracle: intentionally no exporter parser, event iterator,
// provenance code, or production-law copy. Compare every returned state bit.
#include "core/Dsp.h"
#include <iomanip>
#include <iostream>
#include <locale>

int main()
{
    std::cout.imbue(std::locale::classic());
    std::cout << std::setprecision(17);
    cs80::FilterEnv env;
    env.reset();
    for (int i = 0; i != 801; ++i)
    {
        if (i) env.tick(8000.0f, true, true);
        // Ordered ties at zero, while held, during release, and at the final row.
        if (i == 0) { env.noteOff(); env.noteOn(-.7f, 1.3f, .0137f, .02f, .03f, 0.0f, false); }
        if (i == 7) env.noteOff();
        if (i == 11) env.noteOn(-.7f, 1.3f, .0137f, .02f, .03f, 0.0f, true);
        if (i == 61) env.noteOn(-.7f, 1.3f, .0137f, .02f, .03f, 0.0f, false);
        if (i == 200) { env.noteOff(); env.noteOn(-.7f, 1.3f, .0137f, .02f, .03f, 0.0f, true); }
        if (i == 400) { env.noteOn(-.7f, 1.3f, .0137f, .02f, .03f, 0.0f, false); env.noteOff(); }
        if (i == 600) env.reset();
        if (i == 800) env.noteOn(-.7f, 1.3f, .0137f, .02f, .03f, 0.0f, false);
        const auto state = env.getState();
        std::cout << i << ',' << static_cast<double>(state.value) << ','
                  << static_cast<int>(state.stage) << ',' << static_cast<double>(state.elapsed) << '\n';
    }
}
