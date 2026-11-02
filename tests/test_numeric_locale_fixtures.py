import importlib.util
import locale
from pathlib import Path
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("numeric_fixtures", Path(__file__).resolve().parents[1] / "scripts/verify_numeric_locale_fixtures.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class NumericFixtureTests(unittest.TestCase):
    def exercise(self, missing=False, wrong=False):
        state = ["original"]
        calls = []
        def select(category, value=None):
            self.assertEqual(category, locale.LC_NUMERIC)
            if value is not None:
                calls.append(value)
                if missing and value == "de_DE.utf8":
                    raise locale.Error("fixture unavailable")
                state[0] = value
            return state[0]
        def info():
            return {"decimal_point": "." if wrong or state[0] == "C" else ","}
        with patch.object(module.locale, "setlocale", side_effect=select), patch.object(module.locale, "localeconv", side_effect=info):
            if missing:
                with self.assertRaises(locale.Error): module.verify()
            elif wrong:
                with self.assertRaises(ValueError): module.verify()
            else:
                self.assertEqual([r["requested"] for r in module.verify()], ["C", "de_DE.utf8"])
        self.assertEqual(state[0], "original")
        self.assertEqual(calls[-1], "original")

    def test_both_required_fixtures_and_restore(self): self.exercise()
    def test_missing_fixture_fails_and_restores(self): self.exercise(missing=True)
    def test_wrong_decimal_fails_and_restores(self): self.exercise(wrong=True)


if __name__ == "__main__": unittest.main()
