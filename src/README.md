# Overview

@mainpage Overview

This is the main page of the documentation of the [DDNet source code](https://github.com/ddnet/ddnet).

## Structure

The `src` folder is structured into several folders as follows:

- android: Platform-specific code for Android systems.
- antibot: Antibot API integration.
- base: Cross-platform abstraction layer. Contains many utility functions and abstracts most platform-specific code. Contains functions for string manipulation, filesystem, time, networking, threading, debugging, secure random etc.
- engine: Game engine. Mostly implements non-gameplay related funtionality like graphics, sound, input, higher-level networking etc.
- game: Gameplay related code. Separated into client, server and editor.
- macos: Platform-specific code for macOS systems.
- masterping: Tool that collects game server info via an HTTP endpoint and aggregates them.
- @ref mastersrv "mastersrv": The mastersrv maintains a list of all registered DDNet (and Teeworlds) servers.
- rust-bridge: Glue to bridge Rust and C++ code.
- steam: Steam API integration.
- test: Unit tests.
- tools: Additional tools for utility and testing.

## Documentation

We use [doxygen](https://www.doxygen.nl/) to generate this code documentation.
The documentation is updated regularly and available at https://codedoc.ddnet.org/

To generate the documentation locally, install doxygen, then run `doxygen` within the project folder.
The documentation will be written to the `docs` folder.

## Resources

- [Development page on the DDNet Wiki](https://wiki.ddnet.org/wiki/Development)
- [Extra tools pages on the DDNet Wiki](https://wiki.ddnet.org/wiki/Extra_tools)
