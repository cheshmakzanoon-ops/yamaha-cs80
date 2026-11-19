# Native callback PC sampler (Linux/x86-64, opt-in)

Where is a *real* stock VST3 spending its callback CPU while quiet, sustaining eight voices, or waking? See the [actual stock result and limits](../../docs/experiments/native-idle-profile/README.md). This is a separate executable; never use it for timing comparisons, shipment, callback latency or real-time admission. No root, `perf`, privileged kernel settings or rebuild of the module required. Needs glibc/Linux x86-64, GCC C++20, Python 3, GNU binutils, taskset, the project checkout, the official JUCE 8.0.11 VST3 SDK headers/IID support sources and an already-built CS-80 Linux release VST3 ELF.

## New collection

```sh
python3 -B tools/native-vst3-profile/profile.py build --sdk /path/to/VST3_SDK --output /tmp/cs80-pc-host
python3 -B tools/native-vst3-profile/profile.py collect --cpu 2 --host /tmp/cs80-pc-host \
    --module "/path/inside/CS-80 Reference.vst3/Contents/x86_64-linux/CS-80 Reference.so" --output /tmp/cs80-pcs
python3 -B tools/native-vst3-profile/profile.py pack --captures /tmp/cs80-pcs --host /tmp/cs80-pc-host \
    --module "/path/inside/CS-80 Reference.vst3/Contents/x86_64-linux/CS-80 Reference.so" --output /tmp/cs80-pcs.json.gz
python3 -B tools/native-vst3-profile/profile.py replay --evidence /tmp/cs80-pcs.json.gz --json
```

Select an allowed CPU. The requested executable path and acquisition directory must not exist. `collect` runs the fixed 12 fresh processes (including four zero-timer controls); it does not retry failed slots or overwrite retained evidence. Match the plugin's runtime loader environment. Each slot uses a fresh HOME, standard locale and a timeout. No signal handler installed for the zero-period control. A comparable stand-in plugin, stripped binary with no usable symbols, system-wide CPU percentage or standalone core renderer is not an alternative to the identified module.

Without DWARF in the module, the pack still reports bounded physical functions and leaves source lines unknown. For finer attribution, pass `--debug-module /path/to/EXACT-CODE-mapping-twin.so` to `pack`. Use the *same* release compilation and link recipe. For the recorded twin only `Engine.cpp` added `-g`, the project's existing ELF helper checked all allocated object sections and normalized relocations against stock, and that one member was replaced in a private copy of the core archive. The module was relinked out-of-band using the unchanged object/library order. The packer rejects the mapping module unless the linked allocated code, data, addresses and alignments match stock, excluding only the build-ID note. Only the stock release binary is ever loaded and sampled.

## Replay and rebinding

```sh
python3 -B tools/native-vst3-profile/profile.py replay
python3 -B tools/native-vst3-profile/test_profile.py
python3 -B tools/native-vst3-profile/profile.py replay --host /path/to/original/profile_host \
    --module /path/to/original/stock.so --debug-module /path/to/original/mapping-twin.so
```

Plain replay uses the [bundled normalized evidence](../../docs/experiments/native-idle-profile/captures.json.gz) and does not build, load a plug-in, read a clock or request privileges. The optional final form additionally requires the *exact* originally captured binaries (hashes checked), checks the loaded build-ID receipt, ELF executable ranges and reproduces all symbol and source annotations. A different compiler, checkout path, release config or rebuilt module is a new acquisition.

The executable reuses the frozen factory host/scaffold rather than creating another JUCE adapter or altering archived experiments. Codes in the portable PC rows: region 1 = known calibration loop, 2 = measured callback, 3 = first callback of a genuine wake onset. Only regions 2+3 enter callback totals. `@plugin` is the ELF containing the verified `GetPluginFactory`; `@host` is the test executable. Libraries/unknowns remain separate rather than assigning them to speculative callers. The leaf/source is an annotation of that one physical PC, not a second sample.
