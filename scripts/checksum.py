#!/usr/bin/env python3
import argparse
import hashlib
import os

os.chdir(os.path.dirname(__file__) + "/..")

def hash_bytes(b):
	return f"0x{hashlib.sha256(b).hexdigest()[:8]}"

def hash_file(filename):
	with open(filename, "rb") as f:
		return hash_bytes(f.read())

def main():
	p = argparse.ArgumentParser(description="Checksums source files")
	p.add_argument("list_file", metavar="LIST_FILE", help="File listing all the files to hash")
	p.add_argument("extra_file", metavar="EXTRA_FILE", help="File containing extra strings to be hashed")
	args = p.parse_args()

	with open(args.list_file, encoding="utf-8") as f:
		files = f.read().splitlines()
	with open(args.extra_file, "rb") as f:
		extra = f.read().splitlines()
	hashes_files = [hash_file(file) for file in files]
	hashes_extra = [hash_bytes(line) for line in extra]
	hashes = hashes_files + hashes_extra
	print("""\
#include <engine/client/checksum.h>

void CChecksumData::InitFiles()
{
""", end="")
	print(f"\tm_NumFiles = {len(hashes_files)};")
	print(f"\tm_NumExtra = {len(hashes_extra)};")
	for i, h in enumerate(hashes):
		print(f"\tm_aFiles[0x{i:03x}] = {h};")
	print("}")

if __name__ == "__main__":
	main()
