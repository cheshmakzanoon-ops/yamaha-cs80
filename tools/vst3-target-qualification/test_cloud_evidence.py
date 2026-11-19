import json
from pathlib import Path
import shutil
import tempfile
import unittest

import cloud_evidence as e

DATA=Path(__file__).resolve().parent/"cloud-diagnostic"

class Portable(unittest.TestCase):
    def test_replay(self):
        result=e.replay(DATA)
        self.assertEqual(result["timed_callbacks"],18816)
        self.assertEqual(result["untimed_catchup_rows"],768)
        self.assertFalse(result["gates_pass"])
        self.assertFalse(result["dedicated_target_qualified"])

    def test_rejects_losses_or_substitutions(self):
        mutations=[
            ("summary.json",lambda o:o["triplets"].pop(3)),
            ("measurement.json",lambda o:o["slots"]["triplet-03"]["integrity"]["public_artifacts_by_lane"][2]
                 .update({"postwindow.state":"0"*64})),
            ("measurement.json",lambda o:o["slots"]["triplet-07"]["integrity"].update({"schedule_sha256":"0"*64})),
            ("measurement.json",lambda o:o["slots"]["triplet-03"]["external"].update({"wall_ns":1})),
            ("measurement.json",lambda o:o["slots"].pop("untimed")),
            ("measurement.json",lambda o:o["normalized_sha256"].update({"os.csv.gz":"0"*64})),
            ("decision.json",lambda o:o.update({"gates_pass":True})),
        ]
        for file,mutate in mutations:
            with self.subTest(file=file, mutate=mutate), tempfile.TemporaryDirectory() as tmp:
                folder=Path(tmp)/"evidence"; shutil.copytree(DATA,folder)
                path=folder/file; obj=json.loads(path.read_text()); mutate(obj); path.write_text(json.dumps(obj))
                with self.assertRaises(ValueError):
                    e.replay(folder)

if __name__=="__main__":
    unittest.main()
