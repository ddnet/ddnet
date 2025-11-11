#!/usr/bin/env python3

# Simple web server that sends CORS headers to test the Emscripten build.
# From https://stackoverflow.com/a/21957017

from http.server import HTTPServer, SimpleHTTPRequestHandler, test
import sys

class RequestHandler(SimpleHTTPRequestHandler):
	def end_headers(self):
		self.send_header('Cross-Origin-Embedder-Policy', 'require-corp')
		self.send_header('Cross-Origin-Opener-Policy', 'same-origin')
		SimpleHTTPRequestHandler.end_headers(self)

if __name__ == '__main__':
	test(RequestHandler, HTTPServer, port=int(sys.argv[1]) if len(sys.argv) > 1 else 8000)
