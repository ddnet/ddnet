#!/usr/bin/env python3
# Python webhook to discord

import requests
import keys
import sys

def send_discord(message):
    requests.post("https://discordapp.com/api/webhooks/" + keys.webhook, data={"content": message})

if len(sys.argv) != 2:
    print('usage: ' + str(sys.argv[0]) + ' message')
    exit()

send_discord(sys.argv[1])
