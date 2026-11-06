"""Process-correlated local crash artifacts and exact Windows symbol identities; standard library only."""
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import uuid

from .agent_output import quiet, shown
from .errors import ToolError

REPORTER = "EngineCrashReporter.exe"


def read_report(path: Path) -> dict:
    if path.is_dir():
        path = path / "report.json"
    try:
        report = json.loads(path.read_text(encoding="utf-8"))
        if report["schema_version"] != 1 or not isinstance(report["frames"], list) or not isinstance(report["modules"], list):
            raise ValueError("unsupported schema or invalid frames/modules")
        if not all(isinstance(report[key], str) for key in ("message", "kind", "status")):
            raise ValueError("invalid crash summary")
        for frame in report["frames"]:
            if not isinstance(frame, dict) or not isinstance(frame.get("address"), int):
                raise ValueError("invalid stack frame")
            if frame.get("file") is not None and (not isinstance(frame["file"], str) or not isinstance(frame.get("line"), int)):
                raise ValueError("invalid source location")
            if "function" in frame and not isinstance(frame["function"], str):
                raise ValueError("invalid function name")
        return report
    except (OSError, ValueError, KeyError, TypeError) as error:
        raise ToolError(f"Cannot read crash report {path}: {error}") from error


def reports(root: Path) -> list[tuple[Path, dict]]:
    found = []
    for file in sorted(root.glob("*/report.json")):
        try:
            found.append((file, read_report(file)))
        except ToolError:
            pass  # Keep a broken helper artifact without obscuring the original process failure.
    return found


def summary(file: Path, report: dict) -> str:
    lines = [f"Crash ({report['kind']}): {report['message']}"]
    frames = report["frames"]
    meaningful = [frame for frame in frames if frame.get("file") and "cpptrace" not in frame.get("function", "")
                  and "Diagnostics/Internals" not in frame.get("file", "").replace("\\", "/")]
    for frame in (meaningful or frames)[:5]:
        line = frame.get("function", f"0x{frame['address']:X}")
        if frame.get("file"):
            line += f"  {frame['file']}:{frame['line']}"
        lines.append("  " + line)
    lines.append(f"Crash report: {shown(file)} ({report['status']})")
    return "\n".join(lines)


def run_native(executable: Path, arguments: list[str], root: Path, *, cwd: Path | None = None) -> int:
    """One launch owns one directory. Never attribute yesterday's crash to a new command."""
    launch = root / uuid.uuid4().hex
    launch.mkdir(parents=True)
    env = os.environ | {"ENGINE_CRASH_DIRECTORY": str(launch.resolve())}
    log = launch / "stderr.log"
    with log.open("w", encoding="utf-8") as output:
        try:
            code = subprocess.run([str(executable), *arguments], cwd=cwd, env=env, stderr=output).returncode
        except OSError as error:
            raise ToolError(f"Cannot start {executable}: {error}") from error
    crashes = reports(launch)
    if quiet() and crashes:
        for file, report in crashes:
            print(summary(file, report), file=sys.stderr)
    else:
        text = log.read_text(encoding="utf-8", errors="replace")
        if text:
            print(text, file=sys.stderr, end="" if text.endswith("\n") else "\n")
    return code


def output_summaries(lines: list[str]) -> list[str]:
    """A refresh's streamed markers, rather than a scan of any build/history directory."""
    result = []
    seen = set()
    for line in lines:
        if not line.startswith("Crash report: "):
            continue
        try:
            file = Path(line.removeprefix("Crash report: ").strip()).with_name("report.json")
        except ValueError:
            continue
        if file in seen:
            continue
        seen.add(file)
        try:
            result.append(summary(file, read_report(file)))
        except ToolError:
            reasons = [text for text in lines if text.startswith("Crash:")]
            result.append("\n".join([*reasons[:1], line]))
    return result


def pe_identity(path: Path) -> dict:
    """Read the PE CodeView RSDS record without loading or executing the application."""
    data = path.read_bytes()
    try:
        if data[:2] != b"MZ":
            raise ValueError("not a PE executable")
        pe = struct.unpack_from("<I", data, 0x3C)[0]
        if data[pe:pe + 4] != b"PE\0\0":
            raise ValueError("invalid PE signature")
        count = struct.unpack_from("<H", data, pe + 6)[0]
        optional_size = struct.unpack_from("<H", data, pe + 20)[0]
        optional = pe + 24
        magic = struct.unpack_from("<H", data, optional)[0]
        directories = optional + (112 if magic == 0x20B else 96)
        debug_rva, debug_size = struct.unpack_from("<II", data, directories + 6 * 8)
        sections = optional + optional_size
        for i in range(count):
            virtual_size, rva, raw_size, raw = struct.unpack_from("<IIII", data, sections + i * 40 + 8)
            if rva <= debug_rva < rva + max(virtual_size, raw_size):
                offset = raw + debug_rva - rva
                for j in range(debug_size // 28):
                    kind, size, _, location = struct.unpack_from("<IIII", data, offset + j * 28 + 12)
                    if kind == 2 and size >= 25 and data[location:location + 4] == b"RSDS":
                        guid = uuid.UUID(bytes_le=data[location + 4:location + 20]).hex.upper()
                        age = struct.unpack_from("<I", data, location + 20)[0]
                        name = data[location + 24:location + size].split(b"\0", 1)[0].decode("utf-8")
                        return {"guid": guid, "age": age, "name": Path(name).name}
        raise ValueError("no PDB identity")
    except (struct.error, UnicodeError, ValueError) as error:
        raise ToolError(f"Cannot read symbol identity from {path}: {error}") from error


def pdb_identity(path: Path) -> dict:
    data = path.read_bytes()
    try:
        if not data.startswith(b"Microsoft C/C++ MSF 7.00\r\n\x1aDS\0\0\0"):
            raise ValueError("unsupported PDB format")
        block, _, _, size, _, block_map = struct.unpack_from("<6I", data, 32)
        if block not in (512, 1024, 2048, 4096) or size > len(data):
            raise ValueError("invalid PDB directory")
        numbers = struct.unpack_from(f"<{(size + block - 1) // block}I", data, block_map * block)
        directory = b"".join(data[n * block:(n + 1) * block] for n in numbers)[:size]
        count = struct.unpack_from("<I", directory)[0]
        sizes = struct.unpack_from(f"<{count}I", directory, 4)
        position = 4 + count * 4
        stream = b""
        for i, length in enumerate(sizes[:2]):
            blocks = 0 if length == 0xFFFFFFFF else (length + block - 1) // block
            entries = struct.unpack_from(f"<{blocks}I", directory, position)
            position += blocks * 4
            if i == 1:
                stream = b"".join(data[n * block:(n + 1) * block] for n in entries)[:length]
        age = struct.unpack_from("<I", stream, 8)[0]
        guid = uuid.UUID(bytes_le=stream[12:28]).hex.upper()
        return {"guid": guid, "age": age}
    except (struct.error, ValueError, IndexError, OverflowError) as error:
        raise ToolError(f"Cannot read PDB identity from {path}: {error}") from error
