# Playwright harness for the emscripten client

Drives the DDNet emscripten (WebAssembly) client in headless Chromium:
scripted UI/gameplay scenarios with screenshots, usable interactively for
exploratory testing and as regression tests in CI.

## Setup

```sh
npm install
npx playwright install chromium
```

Serve an emscripten client build (the regular build, not `HEADLESS_CLIENT` —
that one has no graphics) with the COOP/COEP headers the client needs:

```sh
cd <dir with index.html, DDNet.js, DDNet.wasm, DDNet.data>
python3 other/emscripten/server.py 8000
```

Without a local build, download the `ddnet-emscripten` artifact of a CI run
(`gh run download <run-id> --repo ddnet/ddnet --name ddnet-emscripten`) and
serve the extracted directory. Note: client.ddnet.org rejects automated
clients with 403, so always test against a locally served build.

## Replaying scenarios

```sh
node run-scenario.js scenarios/menu-settings.json [--url http://localhost:8000]
```

A scenario is a JSON file with startup console commands (`args`) and a list of
`steps`. Each `shot` step saves a screenshot to `out/<scenario>/<name>.png`.
If `goldens/<scenario>/<name>.png` exists, the screenshot is compared against
it (`maxDiffPct` per step, default 1.0; `"compare": false` for volatile views
such as ingame footage with timers — those still verify that the steps and
`waitlog` assertions succeed). Any failed step or mismatch exits non-zero,
with a `.diff.png` written next to the actual image.

Create or refresh goldens locally with `--update-golden`. Goldens used by CI
must be rendered by CI itself (rasterization differs between hosts): after a
run that uploaded the `ddnet-ui-test-screenshots` artifact, seed them with

```sh
node seed-goldens.js <run-id>
```

and commit the `goldens/` directory. Each run writes `out/<scenario>/report.html`
into the artifact with side-by-side golden/actual/diff images for review, and
prints a per-shot table into the GitHub Actions job summary.

`scenarios/gameplay-local-server.json` needs a websocket-enabled server on
port 8303: build with `-DWEBSOCKETS=ON`, then run `DDNet-Server "sv_register 0"`.
The client connects with a plain `ip:port` address — emscripten's socket
emulation tunnels the UDP traffic over a WebSocket to the same port.

## Recording new scenarios

```sh
node repl-driver.js [--url http://localhost:8000]
```

Boots the client to the main menu and then executes JSON commands appended to
`cmds.jsonl`, one per line, e.g.:

```sh
echo '{"id":"01","action":"click","x":1196,"y":26}' >> cmds.jsonl
```

After each command it writes `shots/<id>.png` and `shots/<id>.done.json`, so
you (or an agent) can inspect the result before choosing the next command.
Every successful command is also appended to `record.jsonl` — curate that into
`scenarios/<name>.json` (drop exploratory dead ends, add `shot` steps with
meaningful names). End the session with `{"id":"zz","action":"quit"}`.

Available actions (`lib.js`): `key`, `keydown`, `keyup`, `type`, `move`,
`click`, `down`, `up`, `wheel` (mouse buttons: `+fire`/`+hook` defaults work
ingame), `console` (runs a command via the F1 console), `connect` (joins a
server and waits until fully loaded — use this instead of a `console` connect,
whose console toggle races with map loading), `wait`, `waitlog` (assert on
client log output), `shot`, `quit`.

## Environment notes

- All coordinates are game-screen pixels at the fixed 1280x720 viewport. The
  client only consumes *relative* mouse input (at 2x scale, from the 200
  mouse sensitivity defaults); `lib.js` makes clicks deterministic by clamping
  the game cursor to the top-left corner before each positioning move.
- The client log (`Module.print`) is captured from the browser console; use
  `waitlog` steps as functional assertions (connects, chat round-trips, ...).
- Master servers and skin/community downloads are unreachable under the COOP/
  COEP headers from a localhost origin; the server browser stays empty. Menu
  screenshots are made deterministic via `cl_menu_map ""` in scenario `args`.
- Headless Chromium rejects pointer-lock requests, which logs recurring
  `PAGEERROR: ... not valid for pointer lock` lines; input works regardless.
- The F1 console toggle is swallowed while a map is loading — close the
  console only after loading finished.
- To test IndexedDB persistence across reloads, use
  `chromium.launchPersistentContext(userDataDir)` and two runs of the same
  profile; on clean quit the client saves the config and syncs IDBFS.
