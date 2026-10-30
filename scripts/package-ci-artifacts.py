#!/usr/bin/env python3
"""Package existing CI build products; never build, install, sign or validate them.

Standard library only. Run --self-test for synthetic archive/path/mode fixtures.
The manifest is provenance, not a signature or a claim that all CI gates passed.
"""
from __future__ import annotations

import argparse
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import platform
import re
import stat
import subprocess
import sys
import tarfile
import tempfile
import zipfile

PRODUCT = "CS-80 Reference"
JUCE_SHA = "c352e2489000ffeb9e397051377d75eef2efe641"
ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def safe_name(name: str) -> None:
    parts = PurePosixPath(name).parts
    require(bool(parts) and not name.startswith("/") and ".." not in parts
            and "\\" not in name and ":" not in parts[0], f"Unsafe archive path: {name}")


def collect_products(base: Path, system: str, arch: str) -> list[tuple[str, Path]]:
    """Return only advertised bundles, preserving safe internal symlinks."""
    vst = base / "VST3" / f"{PRODUCT}.vst3"
    if system == "macos":
        products = [vst, base / "AU" / f"{PRODUCT}.component",
                    base / "Standalone" / f"{PRODUCT}.app"]
        binaries = [p / "Contents" / "MacOS" / PRODUCT for p in products]
        for p in products:
            require((p / "Contents" / "Info.plist").is_file(), f"Missing bundle Info.plist: {p}")
    elif system == "linux":
        products = [vst, base / "Standalone" / PRODUCT]
        binaries = [vst / "Contents" / f"{arch}-linux" / f"{PRODUCT}.so", products[1]]
    else:
        require(system == "windows" and arch == "x86_64", "Only native x64 Windows CI is packaged")
        products = [vst, base / "Standalone" / f"{PRODUCT}.exe"]
        binaries = [vst / "Contents" / "x86_64-win" / f"{PRODUCT}.vst3", products[1]]

    for binary in binaries:
        require(binary.is_file() and binary.stat().st_size > 0, f"Missing/empty built binary: {binary}")
        # Windows cannot represent Unix execute bits in synthetic cross-platform
        # fixtures. The CLI permits native packaging only, so real Linux/macOS
        # packages always take this check on their own runner.
        if system != "windows" and os.name != "nt":
            require(bool(binary.stat().st_mode & 0o111), f"Built binary is not executable: {binary}")

    entries: list[tuple[str, Path]] = []
    for product in products:
        require(product.exists() and not product.is_symlink(), f"Missing or linked product root: {product}")
        require(product.resolve().is_relative_to(base.resolve()), f"Product escapes artifact root: {product}")
        paths = [product]
        if product.is_dir():
            # os.walk does not traverse symbolic directory links.
            for folder, dirs, files in os.walk(product, followlinks=False):
                paths.extend(Path(folder) / name for name in sorted(dirs + files))
        for path in sorted(set(paths)):
            name = path.relative_to(base).as_posix()
            safe_name(name)
            mode = path.lstat().st_mode
            require(stat.S_ISREG(mode) or stat.S_ISDIR(mode) or stat.S_ISLNK(mode),
                    f"Unsupported filesystem object: {path}")
            if path.is_symlink():
                require(system != "windows", "Windows package must not contain symlinks")
                target = os.readlink(path)
                require(not os.path.isabs(target), f"Absolute bundle symlink: {path}")
                require(path.exists() and path.resolve().is_relative_to(product.resolve()),
                        f"Escaping or dangling bundle symlink: {path}")
            entries.append((name, path))
    return sorted(entries)


def package(base: Path, out: Path, system: str, arch: str, commit: str,
            metadata: dict, extras: dict[str, bytes]) -> tuple[Path, dict]:
    require(bool(re.fullmatch(r"[0-9a-f]{40}", commit)), "Expected full lowercase Git commit SHA")
    require(arch in {"x86_64", "arm64"}, f"Unsupported native architecture: {arch}")
    require(not out.resolve().is_relative_to(base.resolve()), "Package output must be outside build products")
    entries = collect_products(base, system, arch)
    name = f"cs80-reference-{system}-{arch}-{commit[:12]}"
    extension = ".zip" if system == "windows" else ".tar.gz"
    archive = out / (name + extension)
    manifest_path = out / (name + ".manifest.json")
    sums_path = out / "SHA256SUMS"
    out.mkdir(parents=True, exist_ok=True)
    for path in (archive, manifest_path, sums_path):
        require(not path.exists(), f"Refusing to overwrite package output: {path}")

    inventory = []
    for relative, path in entries:
        item = {"path": relative, "mode": oct(stat.S_IMODE(path.lstat().st_mode))}
        if path.is_symlink():
            item.update(type="symlink", target=os.readlink(path))
        elif path.is_dir():
            item.update(type="directory")
        else:
            item.update(type="file", size=path.stat().st_size, sha256=sha_file(path))
        inventory.append(item)
    for relative, data in sorted(extras.items()):
        safe_name(relative)
        require(relative not in {p for p, _ in entries} and relative != "manifest.json",
                f"Duplicate archive entry: {relative}")
        inventory.append({"path": relative, "type": "file", "mode": "0o644",
                          "size": len(data), "sha256": digest(data)})
    manifest = dict(metadata, schema_version=1, product=PRODUCT, commit=commit,
                    platform=system, architecture=arch,
                    formats=["VST3", "Standalone"] + (["AU"] if system == "macos" else []),
                    package_kind="development CI artifact; not a release-qualification certificate",
                    signature_status="not inspected; no release signing/notarization performed by this helper",
                    files=inventory)
    manifest_bytes = (json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode()
    payload = dict(extras, **{"manifest.json": manifest_bytes})
    created: list[Path] = []
    try:
        with archive.open("xb") as raw:
            created.append(archive)
            if system == "windows":
                with zipfile.ZipFile(raw, "w", compression=zipfile.ZIP_DEFLATED) as z:
                    for relative, path in entries:
                        z.write(path, name + "/" + relative)
                    for relative, data in sorted(payload.items()):
                        z.writestr(name + "/" + relative, data)
            else:
                with tarfile.open(fileobj=raw, mode="w:gz", dereference=False) as tar:
                    for relative, path in entries:
                        # Preserve regular bytes rather than introducing tar
                        # hard-link members for coincident source inodes.
                        tar.inodes.clear()
                        tar.add(path, arcname=name + "/" + relative, recursive=False)
                    for relative, data in sorted(payload.items()):
                        info = tarfile.TarInfo(name + "/" + relative)
                        info.size, info.mode = len(data), 0o644
                        tar.addfile(info, io.BytesIO(data))
        # Verify the archive itself before publishing its checksum evidence.
        verify_archive(archive, name, manifest)
        with manifest_path.open("xb") as stream:
            created.append(manifest_path)
            stream.write(manifest_bytes)
        with sums_path.open("x", encoding="utf-8", newline="\n") as stream:
            created.append(sums_path)
            for path in (archive, manifest_path):
                stream.write(f"{sha_file(path)}  {path.name}\n")
    except Exception:
        for path in created:
            path.unlink(missing_ok=True)
        raise
    return archive, manifest


def verify_archive(archive: Path, root: str, manifest: dict) -> None:
    """Read each archived member; verify contents, relative paths and Unix modes."""
    expected = {root + "/" + item["path"]: item for item in manifest["files"]}
    expected_manifest = root + "/manifest.json"
    seen = set()

    def check(name: str, kind: str, mode: int, data: bytes | None, target: str = "") -> None:
        name = name.rstrip("/")
        safe_name(name)
        require(name not in seen, f"Duplicate archive member: {name}")
        seen.add(name)
        if name == expected_manifest:
            require(data is not None and json.loads(data) == manifest, "Embedded manifest mismatch")
            return
        require(name in expected, f"Unexpected archive member: {name}")
        item = expected[name]
        require(item["type"] == kind, f"Archive member type changed: {name}")
        if kind == "file":
            require(data is not None and len(data) == item["size"]
                    and digest(data) == item["sha256"], f"Archive data mismatch: {name}")
        elif kind == "symlink":
            require(target == item["target"], f"Archive symlink target changed: {name}")
        if archive.suffix != ".zip":
            require(stat.S_IMODE(mode) == int(item["mode"], 8), f"Archive mode changed: {name}")

    if archive.suffix == ".zip":
        with zipfile.ZipFile(archive) as z:
            require(z.testzip() is None, "ZIP CRC verification failed")
            for member in z.infolist():
                check(member.filename, "directory" if member.is_dir() else "file",
                      member.external_attr >> 16, None if member.is_dir() else z.read(member))
    else:
        with tarfile.open(archive, "r:gz") as tar:
            for member in tar:
                require(member.isfile() or member.isdir() or member.issym(),
                        f"Unexpected tar member kind: {member.name}")
                kind = "file" if member.isfile() else "directory" if member.isdir() else "symlink"
                stream = tar.extractfile(member) if member.isfile() else None
                check(member.name, kind, member.mode, stream.read() if stream else None, member.linkname)
    require(seen == set(expected) | {expected_manifest}, "Missing archive members")


def read_ci_context(build: Path) -> tuple[str, dict, dict[str, bytes]]:
    commit = subprocess.check_output(["git", "-C", str(ROOT), "rev-parse", "HEAD"], text=True).strip()
    require(subprocess.run(["git", "-C", str(ROOT), "diff", "--quiet", "HEAD", "--"],
                           check=False).returncode == 0,
            "Tracked source differs from the named commit; package a clean CI checkout")
    if os.environ.get("GITHUB_SHA"):
        require(os.environ["GITHUB_SHA"] == commit, "Checkout and workflow commit differ")
    report = build / "ci-juce-provenance.txt"
    require(report.is_file(), "Missing CI JUCE evidence; run verify-ci-juce-source.cmake first")
    report_data = report.read_bytes()
    fields = dict(line.split("=", 1) for line in report_data.decode().splitlines() if "=" in line)
    require(fields.get("verification") == "PASS" and fields.get("source_clean") == "true"
            and fields.get("actual_sha") == JUCE_SHA and fields.get("actual_version") == "8.0.11",
            "JUCE source evidence is not the required clean, verified 8.0.11 checkout")
    require(fields.get("workflow_commit") == commit, "JUCE evidence belongs to a different workflow commit")
    source = Path(fields.get("configured_source", ""))
    require(source.resolve() == (build / "_deps/juce-src").resolve(),
            "JUCE evidence does not name the fetched CI source directory")
    license_file = source / "LICENSE.md"
    require(license_file.is_file(), "Verified JUCE source license is missing")
    extras = {
        "INSTALL_AND_QUICKSTART.md": (ROOT / "docs/INSTALL_AND_QUICKSTART.md").read_bytes(),
        "DISTRIBUTION_STATUS.md": (ROOT / "docs/CI_DISTRIBUTION_STATUS.md").read_bytes(),
        "licenses/JUCE-LICENSE.md": license_file.read_bytes(),
        "evidence/juce-provenance.txt": report_data,
        "SOURCE.txt": (f"Source commit: {commit}\n"
                       f"https://github.com/cheshmakzanoon-ops/yamaha-cs80/tree/{commit}\n"
                       "Use the full source at this commit; this archive is not a source distribution.\n").encode(),
    }
    metadata = {"juce_sha": JUCE_SHA, "juce_version": "8.0.11",
                "workflow_run_id": os.environ.get("GITHUB_RUN_ID", ""),
                "runner_os": platform.platform(),
                "validation_note": "Consult every required job for this exact commit; package availability is not a pass."}
    return commit, metadata, extras


def self_test() -> None:
    checks = 0

    def rejected(call, why):
        nonlocal checks
        try:
            call()
        except (ValueError, OSError):
            checks += 1
        else:
            raise AssertionError("Expected rejection: " + why)

    with tempfile.TemporaryDirectory(prefix="cs80-package-fixtures-") as temp:
        root = Path(temp)
        for system, arch in [("linux", "x86_64"), ("macos", "arm64"),
                             ("macos", "x86_64"), ("windows", "x86_64")]:
            base = root / f"{system}-{arch}" / "products"
            vst = base / "VST3" / f"{PRODUCT}.vst3"
            if system == "macos":
                products = [vst, base / "AU" / f"{PRODUCT}.component",
                            base / "Standalone" / f"{PRODUCT}.app"]
                binaries = [p / "Contents/MacOS" / PRODUCT for p in products]
                for p in products:
                    (p / "Contents").mkdir(parents=True)
                    (p / "Contents/Info.plist").write_text("fixture plist", encoding="utf-8")
            elif system == "linux":
                binaries = [vst / "Contents/x86_64-linux" / f"{PRODUCT}.so", base / "Standalone" / PRODUCT]
            else:
                binaries = [vst / "Contents/x86_64-win" / f"{PRODUCT}.vst3", base / "Standalone" / f"{PRODUCT}.exe"]
            for binary in binaries:
                binary.parent.mkdir(parents=True, exist_ok=True)
                binary.write_bytes(b"synthetic binary fixture, not a loadable plugin\x00")
                binary.chmod(0o755)
            if system == "macos" and os.name != "nt":
                link = products[-1] / "Contents/Resources/binary-link"
                link.parent.mkdir()
                link.symlink_to("../MacOS/" + PRODUCT)
            extras = {"INSTALL_AND_QUICKSTART.md": b"fixture instructions\n"}
            out = base.parent / "out"
            archive, manifest = package(base, out, system, arch, "a" * 40, {}, extras)
            checks += 1  # package() reopens and verifies every member/hash/mode.
            require(manifest["architecture"] == arch and archive.stat().st_size > 0, "Fixture package missing")
            rejected(lambda: package(base, out, system, arch, "a" * 40, {}, extras), "overwrite")
            original = binaries[0].read_bytes()
            binaries[0].write_bytes(b"")
            rejected(lambda: collect_products(base, system, arch), "empty binary")
            binaries[0].write_bytes(original)
            if system != "windows" and os.name != "nt":
                binaries[0].chmod(0o644)
                rejected(lambda: collect_products(base, system, arch), "lost executable mode")
                binaries[0].chmod(0o755)
                link = vst / "escape"
                link.symlink_to(root)
                rejected(lambda: collect_products(base, system, arch), "absolute escape symlink")
                link.unlink()
                link.symlink_to("../../../../outside")
                rejected(lambda: collect_products(base, system, arch), "relative escape symlink")
                link.unlink()
                link.symlink_to("escape")
                rejected(lambda: collect_products(base, system, arch), "cyclic symlink")
                link.unlink()
            binaries[0].unlink()
            rejected(lambda: collect_products(base, system, arch), "missing binary")
        for unsafe in ("../escape", "/absolute", "C:/drive", "folder/../../escape", "folder\\escape"):
            rejected(lambda value=unsafe: safe_name(value), "unsafe archive path")
        rejected(lambda: package(base, base / "output", "windows", "x86_64", "a" * 40, {}, {}),
                 "output nested inside build products")
    print(f"Packaging self-test passed: {checks} fixture/rejection checks; synthetic artifacts only.")
    if os.name == "nt":
        print("Native symlink/chmod fixtures run on Unix CI; Windows packages reject all symlinks.")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--platform", choices=("linux", "macos", "windows"))
    parser.add_argument("--build-dir", type=Path, default=Path("build"))
    parser.add_argument("--out-dir", type=Path, default=Path("packages"))
    args = parser.parse_args()
    try:
        if args.self_test:
            self_test()
            return 0
        require(args.platform is not None, "--platform is required")
        native = {"Linux": "linux", "Darwin": "macos", "Windows": "windows"}.get(platform.system())
        require(args.platform == native, "Package native CI outputs only; cross-target fixtures use --self-test")
        arch = {"AMD64": "x86_64", "x86_64": "x86_64", "arm64": "arm64", "aarch64": "arm64"}.get(platform.machine())
        require(arch is not None, "Unsupported runner architecture")
        commit, metadata, extras = read_ci_context(args.build_dir.resolve())
        archive, manifest = package(args.build_dir / "CS80Reference_artefacts/Release",
                                    args.out_dir, args.platform, arch, commit, metadata, extras)
        print(json.dumps({"archive": str(archive), "sha256": sha_file(archive),
                          "formats": manifest["formats"], "architecture": arch}, indent=2))
        return 0
    except (ValueError, OSError, subprocess.SubprocessError, tarfile.TarError, zipfile.BadZipFile) as error:
        print(f"Packaging failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
