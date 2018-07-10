#!/usr/bin/env python3
from flask import Flask, make_response, request
import http
import os
import re
import requests

SERVER=os.getenv('MODHELP_SERVER')
# Generate one by right-clicking on the server icon in the sidebar, clicking on
# "Server Settings" → "Webhooks" → "Create Webhook". You can then select the
# channel in which the messages should appear. Copy the "Webhook URL" to the
# following config variable:
# DISCORD_WEBHOOK="https://discordapp.com/api/webhooks/.../..."
DISCORD_WEBHOOK=os.getenv('MODHELP_DISCORD_WEBHOOK')
DISCORD_MESSAGE=os.getenv('MODHELP_DISCORD_FORMAT', "<{player_id}:**{player_name}**> {message}")
DISCORD_MESSAGE_PREFIX=os.getenv('MODHELP_DISCORD_PREFIX')

app = Flask(__name__)

def sanitize(s):
    return re.sub(r"([^\0- 0-9A-Za-z])", r"\\\1", s)

def no_content():
    return make_response("", http.HTTPStatus.NO_CONTENT)

def sanitize_string_values(dictionary):
    return {k: v if not isinstance(v, str) else sanitize(v) for k, v in dictionary.items()}

@app.route("/modhelp", methods=['POST'])
def modhelp():
    json = request.get_json()

    if "server" not in json:
        if SERVER:
            json["server"] = SERVER

    if "server" not in json:
        user = "{port}".format(**json)
    else:
        user = "{server}:{port}".format(**json)

    discord_prefix = DISCORD_MESSAGE_PREFIX + " " if DISCORD_MESSAGE_PREFIX is not None else ""
    discord_message = discord_prefix + DISCORD_MESSAGE.format(**sanitize_string_values(json))

    if DISCORD_WEBHOOK:
        try:
            requests.post(DISCORD_WEBHOOK, data={"username": user, "content": discord_message}).raise_for_status()
        except requests.HTTPError as e:
            print(repr(e))
            raise
    return no_content()
