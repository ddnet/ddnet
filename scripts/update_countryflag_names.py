#!/usr/bin/env python3

"""Fills in the English country names in the country flags index file.

Every entry of `data/countryflags/index.txt` looks like this:

    code
    == <ISO 3166-1 numeric>
    == <English name>

The names are not maintained by hand, they are looked up by country code in the
Unicode CLDR display names for English, which is the same source the client uses
for all of its other English user interface strings. Only the handful of codes
that CLDR does not know, or knows under a name that does not fit a country
selection menu, are listed in `NAME_OVERRIDES`.
"""

from __future__ import annotations

from collections.abc import Iterable
import argparse
import difflib
import json
import os
import re
import sys
import urllib.error
import urllib.request

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
INDEX_PATH = os.path.join("data", "countryflags", "index.txt")

# Pinned, so that regenerating the file twice in a row cannot change it.
CLDR_URL = "https://unpkg.com/cldr-localenames-full@47.0.0/main/en/territories.json"

# `m_aCountryName` in src/game/client/components/countryflags.h
MAX_NAME_BYTES = 64

NAME_OVERRIDES = {
	# CLDR has no ISO 3166-2 subdivisions, and the names it uses for these two
	# Catalan ones are not the ones that the flags in this directory show.
	"ES-CT": "Catalonia",
	"ES-GA": "Galicia",
	# CLDR knows this one, but listing it keeps the exceptional reservations of
	# the index file together in one obvious place.
	"EU": "European Union",
	"GB-ENG": "England",
	"GB-NIR": "Northern Ireland",
	"GB-SCT": "Scotland",
	"GB-WLS": "Wales",
	# Not a country at all, this is the fallback of the country selection.
	"default": "default",
}

FORMAT_DOC = [
	"# Format for each country entry:",
	'# 1. country code (ISO 3166-1 alpha-2, an ISO 3166-2 subdivision, or "default")',
	"# 2. country code (ISO 3166-1 numeric)",
	"# 3. name",
]

# Codes are ISO 3166-1 alpha-2, ISO 3166-2 subdivisions, or the "default" entry.
# Deliberately stricter than the client, which only checks for letters and dashes.
CODE_RE = re.compile(r"(?:[A-Za-z]{2}(?:-[A-Za-z0-9]{1,3})?|default)")
INTEGER_RE = re.compile(r"-?[0-9]+")


def find_entries(lines: list[str]) -> Iterable[tuple[int, str, str]]:
	"""Yield the line index, the comment prefix and the code of every country entry."""
	index = 0
	while index < len(lines):
		entry = parse_code_line(lines[index])
		# An entry is a code line directly followed by its `== ` value line.
		if entry is not None and lines[index + 1 : index + 2] and lines[index + 1].startswith(entry[0] + "== "):
			yield index, entry[0], entry[1]
			index += 2
		else:
			index += 1


def parse_code_line(line: str) -> tuple[str, str] | None:
	"""Split a line into its comment prefix and its code, unless it is not a code line."""
	if line.startswith("##"):  # Section header
		return None
	prefix = "#" if line.startswith("#") else ""
	code = line[len(prefix) :]
	if CODE_RE.fullmatch(code):
		return prefix, code
	return None


def add_names(lines: list[str], names: dict[str, str]) -> list[str]:
	"""Return the lines of the index file with a name line added to every country entry."""
	additions = {}
	removals = set()
	for index, prefix, code in find_entries(lines):
		additions[index + 1] = prefix + "== " + names[code]
		# Drop the name line of a previous run of this script.
		following = lines[index + 2 : index + 3]
		if following and following[0].startswith(prefix + "== ") and not INTEGER_RE.fullmatch(following[0][len(prefix) + 3 :]):
			removals.add(index + 2)
	result = []
	for index, line in enumerate(lines):
		if index in removals and index not in additions:
			continue
		result.append(line)
		if index in additions:
			result.append(additions[index])
	return result


def add_format_doc(lines: list[str]) -> list[str]:
	"""Return the lines of the index file with the documentation of its format in front of the first section."""
	header = next((index for index, line in enumerate(lines) if line.startswith("#####")), None)
	if header is None:
		return lines
	if lines[max(0, header - len(FORMAT_DOC) - 1) : header] == FORMAT_DOC + [""]:
		return lines
	return lines[:header] + FORMAT_DOC + [""] + lines[header:]


def resolve_names(codes: Iterable[str], territories: dict[str, str]) -> dict[str, str]:
	"""Look up the English name of every country code, reporting all problems at once."""
	names = {}
	problems = []
	for code in dict.fromkeys(codes):  # Keep the order of the file, but do not report a code twice
		name = NAME_OVERRIDES.get(code) or territories.get(code)
		if name is None:
			problems.append(f"{code}: no name in the CLDR display names and no override")
			continue
		if INTEGER_RE.fullmatch(name):
			problems.append(f"{code}: name {name!r} is a number, so it cannot be told apart from a country code")
			continue
		if any(character < " " for character in name):
			problems.append(f"{code}: name {name!r} contains a control character")
			continue
		if len(name.encode("utf-8")) >= MAX_NAME_BYTES:
			problems.append(f"{code}: name {name!r} does not fit into {MAX_NAME_BYTES} bytes")
			continue
		names[code] = name
	if problems:
		raise ValueError(f"cannot resolve {len(problems)} country codes:\n" + "\n".join(problems))
	return names


def fetch_territories(url: str, cache: str | None) -> dict[str, str]:
	"""Download the English territory display names of the Unicode CLDR."""
	if cache is not None and os.path.exists(cache):
		with open(cache, encoding="utf-8") as cache_file:
			data = cache_file.read()
	else:
		with urllib.request.urlopen(url) as response:  # Only ever reached with `--url` pointing somewhere else on purpose
			data = response.read().decode("utf-8")
		if cache is not None:
			with open(cache, "w", encoding="utf-8") as cache_file:
				cache_file.write(data)
	return json.loads(data)["main"]["en"]["localeDisplayNames"]["territories"]


def update(text: str, names: dict[str, str]) -> str:
	"""Return the index file with the names of all country entries filled in."""
	newline = "\r\n" if "\r\n" in text else "\n"
	lines = text.replace("\r\n", "\n").split("\n")
	return newline.join(add_format_doc(add_names(lines, names)))


def main() -> int:
	parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0], formatter_class=argparse.RawDescriptionHelpFormatter)
	parser.add_argument("path", nargs="?", default=os.path.join(REPO_ROOT, INDEX_PATH), help=f"index file to update (default: {INDEX_PATH})")
	parser.add_argument("--url", default=CLDR_URL, help="URL of the CLDR territories.json to use")
	parser.add_argument("--cache", help="read the CLDR territories.json from this path, and write it there when it is downloaded")
	parser.add_argument("--check", action="store_true", help="only report whether the index file is up to date, do not write it")
	args = parser.parse_args()

	with open(args.path, encoding="utf-8", newline="") as index_file:
		text = index_file.read()

	lines = text.replace("\r\n", "\n").split("\n")
	names = resolve_names((code for _, _, code in find_entries(lines)), fetch_territories(args.url, args.cache))
	updated = update(text, names)
	if updated == text:
		print(f"{args.path} is up to date ({len(names)} country names)")
		return 0
	if args.check:
		print(f"{args.path} is not up to date:")
		print("".join(difflib.unified_diff(text.splitlines(keepends=True), updated.splitlines(keepends=True), fromfile=args.path, tofile=f"{args.path} (updated)")).rstrip())
		return 1

	with open(args.path, "w", encoding="utf-8", newline="") as index_file:
		index_file.write(updated)
	print(f"Updated {args.path} ({len(names)} country names)")
	return 0


if __name__ == "__main__":
	try:
		sys.exit(main())
	except (OSError, ValueError, KeyError, urllib.error.URLError) as error:
		sys.exit(f"error: {error}")
