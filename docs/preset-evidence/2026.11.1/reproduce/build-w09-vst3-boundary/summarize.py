#!/usr/bin/env python3
"""Create compact evidence from the retained native-module runs."""
import csv
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import xml.etree.ElementTree as ET

out = Path(__file__).resolve().parent
root = out.parent
frozen = 'be460b0b93bdcb8dce5eb8925b18b72706b25e19'
def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()
def read_csv(path):
    with path.open(newline='') as stream:
        return list(csv.DictReader(stream))
def write_csv(path, rows):
    with path.open('w', newline='') as stream:
        writer = csv.DictWriter(stream, list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
def native_xml(path):
    data = path.read_bytes()
    stop = data.index(b'</CS80_PARAMS>') + len(b'</CS80_PARAMS>')
    return ET.fromstring(data[:stop])
def f32(value):
    return struct.pack('<f', float(value))

rows = read_csv(out / 'parameters.csv')
programs = read_csv(out / 'programs.csv')
inventory = json.loads((root / 'build-audit/inventory-final.json').read_text())
identity = json.loads((root / 'build-w05-final-identity/summary.json').read_text())
expected = {(p['name'], key): value for p in inventory['programs']
            for key, value in zip(inventory['parameter_ids'], p['values'])}
assert len(rows) == 4560
assert len(programs) == 12
assert all(f32(r['expected_plain']) == f32(expected[(r['program'], r['id'])]) for r in rows)
assert all(float(r['normalized_error']) <= 1e-6 for r in rows)
assert all(r['actual_plain'] == r['actual_normalized'] for r in rows)
roster = (out / 'native-programs.tsv').read_text().splitlines()
assert len(roster) == 524
assert roster[-1] == '522\tInit Circuit'
run = json.loads((out / 'run-result.json').read_text())
assert run['raw_exit_code'] == 0
checks, failures = map(int, re.search(r'Native factory boundary: (\d+) checks, (\d+) failures', (out / 'run.log').read_text()).groups())
assert (checks, failures) == (45115, 0)

changed = {(p['name'], c['parameter']): c for p in identity['changed_programs'] for c in p['changes']}
focused = []
for row in rows:
    key = row['program'], row['id']
    if key not in changed:
        continue
    change = changed[key]
    assert f32(row['expected_plain']) == f32(change['after'])
    focused.append({'program': row['program'], 'host_index': row['host_index'], 'phase': row['phase'],
                    'id': row['id'], 'before_physical': change['before'],
                    'expected_physical': row['expected_plain'],
                    'expected_normalized': row['expected_normalized'],
                    'actual_normalized': row['actual_normalized'],
                    'controller_plain_api_value': row['actual_plain'],
                    'normalized_error': row['normalized_error']})
assert len(changed) == 42 and len(focused) == 84
write_csv(out / 'changed-values.csv', focused)

states, state_values = [], []
for program in programs:
    name, index = program['name'], int(program['host_index'])
    path = out / f'{index}-state.bin'
    xml = native_xml(path)
    assert xml.get('presetName') == name and int(xml.get('factoryIndex')) == index
    values = {p.get('id'): p.get('value') for p in xml.findall('PARAM')}
    assert all(key in values for key in inventory['parameter_ids'])
    states.append({'name': name, 'host_index': index, 'file': path.name,
                   'bytes': path.stat().st_size, 'sha256': sha(path),
                   'state_schema_version': xml.get('stateSchemaVersion')})
    for key in inventory['parameter_ids']:
        state_values.append({'program': name, 'host_index': index, 'id': key,
                             'expected_physical': float(next(r['expected_plain'] for r in rows if r['program'] == name and r['phase'] == 'load' and r['id'] == key)),
                             'saved_state_physical': float(values[key]),
                             'changed_in_revision': int((name, key) in changed)})
write_csv(out / 'saved-state-physical.csv', state_values)

initial = out / 'initial-audio-queue-only'
negative = out / 'queue-only-reproduction'
identical = {name: (initial / name).read_bytes() == (negative / name).read_bytes()
             for name in ['parameters.csv', 'programs.csv', 'run.log']}
assert all(identical.values())
negative_run = json.loads((negative / 'run-result.json').read_text())
assert negative_run['raw_exit_code'] == 1
assert negative_run['probe_sha256'] == json.loads((initial / 'run-result.json').read_text())['probe_sha256']
negative_states = []
for program in programs:
    path = negative / (program['host_index'] + '-state.bin')
    xml = native_xml(path)
    values = {p.get('id'): p.get('value') for p in xml.findall('PARAM')}
    assert xml.get('presetName') == 'Brass Swell I' and xml.get('factoryIndex') == '222'
    negative_states.append({'requested_name': program['name'], 'requested_host_index': int(program['host_index']),
                            'observed_preset_name': xml.get('presetName'), 'observed_factory_index': int(xml.get('factoryIndex')),
                            'sha256': sha(path), 'bytes': path.stat().st_size,
                            'sample_physical_values': {key: float(values[key]) for key in ['mix', 'volume', 'ch1_pan', 'ch2_pan', 'ch2_noise']}})

module = root / 'build-native/CS80Reference_artefacts/Release/VST3/CS-80 Reference.vst3/Contents/x86_64-linux/CS-80 Reference.so'
sources = ['src/core/FactoryPresets.cpp', 'src/core/FactoryPresets.h', 'src/core/Params.h',
           'src/plugin/PluginProcessor.cpp', 'src/plugin/PluginProcessorLibrary.cpp',
           'src/plugin/HostAutomationBridge.h', 'tests/Vst3AutomationTests.cpp',
           'deps/juce/modules/juce_audio_plugin_client/juce_audio_plugin_client_VST3.cpp',
           'deps/juce/modules/juce_audio_processors_headless/format_types/VST3_SDK/pluginterfaces/vst/ivsteditcontroller.h',
           'build-audit/libcs80_core.a', 'build-audit/inventory-final.json',
           'build-w05-final-identity/summary.json']
diff = subprocess.check_output(['git', 'diff', '--name-only', frozen, '--', 'src', 'tests/Vst3AutomationTests.cpp'], cwd=root, text=True)
assert not diff.strip()
data = {
    'assignment': 44, 'status': 'PASS_FOR_CONTROLLER_SELECTION_AND_STATE_RESTORE',
    'generated_utc': datetime.now(timezone.utc).isoformat(),
    'frozen_source_commit': frozen,
    'observed_head': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip(),
    'qualification': 'The local Linux VST3 was compiled before publication; the checked DSP/preset source is identical to frozen final be460b0. This is not a verification of a downloaded release asset.',
    'sources_match_frozen_commit': True, 'library_revision': inventory['library_revision'],
    'module': {'path': str(module.relative_to(root)), 'bytes': module.stat().st_size, 'sha256': sha(module)},
    'native_program_count': 523, 'all_native_names_and_order_match': True,
    'native_parameter_count': 2378,
    'native_parameter_breakdown': {'application_layout_controls': 296, 'wrapper_program_and_bypass': 2, 'midi_cc_emulation_controls': 2080,
                                  'basis': '2378 measured from native controller; 296 application controls and 16 x 130 MIDI controls identified from the repository layout contract and pinned JUCE source.'},
    'program_control': {'id': 1886548852, 'id_hex': '0x70727374', 'flags': 32769, 'flags_hex': '0x8001',
                        'flag_names': ['kCanAutomate', 'kIsProgramChange'], 'step_count': 522},
    'verification': {'raw_process_exit': run['raw_exit_code'], 'checks': checks, 'failures': failures,
                     'revised_programs': 12, 'core_parameters_per_program': 190, 'phases': ['load', 'restore'],
                     'value_comparisons': len(rows), 'changed_values_in_revision': 42, 'changed_value_phase_comparisons': len(focused),
                     'expected_values_float32_match_final_inventory': True,
                     'normalized_tolerance': 1e-6,
                     'max_load_error': max(float(p['max_load_error']) for p in programs),
                     'max_restore_error': max(float(p['max_restore_error']) for p in programs),
                     'max_roundtrip_error': max(float(p['max_roundtrip_error']) for p in programs),
                     'max_changed_value_error': max(float(r['normalized_error']) for r in focused)},
    'selection_sequence': ['Create actual native component and controller; connect both IConnectionPoint interfaces.',
                           'Set Program through IEditController::setParamNormalized(index/522); send matching offset-zero IParameterChanges in an active zero-frame IAudioProcessor::process call.',
                           'Read IComponent::getState; transfer with IEditController::setComponentState; inspect native values and save processor stream.',
                           'Select Init Circuit at index 522 using the same controller-plus-queue path; confirm its index.',
                           'Restore IComponent::setState, refresh native controller from processor state, confirm original index and all 190 core values.'],
    'fixture': {'sample_rate': 48000, 'sample_type': 'float32', 'process_mode': 'kRealtime', 'max_block_size': 4096,
                'selection_process_frames': 0, 'display_unset': True, 'native_editor_opened': False,
                'notes_or_midi_program_change_sent': False, 'host_context': 'nullptr',
                'component_handler_installed': False, 'host_message_loop': False},
    'plain_api_qualification': 'JUCE core Param::toPlain returns the normalized argument. parameters.csv actual_plain is raw native normalizedParamToPlain API output, not physical hertz/pan/etc. expected_plain is the source physical value. saved-state-physical.csv reads real physical values from processor XML; comparisons use normalized units.',
    'programs': programs, 'saved_states': states,
    'queue_only_observation': {'status': 'UNRESOLVED_FIXTURE_OR_SUPPORTED_PATH_DEFECT',
                               'original_path': 'initial-audio-queue-only', 'exact_reproduction_path': 'queue-only-reproduction',
                               'raw_exit': 1, 'checks': 44592, 'failures': 1336,
                               'source_and_executable_reproduced': True, 'original_and_reproduction_outputs_identical': identical,
                               'invocation': 'Active zero-frame process() with one Program queue point at offset 0 and value index/522; then processor state transferred to connected controller. No preceding controller setParamNormalized call.',
                               'observed_states': negative_states,
                               'interpretation': 'This fixture did not select the requested factory program. Because Program advertises kCanAutomate, this observation is not dismissed as an invalid or unsupported protocol. This task does not distinguish an incomplete host fixture from a supported-path defect. Controller selection passing does not resolve queue-only behavior.'},
    'limits': ['No named DAW was launched or certified.', 'MIDI Program Change routing was not exercised.',
               'No rendered-audio or acoustic fidelity claim follows from this state/mapping probe.',
               'No cross-platform binary or downloaded release archive tested here.',
               'Restore is within the stated normalized tolerance, not bit-identical.',
               'The initial queue-only result remains open for separate focused diagnosis.'],
    'source_and_input_sha256': {name: sha(root / name) for name in sources}
}
(out / 'summary.json').write_text(json.dumps(data, indent=2) + '\n')
report = '''# Actual VST3 factory selection and state evidence — assignment 44

**PASS for the controller-selection and processor-state path:** the actual local Linux VST3 exposes all **523 factory names in the expected order**; every core setting of the **12 revised Circuit programs** matches the final bank after native selection and state restoration. The process exited **0**, including component/controller destruction and module unload.

This is a bounded native-module probe, based on the existing `tests/Vst3AutomationTests.cpp` host. It does not instantiate the plugin processor directly. Production source files were not edited and the full test suite was not rebuilt.

## Coverage and numerical result

| Check | Result |
|---|---:|
| Complete native factory list | 523 names and stable indices verified |
| Revised programs | 12 |
| Core values per program | 190 |
| Native value comparisons, load and restore | 4,560 |
| Changed values covered | All 42, in both phases |
| Explicit internal checks | 45,115 |
| Failures | 0 |
| Maximum load error, normalized | 2.9802322387695312e-8 |
| Maximum restored-versus-source error, normalized | 4.470348358154297e-8 |
| Maximum restored-versus-loaded difference, normalized | 1.4901161193847656e-8 |
| Prespecified normalized tolerance | 1e-6 |

Expected values come from the compiled final core catalogue. All 4,560 expectations were separately checked for float32 identity against `build-audit/inventory-final.json`; all 42 changed values also match assignment 27's final identity diff. The native states contain the selected factory name/index and all 190 core physical values. Small float normalization and state serialization roundoff remains; the result is not a bit-identity claim.

| Revised program | Stable host index |
|---|---:|
'''
for p in programs:
    report += f"| {p['name']} | {p['host_index']} |\n"
report += '''
## Native boundary and state sequence

The probe loads the actual `.so` with `dlopen`, obtains its VST3 factory, creates native `IComponent`, `IAudioProcessor`, and `IEditController` objects, and connects both `IConnectionPoint` interfaces. For each program it calls native `IEditController::setParamNormalized(Program, index / 522.0)`, sends the matching offset-zero Program parameter queue in a zero-frame `IAudioProcessor::process` call, captures processor state, transfers that state to the native controller, and reads every core parameter.

It then selects Init Circuit at host index 522, verifies that index, restores the saved stream through `IComponent::setState`, transfers the restored processor state back into the controller, and verifies the original program index and all 190 values again. Twelve processor streams are retained. The fixture uses 48 kHz, float32, realtime process mode, maximum block size 4096, no editor, no host context/component handler/message loop, and isolated configuration paths.

### Program metadata and parameter count

The native Program ID is **1886548852 / 0x70727374**, with **522 steps** and flags **32769 / 0x8001 = kCanAutomate | kIsProgramChange**. It is explicitly advertised as automatable. The controller reports **2,378 native parameters**, with unique IDs verified per instance. The source-derived decomposition is 296 application controls, Program and bypass, and JUCE's 16 × 130 MIDI-controller emulation controls. The measured 2,378 count must not be mislabeled as the 296-control application layout.

The pinned JUCE `Param::toPlain()` intentionally returns its normalized argument. Consequently, raw `parameters.csv` column `actual_plain` is the native API's reported value, while `expected_plain` is the factory's physical value; these columns are not directly comparable for pan, cutoff, and other scaled controls. The asserted comparison uses `expected_normalized` versus `actual_normalized`. `saved-state-physical.csv` extracts the actual physical values from saved processor XML, and `changed-values.csv` uses explicit column labels.

## Separate unresolved queue-only observation

The first run omitted the controller `setParamNormalized` call and supplied only a Program queue point at offset zero with `index / 522.0` to the active zero-frame processor call. It then captured the processor stream and refreshed the connected controller from that stream. That run exited **1** with **1,336 failures**: the requested factory programs were not selected. It remains a failed observation, separate from the passing result above.

`initial-audio-queue-only/` preserves the original probe source, run log, exit result, parameter readbacks and per-program report. `queue-only-reproduction/` adds complete compiler/run argument arrays and the native state streams. The reproduced executable has the **same SHA256 as the original**, and its full run log, parameter CSV and program CSV are **byte-identical** to the original. All reproduced state snapshots retain `presetName="Brass Swell I"`, `factoryIndex="222"`, with the fixture's initialized core values (for example mix 0.5, volume approximately 0.62, and both pans 0).

Because Program advertises `kCanAutomate`, this is not labeled an invalid or unsupported protocol. This bounded mapping task does **not** resolve whether the queue-only observation reflects an incomplete host fixture or a supported-path defect. The passing controller path does not settle that question. Separate focused diagnosis is required; production behavior was not changed to hide or bypass the result. This is also distinct from MIDI Program Change routing, which was not exercised.

## Binary identity and scope

Tested module: `build-native/CS80Reference_artefacts/Release/VST3/CS-80 Reference.vst3/Contents/x86_64-linux/CS-80 Reference.so`

SHA256: `82eb87ac9e8562f2201b8c4e0b3328224ea44d87c4fd102f44e481f6739a46b2`

Size: 17,326,728 bytes. Final bank revision: **2026.11.1**. The local binary was built before publication; DSP/preset source is identical to frozen final commit `be460b0b93bdcb8dce5eb8925b18b72706b25e19`. No source difference exists between that commit and the sources inspected here. This check does not identify the local binary as a downloaded release asset.

No named DAW, native editor, MIDI Program Change, other operating system, or downloaded release bundle was tested by this probe. It verifies native selection, mapping, catalogue identity and state persistence; it does not establish acoustic fidelity or listening quality.

## Files and reproduction

- `summary.json`: machine-readable scope, results, limits, source hashes and state hashes.
- `probe.cpp`: temporary native host based on the existing repository helper.
- `run-result.json`, `run.log`, `programs.csv`: raw process result and final per-program evidence.
- `parameters.csv`: all 4,560 native comparison rows.
- `native-programs.tsv`: full 523-program native catalogue in host order.
- `changed-values.csv`: all 42 revised values, after load and after restoration.
- `saved-state-physical.csv`: all 2,280 physical core values read from saved processor XML.
- `*-state.bin`: the 12 native processor streams.
- `initial-audio-queue-only/`, `queue-only-reproduction/`: the retained unresolved negative observation.

From the repository root, `python3 build-w09-vst3-boundary/reproduce.py` builds only the small native host and runs the controller/state check into `controller-reproduction/`. `python3 build-w09-vst3-boundary/reproduce.py --mode queue-only` reproduces the negative fixture, expected to exit 1 on this module. Both commands reuse the already-built module and `build-audit/libcs80_core.a`; neither compiles the full plugin or suite. `summarize.py` regenerates these compact reports from retained results. `SHA256SUMS` records all retained report, source, readback, and state files.
'''
(out / 'README.md').write_text(report)
manifest = []
for path in sorted(out.rglob('*')):
    if not path.is_file() or path.name == 'SHA256SUMS' or 'config' in path.parts:
        continue
    manifest.append(f'{sha(path)}  {path.relative_to(out)}')
(out / 'SHA256SUMS').write_text('\n'.join(manifest) + '\n')
print(json.dumps({'status': data['status'], 'checks': checks, 'value_comparisons': len(rows),
                  'changed_value_phase_comparisons': len(focused), 'state_physical_rows': len(state_values),
                  'queue_only_reproduction_outputs_identical': identical,
                  'summary': str(out / 'summary.json')}, indent=2))
