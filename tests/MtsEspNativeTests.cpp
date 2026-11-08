#include "plugin/MtsEspClient.h"
#include "vendor/mts-esp/Client/libMTSClient.h"

#include <cmath>
#include <iostream>

int main()
{
    if (!cs80::MtsEspClient::available()) return 1;
    int noMaster = 0;
    // Real upstream code, not the deterministic C API fixture. This checks
    // lifecycle and the normal no-library/no-master path, not a live DAW.
    for (int instance = 0; instance < 32; ++instance)
    {
        cs80::MtsEspClient client;
        cs80::tuning::TuningTable table;
        table.pitch[69] = 71.5f;
        table.mapped[23] = false;
        const auto before = table;
        if (!client.hasMaster())
        {
            ++noMaster;
            if (client.fillTable(table) || table.pitch != before.pitch || table.mapped != before.mapped)
                return 2;
        }
    }
    auto* client = MTS_RegisterClient();
    if (!client) return 3;
    bool frequenciesValid = true;
    if (!MTS_HasMaster(client))
        for (int note = 0; note < 128; ++note)
        {
            const double expected = 440.0 * std::exp2((note - 69.0) / 12.0);
            frequenciesValid = frequenciesValid
                && std::abs(MTS_NoteToFrequency(client, static_cast<char>(note), -1) - expected)
                    <= expected * 1e-12;
        }
    MTS_DeregisterClient(client);
    std::cout << "Real MTS-ESP client: 32 lifecycles, " << noMaster << " without a master\n";
    return frequenciesValid ? 0 : 4;
}
