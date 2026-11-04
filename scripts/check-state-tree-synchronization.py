#!/usr/bin/env python3
"""Build isolated project sources with frozen existing JUCE objects, then qualify.

No shared sources or build products are modified. All project translation units,
including Engine and the full snapshot entry point, use the isolated headers.
Only unchanged third-party JUCE objects/font data are reused, with SHA-256 records.
"""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import argparse
import datetime
import hashlib
import json
import shlex
import shutil
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build-dir', type=Path, required=True,
                    help='Existing Release Ninja build with a built CS80Snapshots target')
parser.add_argument('--output-dir', type=Path, required=True,
                    help='New isolated output directory; production build files are never modified')
parser.add_argument('--reuse-from', type=Path,
                    help='Reuse project objects only if every relevant source/header and compile command matches')
parser.add_argument('--frozen-objects-manifest', type=Path,
                    help='Optional earlier qualification.json containing hash-verified JUCE 8.0.11/font objects')
parser.add_argument('--panel-font-header', type=Path,
                    help='Optional existing generated PanelFontData.h to freeze in the isolated output')
parser.add_argument('--jobs', type=int, default=2)
args = parser.parse_args()
assert args.jobs > 0
REPO = Path(__file__).resolve().parent.parent
BUILD = args.build_dir.resolve()
cache = (BUILD / 'CMakeCache.txt').read_text()
ROOT = Path(next(line.split('=', 1)[1] for line in cache.splitlines()
                 if line.startswith('CMAKE_HOME_DIRECTORY:INTERNAL=')))
OUT = args.output_dir.resolve()
OUT.mkdir(parents=True, exist_ok=False)
previous_dir = args.reuse_from.resolve() if args.reuse_from else None
previous = json.loads((previous_dir / 'qualification.json').read_text()) if previous_dir else None
frozen_manifest = args.frozen_objects_manifest.resolve() if args.frozen_objects_manifest else None
frozen_inputs = json.loads(frozen_manifest.read_text())['frozen_objects'] if frozen_manifest else []
REGISTRATION = '4f1bd64c7c5db3e61967c8db2ae93612b58f6a30'
EXTENSION_REGISTRATION = '706c673c54772eb45cb9fe41bd7dbf342792c64a'
CLEAR_REGISTRATION = '2ee33dcc8113aaca4141dc7d77fb374384d8560c'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


report = {'registration': REGISTRATION, 'ownership_extension_registration': EXTENSION_REGISTRATION,
          'history_clear_registration': CLEAR_REGISTRATION, 'runner_sha256': sha(Path(__file__)),
          'scope': 'Isolated full CS80Snapshots project, frozen unchanged JUCE objects; no benchmark',
          'started_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
          'base': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=REPO, text=True).strip(),
          'compiler': subprocess.check_output(['c++', '--version'], text=True),
          'status': 'preparing', 'frozen_objects': [], 'compile_commands': {}, 'results': []}
juce_source = Path(next(line.split('=', 1)[1] for line in cache.splitlines()
                       if line.startswith('JUCE_SOURCE_DIR:STATIC=')))
report['juce_revision'] = subprocess.check_output(['git', '-C', str(juce_source), 'rev-parse', 'HEAD'], text=True).strip()
assert report['juce_revision'] == 'c352e2489000ffeb9e397051377d75eef2efe641', 'Use the project-pinned JUCE 8.0.11'
report['juce_version_header_sha256'] = sha(juce_source / 'modules/juce_core/system/juce_StandardHeader.h')
if frozen_manifest:
    report['frozen_inputs_manifest'] = str(frozen_manifest)
    report['frozen_inputs_manifest_sha256'] = sha(frozen_manifest)
panel_directory = BUILD / 'juce_binarydata_cs80_panel_fonts/JuceLibraryCode'
panel_header = args.panel_font_header.resolve() if args.panel_font_header else panel_directory / 'PanelFontData.h'
assert panel_header.exists(), 'Build cs80_panel_fonts first, or pass its existing --panel-font-header'
isolated_panel_directory = OUT / 'generated-panel-fonts'
isolated_panel_directory.mkdir()
shutil.copyfile(panel_header, isolated_panel_directory / 'PanelFontData.h')
report['generated_panel_font_header_source'] = str(panel_header)
report['generated_panel_font_header_sha256'] = sha(panel_header)


def save():
    (OUT / 'qualification.json').write_text(json.dumps(report, indent=2) + '\n')


def execute(command, log, timeout=600):
    log.parent.mkdir(parents=True, exist_ok=True)
    with log.open('w') as stream:
        result = subprocess.run(command, cwd=REPO, stdout=stream, stderr=stream, timeout=timeout)
    return result.returncode


commands = json.loads((BUILD / 'compile_commands.json').read_text())
rows = [r for r in commands if 'CMakeFiles/CS80Snapshots.dir/' in r['command'] and r['file'].startswith(str(ROOT) + '/src/')]
rows.extend(r for r in commands if 'CMakeFiles/cs80_core.dir/' in r['command'])
raw_link = subprocess.check_output(['ninja', '-C', str(BUILD), '-t', 'commands', 'CS80Snapshots'], text=True).splitlines()
link = shlex.split(next(line for line in reversed(raw_link) if ' -o CS80Snapshots_artefacts/Release/CS80Snapshots ' in line))
assert link[:2] == [':', '&&'] and link[-2:] == ['&&', ':']
link = link[2:-2]
assert '&&' not in link and ';' not in link
original_objects = {}
recipes = {}

for row in rows:
    source = Path(row['file']).relative_to(ROOT)
    original = shlex.split(row['command'])
    object_name = original[original.index('-o') + 1]
    target = OUT / 'objects' / (str(source) + '.o')
    target.parent.mkdir(parents=True, exist_ok=True)
    adjusted = []
    for token in original:
        if token == row['file']:
            adjusted.append(str(REPO / source))
        elif token == object_name:
            adjusted.append(str(target))
        elif source == Path('src/plugin/ArtCommon.cpp') and token == '-I' + str(panel_directory):
            adjusted.append('-I' + str(isolated_panel_directory))
        elif token.startswith('-I' + str(ROOT / 'src')):
            adjusted.append(token.replace(str(ROOT / 'src'), str(REPO / 'src'), 1))
        elif token.startswith('-DCS80_GOLDEN_DIR='):
            adjusted.append(token.replace(str(ROOT), str(REPO)))
        elif token.startswith('-I' + str(ROOT / 'deps')):
            adjusted.append(token.replace(str(ROOT / 'deps'), str(REPO / 'deps'), 1))
        elif token.startswith('-I') and not token[2:].startswith('/'):
            adjusted.append('-I' + str((BUILD / token[2:]).resolve()))
        else:
            adjusted.append(token)
    adjusted += ['-I' + str(REPO / 'src/plugin')]
    recipes[str(source)] = (adjusted, target)
    original_objects[object_name] = target
    report['compile_commands'][str(source)] = {'command': adjusted, 'source_sha256': sha(REPO / source)}

core = [target for source, (_, target) in recipes.items() if source.startswith('src/core/')]
mapped_link = []


def frozen_key(path):
    text = str(path)
    return 'modules/' + text.split('/modules/', 1)[1] if '/modules/' in text else Path(text).name


for token in link:
    if token in original_objects:
        mapped_link.append(str(original_objects[token]))
    elif token == 'libcs80_core.a':
        mapped_link.extend(map(str, core))
    elif token.endswith('.o') or token == 'libcs80_panel_fonts.a':
        source = BUILD / token
        if frozen_manifest:
            matches = [item for item in frozen_inputs if frozen_key(item['source']) == frozen_key(source)]
            assert len(matches) == 1, f'Exactly one frozen input required for {source}'
            source = Path(matches[0]['copy'])
            assert sha(source) == matches[0]['sha256'], f'Frozen input hash mismatch: {source}'
        frozen = OUT / 'frozen-juce' / token
        frozen.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, frozen)
        report['frozen_objects'].append({'source': str(source), 'copy': str(frozen), 'sha256': sha(frozen)})
        mapped_link.append(str(frozen))
    else:
        mapped_link.append(token)

contract_path = 'tests/parity/tolerances-v2/state-tree-synchronization.json'
assert (REPO / contract_path).read_bytes() == subprocess.check_output(['git', 'show', REGISTRATION + ':' + contract_path], cwd=REPO)
report['contract_sha256'] = sha(REPO / contract_path)
extension_path = 'tests/parity/tolerances-v2/state-tree-recording-ownership.json'
assert (REPO / extension_path).read_bytes() == subprocess.check_output(['git', 'show', EXTENSION_REGISTRATION + ':' + extension_path], cwd=REPO)
report['ownership_contract_sha256'] = sha(REPO / extension_path)
history_path = 'tests/parity/tolerances-v2/state-tree-history-lock.json'
assert (REPO / history_path).read_bytes() == subprocess.check_output(['git', 'show', EXTENSION_REGISTRATION + ':' + history_path], cwd=REPO)
report['history_contract_sha256'] = sha(REPO / history_path)
clear_path = 'tests/parity/tolerances-v2/state-tree-history-clear.json'
assert (REPO / clear_path).read_bytes() == subprocess.check_output(['git', 'show', CLEAR_REGISTRATION + ':' + clear_path], cwd=REPO)
report['clear_contract_sha256'] = sha(REPO / clear_path)
report['input_headers'] = {str(path.relative_to(REPO)): sha(path)
                           for directory in ['src', 'tests', 'deps']
                           for path in sorted((REPO / directory).rglob('*'))
                           if path.is_file() and (path.suffix in ['.h', '.hpp', '.inl']
                               or (directory == 'deps' and path.suffix in ['.c', '.cpp']))}
if previous is not None:
    changed_headers = {name for name, digest in report['input_headers'].items() if previous['input_headers'].get(name) != digest}
    report['reused_project_source'] = str(previous_dir)
    report['changed_test_headers'] = sorted(changed_headers)
else:
    changed_headers = set()
report['status'] = 'compiling control'
save()


def compile_one(item):
    source, (command, target) = item
    panel_unchanged = source != 'src/plugin/ArtCommon.cpp' or (previous is not None
        and previous.get('generated_panel_font_header_sha256') == report['generated_panel_font_header_sha256'])
    # Test headers are consumed by the snapshot entry point, never by the
    # production plugin/core translation units. A new fixture must rebuild the
    # entry point; unchanged production sources retain their verified objects.
    relevant_headers_unchanged = not changed_headers or (
        source.startswith(('src/plugin/', 'src/core/'))
        and all(path.startswith('tests/') for path in changed_headers))
    if previous is not None and relevant_headers_unchanged and panel_unchanged:
        prior = previous['compile_commands'].get(source)
        if prior is not None:
            prior = {key: prior[key] for key in ['command', 'source_sha256']}
            prior['command'] = [token.replace(str(previous_dir), str(OUT)) for token in prior['command']]
        old_object = previous_dir / 'objects' / (source + '.o')
        if old_object.exists() and prior == report['compile_commands'][source]:
            if old_object != target: shutil.copyfile(old_object, target)
            report['compile_commands'][source]['reused_object_sha256'] = sha(target)
            print('reuse verified source/command', source, flush=True)
            return target
    log = OUT / 'control-build' / (source.replace('/', '_') + '.log')
    code = execute(command, log)
    print('compile', source, code, flush=True)
    if code != 0:
        raise RuntimeError(f'Control compilation failed: {source}; {log}')
    return target


def link_executable(destination, substitutions=None):
    command = list(mapped_link)
    command[command.index('-o') + 1] = str(destination)
    if substitutions:
        command = [str(substitutions.get(token, token)) for token in command]
    code = execute(command, destination.parent / (destination.name + '-link.log'))
    if code != 0:
        raise RuntimeError(f'Link failure: {destination}')
    return command


def run(executable, directory, arguments, expected):
    command = ['bash', str(REPO / 'scripts/run-ui-smoke-linux.sh'), str(executable), *arguments]
    # The harness's documented Openbox exit 70 occurs before the application.
    # Preserve every such attempt and retry the exact command once.
    for attempt in range(2):
        log = directory / f'test-attempt-{attempt + 1}.log'
        code = execute(command, log, timeout=900)
        if code != 70: break
    result = {'command': command, 'exit': code, 'expected_exit': expected, 'log': str(log), 'log_sha256': sha(log)}
    if code != expected:
        raise RuntimeError(f'Unexpected test exit {code} (wanted {expected}); {log}')
    return result


try:
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        list(pool.map(compile_one, recipes.items()))
    control = OUT / 'CS80Snapshots-control'
    report['control_link_command'] = link_executable(control)
    report['results'].append({'name': 'control', **run(control, OUT / 'control-gate', ['--state-tree-test'], 0)})
    report['status'] = 'mutations'
    save()
    undo_path = 'src/plugin/UndoHistory.cpp'
    modern_path = 'src/plugin/ModernLayerState.cpp'
    processor_path = 'src/plugin/PluginProcessor.cpp'
    modulation_path = 'src/plugin/ModernModulationUi.cpp'
    texts = {path: (REPO / path).read_text() for path in [undo_path, modern_path, processor_path, modulation_path]}
    sync = 'void UndoableParameterState::syncNow()\n{\n    syncExternal();'
    omitted = 'void UndoableParameterState::syncNow()\n{\n    if (!onMessageThread()) return;\n    syncExternal();'

    def replace(text, old, new):
        assert text.count(old) == 1, old
        return text.replace(old, new)

    original_worker = replace(texts[undo_path], sync, omitted)
    first = original_worker.index('juce::ValueTree UndoableParameterState::copyState()')
    last = original_worker.index('\nvoid UndoableParameterState::timerCallback()', first)
    original_worker = original_worker[:first] + 'juce::ValueTree UndoableParameterState::copyState()\n{\n    const juce::ScopedLock scoped(lock);\n    syncNow();\n    return juce::AudioProcessorValueTreeState::copyState();\n}\n' + original_worker[last:]
    missing_replay = texts[processor_path]
    for method in ['undo', 'redo']:
        old = f'bool Cs80AudioProcessor::{method}()\n{{\n    const juce::ScopedLock treeLock(apvts.treeLock());'
        missing_replay = replace(missing_replay, old, f'bool Cs80AudioProcessor::{method}()\n{{')
    variants = [
        ('worker-sync-omitted', undo_path, replace(texts[undo_path], sync, omitted), 'worker syncNow reconciles external values into the live tree'),
        ('original-worker-snapshot-mechanism', undo_path, original_worker, 'undo after worker snapshot preserves the unrelated automated value'),
        ('pending-user-overlay-omitted', undo_path, replace(texts[undo_path], 'for (auto* parameter : parameters)', 'if (false) for (auto* parameter : parameters)'), 'worker snapshot overlays the current pending user gesture'),
        ('modern-write-lock-omitted', modern_path, replace(texts[modern_path], 'void ModernLayerState::store(const modern::ModernConfig& newConfig, bool writeTree)\n{\n    const juce::ScopedLock treeLock(apvts.treeLock());', 'void ModernLayerState::store(const modern::ModernConfig& newConfig, bool writeTree)\n{'), 'modern child insertion owns the common APVTS tree lock during actual callbacks'),
        ('modern-read-lock-omitted', modern_path, replace(texts[modern_path], 'void ModernLayerState::reloadFromTreeUsing(void* context, TreeReader reader)\n{\n    const juce::ScopedLock treeLock(apvts.treeLock());', 'void ModernLayerState::reloadFromTreeUsing(void* context, TreeReader reader)\n{'), 'production modern tree reader owns the common APVTS tree lock'),
        ('modern-replay-lock-omitted', processor_path, missing_replay, 'modern subtree undo holds the common lock across its actual mutation callbacks'),
        ('undo-host-gestures-omitted', undo_path, replace(texts[undo_path], 'const bool gesture = state.history.isPerformingUndoRedo()', 'const bool gesture = false && state.history.isPerformingUndoRedo()'), 'undo after worker snapshot brackets only the user-edited parameter'),
        ('worker-suppresses-user-recording', undo_path, replace(texts[undo_path], 'if (onMessageThread())\n        {\n            flag = &writing;', 'if (true)\n        {\n            flag = &writing;'), 'worker tree write never suppresses the final concurrent user edit on redo'),
        ('physical-sync-one-ulp', undo_path, replace(texts[undo_path], 'const float value = raw->load(std::memory_order_relaxed);', 'const float value = std::nextafter(raw->load(std::memory_order_relaxed), std::numeric_limits<float>::infinity());'), 'worker synchronization preserves the exact APVTS physical value in the live tree'),
        ('history-read-lock-omitted', undo_path, replace(texts[undo_path], 'UndoableParameterState::HistorySnapshot UndoableParameterState::readHistoryUsing(void* context, HistoryReader reader) const\n{\n    const juce::ScopedLock scoped(lock);', 'UndoableParameterState::HistorySnapshot UndoableParameterState::readHistoryUsing(void* context, HistoryReader reader) const\n{'), 'production history snapshot reader owns the common APVTS lock'),
        ('transaction-open-lock-omitted', undo_path, replace(texts[undo_path], 'void UndoableParameterState::openTransactionUsing(const juce::String& name, void* context, TransactionWriter writer)\n{\n    const juce::ScopedLock scoped(lock);', 'void UndoableParameterState::openTransactionUsing(const juce::String& name, void* context, TransactionWriter writer)\n{'), 'production user transaction open owns the common APVTS lock'),
        ('factory-history-clear-lock-omitted', processor_path, replace(texts[processor_path],
            '        const juce::ScopedLock treeLock(apvts.treeLock());\n        apvts.syncNow();\n        undoManager.clearUndoHistory();',
            '        apvts.syncNow();\n        undoManager.clearUndoHistory();'),
            'factory load owns the common APVTS lock during actual history action destruction'),
        ('mseg-user-transaction-bypassed', modulation_path, replace(texts[modulation_path],
            '    userEdit.reset();\n    userEdit = std::make_unique<UndoableParameterState::ScopedUserEdit>(processor.getUndoableState(), "MSEG edit");',
            '    if (auto* undo = processor.getAPVTS().undoManager)\n        undo->beginNewTransaction("MSEG edit");'),
            'MSEG mouse-down synchronizes a pending external value before the first drag'),
        ('mseg-six-decimal-serialization-restored', modern_path, replace(texts[modern_path],
            'return juce::String(value, std::numeric_limits<float>::max_digits10, true);',
            'return juce::String(value, 6);'),
            'MSEG redo restores the exact edited shape while preserving the earlier user edit'),
    ]
    for name, path, text, assertion in variants:
        directory = OUT / 'mutations' / name
        source = directory / 'source' / path
        source.parent.mkdir(parents=True, exist_ok=False)
        source.write_text(text)
        command, normal_object = recipes[path]
        mutated_object = directory / 'mutant.o'
        compile_command = [str(source) if token == str(REPO / path) else str(mutated_object) if token == str(normal_object) else token for token in command]
        code = execute(compile_command, directory / 'build.log')
        if code != 0: raise RuntimeError(f'Mutant compile failure does not qualify: {name}')
        executable = directory / 'CS80Snapshots'
        link_command = link_executable(executable, {str(normal_object): mutated_object})
        result = run(executable, directory, ['--state-tree-test'], 1)
        log = Path(result['log']).read_text()
        if 'FAIL: ' + assertion not in log:
            raise RuntimeError(f'Mutant did not fail its intended assertion: {name}')
        report['results'].append({'name': name, 'source': path, 'mutant_sha256': sha(source), 'compile_command': compile_command,
                                  'link_command': link_command, 'intended_assertion': assertion, **result})
        save()
        print('mutation', name, 'DETECTED', flush=True)
    report['status'] = 'existing UI suite'
    save()
    result = run(control, OUT / 'full-ui', ['--out', str(OUT / 'full-ui/screenshots'), '--self-test'], 0)
    report['results'].append({'name': 'restored-full-ui-suite', **result})
    report['status'] = 'passed'
    report['finished_utc'] = datetime.datetime.now(datetime.timezone.utc).isoformat()
    save()
    print('State-tree qualification complete: control, fourteen mutations and full existing UI suite passed.', flush=True)
except BaseException as error:
    report['status'] = 'failed'
    report['error'] = str(error)
    save()
    raise
