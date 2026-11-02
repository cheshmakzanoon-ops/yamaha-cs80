"""Remove real verifier guards in memory; never manufacture real-host evidence."""
import ast
import importlib.util
from pathlib import Path
import types
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("reaper_validator_tests", ROOT / "tests/test_reaper_smoke_validator.py")
TESTS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(TESTS)
SOURCE = ROOT / "scripts/verify-reaper-smoke.py"


class RemoveGuard(ast.NodeTransformer):
    def __init__(self, message):
        self.message = message
        self.removed = 0

    def visit_Expr(self, node):
        call = node.value
        if isinstance(call, ast.Call) and isinstance(call.func, ast.Name) and call.func.id == "require" and len(call.args) == 2:
            reason = call.args[1]
            if isinstance(reason, ast.BinOp):
                reason = reason.left
            if isinstance(reason, ast.Constant) and reason.value == self.message:
                self.removed += 1
                return ast.copy_location(ast.Pass(), node)
        return self.generic_visit(node)


class GuardMutationControls(unittest.TestCase):
    def test_each_removed_metadata_guard_is_detected_by_its_unit_test(self):
        original = TESTS.validator
        messages = ["checkout source mismatch", "source tree mismatch", "host archive identity mismatch",
                    "built/installed plugin mismatch", "contract hash mismatch", "host runner reported failure",
                    "running host version mismatch", "host assertions failed/missing", "host editor did not open",
                    "invalid host-reported latency", "wrong restored ", "lost MIDI notes", "changed persisted envelope",
                    "host editor closed during tour dismissal", "unsupported host contract schema"]
        try:
            for message in messages:
                with self.subTest(removed_guard=message):
                    parsed = ast.parse(SOURCE.read_text(), filename=str(SOURCE))
                    mutation = RemoveGuard(message)
                    altered = ast.fix_missing_locations(mutation.visit(parsed))
                    self.assertEqual(mutation.removed, 1, "mutation must remove exactly its intended guard")
                    candidate = types.ModuleType("reaper_validator_removed_guard")
                    candidate.__file__ = str(SOURCE)
                    exec(compile(altered, str(SOURCE), "exec"), candidate.__dict__)
                    TESTS.validator = candidate
                    result = unittest.TestResult()
                    TESTS.ProvenanceControls("test_identity_state_and_failure_mutations").run(result)
                    TESTS.DryProvenanceControls("test_identity_state_and_failure_mutations").run(result)
                    TESTS.DryContractControls("test_copied_contract_filename_is_versioned").run(result)
                    self.assertEqual(result.errors, [], "unrelated execution errors are not a killed guard mutant")
                    self.assertGreater(len(result.failures), 0, "removed guard survived its specific negative control")
                    print("REJECTED removed metadata guard:", message, "assertion failures:", len(result.failures))
        finally:
            TESTS.validator = original


if __name__ == "__main__":
    unittest.main()
