#!/usr/bin/env python3

import os
import subprocess
import sys

version, source, header = sys.argv[1:]

git_hash = os.environ.get("DDNET_GIT_SHORTREV_HASH")
try:
	git_hash = git_hash or subprocess.check_output(["git", "rev-parse", "--short=16", "HEAD"], stderr=subprocess.DEVNULL).decode().strip()
except (FileNotFoundError, subprocess.CalledProcessError):
	pass
if git_hash is None and os.path.exists("git_revision"):
	# Official source downloads don't contain the git repository
	git_hash = open("git_revision", encoding="utf-8").read().strip()
if git_hash is not None:
	definition = f'"{git_hash}"'
else:
	definition = "0"
if git_hash is not None and version.endswith("-dev"):
	version = f"{version.removesuffix('-dev')}-{git_hash}"


def write(path, contents):
	# Everything including <game/version.h> is rebuilt when this changes
	if not os.path.exists(path) or open(path, encoding="utf-8").read() != contents:
		open(path, "w", encoding="utf-8").write(contents)


write(source, f"#include <game/version.h>\nconst char *GIT_SHORTREV_HASH = {definition};\n")
write(header, f'#define GAME_RELEASE_VERSION "{version}"\n')
