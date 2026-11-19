"""Make an isolated instrumented or count-free Engine from the pinned stock file."""
from pathlib import Path
import argparse
p = argparse.ArgumentParser(description=__doc__)
p.add_argument("stock", type=Path)
p.add_argument("new", type=Path)
p.add_argument("--reach", action="store_true")
a = p.parse_args()
text = a.stock.read_text()
needle = "            source_factory::evaluateCutoff(sourceTones[rank], drives[rank], sourceCutoff[rank]);"
if text.count(needle) != 1:
    p.error("exact existing factory cutoff call not found")
prefix = '#include "cold_cutoff-v2.h"\n'
if a.reach:
    prefix += "extern unsigned long long cs80_cold_attempts, cs80_cold_admitted;\n"
    body = """        {
            bool fast = false;
            cold_cutoff::evaluate(sourceTones[rank], drives[rank], sourceCutoff[rank], &fast);
            ++cs80_cold_attempts;
            cs80_cold_admitted += fast;
        }"""
else:
    body = "            cold_cutoff::evaluate(sourceTones[rank], drives[rank], sourceCutoff[rank]);"
with a.new.open("x") as f:
    f.write(prefix + text.replace(needle, body))
