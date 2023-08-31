#include "api.h"
#include "base/system.h"

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
	"API.console",
	NULL,
	-1,
	API_ConsoleMethods
};
// ============ API Module ============ //

static PyMethodDef APIMethods[] = {
	{NULL, NULL, 0, NULL}
};

static struct PyModuleDef API = {
	PyModuleDef_HEAD_INIT,
	"API",
	NULL,
	-1,
	APIMethods
};

PyMODINIT_FUNC PyInit_API(void) {
	PyObject* APIModule = PyModule_Create(&API);
	PyModule_AddObject(APIModule, "Console", PyModule_Create(&API_ConsoleModule));
	return APIModule;
}