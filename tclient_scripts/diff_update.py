#!/usr/bin/env python3

import hashlib
import json
import os.path
import re
import subprocess
import sys
import tempfile

try:
    from tqdm import tqdm
except ImportError:
    def tqdm(it, *args, **kwargs):
        return it

def collect_file_paths(root):
    result = []
    for dirpath, dirnames, filenames in os.walk(root):
        for filename in filenames:
            absolute_path = os.path.join(dirpath, filename)
            if dirpath == root:
                dirpath_relative = []
            else:
                dirpath_relative = list(os.path.split(os.path.relpath(dirpath, root)))
                if len(dirpath_relative) > 0 and dirpath_relative[0] == "":
                    dirpath_relative = dirpath_relative[1:]
            relative_path = "/".join(dirpath_relative + [filename])
            if "." in filename and not filename.endswith(".exe"):
                result.append((relative_path, absolute_path))
    return sorted(result)

def hash_file(path):
    sha256 = hashlib.sha256()
    with open(path, "rb") as f:
        while True:
            data = f.read(8192)
            if not data:
                break
            sha256.update(data)
    return sha256.hexdigest()

def hash_recursively(root):
    return [(relative, hash_file(absolute)) for relative, absolute in collect_file_paths(root)]

def diff_file_hashes(hashes1, hashes2):
    iter1 = iter(sorted(hashes1))
    iter2 = iter(sorted(hashes2))
    file1, hash1 = next(iter1, (None, None))
    file2, hash2 = next(iter2, (None, None))
    created = []
    updated = []
    removed = []
    while file1 is not None or file2 is not None:
        if file2 is None or (file1 is not None and file1 < file2):
            removed.append(file1)
            file1, hash1 = next(iter1, (None, None))
        elif file1 is None or (file2 is not None and file2 < file1):
            created.append(file2)
            file2, hash2 = next(iter2, (None, None))
        else:
            if hash1 != hash2:
                updated.append(file1)
            file1, hash1 = next(iter1, (None, None))
            file2, hash2 = next(iter2, (None, None))
    return sorted(created + updated), removed

def compare_multiple(l, data):
    total_download = set()
    total_remove = set()

    if len(l) > 1:
        l = tqdm(l, leave=False)

    for path1, path2 in l:
        hashes1 = hash_recursively(path1)
        hashes2 = hash_recursively(path2)
        download, remove = diff_file_hashes(hashes1, hashes2)
        total_download.update(download)
        total_remove.update(remove)

    if total_download & total_remove:
        raise RuntimeError("File was removed in some versions and updated in others")

    if total_download:
        data["download"] = sorted(total_download)
    if total_remove:
        data["remove"] = sorted(total_remove)

def main():
    import argparse
    p = argparse.ArgumentParser(description="""\
Diff two TClient release archives to obtain an update description.

E.g. `python diff_update.py 14.6.2 14.7` with directories for different
versions in the current working directory
""", formatter_class=argparse.RawTextHelpFormatter)
    p.add_argument("version1", metavar="VER1", help="Original version to compare against")
    p.add_argument("version2", metavar="VER2", help="New version")
    args = p.parse_args()

    with open("update.json") as json_file:
      data = json.load(json_file)

    pairs = [
        ("TClient-{}-win64".format(args.version1), "TClient-{}-win64".format(args.version2)),
        ("TClient-{}-linux_x86_64".format(args.version1), "TClient-{}-linux_x86_64".format(args.version2))
    ]

    data.insert(0, {"version": args.version2, "client": True, "server": True})
    compare_multiple(pairs, data[0])

    with open("update.json.new", "w") as json_file:
        json.dump(data, json_file, indent=2, sort_keys=False)

if __name__ == "__main__":
    sys.exit(main())
