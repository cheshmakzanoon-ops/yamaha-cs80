"""Mock-only input controls and real archived OCR parsing; never launches a host."""
from contextlib import ExitStack
import csv
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import call, patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("reaper_runner", ROOT / "scripts/run-reaper-smoke.py")
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)
TSV = (ROOT / "tests/hosts/reaper/fixtures/actual-f651-tour.tsv").read_text()
EMPTY_TSV = TSV.splitlines()[0] + "\n"
WINDOW = "4194590"
GEOMETRY = "WINDOW=4194590\nX=113\nY=160\nWIDTH=1600\nHEIGHT=1028\nSCREEN=0\n"


def changed_tsv(change):
    reader = csv.DictReader(io.StringIO(TSV), delimiter="\t")
    fields = reader.fieldnames
    rows = list(reader)
    change(rows)
    out = io.StringIO()
    writer = csv.DictWriter(out, fieldnames=fields, delimiter="\t", lineterminator="\n")
    writer.writeheader()
    writer.writerows(rows)
    return out.getvalue()


class ArchivedPixelTargetControls(unittest.TestCase):
    def test_actual_archived_ocr_selects_observed_close_label_centre(self):
        geometry = runner.window_geometry(GEOMETRY, WINDOW)
        target = runner.first_tour_target(TSV, geometry)
        self.assertEqual(target["point"], [760, 770])
        self.assertEqual(target["button"]["bounds"], [730, 766, 791, 775])
        self.assertEqual(target["heading"]["text"], "welcome to the cs-80")
        self.assertGreater(target["button"]["confidence"], 80)

    def test_missing_ambiguous_low_confidence_or_out_of_card_text_is_rejected(self):
        def update(matched_text, **changes):
            return lambda rows: [row.update(changes) for row in rows if row["text"] == matched_text]
        def duplicate(rows):
            rows.extend([dict(row, block_num="999") for row in rows.copy()
                         if row["level"] == "5" and row["block_num"] == "73"])
        cases = [("heading", update("Welcome", text="Farewell")),
                 ("step", update("1/9", text="2/9")),
                 ("button", update("CLOSE", text="HIDE")),
                 ("button", update("CLOSE", conf="1")),
                 ("button", update("CLOSE", left="1400")),
                 ("button", duplicate)]
        for name, change in cases:
            with self.subTest(anchor=name, change=change):
                with self.assertRaisesRegex(ValueError, "unsafe first-tour " + name):
                    runner.first_tour_target(changed_tsv(change), runner.window_geometry(GEOMETRY, WINDOW))

    def test_malformed_ocr_rejects_columns_boxes_and_duplicate_words(self):
        def bad_conf(rows):
            next(row for row in rows if row["text"] == "CLOSE")["conf"] = "nan"
        def duplicate(rows):
            rows.append(next(row.copy() for row in rows if row["text"] == "CLOSE"))
        for text in ("wrong\tcolumns\n", changed_tsv(bad_conf), changed_tsv(duplicate)):
            with self.subTest(text=text[:20]), self.assertRaises(ValueError):
                runner.observed_text_lines(text)

    def test_wrong_or_malformed_window_geometry_is_rejected(self):
        for text in (GEOMETRY.replace("1600", "1599"), GEOMETRY.replace("X=113", "X=-1"),
                     GEOMETRY + "WIDTH=1600\n", GEOMETRY.replace("WINDOW=4194590", "WINDOW=1"),
                     GEOMETRY.replace("HEIGHT=1028", "HEIGHT=x"), ""):
            with self.subTest(geometry=text), self.assertRaises(ValueError):
                runner.window_geometry(text, WINDOW)

    def test_post_click_requires_actual_first_tour_text_to_be_absent(self):
        geometry = runner.window_geometry(GEOMETRY, WINDOW)
        self.assertIsNone(runner.require_tour_absent(EMPTY_TSV, geometry))
        with self.assertRaisesRegex(ValueError, "first-tour text remains after click"):
            runner.require_tour_absent(TSV, geometry)

    def test_callback_settings_are_read_only_and_structurally_checked(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "settings.xml"
            self.assertFalse(runner.tour_seen(path))
            for data, seen in [('<CS80_UI_SETTINGS tourSeen="0"/>', False),
                               ('<CS80_UI_SETTINGS tourSeen="1"/>', True)]:
                path.write_text(data)
                self.assertEqual(runner.tour_seen(path), seen)
                self.assertEqual(path.read_text(), data)
            for data in ['<wrong tourSeen="1"/>', '<CS80_UI_SETTINGS tourSeen="perhaps"/>']:
                path.write_text(data)
                with self.assertRaisesRegex(ValueError, "invalid tour settings"):
                    runner.tour_seen(path)


class BoundedEditorInputControls(unittest.TestCase):
    def dispatch(self, out, *, outputs=None, ocr=None, persist=True, run_failure=None):
        stack = ExitStack()
        self.addCleanup(stack.close)
        capture = stack.enter_context(patch.object(runner, "capture"))
        query = stack.enter_context(patch.object(runner.subprocess, "check_output",
                                  side_effect=outputs or [WINDOW+"\n", GEOMETRY, GEOMETRY, WINDOW+"\n"]))
        def action(argv, **kwargs):
            if run_failure and run_failure in argv:
                raise subprocess.CalledProcessError(1, argv)
            if "click" in argv and persist:
                settings = out / "isolated-home/.config/CS80Reference/ui-settings.xml"
                settings.parent.mkdir(parents=True, exist_ok=True)
                settings.write_text('<CS80_UI_SETTINGS version="1" tourSeen="1"/>')
        run = stack.enter_context(patch.object(runner.subprocess, "run", side_effect=action))
        read_ocr = stack.enter_context(patch.object(runner, "read_ocr", side_effect=ocr or [TSV, EMPTY_TSV]))
        sleep = stack.enter_context(patch.object(runner.time, "sleep"))
        return capture, query, run, read_ocr, sleep

    def test_exactly_one_observed_click_in_unique_instrument_window(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp)
            capture, query, run, read_ocr, sleep = self.dispatch(out)
            runner.dismiss_tour_and_capture(out)
            self.assertEqual(run.call_args_list, [
                call(["xdotool", "windowactivate", "--sync", WINDOW], check=True, timeout=10),
                call(["xdotool", "mousemove", "--sync", "760", "770"], check=True, timeout=10),
                call(["xdotool", "click", "--clearmodifiers", "1"], check=True, timeout=10)])
            self.assertEqual(capture.call_args_list, [call(out, "host-editor-before-dismiss.png"),
                call(out, "host-editor-ready-to-dismiss.png"), call(out, "host-editor.png")])
            sleep.assert_called_once_with(1)
            record = json.loads((out / "editor-dismissal.json").read_text())
            self.assertEqual((record["window"], record["action"], record["count"]), (WINDOW, "left_click", 1))
            self.assertFalse(record["tour_seen_before"])
            self.assertTrue(record["tour_seen_after"])
            self.assertTrue(record["tour_text_absent_after"])
            self.assertEqual((out / "tour-settings.xml").read_text(),
                             '<CS80_UI_SETTINGS version="1" tourSeen="1"/>')

    def test_host_wrapper_escape_is_not_a_tour_dismissal(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp)
            _, _, run, _, _ = self.dispatch(out)
            runner.dismiss_tour_and_capture(out)
            self.assertFalse(any("Escape" in item.args[0] for item in run.call_args_list),
                             "Escape routed to the REAPER wrapper closes the editor")

    def test_no_input_on_absent_ambiguous_or_invalid_window(self):
        for result in ("", "4194590\n4194591\n", "not-a-window\n"):
            with self.subTest(search_result=result), tempfile.TemporaryDirectory() as tmp:
                out = Path(tmp)
                capture, _, run, _, sleep = self.dispatch(out, outputs=[result])
                with self.assertRaisesRegex(ValueError, "^expected one visible instrument window for bounded tour dismissal$"):
                    runner.dismiss_tour_and_capture(out)
                run.assert_not_called()
                sleep.assert_not_called()
                capture.assert_called_once_with(out, "host-editor-before-dismiss.png")
                self.assertFalse((out / "editor-dismissal.json").exists())

    def test_failed_activation_never_clicks_or_claims_dismissal(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp)
            capture, _, run, ocr, sleep = self.dispatch(out, run_failure="windowactivate")
            with self.assertRaises(subprocess.CalledProcessError):
                runner.dismiss_tour_and_capture(out)
            run.assert_called_once_with(["xdotool", "windowactivate", "--sync", WINDOW], check=True, timeout=10)
            ocr.assert_not_called(); sleep.assert_not_called()
            self.assertFalse((out / "editor-dismissal.json").exists())

    def test_stale_geometry_lost_focus_or_missing_tour_never_clicks(self):
        cases = [(dict(outputs=[WINDOW+"\n", GEOMETRY, GEOMETRY.replace("X=113", "X=114")]), "geometry changed"),
                 (dict(outputs=[WINDOW+"\n", GEOMETRY, GEOMETRY, "123\n"]), "lost focus"),
                 (dict(ocr=[EMPTY_TSV]), "unsafe first-tour heading")]
        for kwargs, reason in cases:
            with self.subTest(reason=reason), tempfile.TemporaryDirectory() as tmp:
                out = Path(tmp)
                _, _, run, _, sleep = self.dispatch(out, **kwargs)
                with self.assertRaisesRegex(ValueError, reason):
                    runner.dismiss_tour_and_capture(out)
                self.assertFalse(any("click" in item.args[0] or "mousemove" in item.args[0]
                                     for item in run.call_args_list))
                sleep.assert_not_called()
                self.assertFalse((out / "editor-dismissal.json").exists())

    def test_failed_pointer_or_click_never_claims_dismissal(self):
        for command in ("mousemove", "click"):
            with self.subTest(command=command), tempfile.TemporaryDirectory() as tmp:
                out = Path(tmp)
                _, _, run, _, sleep = self.dispatch(out, run_failure=command)
                with self.assertRaises(subprocess.CalledProcessError):
                    runner.dismiss_tour_and_capture(out)
                if command == "mousemove":
                    self.assertFalse(any("click" in item.args[0] for item in run.call_args_list))
                sleep.assert_not_called()
                self.assertFalse((out / "editor-dismissal.json").exists())

    def test_missing_callback_or_remaining_tour_never_claims_dismissal(self):
        for kwargs, reason in [(dict(persist=False), "callback did not persist"),
                               (dict(ocr=[TSV, TSV]), "first-tour text remains")]:
            with self.subTest(reason=reason), tempfile.TemporaryDirectory() as tmp:
                out = Path(tmp)
                self.dispatch(out, **kwargs)
                with self.assertRaisesRegex(ValueError, reason):
                    runner.dismiss_tour_and_capture(out)
                self.assertFalse((out / "editor-dismissal.json").exists())

    def test_preexisting_seen_setting_prevents_any_input(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp)
            path = out / "isolated-home/.config/CS80Reference/ui-settings.xml"
            path.parent.mkdir(parents=True)
            path.write_text('<CS80_UI_SETTINGS tourSeen="1"/>')
            _, _, run, ocr, _ = self.dispatch(out)
            with self.assertRaisesRegex(ValueError, "already marked seen"):
                runner.dismiss_tour_and_capture(out)
            run.assert_not_called(); ocr.assert_not_called()


if __name__ == "__main__":
    unittest.main()
