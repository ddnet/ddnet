mastersrv
=========

The mastersrv maintains a list of all registered DDNet (and Teeworlds) servers.
Game servers try to register themselves with the mastersrv via an HTTP POST
request on /ddnet/15/register. The mastersrv delays the registration until the
the game server can prove it actually listens on the provided port
(`need_challenge`). In order to do that, the mastersrv sends a UDP message
containing an opaque token to the game server. The game server will include
this token in its next register and the register process will succeed
(`success`).

To build the mastersrv, have a recent version of
[Rust](https://www.rust-lang.org/) installed and run `cargo build --release` in
the `src/mastersrv` folder. The resulting binary should appear in
`src/mastersrv/target/release/mastersrv`.

Use `--help` to see some of the options of the mastersrv.

servers.json
============

The `servers.json` file produced by the mastersrv is not actually served
automatically, it needs to be served by another HTTPS server, probably by the
same as the reverse proxy handling HTTPS for the mastersrv. It is suggested to
serve the `servers.json` file at the path `/ddnet/15/servers.json`.

Logs
====

To enable logs, set the environment variable `RUST_LOG=mastersrv,info`. Logs
should appear already when just starting the mastersrv.

Reverse proxy
=============

In order to run this mastersrv in production, you must put it behind a reverse
proxy that handles HTTPS, such as nginx, Caddy, or Apache.

This means the mastersrv cannot determine the origin IP address by looking at
the IP address of the request, it will come from the reverse proxy, likely on
localhost. The mastersrv expects that the reverse proxy adds a header where it
puts the original requester IP address, e.g. `Connecting-IP`. Add
`--connecting-ip-header Connecting-IP` to the command line options of the
mastersrv.

For nginx, add `proxy_set_header Connecting-IP $remote_addr;`

For Caddy, add `header_up Connecting-IP {http.request.remote}`.

For Apache, run `a2enmod headers` and add `RequestHeader set Connecting-IP "%{REMOTE_ADDR}s"` to the
config.

If you're behind Cloudflare, instead use `--connecting-ip-header
CF-Connecting-IP` without any changes.
