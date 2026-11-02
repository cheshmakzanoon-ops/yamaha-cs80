#!/usr/bin/env python3
"""Check Linux persistence-test fixtures; never skip an unavailable locale."""
import json
import locale
import sys


def verify():
    original = locale.setlocale(locale.LC_NUMERIC)
    result = []
    try:
        for name, separator in (("C", "."), ("de_DE.utf8", ",")):
            resolved = locale.setlocale(locale.LC_NUMERIC, name)
            observed = locale.localeconv()["decimal_point"]
            if observed != separator:
                raise ValueError(f"{name}: expected decimal {separator!r}, got {observed!r}")
            result.append({"requested": name, "resolved": resolved, "decimal_point": observed})
    finally:
        locale.setlocale(locale.LC_NUMERIC, original)
    return result


if __name__ == "__main__":
    try:
        print(json.dumps({"required_numeric_locales": verify()}))
    except (locale.Error, ValueError) as error:
        print(f"Required numeric locale fixture failed: {error}", file=sys.stderr)
        sys.exit(1)
