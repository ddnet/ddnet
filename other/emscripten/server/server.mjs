// Runs the server compiled with Emscripten using Node.js.
//
// Clients connect with WebSockets to the server port (sv_port), for example
// the web client with `ws://<host>:<port>`. Native clients cannot connect, as
// UDP is not supported.
//
// Install the dependencies with `npm install --omit=dev` and start the server
// with `node server.mjs [arguments]`. The arguments are passed to the server.
// The server uses the current directory and the user directory like the
// native server, so `storage.cfg`, `autoexec_server.cfg` and the `data`
// directory work the same way.
//
// Environment variables:
// - DDNET_TLS_CERT and DDNET_TLS_KEY: Paths of a certificate and its private
//   key to accept secure WebSocket connections (`wss://`), which are required
//   by clients served via HTTPS.
// - DDNET_TRUST_PROXY: Set to `1` to use the client address from the
//   `X-Forwarded-For` header when the server is behind a reverse proxy.

import fs from 'node:fs';
import http from 'node:http';
import https from 'node:https';
import process from 'node:process';
import {fileURLToPath} from 'node:url';
import {WebSocketServer} from 'ws';

import createServer from './DDNet-Server.mjs';

const SIGNALS = {SIGINT: 2, SIGTERM: 15};

const {DDNET_TLS_CERT: tlsCertPath, DDNET_TLS_KEY: tlsKeyPath} = process.env;
if (!tlsCertPath != !tlsKeyPath) {
	console.error('DDNET_TLS_CERT and DDNET_TLS_KEY must both be set to accept secure WebSocket connections');
	process.exit(1);
}
const tlsOptions = tlsCertPath && tlsKeyPath ? {
	cert: fs.readFileSync(tlsCertPath),
	key: fs.readFileSync(tlsKeyPath),
} : null;
const trustProxy = process.env.DDNET_TRUST_PROXY == '1';
const httpServers = new Map();

function remoteAddress(request) {
	const forwardedFor = request.headers['x-forwarded-for'];
	if (trustProxy && forwardedFor) {
		return forwardedFor.split(',')[0].trim();
	}
	return request.socket.remoteAddress;
}

const websocketServer = {
	listen(port, accept) {
		const httpServer = tlsOptions ? https.createServer(tlsOptions) : http.createServer();
		httpServer.on('request', (request, response) => {
			response.writeHead(426, {'Content-Type': 'text/plain'});
			response.end('This is a DDNet server, connect to it with a client that supports WebSockets.\n');
		});
		httpServer.on('error', (error) => {
			console.error(`websockets: failed to listen on port ${port}: ${error.message}`);
			process.exit(1);
		});
		// Emscripten's UDP emulation requests the "binary" subprotocol, which is selected by default
		const webSocketServer = new WebSocketServer({server: httpServer, maxPayload: 64 * 1024});
		webSocketServer.on('connection', (webSocket, request) => accept(webSocket, remoteAddress(request)));
		httpServer.listen(port);
		httpServers.set(port, httpServer);
		console.log(`websockets: listening on ${tlsOptions ? 'wss' : 'ws'}://[::]:${port}`);
	},
	close(port) {
		httpServers.get(port)?.close();
		httpServers.delete(port);
	},
};

// Mount the host filesystem at the same paths, so the server can access files
// like the native server does.
function mountHostFilesystem(FS) {
	if (process.platform == 'win32') {
		FS.mkdir('/ddnet');
		FS.mount(FS.filesystems.NODEFS, {root: process.cwd()}, '/ddnet');
		FS.chdir('/ddnet');
		return;
	}
	for (const entry of fs.readdirSync('/')) {
		const path = `/${entry}`;
		if (['dev', 'proc', 'sys'].includes(entry) || !fs.statSync(path, {throwIfNoEntry: false})?.isDirectory()) {
			continue;
		}
		if (!FS.analyzePath(path).exists) {
			FS.mkdir(path);
		}
		FS.mount(FS.filesystems.NODEFS, {root: path}, path);
	}
	FS.chdir(process.cwd());
}

const server = await createServer({
	arguments: process.argv.slice(2),
	thisProgram: fileURLToPath(new URL('DDNet-Server.mjs', import.meta.url)),
	websocketServer,
	preRun: [(module) => {
		mountHostFilesystem(module.FS);
		for (const name of ['HOME', 'XDG_DATA_HOME']) {
			if (process.env[name] !== undefined && process.platform != 'win32') {
				module.ENV[name] = process.env[name];
			}
		}
	}],
	onExit: (status) => process.exit(status),
	onAbort: () => process.exit(1),
});

// Request a graceful shutdown like the native server, signals received again terminate it immediately
for (const [name, number] of Object.entries(SIGNALS)) {
	process.on(name, () => server._raise(number));
}
