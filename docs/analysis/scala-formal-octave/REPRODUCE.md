# Replay the retained observation

Run `python3 audit.py` in this directory to verify the retained outputs against
both formulas and the recorded audit. This verifies numerical observations, not
the provenance hashes of software that is deliberately not included.

For a fresh oracle, obtain the official Scala 1.88e Linux command-line distribution
from the URL in audit.json, check its archive/executable hashes, and obey its
license. In a new scratch directory, copy nonuniform.scl and formal3.kbm, start
the official executable, and issue these commands:

```
FILE fresh-formal3.txt
LOAD nonuniform.scl
LOAD/MAPPING formal3.kbm
SHOW SCALE
SHOW MAPPING 54 68
SEND/FREQUENCY
CLOSE
EXIT
```

Repeat separately with formal0.kbm and a different output name to observe the
zero-degree discrepancy. Never overwrite the retained outputs with a fresh run.
The project's zero default is deliberately preserved rather than copied from
this legacy executable's contradictory behavior.
