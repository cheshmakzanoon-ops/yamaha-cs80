import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import time
import unittest

spec = importlib.util.spec_from_file_location('execution', Path(__file__).with_name('execution.py'))
p = importlib.util.module_from_spec(spec); spec.loader.exec_module(p)


class ExecutionTests(unittest.TestCase):
    def test_success_ledger_and_retry_rejection(self):
        with tempfile.TemporaryDirectory() as t:
            runner = p.Commands(Path(t) / 'logs', total_seconds=10, progress_seconds=5)
            output, _ = runner.run('once', ['/bin/echo', 'receipt'], cwd=t)
            self.assertEqual(output.read_text().strip(), 'receipt')
            self.assertEqual(runner.ledger[0]['exit'], 0)
            with self.assertRaises(ValueError): runner.run('once', ['/bin/true'], cwd=t)
            runner.release_stage()
            runner.run('release', ['/bin/true'], cwd=t)
            self.assertEqual(runner.ledger[-1]['phase'], 'full-release')
            with self.assertRaises(ValueError): runner.release_stage()

    def test_failure_stops_and_records(self):
        with tempfile.TemporaryDirectory() as t:
            runner = p.Commands(Path(t) / 'logs', total_seconds=10, progress_seconds=5)
            with self.assertRaises(ValueError): runner.run('failure', ['/bin/sh', '-c', 'exit 7'], cwd=t)
            self.assertEqual(runner.ledger[0]['exit'], 7)
            with self.assertRaises(ValueError): runner.run('different-label-retry', ['/bin/true'], cwd=t)
            with self.assertRaises(ValueError): runner.release_stage()
            self.assertIn('error', json.loads((runner.out / 'execution-ledger.json').read_text())[0])

    def test_timeout_and_expired_global_deadline_not_rescued(self):
        with tempfile.TemporaryDirectory() as t:
            runner = p.Commands(Path(t) / 'logs', total_seconds=10, progress_seconds=5)
            with self.assertRaises(subprocess.TimeoutExpired):
                runner.run('timeout', ['/bin/sh', '-c', 'sleep 2'], cwd=t, timeout=.05)
            self.assertIn('TimeoutExpired', runner.ledger[0]['error'])
            runner.deadline = time.monotonic() - 1
            with self.assertRaises(ValueError): runner.release_stage()
            with self.assertRaises(ValueError): runner.run('late', ['/bin/true'], cwd=t)
            self.assertEqual(len(runner.ledger), 1)

    def test_nonzero_exit_kills_own_background_descendant(self):
        with tempfile.TemporaryDirectory() as t:
            runner = p.Commands(Path(t) / 'logs', total_seconds=10, progress_seconds=5)
            with self.assertRaises(ValueError):
                runner.run('failed-with-child', ['/bin/sh', '-c', 'sleep 30 & echo $!; exit 7'], cwd=t)
            pid = int((runner.out / 'failed-with-child.stdout').read_text().strip())
            stat = Path('/proc') / str(pid) / 'stat'
            terminated = False
            for _ in range(20):
                if not stat.exists(): terminated = True; break
                try: state = stat.read_text().split(') ', 1)[1].split()[0]
                except FileNotFoundError: terminated = True; break
                if state == 'Z': terminated = True; break  # Dead; waiting for init to reap.
                time.sleep(.02)
            self.assertTrue(terminated, 'background child remained runnable after failed command')

    def test_progress_milestone_is_not_an_abort_or_pass(self):
        with tempfile.TemporaryDirectory() as t:
            runner = p.Commands(Path(t) / 'logs', total_seconds=10, progress_seconds=5)
            runner.milestone = time.monotonic() - 1
            runner.run('late-compatibility', ['/bin/true'], cwd=t)
            progress = json.loads((runner.out / 'progress.json').read_text())
            self.assertTrue(progress['compatibility_progress_milestone_passed'])
            self.assertEqual(progress['phase'], 'compatibility')
            runner.release_stage()
            self.assertEqual(json.loads((runner.out / 'progress.json').read_text())['phase'], 'full-release')

    def test_pin_witness(self):
        with tempfile.TemporaryDirectory() as t:
            runner = p.Commands(Path(t) / 'logs', total_seconds=10, progress_seconds=5)
            output, _ = runner.run('pin', ['/bin/true'], cwd=t, pin=True)
            self.assertEqual(output.read_text().strip(), 'verified_child_affinity=' + str(runner.cpu))

if __name__ == '__main__': unittest.main()
