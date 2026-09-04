#!/usr/bin/env python3

import os
import subprocess

changed_files = ""
try:
	changed_files = subprocess.check_output(["git", "status", "--porcelain"], stderr=subprocess.DEVNULL).decode().strip()
except (FileNotFoundError, subprocess.CalledProcessError):
	pass

git_hash = os.environ.get("DDNET_GIT_SHORTREV_HASH")
try:
	git_hash = git_hash or subprocess.check_output(["git", "rev-parse", "--short=16", "HEAD"], stderr=subprocess.DEVNULL).decode().strip()
except (FileNotFoundError, subprocess.CalledProcessError):
	pass
if git_hash is not None:
	dirty = ""
	if changed_files != "":
		dirty = "-dirty"
	definition = f'"{git_hash}{dirty}"'
else:
	definition = "0"
print("#include <game/version.h>")
print(f"const char *GIT_SHORTREV_HASH = {definition};")
