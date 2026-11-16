# Mini RAM Filesystem

## Scope

An educational Linux VFS filesystem storing file contents in RAM.

Supported:
- Mount and unmount
- Create, read and write files
- Create and list directories
- Append, truncate, unlink and remove empty directories

Maximum file size: 16 MiB.
No global filesystem memory quota.
No persistent storage, mmap, rename or hard-link implementation.

## Requirements

Ubuntu Linux, matching kernel headers, build-essential, kmod and Python 3.
See results/environment/ for the actual tested environment.

## Build

From the project root:

    python3 tools/gen_compat.py
    make -C /lib/modules/$(uname -r)/build M="$PWD/kernel/miniramfs" modules

## Load and mount

    sudo insmod kernel/miniramfs/miniramfs.ko
    sudo mkdir -p /mnt/miniramfs
    sudo mount -t miniramfs none /mnt/miniramfs
    sudo chown "$(id -u):$(id -g)" /mnt/miniramfs
    findmnt -M /mnt/miniramfs

## Test

    python3 tools/test_miniramfs.py

## Stop

Close files and leave the mount directory first.

    cd ~/miniProject2
    sudo umount /mnt/miniramfs
    sudo rmmod miniramfs

Unmounting destroys this filesystem instance and its contents.

## Implementation

The module supplies filesystem registration, mount context, superblock
initialization and inode/directory creation logic.

It reuses Linux VFS helpers and ram_aops for directory traversal,
page-cache-backed file I/O and basic metadata operations.

## Results

Record actual test outcomes in results/ and report/.
