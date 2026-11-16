from pathlib import Path
import platform
import re

release = platform.release()
base = release.rsplit("-", 1)[0]

candidates = [
    Path("/lib/modules") / release / "build",
    Path("/lib/modules") / release / "source",
    Path("/usr/src") / ("linux-headers-" + release),
    Path("/usr/src") / ("linux-headers-" + base),
]

headers = None

for candidate in candidates:
    fs = candidate / "include/linux/fs.h"
    dc = candidate / "include/linux/dcache.h"
    if fs.is_file() and dc.is_file():
        headers = candidate
        break

if headers is None:
    raise SystemExit(
        "Cannot find matching fs.h and dcache.h. "
        "Check the installed kernel headers."
    )

fs_text = (headers / "include/linux/fs.h").read_text()
dc_text = (headers / "include/linux/dcache.h").read_text()

if "simple_inode_init_ts" not in fs_text:
    raise SystemExit(
        "This example requires headers providing simple_inode_init_ts."
    )

mkdir_returns_dentry = bool(re.search(
    r"struct\s+dentry\s*\*\s*\(\s*\*\s*mkdir\s*\)",
    fs_text
))

persistent_dentry = bool(re.search(
    r"\bd_make_persistent\s*\(",
    dc_text
))

project = Path(__file__).resolve().parents[1]
output = project / "kernel/miniramfs/compat.h"

output.write_text(
    "/* Generated from the installed kernel headers. */\n"
    f"#define MP_MKDIR_RETURNS_DENTRY {int(mkdir_returns_dentry)}\n"
    f"#define MP_HAS_PERSISTENT_DENTRY {int(persistent_dentry)}\n"
)

print("Running kernel:", release)
print("Headers:", headers.resolve())
print(output.read_text())
