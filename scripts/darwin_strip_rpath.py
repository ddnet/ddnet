import argparse
import subprocess
import re

def split_cmds(lines):
	cmds = []
	current = []
	load_cmd_regex = re.compile(r"^Load command \d+$")
	for line in lines:
		if load_cmd_regex.match(line):
			cmds.append(current)
			current = []
			continue

		current.append(line.strip())

	return cmds[1:]

def main():
	p = argparse.ArgumentParser(description="Strip LC_RPATH commands from executable")

	p.add_argument('otool', help="Path to otool")
	p.add_argument('install_name_tool', help="Path to install_name_tool")
	p.add_argument('executable', metavar="EXECUTABLE", help="The executable to strip")
	args = p.parse_args()

	otool = args.otool
	install_name_tool = args.install_name_tool
	executable = args.executable

	cmds = split_cmds(subprocess.check_output([otool, "-l", executable]).decode().splitlines())
	lc_rpath_cmds = [cmd for cmd in cmds if cmd[0] == "cmd LC_RPATH"]

	path_regex = re.compile(r"^path (.*) \(offset \d+\)$")
	rpaths = {k[0] for k in [[path_regex.match(part).group(1) for part in cmd if path_regex.match(part)] for cmd in lc_rpath_cmds]}
	print("Found paths:")

	for path in rpaths:
		print("\t" + path)
		subprocess.check_call([install_name_tool, "-delete_rpath", path, executable])

if __name__ == '__main__':
	main()
