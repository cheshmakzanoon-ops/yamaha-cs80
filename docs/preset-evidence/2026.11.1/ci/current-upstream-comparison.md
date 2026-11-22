# Official beta comparison — source-bound review

**All three comparison lanes passed; the stable 1.0.4 gate remains failed.** Run [45060638632](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45060638632), attempt 1, workflow source 96c466f0e7c25055ac8e745d2c796af643860fc6.

Every lane downloaded native source **6d3cc220edef5a4a2c7cd968ea8a786c7c83198c**, originating failed build **45044040694 / attempt 1**, VST3 artifact **14768852300**, 6,892,184 bytes. API, expected and actual download SHA256 all match **b878f70ed85ab6e5aefd757891895b96714087c0fb6f170feb26b385da95c698**.

| Lane | Job | Seed | Actual JSON/JUnit pass count | Process / logging exits |
|---|---:|---|---:|---|
| gui-current-regression | 137325542552 | 0x3ca4063 | 31/31 | 0 / 0 |
| gui-inclusive | 137325542836 | 0x63421ca | 31/31 | 0 / 0 |
| headless-subset | 137325542917 | 0x4f31605 | 25/25 | 0 / 0 |

Each lane logged strictness **10** and explicit SUCCESS, one repeat and zero validation retries. All reported cases passed, with zero failed/error/skipped cases. Headless is a subset; GUI lanes ran under Xvfb.

The official release tag is **v2.0.0-beta3**, commit **37d6a37bc752606c524f7f3febffc73e0945a740**, JUCE **9.0.3 / be29c81492b6151c8ea8d14c840e1311963b3a83** according to retained workflow provenance. The archive was verified against **2e23d311c8c2664457b11be8f6cf4ed29cfdc08765c71c43934a1ed3360f118f**. Actual executable version is **pluginval - 2.0.0**, binary SHA256 **79612f9dd1a93f24920a7244a46c50bc7ff1bdf8174c013acee19032f5047f24**.

All lanes recorded the same plugin native SHA256 **270d91d746d7bc4496c0097755b997f0ba102728c3d17f7050225f1392250265** and moduleinfo.json SHA256 **01f8d197948fffeec78d876a71bccad19a5319b33f7f3069d098bb8fcad4a1f4**.

**Naming limitation:** gui-current-regression uses seed 0x3ca4063. None of these comparison lanes replays the current failed stable GUI seed **0x4369262**. Different tool/JUCE versions, inventories and seeds do not establish why stable 1.0.4 crashed, and do not qualify the blocked versioned release.

Three original comparison evidence ZIPs were checked against API size/SHA256 and ZIP CRC; all member hashes, terminal job logs, original JSON/JUnit and provenance are retained. See verification.json, api-provenance.json and download-verification.json. No rerun, CI change or local plugin execution was performed.
