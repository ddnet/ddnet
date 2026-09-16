#!/usr/bin/env python3

import os
import subprocess
import sys

version, source = sys.argv[1:]

git_hash = os.environ.get("DDNET_GIT_SHORTREV_HASH")
try:
	git_hash = git_hash or subprocess.check_output(["git", "rev-parse", "--short=16", "HEAD"], stderr=subprocess.DEVNULL).decode().strip()
except (FileNotFoundError, subprocess.CalledProcessError):
	pass
if git_hash is None:
	# Official source downloads don't contain the git repository
	try:
		with open("git_revision", encoding="utf-8") as f:
			git_hash = f.read().strip()
	except FileNotFoundError:
		pass
if git_hash is not None:
	definition = f'"{git_hash}"'
else:
	definition = "0"
if git_hash is not None and version.endswith("-dev"):
	version = f"{version.removesuffix('-dev')}-{git_hash}"


def update(path, contents):
	try:
		with open(path, encoding="utf-8") as f:
			if f.read() == contents:
				return
	except FileNotFoundError:
		pass
	with open(path, "w", encoding="utf-8") as f:
		f.write(contents)


# For compatibility with DDNet client 15.8 and older GAME_VERSION needs to
# include the prefix `0.6` because this was used for a "Compatible version"
# filter in the server browser.
update(
	source,
	f"""#include <game/version.h>
const char *GIT_SHORTREV_HASH = {definition};
const char *GAME_RELEASE_VERSION = "{version}";
const char *GAME_VERSION = "0.6, {version}";
""",
)
