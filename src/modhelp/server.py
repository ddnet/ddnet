from flask import Flask, make_response, request
import http
import re
import requests

SERVER=None
# Generate one by right-clicking on the server icon in the sidebar, clicking on
# "Server Settings" → "Webhooks" → "Create Webhook". You can then select the
# channel in which the messages should appear. Copy the "Webhook URL" to the
# following config variable:
# DISCORD_WEBHOOK="https://discordapp.com/api/webhooks/.../..."
DISCORD_WEBHOOK=None

app = Flask(__name__)

def sanitize(s):
    return re.sub(r"([^\0- 0-9A-Za-z])", r"\\\1", s)

def no_content():
    return make_response("", http.HTTPStatus.NO_CONTENT)

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
    message = "<{player_id}:{player_name}> {message}".format(**json)

    if DISCORD_WEBHOOK:
        try:
            requests.post(DISCORD_WEBHOOK, data={"username": user, "content": sanitize(message)}).raise_for_status()
        except requests.HTTPError as e:
            print(repr(e))
            raise
    return no_content()
