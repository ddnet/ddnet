#!/usr/bin/env python3
import argparse
import os
import re
import subprocess
import tempfile

os.chdir(os.path.dirname(__file__) + "/..")
PATH = "src/"


CURL_RE=re.compile(r"\bcurl_\w*")

def get_curl_calls(path):
	names = set()
	for directory, _, files in os.walk(path):
		for filename in files:
			if (filename.endswith(".cpp") or
					filename.endswith(".c") or
					filename.endswith(".h")):
				with open(os.path.join(directory, filename), encoding="utf-8") as f:
					contents = f.read()
				names = names.union(CURL_RE.findall(contents))
	return names

def assembly_source(names):
	names = sorted(names)
	result = []
	for name in names:
		result.append(f".type {name},@function")
	for name in names:
		result.append(f".global {name}")
	for name in names:
		result.append(f"{name}:")
	return "\n".join(result + [""])

DEFAULT_OUTPUT="libcurl.so"
DEFAULT_SONAME="libcurl.so.4"

def main():
	p = argparse.ArgumentParser(description="Create a stub shared object for linking")
	p.add_argument("-k", "--keep", action="store_true", help="Keep the intermediary assembly file")
	p.add_argument("--output", help=f"Output filename (default: {DEFAULT_OUTPUT})", default=DEFAULT_OUTPUT)
	p.add_argument("--soname", help=f"soname of the produced shared object (default: {DEFAULT_SONAME})", default=DEFAULT_SONAME)
	p.add_argument("--functions", metavar="FUNCTION", nargs="+", help="Function symbols that should be put into the shared object (default: look for curl_* names in the source code)")
	p.add_argument("--link-args", help="Colon-separated list of additional linking arguments")
	args = p.parse_args()

	if args.functions is not None:
		functions = args.functions
	else:
		functions = get_curl_calls(PATH)
	extra_link_args = []
	if args.link_args:
		extra_link_args = args.link_args.split(":")

	with tempfile.NamedTemporaryFile("w", suffix=".s", delete=not args.keep) as f:
		if args.keep:
			print(f"using {f.name} as temporary file")
		f.write(assembly_source(functions))
		f.flush()
		subprocess.check_call([
			"cc",
		] + extra_link_args + [
			"-shared",
			"-nostdlib", # don't need to link to libc
			f"-Wl,-soname,{args.soname}",
			"-o", args.output,
			f.name,
		])
		subprocess.check_call(["strip", args.output])

if __name__ == '__main__':
	main()
