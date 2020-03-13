#!/usr/bin/env python3
import re
import socket

import requests

def escape(text):
    text = re.sub(r'([_\\~|\*`])', r'\\\1', text)  # escape markdown
    return text.replace('@', '@\u200b')  # escape mentions

def run(port, url):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(('127.0.0.1', port))

    while True:
        try:
            data, _ = sock.recvfrom(256)
        except KeyboardInterrupt:
            return sock.close()

        try:
            server, user, key, cmd = data.decode().split('\t')
        except ValueError:
            continue

        js = {
            'username': server,
            'content': escape('{!r} ({}) used command {!r}'.format(user, key, cmd))
        }

        requests.post(url, data=js)

def main():
    import argparse
    parser = argparse.ArgumentParser(description='Discord Mod Log')
    parser.add_argument('--port', required=True, type=int, help='Port to listen on')
    parser.add_argument('--url', required=True, help='URL of the Discord Webhook')
    args = parser.parse_args()

    run(args.port, args.url)

if __name__ == '__main__':
    main()
