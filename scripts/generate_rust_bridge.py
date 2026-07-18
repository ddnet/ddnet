#!/usr/bin/env python3

import argparse
import os
import subprocess
import sys
import tomllib

os.chdir(os.path.dirname(__file__) + "/..")


def find_cxxbridge_version():
	with open("Cargo.toml") as f:
		cargo_toml = f.read()
	cargo_toml = tomllib.loads(cargo_toml)
	cxx_version = cargo_toml["workspace"]["dependencies"]["cxx"]
	if not cxx_version.startswith("="):
		raise RuntimeError("expected cxx package to pin a specific version")
	return cxx_version[1:]


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
	p.add_argument("--cxx-version", action="store_true", help="Print cxx version and exit")
	args = p.parse_args()

	cxxbridge_version = find_cxxbridge_version()
	if args.cxx_version:
		print(cxxbridge_version)
		return
	cxxbridge = find_cxxbridge(version=cxxbridge_version)
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
