from hashlib import sha256
import argparse
import base64
import binascii
import secrets


def _crc32(bytes_):
	return binascii.crc32(bytes_).to_bytes(4, "big")


def _add_crc32(s):
	return s + _urlsafe_encode(_crc32(s.encode("ascii")))


def _urlsafe_encode(bytes_):
	return base64.urlsafe_b64encode(bytes_).decode("ascii").rstrip("=")


def _generate_token_impl():
	token = secrets.token_urlsafe(22)
	shared_prefix = token[:6]
	user_token = _add_crc32(f"ddtc_{token}")

	half_sha256 = _urlsafe_encode(sha256(user_token.encode("ascii")).digest()[:16])
	mastersrv_token = _add_crc32(f"ddvc_{shared_prefix}{half_sha256}")
	return user_token, mastersrv_token


def _unwanted_token(token):
	return "-" in token or token.count("_") > 1


def generate_token(verbose=False):
	while True:
		if verbose:
			print(".", end="", flush=True)
		user_token, mastersrv_token = _generate_token_impl()
		if _unwanted_token(user_token) or _unwanted_token(mastersrv_token):
			continue
		return user_token, mastersrv_token


def main():
	parser = argparse.ArgumentParser(description="Generate a community token for use with the mastersrv")
	parser.add_argument("-v", "--verbose", action="store_true", help="Show generation tries")
	args = parser.parse_args()

	user_token, mastersrv_token = generate_token(verbose=args.verbose)
	if args.verbose:
		print()
	print(f"user:      {user_token}")
	print(f"mastersrv: {mastersrv_token}")


if __name__ == "__main__":
	main()
