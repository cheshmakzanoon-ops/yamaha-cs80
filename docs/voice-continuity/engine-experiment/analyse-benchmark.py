#!/usr/bin/env python3
"""Recompute every aggregate from retained raw callbacks, then apply frozen gates."""
import csv, gzip, hashlib, itertools, json, math, pathlib, statistics, sys
root = pathlib.Path(sys.argv[1])
raw_path = root / "callback-times.csv.gz"
if raw_path.exists():
    stream = gzip.open(raw_path, "rt", newline="")
else:
    raw_path = root / "callback-times.csv"
    stream = raw_path.open(newline="")
with stream:
    raw = list(csv.DictReader(stream))
with (root / "results.csv").open(newline="") as stream:
    rows = list(csv.DictReader(stream))
assert len(rows) == 288 and len(raw) == 73728
groups = {}
for row in raw:
    pair, arm, callback, order, ns = (int(row[k]) for k in ("pair", "arm", "callback", "order", "nanoseconds"))
    assert 0 <= callback < 256 and ns > 0 and arm == (pair + callback + order) % 2
    groups.setdefault((pair, arm), []).append((callback, ns))
expected = set(itertools.product((44100,48000,96000),(128,256),
    ("source_both","source_panel","panel_both"),("static","moving"),
    ((0,"cold"),(0,"retired"),(1,"held"),(8,"held"))))
seen, pairs, overruns = set(), {}, []
for row in rows:
    arm = 1 if row["arm"] == "candidate_enabled" else 0
    key = (int(row["pair"]), arm)
    observations = groups[key]
    assert sorted(b for b,_ in observations) == list(range(256))
    values = sorted(ns for _,ns in observations)
    rate, block, voices = (int(row[k]) for k in ("rate","block","voices"))
    config = (rate, block, row["scene"], row["controls"], (voices,row["idle_kind"]))
    seen.add(config)
    median, p99, maximum = statistics.median(values), values[math.ceil(.99*256)-1], values[-1]
    budget = 1e9*block/rate
    assert median == float(row["median_ns"]) and p99 == int(row["p99_ns"]) and maximum == int(row["max_ns"])
    assert int(row["overruns"]) == sum(ns > budget for ns in values)
    assert int(row["active"]) == voices and int(row["effective"]) == arm and int(row["rejected"]) == 0
    for b,ns in observations:
        if ns > budget:
            overruns.append(dict(pair=key[0],arm=row["arm"],callback=b,nanoseconds=ns,RTF=ns/budget))
    row = dict(row, median_RTF=median/budget, p99_RTF=p99/budget, maximum_RTF=maximum/budget)
    pairs.setdefault(key[0],{})[arm]=row
assert seen == expected and len(pairs) == 144
eight, idle = [], []
for pair, arms in pairs.items():
    assert set(arms) == {0,1}
    base,candidate = arms[0],arms[1]
    assert all(base[k] == candidate[k] for k in ("rate","block","scene","controls","voices","idle_kind"))
    if int(candidate["voices"]) == 8:
        ratio=float(candidate["median_ns"])/float(base["median_ns"])
        eight.append(dict(pair=pair,ratio=ratio,passed=ratio<=1.35))
    if int(candidate["rate"])==48000 and int(candidate["voices"])==0:
        idle.append(dict(pair=pair,kind=candidate["idle_kind"],median_RTF=candidate["median_RTF"],passed=candidate["median_RTF"]<=.35))
assert len(eight)==36 and len(idle)==24
passed=all(x["passed"] for x in eight+idle)
out={"rows":len(rows),"raw_observations":len(raw),"all_raw_aggregates_verified":True,
     "eight_active_pairs":eight,"48000_zero_audible_rows":idle,"cost_gates_passed":passed,
     "retained_overruns":overruns,"overrun_count":len(overruns),
     "eight_active_ratio_range":[min(x["ratio"] for x in eight),max(x["ratio"] for x in eight)],
     "48000_idle_median_RTF_range":[min(x["median_RTF"] for x in idle),max(x["median_RTF"] for x in idle)],
     "maximum_RTF":max(r["maximum_RTF"] for pair in pairs.values() for r in pair.values()),
     "decision":"recommend separately reviewed opt-in integration design" if passed else "revise exact-cache variant once or reject integration at this budget",
     "production_integrated":False,
     "sha256":{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (root/"results.csv",raw_path)}}
target=root/"summary.json"
with target.open("x") as stream: json.dump(out,stream,indent=2);stream.write("\n")
print(json.dumps({k:out[k] for k in ("rows","raw_observations","cost_gates_passed","eight_active_ratio_range","48000_idle_median_RTF_range","overrun_count","maximum_RTF","decision")},indent=2))
