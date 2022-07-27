import argparse
import csv
import sys

def check_name(kind, qualifiers, typ, name):
	if kind == "variable":
		return check_variable_name(qualifiers, typ, name)
	if kind in "class struct".split():
		if name[0] not in "CI":
			return "should start with 'C' (or 'I' for interfaces)"
		if len(name) < 2:
			return "must be at least two characters long"
		if not name[1].isupper():
			return "must start with an uppercase letter"
	if kind == "enum_constant":
		if not name.isupper():
			return "must only contain uppercase letters, digits and underscores"
	return None

ALLOW = set("""
	dx dy
	fx fy
	mx my
	ix iy
	px py
	sx sy
	wx wy
	x0 x1
	y0 y1
""".split())

def check_variable_name(qualifiers, typ, name):
	if qualifiers == "" and typ == "" and name == "argc":
		return None
	if qualifiers == "" and typ == "pp" and name == "argv":
		return None
	if qualifiers == "cs":
		# Allow all uppercase names for constant statics.
		if name.isupper():
			return None
		qualifiers = "s"
	# Allow single lowercase letters as member and variable names.
	if qualifiers in ["m", ""] and len(name) == 1 and name.islower():
		return None
	prefix = "".join([qualifiers, "_" if qualifiers else "", typ])
	if not name.startswith(prefix):
		return f"should start with {prefix!r}"
	if name in ALLOW:
		return None
	name = name[len(prefix):]
	if not name[0].isupper():
		if prefix:
			return f"should start with an uppercase letter after the prefix {prefix!r}"
		return "should start with an uppercase letter"
	return None

def main():
	p = argparse.ArgumentParser(description="Check identifiers (input via stdin in CSV format from extract_identifiers.py) for naming style in DDNet code")
	p.parse_args()

	identifiers = list(csv.DictReader(sys.stdin))

	unclean = False
	for i in identifiers:
		error = check_name(i["kind"], i["qualifiers"], i["type"], i["name"])
		if error:
			unclean = True
			print(f"{i['file']}:{i['line']}:{i['column']}: {i['name']}: {error}")
	return unclean

if __name__ == "__main__":
	sys.exit(main())
