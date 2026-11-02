import collections
import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys

BASE = Path('/workspace/scratch/aa102d573ba3')
SOURCE = BASE / 'cs80-tone-editor'
EVIDENCE = BASE / 'cs80-host-tool-evidence/factory-tone-editor'
BUILD = BASE / 'cs80-panel-s2/build-native-red'
DEST = EVIDENCE / 'qualification'
HEAD = '59522333955aea4801204ae37c57f40257ac2a08'


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def git(*args):
    return subprocess.check_output(['git', '-C', str(SOURCE), *args], text=True).strip()


def now():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()


def variants(original):
    begin = original.index('    juce::ComboBox* hostChoice(')
    end = original.index('    // One segment of the three-way render-policy switch.', begin)
    helper = original[begin:end]
    attachment = '''        if (parameter != nullptr)
            comboAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
                proc.getAPVTS(), id, *raw));
        else
            raw->setEnabled(false);
'''
    assert helper.count(attachment) == 1
    one_way = '''        if (parameter != nullptr)
        {
            raw->setSelectedItemIndex(parameter->getIndex(), juce::dontSendNotification);
            raw->onChange = [raw, parameter]
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost(parameter->convertTo0to1((float) raw->getSelectedItemIndex()));
                parameter->endChangeGesture();
            };
        }
        else
            raw->setEnabled(false);
'''
    inserts = {
        'wrong-target': helper.replace('proc.getAPVTS(), id, *raw)',
                                      'proc.getAPVTS(), Cs80AudioProcessor::parameterId(cs80::P_mix), *raw)'),
        'one-way-binding': helper.replace(attachment, one_way),
        'early-publication': helper.replace(attachment, attachment + '''        raw->onChange = [processor = &proc]
        {
            juce::AudioBuffer<float> audio(2, 0);
            juce::MidiBuffer midi;
            processor->processBlock(audio, midi);
        };
'''),
        'no-keyboard-focus': helper.replace('control->setWantsKeyboardFocus(true);',
                                          'control->setWantsKeyboardFocus(false);'),
        'stored-engine-gate': helper.replace(attachment, attachment +
                                            '        raw->setEnabled(proc.getParamValue(cs80::P_sourcePerformance) >= 0.5f);\n'),
    }
    for value in inserts.values():
        assert value != helper
    return {name: (original[:begin] + value + original[end:], helper, value)
            for name, value in inserts.items()}


def validate(name, code, log, ui_header):
    failures = [line[6:] for line in log.splitlines() if line.startswith('FAIL: ')]
    summary = re.findall(r'Factory tone editor component: (\d+) checks, (\d+) failures; desktop components0', log)
    assert summary == [('56', str(len(failures)))], (name, summary, failures)
    assert 'NOT RUN' not in log
    if name.startswith('positive-'):
        assert code == 0 and not failures, (name, code, failures)
        return failures
    assert code == 1 and failures, (name, code, failures)
    choice_names = set(re.findall(r'expectChoice\([^;]*?,\s*"([^"]+)"\)', ui_header))
    if name == 'wrong-target':
        allowed = choice_names | {
            'manual revision selection is one balanced host gesture',
            'selected revision reaches main and active extension at the next audio transaction',
            'fresh ENGINE/PURE combinations keep saved revision opt-in selectable',
        }
        assert set(failures) <= allowed, failures
        assert 'manual revision selection reaches the actual host parameter' in failures
        assert 'manual revision selection is one balanced host gesture' in failures
    elif name == 'one-way-binding':
        assert set(failures) <= choice_names, failures
        for required in ('external parameter change synchronizes the editor asynchronously',
                         'session restoration synchronizes explicit V2',
                         'user preset recall synchronizes V2 rather than live V1',
                         'A/B recalls the copied V2 in the editor'):
            assert required in failures, (required, failures)
    else:
        expected = {
            'early-publication': ['manual revision selection waits for the audio transaction'] * 2,
            'no-keyboard-focus': ['factory revision selector is focusable with accessible title and description'],
            'stored-engine-gate': ['fresh ENGINE/PURE combinations keep saved revision opt-in selectable'] * 2,
        }[name]
        assert collections.Counter(failures) == collections.Counter(expected), (name, failures, expected)
    return failures


def run_case(name):
    assert git('rev-parse', 'HEAD') == HEAD
    assert git('status', '--porcelain') == ''
    positive = json.loads((EVIDENCE / 'green-corrected/receipt.json').read_text())
    old = json.loads((EVIDENCE / 'green/receipt.json').read_text())
    assert git('rev-parse', 'HEAD:src') == positive['source_tree']
    for path, expected in positive['inputs'].items():
        assert sha(SOURCE / path) == expected, path
    for row in positive['linked_objects']:
        assert sha(row['path']) == row['sha256'], row['path']
    positive_binary = EVIDENCE / 'green-corrected/components'
    assert sha(positive_binary) == positive['binary_sha256']
    d = DEST / name
    d.mkdir(parents=True, exist_ok=False)
    receipt = {'head': HEAD, 'source_tree': positive['source_tree'], 'name': name,
               'started_at': now(), 'scope': 'unpeered component source counterfactual; no native window/input/paint/DAW evidence',
               'positive_binary_sha256': positive['binary_sha256'],
               'positive_receipt_sha256': sha(EVIDENCE / 'green-corrected/receipt.json'),
               'test_source_sha256': sha(SOURCE / 'tests/FactoryToneRevisionUiTests.h')}
    print('LOCKED_EDITOR_CONTROL', name, receipt['started_at'], flush=True)
    original = (SOURCE / 'src/plugin/AdvancedPanel.cpp').read_text()
    binary = positive_binary
    link = positive['link_argv']
    if not name.startswith('positive-'):
        mutated, before, after = variants(original)[name]
        cpp = d / 'AdvancedPanel.cpp'
        cpp.write_text(mutated)
        obj = d / 'AdvancedPanel.o'
        args = old['compile_argv'].copy()
        args[args.index('-o') + 1] = str(obj)
        args[args.index('-c') + 1] = str(cpp)
        args[1:1] = ['-iquote', str(SOURCE / 'src/plugin')]
        link = [str(obj) if arg == positive['unchanged_advanced_object'] else arg for arg in positive['link_argv']]
        binary = d / 'components'
        link[link.index('-o') + 1] = str(binary)
        link = [('-Wl,--dependency-file=' + str(d / 'link.d')) if a.startswith('-Wl,--dependency-file=') else a for a in link]
        receipt.update({'original_source_sha256': hashlib.sha256(original.encode()).hexdigest(),
                        'mutant_source_sha256': sha(cpp), 'replacement_before': before,
                        'replacement_after': after, 'compile_argv': args, 'link_argv': link})
        (d / 'started-receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
        for label, command in [('compile', args), ('link', link)]:
            with (d / (label + '.log')).open('w') as f:
                subprocess.run(command, cwd=BUILD, stdout=f, stderr=subprocess.STDOUT, check=True, timeout=240)
        receipt['mutant_object_sha256'] = sha(obj)
    linked = []
    for arg in link:
        p = Path(arg)
        if not p.is_absolute():
            p = BUILD / p
        if p.is_file() and p.suffix in ('.o', '.a'):
            linked.append({'path': str(p), 'sha256': sha(p)})
    assert len(linked) == 45, len(linked)
    env = os.environ.copy()
    env.pop('DISPLAY', None)
    env['XDG_CACHE_HOME'] = str(d / 'cache')
    env['XDG_CONFIG_HOME'] = str(d / 'config')
    with (d / 'test.log').open('w') as f:
        result = subprocess.run([str(binary)], env=env, stdout=f, stderr=subprocess.STDOUT, timeout=150)
    log = (d / 'test.log').read_text()
    print(log, flush=True)
    for path, expected in positive['inputs'].items():
        assert sha(SOURCE / path) == expected, path
    for row in positive['linked_objects'] + linked:
        assert sha(row['path']) == row['sha256'], row['path']
    assert sha(positive_binary) == positive['binary_sha256']
    assert git('rev-parse', 'HEAD') == HEAD
    receipt.update({'finished_at': now(), 'exit_code': result.returncode, 'binary_sha256': sha(binary),
                    'linked_objects': linked,
                    'failures': [line[6:] for line in log.splitlines() if line.startswith('FAIL: ')]})
    (d / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    validate(name, result.returncode, log, (SOURCE / 'tests/FactoryToneRevisionUiTests.h').read_text())
    receipt['registered_prediction_matched'] = True
    (d / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print('CONTROL_COMPLETE', name, receipt['finished_at'], flush=True)


if __name__ == '__main__':
    if len(sys.argv) == 2 and sys.argv[1] == '--all':
        cases = ['positive-before', 'wrong-target', 'one-way-binding', 'early-publication',
                 'no-keyboard-focus', 'stored-engine-gate', 'positive-after']
        for name in cases:
            subprocess.run(['flock', str(BASE / 'cs80-heavy-check.lock'), sys.executable,
                            str(Path(__file__).resolve()), name], check=True)
    else:
        assert len(sys.argv) == 2
        run_case(sys.argv[1])
