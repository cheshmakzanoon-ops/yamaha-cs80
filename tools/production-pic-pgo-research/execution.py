"""Single-attempt bounded subprocess ledger for the prospective research trial."""
import json
import os
from pathlib import Path
import signal
import subprocess
import time


def require(ok, message):
    if not ok: raise ValueError(message)


class Commands:
    def __init__(self, out, *, total_seconds=18000, progress_seconds=5400):
        self.out = Path(out); self.out.mkdir(parents=True, exist_ok=False)
        self.started = time.monotonic()
        self.deadline = self.started + total_seconds
        self.milestone = self.started + progress_seconds
        self.first_stage = True
        self.ledger = []
        self.terminal_failure = False
        self.cpu = min(os.sched_getaffinity(0))

    def persist(self):
        (self.out / 'execution-ledger.json').write_text(json.dumps(self.ledger, indent=2) + '\n')
        progress = {'phase': 'compatibility' if self.first_stage else 'full-release',
                    'elapsed_seconds': time.monotonic() - self.started,
                    'compatibility_progress_milestone_passed': self.first_stage and time.monotonic() >= self.milestone,
                    'hard_deadline_seconds': self.deadline - self.started,
                    'terminal_failure': self.terminal_failure}
        (self.out / 'progress.json').write_text(json.dumps(progress, indent=2) + '\n')

    def remaining(self):
        require(not self.terminal_failure, 'previous command failed; experiment is terminal')
        remaining = self.deadline - time.monotonic()
        require(remaining > 0, 'experiment hard deadline exhausted')
        return remaining

    def release_stage(self):
        self.remaining()  # A phase switch cannot rescue the global deadline.
        require(self.first_stage, 'release stage already entered')
        self.first_stage = False
        self.persist()

    def run(self, label, argv, *, cwd, timeout=None, pin=False, env=None):
        require(label and all(c.isalnum() or c in '-_.' for c in label), 'safe command label required')
        require(not any(row['label'] == label for row in self.ledger), 'command retry forbidden')
        argv = list(map(str, argv)); require(bool(argv), 'empty command')
        duration = self.remaining()
        if timeout is not None:
            require(timeout > 0, 'positive command timeout required')
            duration = min(duration, timeout)
        actual = argv
        if pin:
            wrapper = ('import os,sys;c=int(sys.argv[1]);os.sched_setaffinity(0,{c});'
                       'assert os.sched_getaffinity(0)=={c};'
                       'print("verified_child_affinity="+str(c),flush=True);os.execvp(sys.argv[2],sys.argv[2:])')
            actual = ['python3', '-c', wrapper, str(self.cpu), *argv]
        stdout, stderr = self.out / (label + '.stdout'), self.out / (label + '.stderr')
        record = {'label': label, 'argv': argv, 'executed_argv': actual, 'cwd': str(cwd),
                  'timeout_seconds': duration, 'pinned_cpu': self.cpu if pin else None,
                  'phase': 'compatibility' if self.first_stage else 'full-release',
                  'start_elapsed_seconds': time.monotonic() - self.started, 'exit': None}
        self.ledger.append(record); self.persist()
        child = None
        try:
            with stdout.open('xb') as out, stderr.open('xb') as err:
                child = subprocess.Popen(actual, cwd=cwd, env=None if env is None else {**os.environ, **env},
                                         stdout=out, stderr=err, start_new_session=True)
                record['pid'] = child.pid; self.persist()
                try:
                    record['exit'] = child.wait(timeout=duration)
                except BaseException:
                    # Only our fresh process group; kill compiler/test descendants too.
                    try: os.killpg(child.pid, signal.SIGKILL)
                    except ProcessLookupError: pass
                    record['exit'] = child.wait()
                    raise
            require(record['exit'] == 0, label + ' failed; no retry')
            self.remaining()
            return stdout, stderr
        except BaseException as error:
            if child is not None:
                # A failed leader may have exited while its own background workers remain.
                try: os.killpg(child.pid, signal.SIGKILL)
                except ProcessLookupError: pass
            self.terminal_failure = True
            record['error'] = repr(error)
            raise
        finally:
            record['end_elapsed_seconds'] = time.monotonic() - self.started
            self.persist()
