import binascii
import hashlib
import os
import re
import sys

AUTH_ADD_REGEX=re.compile(r'^\s*auth_add\s+(?P<username>"[^"]*"|[^"\s]+)\s+(?P<level>"[^"]*"|[^"\s]+)\s+(?P<password>"[^"]*"|[^"\s]+)\s*$')
AUTH_ADD_PRESENT_REGEX=re.compile(r'(^|\W)auth_add($|\W)')

def hash_password(password):
	salt = os.urandom(8)
	h = hashlib.md5()
	h.update(password.encode())
	h.update(salt)
	return h.hexdigest(), binascii.hexlify(salt).decode('ascii')

def auth_add_p_line(username, level, pwhash, salt):
	if level not in ('admin', 'mod', 'moderator', 'helper'):
		print("Warning: level ({}) not one of admin, mod and helper.".format(level), file=sys.stderr)
	if repr(level) != "'{}'".format(level):
		print("Warning: level ({}) contains weird symbols, config line is possibly malformed.".format(level), file=sys.stderr)
	if repr(username) != "'{}'".format(username):
		print("Warning: username ({}) contains weird symbols, config line is possibly malformed.".format(username), file=sys.stderr)
	return "auth_add_p {} {} {} {}".format(username, level, pwhash, salt)

def auth_add_p_line_from_pw(username, level, password):
	if len(password) < 8:
		print("Warning: password too short for a long-term password.", file=sys.stderr)
	pwhash, salt = hash_password(password)
	return auth_add_p_line(username, level, pwhash, salt)

def parse_line(line):
	m = AUTH_ADD_REGEX.match(line)
	if not m:
		if AUTH_ADD_PRESENT_REGEX.search(line):
			print("Warning: Funny-looking line with 'auth_add', not touching it:")
			print(line, end="")
		return
	password = m.group('password')
	if password.startswith('"'):
		password = password[1:-1] # Strip quotes.
	return m.group('username'), m.group('level'), password

def main():
	import argparse
	import tempfile

	p = argparse.ArgumentParser(description="Hash passwords in a way suitable for DDNet configs.")
	p.add_argument('--new', '-n', nargs=3, metavar=("USERNAME", "LEVEL", "PASSWORD"), action='append', default=[], help="username, level and password of the new user")
	p.add_argument('config', nargs='?', metavar="CONFIG", help="config file to update.")
	args = p.parse_args()
	if not args.new and args.config is None:
		p.error("expected at least one of --new and CONFIG")

	use_stdio = args.config is None or args.config == '-'
	if use_stdio:
		if args.config is None:
			input_file = open(os.devnull)
		else:
			input_file = sys.stdin
		output_file = sys.stdout
	else:
		input_file = open(args.config)
		output_file = tempfile.NamedTemporaryFile('w', dir=os.getcwd(), prefix="{}.".format(args.config), delete=False)

	for line in input_file:
		parsed = parse_line(line)
		if parsed is None:
			print(line, end="", file=output_file)
		else:
			print(auth_add_p_line_from_pw(*parsed), file=output_file)

	for auth_tuple in args.new:
		print(auth_add_p_line_from_pw(*auth_tuple), file=output_file)

	if not use_stdio:
		input_file.close()
		output_filename = output_file.name
		output_file.close()
		os.rename(output_filename, args.config)

if __name__ == '__main__':
	main()
