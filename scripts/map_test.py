#!/usr/bin/env python3

from __future__ import annotations  # FIXME(PY3.9)

from pathlib import Path
import re
import subprocess
import sys


def find_maps_recursively(path: Path) -> list[Path]:
	map_files = []
	for file in Path.iterdir(path):
		if file.is_dir():
			map_files += find_maps_recursively(file)
		elif file.name.endswith(".map"):
			map_files += [file]
	return sorted(map_files)


def run_map_test_file(map_file: Path, input_folder: Path, output_folder: Path, map_test_executable: Path, map_test_argument: str | None) -> bool:
	output_file_path = output_folder / map_file.relative_to(input_folder).with_suffix(".log")
	output_file_path.parent.mkdir(parents=True, exist_ok=True)

	result = subprocess.run(
		[map_test_executable, map_test_argument, str(map_file)] if map_test_argument else [map_test_executable, str(map_file)],
		capture_output=True,
		text=True,
		check=False,
		encoding="utf-8",
	)

	# Strip timestamps from logs so we can compare output before and after changes to map_test tool.
	cleaned_output = "\n".join(re.sub(r"^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2} ", "", line) for line in result.stdout.splitlines())

	with output_file_path.open("w", encoding="utf-8") as log_file:
		log_file.write(cleaned_output)

	return result.returncode == 0


def run_map_test_folder(input_folder: Path, output_folder: Path, map_test_executable: Path, map_test_argument: str | None) -> int:
	if not input_folder.is_dir():
		print(f"Error: Input folder '{input_folder}' does not exist.")
		return 1

	if not map_test_executable.is_file():
		print(f"Error: Map test executable '{map_test_executable}' does not exist.")
		return 1

	if not output_folder.exists():
		output_folder.mkdir(parents=True)

	print(f"Input folder: {input_folder}")
	print(f"Output folder: {output_folder}")
	print(f"Map test executable: {map_test_executable}")
	print()

	all_map_files = find_maps_recursively(input_folder)
	failed_map_files = []
	for map_file_index, map_file in enumerate(all_map_files):
		print(f"[{map_file_index + 1}/{len(all_map_files)}]  {map_file.relative_to(input_folder)}  ", end="", flush=True)
		if run_map_test_file(map_file, input_folder, output_folder, map_test_executable, map_test_argument):
			print("[OK]")
		else:
			print("[FAILED]")
			failed_map_files.append(map_file)

	if failed_map_files:
		print()
		print("Map test failed for the following maps:")
		for map_file in failed_map_files:
			print(f"- {map_file}")
		return 1
	return 0


def map_test_main() -> int:
	if len(sys.argv) < 4 or len(sys.argv) > 5:
		print("Usage: python map_test_all.py <input_folder> <output_folder> <map_test_executable> [map_test_argument]")
		return 1
	input_folder = Path(sys.argv[1]).resolve()
	output_folder = Path(sys.argv[2]).resolve()
	map_test_executable = Path(sys.argv[3]).resolve()
	map_test_argument = sys.argv[4] if len(sys.argv) == 5 else None
	return run_map_test_folder(input_folder, output_folder, map_test_executable, map_test_argument)


if __name__ == "__main__":
	sys.exit(map_test_main())
