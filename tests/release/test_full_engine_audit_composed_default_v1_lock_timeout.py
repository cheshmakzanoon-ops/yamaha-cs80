#!/usr/bin/env python3
"""Queue time is not test time; a timed-out test cannot outlive its private lock."""
import fcntl
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest

SPEC = importlib.util.spec_from_file_location('composed_default_v1_mutation_runner', Path(__file__).with_name('qualify_full_engine_audit_composed_default_v1_mutations.py'))
RUNNER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(RUNNER)


class LockTimeoutTests(unittest.TestCase):
    def command(self, command, lock, **kwargs):
        builder = getattr(RUNNER, 'bounded_command', None)
        self.assertTrue(callable(builder), 'Runtime deadline must start inside the acquired lock')
        return builder(command, lock, **kwargs)

    def test_frozen_runtime_budget_is_inside_flock(self):
        self.assertEqual(self.command(['fixture'], Path('/tmp/private-fixture-lock')),
                         ['flock', '/tmp/private-fixture-lock', 'timeout', '--kill-after=5s', '300s', 'fixture'])

    def test_waiting_for_lock_does_not_consume_runtime_budget(self):
        with tempfile.TemporaryDirectory(prefix='cs80-lock-queue-') as directory:
            lock = Path(directory) / 'private.lock'
            marker = Path(directory) / 'started'
            with lock.open('w') as held:
                fcntl.flock(held, fcntl.LOCK_EX)
                # The witness is a trivial filesystem command, not a Python
                # cold-start speed test. Production still has its 300s budget.
                command = self.command(['touch', str(marker)], lock, runtime=1, grace=0.2)
                process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                try:
                    time.sleep(1.2)  # Exceeds runtime allowance, entirely waiting for the lock.
                    self.assertIsNone(process.poll())
                    self.assertFalse(marker.exists())
                finally:
                    fcntl.flock(held, fcntl.LOCK_UN)
                output = process.communicate(timeout=5)
                self.assertEqual(process.returncode, 0, output)
                self.assertTrue(marker.is_file())

    def test_timeout_terminates_its_child_group_before_lock_release(self):
        with tempfile.TemporaryDirectory(prefix='cs80-lock-child-') as directory:
            lock, pids = Path(directory) / 'private.lock', Path(directory) / 'pids.json'
            child = 'import signal,time; signal.signal(signal.SIGTERM,signal.SIG_IGN); time.sleep(10)'
            parent = ('import signal,subprocess,sys,os,json,time; from pathlib import Path; '
                      'signal.signal(signal.SIGTERM,signal.SIG_IGN); '
                      f'p=subprocess.Popen([sys.executable,"-c",{child!r}]); '
                      f'Path({str(pids)!r}).write_text(json.dumps([os.getpid(),p.pid])); time.sleep(10)')
            process = subprocess.Popen(self.command([sys.executable, '-c', parent], lock, runtime=3, grace=0.2),
                                       stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            deadline = time.monotonic() + 5
            while not pids.is_file() and time.monotonic() < deadline:
                time.sleep(0.01)
            self.assertTrue(pids.is_file(), 'Timeout fixture did not start')
            self.assertEqual(subprocess.run(['flock', '-n', str(lock), 'true']).returncode, 1)
            process.communicate(timeout=5)
            self.assertIn(process.returncode, (124, 137), 'Timeout/forced cleanup must not look like test success')
            for pid in json.loads(pids.read_text()):
                status = Path('/proc') / str(pid) / 'stat'
                if status.exists():
                    self.assertEqual(status.read_text().split(') ', 1)[1].split()[0], 'Z',
                                     f'Test process {pid} remains runnable after cleanup')
            self.assertEqual(subprocess.run(['flock', '-n', str(lock), 'true']).returncode, 0)


if __name__ == '__main__':
    unittest.main(verbosity=2)
