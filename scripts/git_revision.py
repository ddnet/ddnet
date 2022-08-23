import errno
import subprocess
try:
	git_hash = subprocess.check_output(["git", "rev-parse", "--short=16", "HEAD"], stderr=subprocess.DEVNULL).decode().strip()
	definition = f'"{git_hash}"'
except FileNotFoundError as e:
	if e.errno != errno.ENOENT:
		raise
	definition = "0"
except subprocess.CalledProcessError:
	definition = "0"
print(f"const char *GIT_SHORTREV_HASH = {definition};")
