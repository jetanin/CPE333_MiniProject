from pathlib import Path
import errno
import os
import subprocess
import tempfile

mount = Path("/mnt/miniramfs")

fs_type = subprocess.check_output(
    ["findmnt", "-n", "-o", "FSTYPE", "-M", str(mount)],
    text=True
).strip()

if fs_type != "miniramfs":
    raise SystemExit("STOP: /mnt/miniramfs is not mounted as miniramfs")

root = Path(tempfile.mkdtemp(prefix="validation-", dir=mount))
passed = 0

def check(condition, name):
    global passed
    if not condition:
        raise AssertionError(name)
    passed += 1
    print(f"PASS {passed:02d}: {name}", flush=True)

empty = root / "empty.txt"
empty.touch()
check(empty.stat().st_size == 0, "create empty file")
check(empty.read_bytes() == b"", "read empty file")

nested = root / "a" / "b"
nested.mkdir(parents=True)
check(nested.is_dir(), "create nested directories")

text = nested / "text.txt"
text.write_text("Hello Operating Systems\n")
check(text.read_text() == "Hello Operating Systems\n", "write/read text")

with text.open("a") as stream:
    stream.write("Appended\n")
check(
    text.read_text() == "Hello Operating Systems\nAppended\n",
    "append data"
)

text.write_text("X\n")
check(text.read_bytes() == b"X\n", "overwrite and truncate old data")

payload = bytes(range(256)) * 513
binary = root / "binary.bin"
binary.write_bytes(payload)
check(binary.read_bytes() == payload, "binary data across multiple pages")
check(binary.stat().st_size == len(payload), "correct file size")

sparse = root / "sparse.bin"
with sparse.open("wb") as stream:
    stream.seek(8192)
    stream.write(b"Z")
check(
    sparse.read_bytes() == b"\0" * 8192 + b"Z",
    "seek beyond EOF and read zero-filled gap"
)

with binary.open("r+b") as stream:
    stream.truncate(10)
check(binary.read_bytes() == payload[:10], "shrink file")

with binary.open("r+b") as stream:
    stream.truncate(20)
check(
    binary.read_bytes() == payload[:10] + b"\0" * 10,
    "extend truncated file with zeros"
)

try:
    (root / "missing.txt").read_bytes()
except FileNotFoundError:
    check(True, "missing file reports ENOENT")
else:
    raise AssertionError("missing file unexpectedly readable")

try:
    nested.mkdir()
except FileExistsError:
    check(True, "duplicate directory reports EEXIST")
else:
    raise AssertionError("duplicate directory unexpectedly created")

limit_file = root / "limit.bin"
fd = os.open(limit_file, os.O_CREAT | os.O_WRONLY, 0o600)
try:
    try:
        os.pwrite(fd, b"X", 16 * 1024 * 1024)
    except OSError as error:
        check(error.errno == errno.EFBIG, "enforce 16 MiB file limit")
    else:
        raise AssertionError("write beyond file limit unexpectedly succeeded")
finally:
    os.close(fd)

empty.unlink()
check(not empty.exists(), "unlink file")

temporary_dir = root / "remove-me"
temporary_dir.mkdir()
temporary_dir.rmdir()
check(not temporary_dir.exists(), "remove empty directory")

print(f"\nALL {passed} CHECKS PASSED")
print(f"Test files retained at: {root}")
