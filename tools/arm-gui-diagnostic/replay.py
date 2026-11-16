"""One bounded debugger replay. Never a substitute for standard validation."""
import argparse, hashlib, json, os, pathlib, signal, subprocess

def digest(path):
    return hashlib.sha256(pathlib.Path(path).read_bytes()).hexdigest()

def replay(lldb, validator, component, out, timeout=300):
    out.mkdir(parents=True, exist_ok=False)
    binary = component / 'Contents/MacOS/CS-80 Reference'
    inputs = {str(p): digest(p) for p in (validator, binary)}
    command = [str(lldb), '--batch', '--no-lldbinit',
               '-o', 'settings set auto-confirm true',
               '-o', 'breakpoint set --name abort',
               '-o', 'run', '-o', 'thread backtrace all', '-o', 'image list',
               '-o', 'process status', '--', str(validator),
               '--strictness-level', '10', '--validate-in-process',
               '--timeout-ms', '120000', '--random-seed', '0x41dd1ce',
               '--output-dir', str(out / 'pluginval'),
               '--output-filename', 'cs80-au-gui-seed', str(component)]
    result = {'purpose':'diagnostic only; original validation remains failed',
              'seed':'0x41dd1ce', 'command':command, 'input_sha256':inputs,
              'timeout_seconds':timeout, 'timed_out':False}
    (out / 'pluginval').mkdir()
    with (out / 'lldb.log').open('wb') as log:
        p = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
        try:
            result['debugger_exit'] = p.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            result['timed_out'] = True
            try:
                os.killpg(p.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            result['debugger_exit'] = p.wait()
    result['inputs_unchanged'] = all(digest(p) == h for p,h in inputs.items())
    log = (out / 'lldb.log').read_text(errors='replace')
    result['original_assertion_observed'] = 'AppleParavirtCommandBuffer' in log and '_commandCurrentStorage.currentOffset' in log
    result['abort_breakpoint_observed'] = 'stop reason = breakpoint' in log and 'abort' in log
    result['success_marker_observed'] = any(line.strip() == 'SUCCESS' for line in log.splitlines())
    (out / 'receipt.json').write_text(json.dumps(result,indent=2))
    print(json.dumps(result,indent=2))
    # Even a captured fault is not a successful validation. LLDB can exit 0
    # after a breakpoint, so never propagate that as a plugin pass.
    return 0 if (result['debugger_exit'] == 0 and result['success_marker_observed']
                 and not result['timed_out'] and result['inputs_unchanged']
                 and not result['abort_breakpoint_observed'] and not result['original_assertion_observed']) else 1

if __name__ == '__main__':
    p=argparse.ArgumentParser()
    for name in ('lldb','validator','component','out'):
        p.add_argument('--'+name,type=pathlib.Path,required=True)
    a=p.parse_args()
    raise SystemExit(replay(a.lldb.resolve(),a.validator.resolve(),a.component.resolve(),a.out.resolve()))
