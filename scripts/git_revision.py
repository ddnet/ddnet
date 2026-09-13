#!/usr/bin/env python3

import sys
import os
import subprocess

if len(sys.argv) > 1 and sys.argv[1] == "ls-files":
    try:
        files = subprocess.check_output(["git", "ls-files"], stderr=subprocess.DEVNULL).decode().strip()
        print(files)
    except (FileNotFoundError, subprocess.CalledProcessError):
        pass
    sys.exit(0)

git_hash = os.environ.get("DDNET_GIT_SHORTREV_HASH")
try:
	git_hash = git_hash or subprocess.check_output(["git", "describe", "--always", "--abbrev=16", "--dirty=-dirty", "--exclude=*"], stderr=subprocess.DEVNULL).decode().strip()
except (FileNotFoundError, subprocess.CalledProcessError):
	pass

try:
	git_hash = git_hash or subprocess.check_output(["git", "rev-parse", "--short=16", "HEAD"], stderr=subprocess.DEVNULL).decode().strip()
except (FileNotFoundError, subprocess.CalledProcessError):
	pass

if git_hash is not None:
	definition = f'"{git_hash}"'
else:
	definition = "0"
print("#include <game/version.h>")
print(f"const char *GIT_SHORTREV_HASH = {definition};")
