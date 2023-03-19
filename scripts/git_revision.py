import os
import subprocess
git_hash = os.environ.get("DDNET_GIT_SHORTREV_HASH")
try:
	git_hash = git_hash or subprocess.check_output(["git", "rev-parse", "--short=16", "HEAD"], stderr=subprocess.DEVNULL).decode().strip()
except (FileNotFoundError, subprocess.CalledProcessError):
	pass
if git_hash is not None:
	definition = f'"{git_hash}"'
else:
	definition = "0"
print(f"const char *GIT_SHORTREV_HASH = {definition};")
