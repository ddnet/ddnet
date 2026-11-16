// Runs the server compiled with Emscripten in a Cloudflare Durable Object.
//
// Clients connect with secure WebSockets to the Worker, for example the web
// client with `wss://<worker host>:443`. Every path of the URL is a separate
// server in its own Durable Object, the root path is the server `default`.
//
// - The files in the `files` directory (see wrangler.toml) are copied into
//   the working directory of the server when it starts, for example
//   `files/autoexec_server.cfg` and the maps in `files/data/maps`.
// - The variable or secret `DDNET_COMMANDS` contains additional console
//   commands that are executed when the server starts, separated by `;`.
// - The home directory, which contains the user directory with the ranks
//   database, is stored in the Durable Object storage.
//
// The server keeps running while clients are connected and starts again
// when a client connects after the Durable Object was evicted.

import {DurableObject} from 'cloudflare:workers';

import createServer from './DDNet-Server.mjs';
import wasmModule from './DDNet-Server.wasm';

const WORKING_DIRECTORY = '/ddnet';
const HOME_DIRECTORY = '/home/ddnet';
const DEFAULT_STORAGE_CFG = 'add_path $USERDIR\nadd_path $DATADIR\nadd_path $CURRENTDIR\n';
const PERSIST_INTERVAL_MS = 10 * 1000;
const PERSIST_CHUNK_SIZE = 1024 * 1024;

function dirname(path) {
	return path.slice(0, path.lastIndexOf('/')) || '/';
}

export class DDNetServer extends DurableObject {
	constructor(ctx, env) {
		super(ctx, env);
		// Promise of the function which accepts WebSockets, while the server is starting or running
		this.accept = null;
		this.module = null;
		this.persistInterval = null;
		// Size and modification time of the persisted files
		this.persistedFiles = new Map();
		this.ctx.storage.sql.exec('CREATE TABLE IF NOT EXISTS files (path TEXT NOT NULL, chunk INTEGER NOT NULL, data BLOB NOT NULL, PRIMARY KEY (path, chunk))');
	}

	async fetch(request) {
		if (request.headers.get('Upgrade')?.toLowerCase() != 'websocket') {
			return new Response('This is a DDNet server, connect to it with a client that supports WebSockets.\n', {status: 426});
		}
		let accept;
		try {
			accept = await this.start();
		} catch (error) {
			return new Response(`The server could not be started: ${error instanceof Error ? error.message : error}\n`, {status: 503});
		}
		const {0: client, 1: server} = new WebSocketPair();
		server.accept();
		accept(server, request.headers.get('CF-Connecting-IP') ?? '127.0.0.1');
		const protocols = (request.headers.get('Sec-WebSocket-Protocol') ?? '').split(',').map((protocol) => protocol.trim());
		const headers = new Headers();
		if (protocols.includes('binary')) {
			headers.set('Sec-WebSocket-Protocol', 'binary');
		}
		return new Response(null, {status: 101, webSocket: client, headers});
	}

	start() {
		this.accept ??= this.run().catch((error) => {
			this.accept = null;
			throw error;
		});
		return this.accept;
	}

	async run() {
		const files = await this.loadFiles();
		return new Promise((resolve, reject) => {
			createServer({
				arguments: this.env.DDNET_COMMANDS ? [this.env.DDNET_COMMANDS] : [],
				instantiateWasm(imports, receiveInstance) {
					const instance = new WebAssembly.Instance(wasmModule, imports);
					receiveInstance(instance, wasmModule);
					return instance.exports;
				},
				preRun: [(module) => {
					this.module = module;
					this.prepareFilesystem(module, files);
				}],
				websocketServer: {
					listen: (port, accept) => {
						this.persistInterval = setInterval(() => this.persist(), PERSIST_INTERVAL_MS);
						resolve(accept);
					},
				},
				onExit: (status) => {
					this.stop();
					reject(new Error(`exit status ${status}`));
				},
				onAbort: (reason) => {
					this.stop();
					reject(new Error(`aborted: ${reason}`));
				},
			}).catch(reject);
		});
	}

	stop() {
		clearInterval(this.persistInterval);
		this.persistInterval = null;
		if (this.module) {
			this.persist();
			this.module = null;
		}
		this.accept = null;
	}

	async loadFiles() {
		const files = new Map();
		if (!this.env.FILES) {
			return files;
		}
		const index = await this.env.FILES.fetch('https://files/files.json');
		if (!index.ok) {
			console.warn('files/files.json not found, no files are copied to the server');
			return files;
		}
		await Promise.all((await index.json()).map(async (path) => {
			const response = await this.env.FILES.fetch(`https://files/${path.split('/').map(encodeURIComponent).join('/')}`);
			if (response.ok) {
				files.set(path, new Uint8Array(await response.arrayBuffer()));
			} else {
				console.warn(`files/${path} could not be loaded: ${response.status}`);
			}
		}));
		return files;
	}

	prepareFilesystem(module, files) {
		const {FS, ENV} = module;
		for (const [path, data] of files) {
			const fullPath = `${WORKING_DIRECTORY}/${path}`;
			FS.mkdirTree(dirname(fullPath));
			FS.writeFile(fullPath, data);
		}
		// Required to find the data directory
		FS.mkdirTree(`${WORKING_DIRECTORY}/data/mapres`);
		if (!files.has('storage.cfg')) {
			FS.writeFile(`${WORKING_DIRECTORY}/storage.cfg`, DEFAULT_STORAGE_CFG);
		}
		FS.chdir(WORKING_DIRECTORY);
		ENV.HOME = HOME_DIRECTORY;
		FS.mkdirTree(HOME_DIRECTORY);
		this.restore(FS);
	}

	// Restores the files in the home directory from the Durable Object storage.
	restore(FS) {
		const chunks = new Map();
		for (const {path, data} of this.ctx.storage.sql.exec('SELECT path, data FROM files ORDER BY path, chunk')) {
			chunks.set(path, [...(chunks.get(path) ?? []), new Uint8Array(/** @type {ArrayBuffer} */ (data))]);
		}
		for (const [path, parts] of chunks) {
			const data = new Uint8Array(parts.reduce((size, part) => size + part.length, 0));
			parts.reduce((offset, part) => {
				data.set(part, offset);
				return offset + part.length;
			}, 0);
			FS.mkdirTree(dirname(path));
			FS.writeFile(path, data);
			const mtime = FS.stat(path).mtime.getTime();
			this.persistedFiles.set(path, {size: data.length, mtime, time: mtime + 1});
		}
	}

	// Stores the changed files of the home directory in the Durable Object storage.
	persist() {
		const {FS} = this.module;
		const files = new Map();
		const walk = (directory) => {
			for (const name of FS.readdir(directory)) {
				const path = `${directory}/${name}`;
				if (name == '.' || name == '..' || name.endsWith('-shm')) {
					// The SQLite shared memory file is recreated from the write-ahead log
					continue;
				}
				const stat = FS.stat(path);
				if (FS.isDir(stat.mode)) {
					walk(path);
				} else if (FS.isFile(stat.mode)) {
					files.set(path, stat);
				}
			}
		};
		walk(HOME_DIRECTORY);

		const time = Date.now();
		const sql = this.ctx.storage.sql;
		this.ctx.storage.transactionSync(() => {
			for (const [path, stat] of files) {
				const mtime = stat.mtime.getTime();
				const persisted = this.persistedFiles.get(path);
				// The time does not advance while code runs in Workers, so files modified at the time of the last check may have changed after it
				if (persisted && persisted.size == stat.size && persisted.mtime == mtime && mtime < persisted.time) {
					continue;
				}
				const data = FS.readFile(path);
				sql.exec('DELETE FROM files WHERE path = ?', path);
				let chunk = 0;
				do {
					sql.exec('INSERT INTO files (path, chunk, data) VALUES (?, ?, ?)', path, chunk, data.slice(chunk * PERSIST_CHUNK_SIZE, (chunk + 1) * PERSIST_CHUNK_SIZE).buffer);
					chunk++;
				} while (chunk * PERSIST_CHUNK_SIZE < data.length);
				this.persistedFiles.set(path, {size: stat.size, mtime, time});
			}
			for (const path of this.persistedFiles.keys()) {
				if (!files.has(path)) {
					sql.exec('DELETE FROM files WHERE path = ?', path);
					this.persistedFiles.delete(path);
				}
			}
		});
	}
}

export default {
	async fetch(request, env) {
		const name = decodeURIComponent(new URL(request.url).pathname.slice(1)) || 'default';
		return env.DDNET_SERVER.get(env.DDNET_SERVER.idFromName(name)).fetch(request);
	},
};
