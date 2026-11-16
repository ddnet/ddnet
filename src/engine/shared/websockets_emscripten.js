// WebSocket server for the Emscripten build of the server, see websockets_emscripten.cpp.
//
// Every WebSocket connection is treated like a UDP peer: each binary message
// contains one packet. This is compatible with the native server built with
// WebSockets support and with the web client, which uses the WebSocket
// emulation of UDP sockets provided by Emscripten.
//
// The host, i.e. the scripts in other/emscripten/server, must set
// `Module.websocketServer` to an object with the following functions:
//
// - `listen(port, accept)`: Called when the server opens a socket on the port.
//   The host must call `accept(webSocket, remoteAddress)` for every WebSocket
//   connection to the port, where `webSocket` is a `WebSocket` (or an object
//   with the same interface, like the ones of the `ws` package) and
//   `remoteAddress` is the IP address of the client as string.
// - `close(port)`: Called when the server closes the socket.

addToLibrary({
	$WebsocketServer: {
		// Maximum number of packets queued per socket before new ones are dropped.
		MAX_QUEUED_PACKETS: 16384,
		// Maximum number of bytes buffered per connection before packets are dropped.
		MAX_BUFFERED_BYTES: 1024 * 1024,
		// The first message sent by Emscripten's UDP emulation contains the port of the client.
		PORT_MESSAGE: [255, 255, 255, 255, 112, 111, 114, 116],
		// Control message which tells the server that the client closed the connection.
		CLOSE_PACKET: new Uint8Array([0x10, 0x0e, 0x00, 0x04]),

		sockets: [],
		connections: new Map(),
		listeningPorts: new Map(),
		nextPort: 1,
		wakeUp: null,

		parseAddress(address) {
			address = String(address).replace(/^\[|\]$/g, '').replace(/%.*$/, '');
			const ipv4Match = address.match(/^(?:::ffff:)?(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})$/i);
			if (ipv4Match) {
				const ip = new Uint8Array(16);
				for (let i = 0; i < 4; i++) {
					const part = Number(ipv4Match[i + 1]);
					if (part > 255) {
						return null;
					}
					ip[i] = part;
				}
				return {ip, ipv6: false};
			}
			const halves = address.split('::');
			if (halves.length > 2) {
				return null;
			}
			const parseGroups = (half) => (half === '' ? [] : half.split(':').map((group) => (/^[0-9a-f]{1,4}$/i.test(group) ? parseInt(group, 16) : NaN)));
			const head = parseGroups(halves[0]);
			const tail = halves.length == 2 ? parseGroups(halves[1]) : [];
			const numMissing = 8 - head.length - tail.length;
			if ((halves.length == 2 && numMissing < 1) || (halves.length == 1 && numMissing != 0)) {
				return null;
			}
			const groups = [...head, ...new Array(numMissing).fill(0), ...tail];
			if (groups.some(Number.isNaN)) {
				return null;
			}
			const ip = new Uint8Array(16);
			groups.forEach((group, i) => {
				ip[i * 2] = group >> 8;
				ip[i * 2 + 1] = group & 0xff;
			});
			return {ip, ipv6: true};
		},

		connectionKey(ip, ipv6, port) {
			return `${ipv6 ? 6 : 4}:${ip.subarray(0, ipv6 ? 16 : 4).join('.')}:${port}`;
		},

		// Assigns a unique port to every connection, as the remote port is not known in all environments.
		allocatePort(ip, ipv6) {
			for (let i = 0; i < 65535; i++) {
				const port = WebsocketServer.nextPort;
				WebsocketServer.nextPort = port % 65535 + 1;
				if (!WebsocketServer.connections.has(WebsocketServer.connectionKey(ip, ipv6, port))) {
					return port;
				}
			}
			return 0;
		},

		queuePacket(connection, data) {
			const queue = WebsocketServer.sockets[connection.socket]?.queue;
			if (!queue || queue.length >= WebsocketServer.MAX_QUEUED_PACKETS) {
				return;
			}
			queue.push({connection, data});
			WebsocketServer.wakeUp?.();
		},

		accept(port, webSocket, remoteAddress) {
			const address = WebsocketServer.parseAddress(remoteAddress);
			const socket = address ? WebsocketServer.sockets.findIndex((candidate) => candidate?.port == port && candidate.ipv6 == address.ipv6) : -1;
			const connectionPort = socket >= 0 ? WebsocketServer.allocatePort(address.ip, address.ipv6) : 0;
			if (connectionPort == 0) {
				err(`websockets: rejected connection from '${remoteAddress}'`);
				webSocket.close(1008, 'Address not supported');
				return;
			}
			const connection = {
				socket,
				webSocket,
				ip: address.ip,
				ipv6: address.ipv6,
				port: connectionPort,
				key: WebsocketServer.connectionKey(address.ip, address.ipv6, connectionPort),
				receivedFirstMessage: false,
				closed: false,
			};
			WebsocketServer.connections.set(connection.key, connection);
			webSocket.binaryType = 'arraybuffer';
			webSocket.addEventListener('message', (event) => {
				if (typeof event.data == 'string') {
					return;
				}
				const data = ArrayBuffer.isView(event.data) ? new Uint8Array(event.data.buffer, event.data.byteOffset, event.data.byteLength) : new Uint8Array(event.data);
				const isFirstMessage = !connection.receivedFirstMessage;
				connection.receivedFirstMessage = true;
				if (isFirstMessage && data.length == WebsocketServer.PORT_MESSAGE.length + 2 && WebsocketServer.PORT_MESSAGE.every((byte, i) => data[i] == byte)) {
					return;
				}
				WebsocketServer.queuePacket(connection, data);
			});
			webSocket.addEventListener('close', () => {
				if (connection.closed) {
					return;
				}
				connection.closed = true;
				WebsocketServer.connections.delete(connection.key);
				WebsocketServer.queuePacket(connection, WebsocketServer.CLOSE_PACKET);
			});
		},
	},

	WebsocketCreateJsImpl__deps: ['$WebsocketServer'],
	WebsocketCreateJsImpl: (port, ipv6) => {
		const host = Module['websocketServer'];
		if (!host) {
			err('websockets: Module.websocketServer must be set to accept WebSocket connections');
			return -1;
		}
		const socket = {port, ipv6: !!ipv6, queue: []};
		let index = WebsocketServer.sockets.indexOf(null);
		if (index < 0) {
			index = WebsocketServer.sockets.length;
		}
		WebsocketServer.sockets[index] = socket;
		const numListening = WebsocketServer.listeningPorts.get(port) ?? 0;
		if (numListening == 0) {
			try {
				host.listen(port, (webSocket, remoteAddress) => WebsocketServer.accept(port, webSocket, remoteAddress));
			} catch (e) {
				err(`websockets: failed to listen on port ${port}: ${e}`);
				WebsocketServer.sockets[index] = null;
				return -1;
			}
		}
		WebsocketServer.listeningPorts.set(port, numListening + 1);
		return index;
	},

	WebsocketDestroyJsImpl__deps: ['$WebsocketServer'],
	WebsocketDestroyJsImpl: (socket) => {
		const port = WebsocketServer.sockets[socket].port;
		WebsocketServer.sockets[socket] = null;
		for (const connection of WebsocketServer.connections.values()) {
			if (connection.socket == socket) {
				connection.closed = true;
				WebsocketServer.connections.delete(connection.key);
				connection.webSocket.close(1001, 'Server shutdown');
			}
		}
		const numListening = WebsocketServer.listeningPorts.get(port) - 1;
		if (numListening == 0) {
			WebsocketServer.listeningPorts.delete(port);
			Module['websocketServer'].close?.(port);
		} else {
			WebsocketServer.listeningPorts.set(port, numListening);
		}
	},

	WebsocketRecvJsImpl__deps: ['$WebsocketServer'],
	WebsocketRecvJsImpl: (socket, dataPtr, maxSize, ipPtr, portPtr, ipv6Ptr) => {
		const queue = WebsocketServer.sockets[socket].queue;
		while (queue.length) {
			const {connection, data} = queue.shift();
			// Packets are never larger than the buffer, larger messages cannot be valid
			if (data.length > maxSize || data.length == 0) {
				continue;
			}
			HEAPU8.set(data, dataPtr);
			HEAPU8.set(connection.ip, ipPtr);
			HEAPU16[portPtr >> 1] = connection.port;
			HEAPU8[ipv6Ptr] = connection.ipv6 ? 1 : 0;
			return data.length;
		}
		return 0;
	},

	WebsocketSendJsImpl__deps: ['$WebsocketServer'],
	WebsocketSendJsImpl: (socket, dataPtr, size, ipPtr, port) => {
		const ipv6 = WebsocketServer.sockets[socket].ipv6;
		const connection = WebsocketServer.connections.get(WebsocketServer.connectionKey(HEAPU8.subarray(ipPtr, ipPtr + 16), ipv6, port));
		if (!connection || connection.webSocket.readyState != 1) {
			return -1;
		}
		// Drop packets like UDP when the client cannot keep up
		if ((connection.webSocket.bufferedAmount ?? 0) > WebsocketServer.MAX_BUFFERED_BYTES) {
			return size;
		}
		try {
			connection.webSocket.send(HEAPU8.slice(dataPtr, dataPtr + size));
		} catch (e) {
			return -1;
		}
		return size;
	},

	WebsocketWaitJsImpl__deps: ['$WebsocketServer'],
	WebsocketWaitJsImpl__async: true,
	WebsocketWaitJsImpl: (socketsPtr, numSockets, timeoutMs) => Asyncify.handleAsync(async () => {
		const sockets = Array.from(HEAP32.subarray(socketsPtr >> 2, (socketsPtr >> 2) + numSockets));
		const hasData = () => sockets.some((socket) => WebsocketServer.sockets[socket]?.queue.length > 0);
		if (!hasData() && timeoutMs > 0) {
			await new Promise((resolve) => {
				const timeout = setTimeout(resolve, timeoutMs);
				WebsocketServer.wakeUp = () => {
					clearTimeout(timeout);
					resolve();
				};
			});
			WebsocketServer.wakeUp = null;
		}
		return hasData() ? 1 : 0;
	}),
});
