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
from PIL import Image

SOURCES = {
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
    last = None
    for attempt in range(3):
        try:
            req = urllib.request.Request(url, headers={"User-Agent": "CS80-circuit-research/1.0"})
            with urllib.request.urlopen(req, timeout=30) as response:
                if response.geturl().split("/")[2] != "www.therogoffs.com":
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

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--page", choices=tuple(SOURCES), required=True)
    args = parser.parse_args()
    url = SOURCES[args.page]
    data = download(url)
    with Image.open(io.BytesIO(data)) as source:
        source.load()
        gray = source.convert("L")
    w, h = gray.size
    print("CS80_SOURCE_METADATA " + json.dumps({
        "page": args.page, "url": url, "sha256": hashlib.sha256(data).hexdigest(),
        "bytes": len(data), "width": w, "height": h,
        "purpose": "Original Yamaha schematic analysis; not measured hardware evidence"
    }), flush=True)
    boxes = []
    # Overlap preserves circuit connections that cross a tile boundary.
    for row in range(2):
        for col in range(2):
            boxes.append((f"{args.page}-r{row}-c{col}", (
                max(0, col * w // 2 - 64), max(0, row * h // 2 - 64),
                min(w, (col + 1) * w // 2 + 64), min(h, (row + 1) * h // 2 + 64)
            ), True))
    if max(w, h) <= 2400:
        boxes.append((f"{args.page}-full-native", (0, 0, w, h), False))
    # Native-pixel detail crops, selected from inspected source coordinates.
    focus = {
        "m": [("filter-detail", (1180, 1080, 3250, 1850))],
        "tws": [
            ("voice1-upper", (650, 550, 1250, 1850)),
            ("voice1-lower", (650, 1600, 1250, 3100)),
        ],
    }
    for name, box in focus.get(args.page, []):
        if box[2] > w or box[3] > h or max(box[2]-box[0], box[3]-box[1]) > 2400:
            raise ValueError("focus crop outside verified source bounds")
        boxes.append((f"{args.page}-{name}", box, False))
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

if __name__ == "__main__":
    main()
