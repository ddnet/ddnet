#!/usr/bin/env python3

import hashlib
import os
from pathlib import Path

def enumerate_data_sorted(base_path : Path) -> list[str]:
	entries = []
	for root, _, files in os.walk(base_path.joinpath("data")):
		for file in files:
			entries.append(Path(root).joinpath(file).relative_to(base_path).as_posix())
	entries.sort()
	return entries

def sha256_file(path : Path) -> str:
	sha256 = hashlib.sha256()
	with open(path, "rb") as file:
		while file_chunk := file.read(65536):
			sha256.update(file_chunk)
	return sha256.hexdigest()

def generate_asset_integrity_file(base_path : Path):
	asset_files = enumerate_data_sorted(base_path)
	index_lines = map(lambda asset: f"{asset} {sha256_file(base_path.joinpath(asset))}", asset_files)
	index_string = "\n".join(index_lines) + "\n"
	aggregated_hash = hashlib.sha256(index_string.encode("utf-8")).hexdigest()
	with base_path.joinpath("integrity.txt").open("w", encoding="utf-8") as index_file:
		index_file.write(aggregated_hash)
		index_file.write("\n")
		index_file.write(index_string)

if __name__ == "__main__":
	generate_asset_integrity_file(Path("assets/asset_integrity_files"))
