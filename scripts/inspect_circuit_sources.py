#!/usr/bin/env python3
"""Bounded source inspection for original Yamaha service schematic analysis.
No downloaded source or crop is added to the repository. This is research
evidence, not a hardware calibration measurement or DSP correctness test.
"""
import argparse
import base64
import hashlib
import io
import json
import time
import urllib.request
import re
import subprocess
import tempfile
from pathlib import Path
from urllib.parse import urlparse
from PIL import Image

SOURCES = {
    "mwiring45": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/45%20-%20M13-14%20Circuit%20Board%20%26%20Wiring.jpg",
    "mwiring46": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/46%20-%20M15-16%20Circuit%20Board%20%26%20Wiring.jpg",
    "mwiring47": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/47%20-%20M17-18%20Circuit%20Board%20%26%20Wiring.jpg",
    "icguide_152": "https://www.goltsynth.com/resources/downloads/CS/yamaha_ic_guidebook.pdf",
    "icguide_sub": "https://www.goltsynth.com/resources/downloads/CS/yamaha_ic_guidebook.pdf",
    "sub29": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/29%20-%20SUB%20Circuit%20Diagram.jpg",
    "sub30": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/30%20-%20SUB%20Circuit%20Board%20%26%20Wiring.jpg",
    "ej67": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/67%20-%20Panel%205%20%28EJ%2C%20EXP%2C%20PFS%29%20Circuit%20Diagram.jpg",
    "t51w": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/32%20-%20T51%20Circuit%20Board%20%26%20Wiring.jpg",
    "t52w": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/34%20-%20T52%20Circuit%20Board%20%26%20Wiring.jpg",
    "t53w": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/36%20-%20T53%20Circuit%20Board%20%26%20Wiring.jpg",
    "t54w": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/38%20-%20T54%20Circuit%20Board%20%26%20Wiring.jpg",
    "adjupdates_a": "https://www.therogoffs.com/cs80/CS80%20Adjustment%20Procedures%20and%20updates.PDF",
    "adjupdates_b": "https://www.therogoffs.com/cs80/CS80%20Adjustment%20Procedures%20and%20updates.PDF",
    "twswiring": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/18%20-%20TWS%20Circuit%20Board%20%26%20Wiring.jpg",
    "kas04": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/04%20-%20KAS%20schematic.jpg",
    "kas05": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/05%20-%20KAS%20Circuit%20Board%20%26%20Wiring.jpg",
    "cs80adjust": "https://www.therogoffs.com/cs80/CS80%20Adjustment%20Procedures%20and%20updates.PDF",
    "kasguide14": "https://dragonflyalley.com/synth/jurgenHaiblePages/CSSynth/jh_CSSynthServiceGuide_p14.gif",
    "kasguide21": "https://dragonflyalley.com/synth/jurgenHaiblePages/CSSynth/jh_CSSynthServiceGuide_p21.gif",
    "kasguide22": "https://dragonflyalley.com/synth/jurgenHaiblePages/CSSynth/jh_CSSynthServiceGuide_p22.gif",
    "kasguide23": "https://dragonflyalley.com/synth/jurgenHaiblePages/CSSynth/jh_CSSynthServiceGuide_p23.gif",
    "icguide_vco": "https://www.goltsynth.com/resources/downloads/CS/yamaha_ic_guidebook.pdf",
    "icguide_kas": "https://www.goltsynth.com/resources/downloads/CS/yamaha_ic_guidebook.pdf",
    "t51": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/31%20-%20T51%20Circuit%20Diagram.jpg",
    "t52": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/33%20-%20T52%20Circuit%20Diagram.jpg",
    "t53": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/35%20-%20T53%20Circuit%20Diagram.jpg",
    "t54": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/37%20-%20T54%20Circuit%20Diagram.jpg",
    "mkb": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/63%20-%20Panel%201%20%28MKB%29%20Circuit%20Diagram.jpg",
    "toneselector": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/65%20-%20Panel%202%20%28Tone%20Selector%29%20Circuit%20Diagram.jpg",
    "kbcwiring": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/16%20-%20KBC%201-2%20Circuit%20Board%20%26%20Wiring.jpg",
    "kasics": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/06%20-%20KAS%20ICs.jpg",
    "icguide_156": "https://www.goltsynth.com/resources/downloads/CS/yamaha_ic_guidebook.pdf",
    "icguide_offset": "https://www.goltsynth.com/resources/downloads/CS/yamaha_ic_guidebook.pdf",
    "panel2": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/64%20-%20Panel%202%20Circuit%20Diagram.jpg",
    "ba": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/27%20-%20BA%20Circuit%20Diagram.jpg",
    "panel3": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/66%20-%20Panel%203%20Circuit%20Diagram.jpg",
    "rs": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/39%20-%20R51-52%20Circuit%20Diagram.jpg",
    "kbc": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/15%20-%20KBC%201-2%20Circuit%20Diagram.jpg",
    "icguide_index": "https://www.goltsynth.com/resources/downloads/CS/yamaha_ic_guidebook.pdf",
    "cs20m_q": "https://www.polynominal.com/sample-packs/yamaha-cs20m/Yamaha-CS-20m-service-Manual.pdf",
    "icguide": "https://www.goltsynth.com/resources/downloads/CS/yamaha_ic_guidebook.pdf",
    "ig00156": "https://hygunduz5.wordpress.com/wp-content/uploads/2025/11/ig00156.png",
    "ig00156circuit": "https://hygunduz5.wordpress.com/wp-content/uploads/2025/11/vcfcircuit.png",
    "mics42": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/42%20-%20M%20Circuit%20ICs.jpg",
    "mics52": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/52%20-%20M%20Circuit%20ICs.jpg",
    "mwiring44": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/44%20-%20M11-12%20Circuit%20Board%20%26%20Wiring.jpg",
    "trg12": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/19%20-%20TRG%201-2%20Circuit%20Diagram.jpg",
    "trg3": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/21%20-%20TRG%203%20Circuit%20Diagram.jpg",
    "trg4": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/23%20-%20TRG%204%20Circuit%20Diagram.jpg",
    "trg5": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/25%20-%20TRG%205%20Circuit%20Diagram.jpg",
    "sh": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/07%20-%20SH%20Circuit%20Diagram.jpg",
    "tws": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/17%20-%20TWS%20Circuit%20Diagram.jpg",
    "m": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/41%20-%20M%20Circuit%20Diagram.jpg",
    "panel1": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/62%20-%20Panel%201%20Circuit%20Diagram.jpg",
    "tkc": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/09%20-%20TKC%20Circuit%20Diagram.jpg",
    "tsb1": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/11%20-%20TSB%201%20Circuit%20Diagram.jpg",
    "tsb2": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/13%20-%20TSB%202%20Circuit%20Diagram.jpg",
}
MAX_BYTES = 16 * 1024 * 1024
Image.MAX_IMAGE_PIXELS = 40_000_000

def download(url):
    source_host = urlparse(url).hostname
    if source_host not in {"www.therogoffs.com", "hygunduz5.wordpress.com", "www.goltsynth.com", "www.polynominal.com", "dragonflyalley.com"}:
        raise ValueError("source host outside fixed allowlist")
    last = None
    for attempt in range(3):
        try:
            req = urllib.request.Request(url, headers={"User-Agent": "CS80-circuit-research/1.0"})
            with urllib.request.urlopen(req, timeout=30) as response:
                if urlparse(response.geturl()).hostname != source_host:
                    raise ValueError("unexpected source redirect")
                data = response.read(MAX_BYTES + 1)
                if len(data) > MAX_BYTES:
                    raise ValueError("source exceeds size bound")
                return data
        except Exception as exc:
            last = exc
            if attempt < 2:
                time.sleep(2 * (attempt + 1))
    raise RuntimeError(f"Cannot read source after three attempts: {last}")

def emit_image(gray, page, url, data):
    w, h = gray.size
    print("CS80_SOURCE_METADATA " + json.dumps({
        "page": page, "url": url, "sha256": hashlib.sha256(data).hexdigest(),
        "bytes": len(data), "width": w, "height": h,
        "purpose": "Original Yamaha schematic analysis; not measured hardware evidence"
    }), flush=True)
    boxes = []
    # Dense resistor and keyboard-code labels require native pixels, not overview resampling.
    if page in {"mwiring45", "mwiring46", "mwiring47", "t51", "t52", "t53", "t54", "panel1", "mkb", "toneselector", "kas04", "kas05", "twswiring", "t51w", "t52w", "t53w", "t54w", "sub29", "sub30", "ej67"}:
        cols, rows = (w + 2199) // 2200, (h + 2199) // 2200
        if cols * rows > 16:
            raise ValueError("native preset grid exceeds 16-tile bound")
        for row in range(rows):
            for col in range(cols):
                box = (max(0, col * 2200 - 64), max(0, row * 2200 - 64),
                       min(w, (col + 1) * 2200 + 64), min(h, (row + 1) * 2200 + 64))
                if max(box[2]-box[0], box[3]-box[1]) > 2400:
                    raise ValueError("native preset tile exceeds 2400-pixel bound")
                boxes.append((f"{page}-native-r{row}-c{col}", box, False))
    else:
        # Overlap preserves circuit connections that cross a tile boundary.
        for row in range(2):
            for col in range(2):
                boxes.append((f"{page}-r{row}-c{col}", (
                    max(0, col * w // 2 - 64), max(0, row * h // 2 - 64),
                    min(w, (col + 1) * w // 2 + 64), min(h, (row + 1) * h // 2 + 64)
                ), True))
    if max(w, h) <= 2400:
        boxes.append((f"{page}-full-native", (0, 0, w, h), False))
    # Native-pixel detail crops, selected from inspected source coordinates.
    focus = {
        "panel2": [("contour-controls", (3350, 1900, 4500, 3250))],
        "kbc": [
            ("decoder-rails", (550, 1400, 1400, 1700)),
            ("reference-routing", (1400, 1700, 2600, 3100)),
            ("decoder", (500, 700, 2400, 2200)),
            ("ladder", (1750, 800, 2600, 3000)),
            ("output", (2550, 650, 3550, 1600)),
        ],
        "trg5": [
            ("speed-upper", (3750, 450, 4650, 1750)),
            ("speed-lower", (3750, 1700, 4650, 3200)),
        ],
        "m": [("al-receiver", (1600, 1950, 2450, 2900)),
              ("filter-detail", (1180, 1080, 3250, 1850)),
              ("oscillator-detail", (400, 1100, 1800, 2600))],
        "tws": [
            ("hold-glyphs-left", (650, 1700, 3050, 2050)),
            ("hold-glyphs-right", (3000, 1700, 5100, 2050)),
            ("voice1-upper", (650, 550, 1250, 1850)),
            ("voice1-lower", (650, 1600, 1250, 3100)),
        ],
    }
    for name, box in focus.get(page, []):
        if box[2] > w or box[3] > h or max(box[2]-box[0], box[3]-box[1]) > 2400:
            raise ValueError("focus crop outside verified source bounds")
        boxes.append((f"{page}-{name}", box, False))
    for ident, box, resize in boxes:
        crop = gray.crop(box)
        if resize:
            crop.thumbnail((1800, 1800), Image.Resampling.LANCZOS)
        out = io.BytesIO()
        crop.save(out, format="PNG", optimize=True)
        encoded = base64.b64encode(out.getvalue()).decode("ascii")
        print("CS80_CROP_BEGIN " + json.dumps({
            "id": ident, "source_box": list(box),
            "width": crop.width, "height": crop.height, "resized": resize,
            "png_sha256": hashlib.sha256(out.getvalue()).hexdigest(),
            "base64_length": len(encoded)
        }), flush=True)
        for index, start in enumerate(range(0, len(encoded), 4096)):
            print(f"CS80_CROP_DATA {ident} {index:04d} {encoded[start:start+4096]}", flush=True)
        print(f"CS80_CROP_END {ident}", flush=True)

def inspect_pdf(url, data, key):
    # No OCR or full-document rendering. Text extraction locates at most four pages.
    with tempfile.TemporaryDirectory(prefix="cs80-icguide-") as directory:
        root = Path(directory)
        pdf = root / "source.pdf"
        pdf.write_bytes(data)
        info = subprocess.run(["pdfinfo", str(pdf)], check=True, capture_output=True,
                              text=True, timeout=30).stdout
        print("CS80_PDF_INFO " + json.dumps({"url": url, "sha256": hashlib.sha256(data).hexdigest(),
              "bytes": len(data), "pdfinfo": info}), flush=True)
        match = re.search(r"^Pages:\s+(\d+)", info, re.MULTILINE)
        if not match or not 1 <= int(match.group(1)) <= 500:
            raise ValueError("PDF page count outside 1..500 bound")
        count = int(match.group(1))
        if key == "cs20m_q":
            if count < 52:
                raise ValueError("CS-20M fixed target page52 absent")
            selected = [52]
            print("CS80_PDF_SELECTION " + json.dumps({"rendered_pages": selected,
                  "reason": "fixed original CS-20M Figure F-4 source target"}), flush=True)
        elif key in {"adjupdates_a", "adjupdates_b"}:
            if count < 36:
                raise ValueError("CS80 adjustment fixed updates page36 absent")
            selected = [31, 32, 33] if key == "adjupdates_a" else [34, 35, 36]
            print("CS80_PDF_SELECTION " + json.dumps({"rendered_pages": selected,
                  "reason": "fixed original update pages; verify revision applicability"}), flush=True)
        elif key == "cs80adjust":
            if count < 30:
                raise ValueError("CS80 adjustment fixed target page30 absent")
            selected = [2, 29, 30]
            print("CS80_PDF_SELECTION " + json.dumps({"rendered_pages": selected,
                  "reason": "CS80 adjustment contents and keyboard calibration candidates"}), flush=True)
        elif key == "icguide_152":
            if count < 34:
                raise ValueError("guidebook fixed IG00152 candidate page34 absent")
            selected = [33, 34]
            print("CS80_PDF_SELECTION " + json.dumps({"rendered_pages": selected,
                  "reason": "index IG00152 section2-3 and verified adjacent offsets; verify identity and AL port evidence"}), flush=True)
        elif key == "icguide_sub":
            if count < 30:
                raise ValueError("guidebook fixed suboscillator candidate page30 absent")
            selected = [29, 30]
            print("CS80_PDF_SELECTION " + json.dumps({"rendered_pages": selected,
                  "reason": "index IG00150 section2-1 and verified section2-4 offset; verify rendered identity"}), flush=True)
        elif key == "icguide_vco":
            if count < 40:
                raise ValueError("guidebook fixed oscillator candidate page40 absent")
            selected = [35, 36, 39, 40]
            print("CS80_PDF_SELECTION " + json.dumps({"rendered_pages": selected,
                  "reason": "index-derived IG00153/IG00158 candidates; verify rendered identity"}), flush=True)
        elif key == "icguide_kas":
            if count < 18:
                raise ValueError("guidebook fixed keyboard IC candidate pages15..18 absent")
            selected = [15, 16, 17, 18]
            print("CS80_PDF_SELECTION " + json.dumps({"rendered_pages": selected,
                  "reason": "index-derived YM26600/YM26700 candidates; verify rendered identity"}), flush=True)
        elif key == "icguide_156":
            if count < 38:
                raise ValueError("guidebook fixed IG00156 pages37..38 absent")
            selected = [37, 38]
            print("CS80_PDF_SELECTION " + json.dumps({"rendered_pages": selected,
                  "reason": "fixed IG00156 pages after index/offset inspection"}), flush=True)
        elif key == "icguide_offset":
            if count < 28:
                raise ValueError("guidebook fixed offset sample page28 absent")
            selected = [12, 20, 28]
            print("CS80_PDF_SELECTION " + json.dumps({"rendered_pages": selected,
                  "reason": "fixed sample pages to resolve printed-section offset"}), flush=True)
        elif key == "icguide_index":
            if count < 6:
                raise ValueError("guidebook fixed index pages4..6 absent")
            selected = [4, 5, 6]
            print("CS80_PDF_SELECTION " + json.dumps({"rendered_pages": selected,
                  "reason": "fixed continuation of inspected guidebook index"}), flush=True)
        else:
            txt = root / "source.txt"
            subprocess.run(["pdftotext", "-layout", str(pdf), str(txt)], check=True, timeout=45)
            if txt.stat().st_size > 8 * 1024 * 1024:
                raise ValueError("PDF extracted text exceeds 8MiB bound")
            pages = txt.read_text(encoding="utf-8", errors="replace").split("\f")
            found = []
            for number, content in enumerate(pages[:count], 1):
                hits = list(re.finditer(r"IG\s*0{2}156", content, re.IGNORECASE))
                if hits:
                    found.append(number)
                    excerpts = [content[max(0, hit.start()-180):hit.end()+500] for hit in hits[:4]]
                    print("CS80_PDF_TEXT_MATCH " + json.dumps({"page": number, "excerpts": excerpts}), flush=True)
            selected = found[:4] if found else list(range(1, min(3, count)+1))
            print("CS80_PDF_SELECTION " + json.dumps({"matched_pages": found,
                  "rendered_pages": selected, "fallback_first_three": not bool(found)}), flush=True)
        for number in selected:
            prefix = root / f"page-{number}"
            subprocess.run(["pdftoppm", "-f", str(number), "-l", str(number),
                            "-singlefile", "-scale-to", "1800", "-png",
                            str(pdf), str(prefix)], check=True, timeout=45,
                           stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
            png = prefix.with_suffix(".png")
            if png.stat().st_size > MAX_BYTES:
                raise ValueError("PDF rendered page exceeds image bound")
            with Image.open(png) as source:
                source.load()
                gray = source.convert("L")
            emit_image(gray, f"{key}-page{number}", url, data)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--page", choices=tuple(SOURCES), required=True)
    args = parser.parse_args()
    url = SOURCES[args.page]
    data = download(url)
    if args.page in {"icguide_152", "icguide_sub", "icguide", "icguide_index", "icguide_offset", "icguide_156", "icguide_kas", "icguide_vco", "cs20m_q", "cs80adjust", "adjupdates_a", "adjupdates_b"}:
        if not data.startswith(b"%PDF-"):
            raise ValueError("guidebook source is not a PDF")
        inspect_pdf(url, data, args.page)
        return
    with Image.open(io.BytesIO(data)) as source:
        source.load()
        gray = source.convert("L")
    emit_image(gray, args.page, url, data)

if __name__ == "__main__":
    main()
