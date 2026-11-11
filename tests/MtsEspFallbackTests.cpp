#include "plugin/MtsEspClient.h"
#include <iostream>
int main()
{
    cs80::MtsEspClient client;
    cs80::tuning::TuningTable table;
    table.pitch[69] = 72.125f;
    table.mapped[23] = false;
    const auto before = table;
    if (client.available() || client.hasMaster() || client.fillTable(table)
        || table.pitch != before.pitch || table.mapped != before.mapped)
    {
        std::cerr << "no-client build must preserve local tuning\n";
        return 1;
    }
    cs80::MtsEspClient::ChannelTables tables;
    tables[15] = before;
    const auto bankBefore = tables;
    if (client.fillChannelTables(tables) || client.fillChannelTables(tables, true)) return 2;
    for (int channel=0;channel<16;++channel)
        if (tables[channel].pitch != bankBefore[channel].pitch
            || tables[channel].mapped != bankBefore[channel].mapped) return 3;
    return 0;
}
