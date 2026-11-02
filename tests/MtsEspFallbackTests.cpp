#include "plugin/MtsEspClient.h"
#include <iostream>
int main()
{
    cs80::MtsEspClient shim;
    cs80::tuning::TuningTable table;
    const auto before = table;
    shim.parseMidi(nullptr, 0);
    if (shim.available() || shim.hasMaster() || shim.fillTable(table)
        || table.pitch != before.pitch || table.mapped != before.mapped)
    {
        std::cerr << "no-client build must remain unavailable and preserve local tuning\n";
        return 1;
    }
    return 0;
}
