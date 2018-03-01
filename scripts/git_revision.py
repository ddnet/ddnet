import subprocess
try:
	git_hash = subprocess.check_output(["git", "rev-parse", "--short=16", "HEAD"], stderr=None).decode().strip()
	definition = '"{}"'.format(git_hash)
except (FileNotFoundError, subprocess.CalledProcessError):
	definition = "0";
print("const char *GIT_SHORTREV_HASH = {};".format(definition))
