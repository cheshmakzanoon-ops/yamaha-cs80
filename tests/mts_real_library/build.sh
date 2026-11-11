#!/bin/bash
set -euxo pipefail
cd "$(dirname "$0")"
repo=$(python -c 'import json; print(json.load(open("config.json"))["repo"])')
g++ --version | head -1
g++ -std=c++17 -O2 -Wall -Wextra master.cpp Master/libMTSMaster.cpp -ldl -o master
g++ -std=c++17 -O2 -Wall -Wextra -DCS80_MTS_ESP_TEST_BACKEND=1 -I"$repo/src" client.cpp "$repo/src/plugin/MtsEspClient.cpp" Client/libMTSClient.cpp -ldl -o client
sha256sum master.cpp client.cpp prepare.py run.py build.sh master client > postbuild.sha256
cat postbuild.sha256
