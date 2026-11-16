"""One native/debugger comparison, never a replacement validation pass."""
import argparse, hashlib, json, os, pathlib, signal, subprocess, time

SEED = '0x41dd1ce'

def digest(path):
    return hashlib.sha256(pathlib.Path(path).read_bytes()).hexdigest()

def unchanged(inputs):
    try:
        return all(digest(p) == h for p, h in inputs.items())
    except OSError:
        return False

def execute(command, folder, timeout):
    folder.mkdir()
    result = {'command': command, 'timeout_seconds': timeout, 'timed_out': False,
              'exit': None, 'started_unix': time.time()}
    with (folder / 'process.log').open('wb') as log:
        try:
            p = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
            try:
                result['exit'] = p.wait(timeout=timeout)
            except subprocess.TimeoutExpired:
                result['timed_out'] = True
                try:
                    os.killpg(p.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                result['exit'] = p.wait()
        except OSError as error:
            result['launch_error'] = str(error)
    result['finished_unix'] = time.time()
    text = (folder / 'process.log').read_text(errors='replace')
    result['success_marker'] = any(line.strip() == 'SUCCESS' for line in text.splitlines())
    result['original_assertion'] = 'AppleParavirtCommandBuffer' in text and '_commandCurrentStorage.currentOffset' in text
    return result

def classify(result, inferior=None, debugger=False):
    if result.get('timed_out'):
        return 'timeout'
    if result.get('launch_error'):
        return 'launch_failure'
    if debugger:
        if not isinstance(inferior, dict):
            return 'missing_inferior_evidence'
        if inferior.get('state') in ('stopped', 'crashed', 'suspended'):
            return 'captured_stop'
        if inferior.get('state') != 'exited':
            return 'inferior_launch_or_state_failure'
        if inferior.get('exit_status') != 0:
            return 'inferior_failure'
    if result['original_assertion']:
        return 'assertion_observed'
    if result['exit'] != 0:
        return 'debugger_failure' if debugger else 'native_failure'
    return 'clean_nonreproduction' if result['success_marker'] else 'missing_success'

def inspector(path):
    # LLDB executes this after normal completion or via its batch crash hook.
    return """import json, lldb
p = lldb.debugger.GetSelectedTarget().GetProcess()
state = p.GetState()
record = {'state': lldb.SBDebugger.StateAsCString(state), 'exit_status': p.GetExitStatus(), 'pid': p.GetProcessID()}
if state in (lldb.eStateStopped, lldb.eStateCrashed, lldb.eStateSuspended):
    record['threads'] = [{'id': t.GetThreadID(), 'stop_reason': t.GetStopReason(), 'description': t.GetStopDescription(1024)} for t in p]
    lldb.debugger.HandleCommand('thread backtrace all')
    lldb.debugger.HandleCommand('image list')
lldb.debugger.HandleCommand('process status')
with open(%r, 'w') as stream:
    json.dump(record, stream, indent=2)
""" % str(path)

def replay(lldb, validator, component, out, timeout=300):
    out.mkdir(parents=True, exist_ok=False)
    binary = component / 'Contents/MacOS/CS-80 Reference'
    inputs = {str(p): digest(p) for p in (validator, binary)}
    result = {'purpose': 'diagnostic comparison only; original validation remains failed',
              'seed': SEED, 'input_sha256': inputs, 'runs': [], 'accepted': False,
              'environment': {k: os.environ.get(k) for k in ('HOME','TMPDIR','DISPLAY','LANG','ImageVersion','ImageOS')}}
    def args(folder):
        (folder / 'pluginval').mkdir(parents=True)
        return [str(validator), '--strictness-level', '10', '--validate-in-process',
                '--timeout-ms', '120000', '--random-seed', SEED,
                '--output-dir', str(folder / 'pluginval'), '--output-filename', 'cs80-au-gui-seed', str(component)]
    native_args = args(out / 'native-output')
    native = execute(native_args, out / 'native', timeout)
    native['classification'] = classify(native)
    result['runs'].append(native)
    result['inputs_unchanged_after_native'] = unchanged(inputs)
    # Failures do not suppress the paired run; changed/missing inputs do.
    if result['inputs_unchanged_after_native']:
        evidence = out / 'inferior.json'
        script = out / 'inspect_inferior.py'
        script.write_text(inspector(evidence))
        inspect_command = 'script exec(open(%r).read())' % str(script)
        command = [str(lldb), '--batch', '--no-lldbinit', '-o', 'settings set auto-confirm true',
                   '-o', 'breakpoint set --name abort', '-o', 'run', '-o', inspect_command,
                   '-k', inspect_command, '--', *args(out / 'debugger-output')]
        debug = execute(command, out / 'debugger', timeout)
        try:
            inferior = json.loads(evidence.read_text())
        except (OSError, ValueError):
            inferior = None
        debug['inferior'] = inferior
        debug['classification'] = classify(debug, inferior, debugger=True)
        result['runs'].append(debug)
    else:
        result['comparison_blocked'] = 'inputs changed or disappeared after native run'
    result['inputs_unchanged_final'] = unchanged(inputs)
    result['accepted'] = (len(result['runs']) == 2 and result['inputs_unchanged_final'] and
                          all(r['classification'] == 'clean_nonreproduction' for r in result['runs']))
    (out / 'receipt.json').write_text(json.dumps(result, indent=2))
    print(json.dumps(result, indent=2))
    return 0 if result['accepted'] else 1

if __name__ == '__main__':
    p = argparse.ArgumentParser()
    for name in ('lldb', 'validator', 'component', 'out'):
        p.add_argument('--' + name, type=pathlib.Path, required=True)
    a = p.parse_args()
    raise SystemExit(replay(a.lldb.resolve(), a.validator.resolve(), a.component.resolve(), a.out.resolve()))
