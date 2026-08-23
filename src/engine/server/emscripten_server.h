#ifndef ENGINE_SERVER_EMSCRIPTEN_SERVER_H
#define ENGINE_SERVER_EMSCRIPTEN_SERVER_H

#include <base/detect.h>

#if defined(CONF_PLATFORM_EMSCRIPTEN)

#include <cstddef>

/**
 * Starts the server on a separate thread in the current process. The server is
 * reachable via the in-memory loopback transport, see `net_loopback_set_enabled`.
 *
 * @param ppArguments The command line arguments to pass to the server.
 * @param NumArguments The number of command line arguments.
 *
 * @return `true` if the server was started, `false` if it is already running.
 */
bool StartEmscriptenServer(const char **ppArguments, size_t NumArguments);

/**
 * Executes a command in the console of the running server.
 *
 * @param pCommand The command to execute.
 */
void ExecuteEmscriptenServerCommand(const char *pCommand);

/**
 * @return `true` if the server thread is currently running, `false` otherwise.
 */
bool IsEmscriptenServerRunning();

#endif

#endif
