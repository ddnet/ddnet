#!/usr/bin/env python3

import argparse
from datetime import datetime
import lzma
from pathlib import Path
import re
import shutil
import subprocess
import tarfile
import urllib.error
import urllib.parse
import urllib.request

# TODO: 2027 or later: remove backwards compatibility for parsing filename without version and architecture
CRASH_FILENAME_PATTERN = re.compile(r"(DDNet|DDNet-Server)(_([0-9\.\-]+))?_((win32|win64)(-steam)?)(_([a-zA-Z0-9]+))?_crash_log_([0-9]{4}-[0-9]{2}-[0-9]{2}_[0-9]{2}-[0-9]{2}-[0-9]{2})_([0-9]+)_([0-9A-Fa-f]*)")
IMAGE_BASE_PATTERN = re.compile(r"^ImageBase\s+([0-9A-Fa-f]+)$", re.MULTILINE)
DATE_TIME_PATTERN = re.compile(r"^Error occurred on (.+)\.$")
ERROR_MESSAGE_PATTERN = re.compile(r"^.+?\.exe caused .+\.$")
STACK_LINE_PATTERN = re.compile(r"^[0-9A-Fa-f]+\s+[0-9A-Fa-f]+\s+[0-9A-Fa-f]+\s+[0-9A-Fa-f]+\s+(.+)")
DEBUG_SYMBOL_PATTERN = re.compile(r"(.+\.exe)!(.*?)\+(0x[0-9A-Fa-f]+)")
RELEASE_SYMBOL_PATTERN = re.compile(r"(.+\.exe)!(0x[0-9A-Fa-f]+)")
UNRESOLVED_SYMBOL_PATTERN = re.compile(r"^0x[0-9A-Fa-f]+: \?\? .+$")


def run_command(args: list[str]) -> str:
	try:
		result = subprocess.run(
			args,
			check=True,
			stdin=subprocess.DEVNULL,
			stdout=subprocess.PIPE,
			stderr=subprocess.PIPE,
			text=True,
		)
		return result.stdout.strip()
	except subprocess.CalledProcessError as error:
		raise RuntimeError(f"{error}\n{error.stdout}\n{error.stderr}") from error


class ParsedFilename:
	def __init__(self, executable, version, platform, architecture, timestamp, commit) -> None:
		self.executable = executable
		self.version = version
		self.platform = platform
		self.architecture = architecture
		self.timestamp = timestamp
		self.commit = commit


# TODO: 2027 or later: remove backwards compatibility for parsing filename without version and architecture
def parse_crash_filename(filename: str) -> ParsedFilename | None:
	match = CRASH_FILENAME_PATTERN.search(filename)
	if not match:
		return None
	return ParsedFilename(
		executable=match.group(1),
		version=match.group(3),
		platform=match.group(4),
		architecture=match.group(8),
		timestamp=datetime.strptime(match.group(9), "%Y-%m-%d_%H-%M-%S"),
		commit=match.group(11),
	)


def determine_image_base(executable: Path) -> int:
	output = run_command(["objdump", "-x", str(executable)])
	match = IMAGE_BASE_PATTERN.search(output)
	if not match:
		raise RuntimeError("Could not determine ImageBase from objdump output. Make sure you installed GNU objdump.")
	return int(match.group(1), 16)


def determine_date_time(lines: list[str]) -> datetime:
	for line in lines:
		match = DATE_TIME_PATTERN.search(line)
		if match:
			return datetime.strptime(match.group(1), "%A, %B %d, %Y at %H:%M:%S")
	raise RuntimeError("Crash log does not contain the date and time of the crash.\nIt was likely not possible to determine a more detailed stack trace due to the severity of the crash.")


def determine_error_message(lines: list[str]) -> str:
	for line in lines:
		match = ERROR_MESSAGE_PATTERN.search(line)
		if match:
			return match.group(0)
	raise RuntimeError("Crash log does not contain the error message of the crash.\nIt was likely not possible to determine a more detailed stack trace due to the severity of the crash.")


def download_symbols_executable(parsed_filename: ParsedFilename | None) -> Path:
	print("Executable containing debug symbols was not specified. Trying to find it automatically...")

	if not parsed_filename:
		raise RuntimeError("Executable containing debug symbols cannot be identified, because the crash log filename has an unexpected format.\nMake sure to keep the original filename of the crash log, or specify the executable manually with the `--executable` parameter.")

	if parsed_filename.executable not in ["DDNet", "DDNet-Server"]:
		raise RuntimeError("Executable containing debug symbols cannot be identified. Only official releases of DDNet and DDNet-Server have downloadable debug symbols.\nSpecify the executable manually with the `--executable` parameter if you have it.")

	# TODO: 2027 or later: remove backwards compatibility for parsing filename without version and architecture
	if not parsed_filename.version:
		# TODO: Could implement search for correct folder. And parse HTML of https://ddnet.org/downloads/symbols/ to find correct symbols...
		raise RuntimeError("Executable containing debug symbols cannot be identified, because the crash log filename has an unsupported format: missing version string.\nMake sure to keep the original filename of the crash log, or specify the executable manually with the `--executable` parameter.")
	if not parsed_filename.architecture:
		print("Warning: The crash log filename does not specify the architecture, so we will assume it is not ARM64.")
		print("         Try manually using the ARM64 executable if you see unresolved symbols below.")

	# The platform optionally includes `-steam`, but Steam does not support the ARM64 release.
	if parsed_filename.architecture == "arm64":
		symbols_platform_string = "win-arm64"
	else:
		symbols_platform_string = parsed_filename.platform

	symbols_cache_path = Path(__file__).resolve().parent / "drmingw_symbols_cache"
	symbols_cache_path.mkdir(exist_ok=True)
	symbols_folder_name = f"DDNet-{parsed_filename.version}-{symbols_platform_string}-{parsed_filename.commit}-symbols"
	symbols_folder_path = symbols_cache_path / symbols_folder_name
	if not symbols_folder_path.is_dir():
		symbols_archive_name = f"{symbols_folder_name}.tar.xz"
		archive_path = symbols_cache_path / symbols_archive_name
		symbols_url = urllib.parse.urljoin("https://ddnet.org/downloads/symbols/", symbols_archive_name)
		print(f"Downloading symbols from {symbols_url}")
		try:
			urllib.request.urlretrieve(symbols_url, archive_path)
		except urllib.error.HTTPError as error:
			raise RuntimeError("Failed to download debug symbols. They are likely not available for this crash dump.\nCheck your internet connection and make sure the crash log has its original filename.") from error
		print(f"Extracting symbols to {symbols_folder_path.name}")
		symbols_folder_path.mkdir()
		try:
			with lzma.open(archive_path) as file:
				with tarfile.open(fileobj=file) as tar:
					for executable in ["DDNet.exe", "DDNet-Server.exe"]:
						tar.extract(executable, path=symbols_folder_path, filter="data")
		except Exception as error:
			shutil.rmtree(symbols_folder_path, ignore_errors=True)
			raise RuntimeError("Failed to extract debug symbols. The debug symbols archive may be corrupted.\nPlease report this error if it persists.") from error
		except BaseException as error:
			shutil.rmtree(symbols_folder_path, ignore_errors=True)
			raise error
		finally:
			archive_path.unlink()

	executable_name = f"{parsed_filename.executable}.exe"
	symbols_executable = symbols_folder_path / executable_name
	if not symbols_executable.is_file():
		raise RuntimeError(f"Folder for matching symbols '{symbols_folder_path}' was found in cache but it does not contain the executable '{executable_name}'.")
	print(f"Found matching symbols in cache: {symbols_executable.relative_to(symbols_executable.parent.parent)}")
	return symbols_executable


def symbolize_address(executable: Path, address: int) -> str:
	return run_command([
		"addr2line",
		"-e",
		str(executable),
		"-a",  # Show addresses
		"-p",  # Pretty print
		"-f",  # Show function names
		"-C",  # Demangle function names
		"-i",  # Unwind inlined functions
		f"0x{address:X}",
	])


def process_crash_log(crash_log: Path, executable: Path | None):
	print(f"Crash log: {crash_log.name}")
	try:
		with crash_log.open("r", encoding="utf-8") as f:
			lines = [line.strip() for line in f.readlines()]
	except OSError as error:
		raise RuntimeError(f"Crash log could not be opened: {crash_log}") from error

	parsed_filename = parse_crash_filename(crash_log.stem)
	if parsed_filename:
		print(f"  Executable: {parsed_filename.executable}")
		print(f"  Version: {parsed_filename.version or 'unknown'}")
		print(f"  Platform: {parsed_filename.platform}")
		print(f"  Architecture: {parsed_filename.architecture or 'unknown'}")
		print(f"  Git commit: {parsed_filename.commit}")
		print(f"  Launch time: {parsed_filename.timestamp}")
	else:
		print("  Warning: Crash log filename has unexpected format. Make sure to keep its original name.")
	print(f"  Crash time: {determine_date_time(lines)}")
	print(f"  Error message: {determine_error_message(lines)}")
	print()

	executable_error = None
	try:
		if not executable:
			executable = download_symbols_executable(parsed_filename)
		if not executable.is_file():
			raise RuntimeError(f"Executable not found: {executable}")
	except RuntimeError as error:
		# Continue and print the stack trace without symbols anyway because it might still be useful.
		print("Executable not found. Continuing with unsymbolized stack trace.")
		executable = None
		executable_error = error

	if executable:
		print()
		print(f"Executable: {executable.relative_to(executable.parent.parent)}")
		image_base = determine_image_base(executable)
		print(f"  Image base: 0x{image_base:X}")

	print()
	print("Stack trace:")
	print_verbatim = False
	found_unresolved_symbols = False
	for line in lines:
		stack_line_match = STACK_LINE_PATTERN.search(line)
		if not stack_line_match:
			if print_verbatim:
				if line:
					print(line)
				else:
					print_verbatim = False
			continue
		line = stack_line_match.group(1)

		# Detect if crash dump was created in debug mode, then print the lines following each stack line verbatim
		# as the crash log already contains a readable stack trace with function names and source code.
		if DEBUG_SYMBOL_PATTERN.search(line):
			print_verbatim = True
			print(line)
			continue
		print_verbatim = False

		# Read address relative to image base from crash log and symbolize it.
		if executable:
			executable_release_match = RELEASE_SYMBOL_PATTERN.search(line)
			if executable_release_match and executable_release_match.group(1) == executable.name:
				relative_addr = int(executable_release_match.group(2), 16)
				absolute_addr = image_base + relative_addr
				symbolized_addr = symbolize_address(executable, absolute_addr)
				if UNRESOLVED_SYMBOL_PATTERN.search(symbolized_addr):
					found_unresolved_symbols = True
				print(symbolized_addr)
				continue

		# Print the line verbatim if it could not be symbolized.
		print(line)

	if found_unresolved_symbols:
		print()
		print("Warning: Found unresolved symbols. Make sure to select the matching executable.")

	if executable_error:
		raise executable_error


def main():
	parser = argparse.ArgumentParser(description="Symbolize a crash log produced by Dr. Mingw ExcHndl.")
	parser.add_argument("crash_log", type=Path, help="Path to crash log file (.RTP)")
	parser.add_argument("--executable", type=Path, help="Path to executable with debug symbols (.exe)")
	parser.add_argument("--verbose", action="store_true", help="Print full stack trace on errors")
	args = parser.parse_args()
	try:
		process_crash_log(args.crash_log, args.executable)
	except RuntimeError as error:
		print()
		print("Script error:")
		print(error.args[0])
		if args.verbose:
			print()
			raise error
		print("Pass the `--verbose` parameter to print the full stack trace.")


if __name__ == "__main__":
	main()
