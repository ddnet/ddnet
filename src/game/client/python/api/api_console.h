//
// Created by danii on 18.09.2023.
//

#ifndef DDNET_API_CONSOLE_H
#define DDNET_API_CONSOLE_H

#include "api.h"
#include "api_vector2.h"

// ============ API.Console Module ============ //
static PyObject* API_Console_debug(PyObject* self, PyObject* args) {
	char* message;
	PyArg_ParseTuple(args, "s", &message);

	dbg_msg("Python Script", message);
	Py_RETURN_NONE;
}

static PyMethodDef API_ConsoleMethods[] = {
	{"debug", API_Console_debug, METH_VARARGS, "Prints a debug message"},
	{NULL, NULL, 0, NULL}
};

static struct PyModuleDef API_ConsoleModule = {
	PyModuleDef_HEAD_INIT,
	"API.Console",
	NULL,
	-1,
	API_ConsoleMethods
};

PyMODINIT_FUNC PyInit_API_Console(void) {
	PyObject* module = PyModule_Create(&API_ConsoleModule);

	return module;
}

#endif // DDNET_API_CONSOLE_H
