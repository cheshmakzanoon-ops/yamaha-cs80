# Negative map-degree reference

Source: official [Scala downloads](https://www.huygens-fokker.org/scala/downloads.html).
The scratch-only, unmodified Scala 1.88e Linux binary came from
https://www.huygens-fokker.org/software/scala-18-pc64-linux.tar.gz .
Archive SHA256: `be57f66406ce0f266b9440f70238cf94cde5b426940699c9966d18bcc83c744a`.
Binary SHA256: `3e2ac64f77b34719b441ea138453ceb0a9c38b8e6976931b3bbe66324206f51c`.
These identities were independently checked against the actual scratch files.
The official software is not redistributed here.

Load the retained `nonuniform.scl` and either `negative1.kbm` or `negative2.kbm`
in that official version. Use fresh output paths, not the archived outputs:

```
FILE /absolute/path/to/fresh-output.txt
LOAD /absolute/path/to/nonuniform.scl
LOAD/MAPPING /absolute/path/to/negative1.kbm
SHOW MAPPING 56 65
SEND/FREQUENCY
CLOSE
EXIT
```

Repeat with negative2. The raw retained terminal-file outputs preserve the
original spacing, including trailing spaces. Run `python3 audit.py` to validate
both 128-key outputs against independent integer-degree arithmetic and the saved
JSON observations. This output-only audit does not rerun Scala, rehash an absent
binary/archive or prove source provenance by itself. The JSON records the
original provenance hashes separately.
