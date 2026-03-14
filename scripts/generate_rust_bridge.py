#!/usr/bin/env python3

import os
import subprocess
import sys
import argparse

os.chdir(os.path.dirname(__file__) + "/..")

def find_cxxbridge(version):
	for binary in ["cxxbridge"]:
		try:
			out = subprocess.check_output([binary, "--version"])
		except FileNotFoundError:
			continue
		if f"cxxbridge {version}\n" in out.decode("utf-8"):
			return binary
	print(f"Found no cxxbridge {version}")
	sys.exit(-1)

FILES = {
	"src/engine/shared/rust_version.rs": "src/rust-bridge/engine/shared/rust_version",
	"src/engine/console.rs": "src/rust-bridge/cpp/console",
}

def main():
	p = argparse.ArgumentParser(description="Generate src/rust-bridge")
	_args = p.parse_args()

	cxxbridge = find_cxxbridge(version="1.0.71")
	for input_, output_prefix in FILES.items():
		subprocess.check_call([
			cxxbridge,
			input_,
			"--output",
			f"{output_prefix}.cpp",
			"--output",
			f"{output_prefix}.h",
		])


if __name__ == "__main__":
	main()
