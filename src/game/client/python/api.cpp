#include "api.h"
#include "base/system.h"

// ============ API.Input Module ============ //
enum API_Input_Direction {
	DIRECTION_LEFT = -1,
	DIRECTION_RIGHT = 1,
	DIRECTION_NONE = 0,
};

static PyObject* API_Input_move(PyObject* self, PyObject* args) {
	API_Input_Direction direction;
	PyArg_ParseTuple(args, "i", &direction);

	PythonAPI_GameClient->pythonController.inputs[g_Config.m_ClDummy].m_Direction = direction;
	Py_RETURN_NONE;
}

static PyObject* API_Input_jump(PyObject* self, PyObject* args) {
	PythonAPI_GameClient->pythonController.inputs[g_Config.m_ClDummy].m_Jump = 4;
	Py_RETURN_NONE;
}

static PyObject* API_Input_hook(PyObject* self, PyObject* args) {
	bool hook;
	PyArg_ParseTuple(args, "b", &hook);
	PythonAPI_GameClient->pythonController.inputs[g_Config.m_ClDummy].m_Hook = hook;
	Py_RETURN_NONE;
}

static PyObject* API_Input_fire(PyObject* self, PyObject* args) {
	PythonAPI_GameClient->m_Controls.m_aInputData[g_Config.m_ClDummy].m_Fire = (PythonAPI_GameClient->m_Controls.m_aInputData[g_Config.m_ClDummy].m_Fire + 1) % 64;
	PythonAPI_GameClient->pythonController.inputs[g_Config.m_ClDummy].m_Fire = PythonAPI_GameClient->m_Controls.m_aInputData[g_Config.m_ClDummy].m_Fire;
	Py_RETURN_NONE;
}

static PyObject* API_Input_setBlockUserInput(PyObject* self, PyObject* args) {
	bool block;
	PyArg_ParseTuple(args, "b", &block);

	PythonAPI_GameClient->pythonController.blockUserInput = block;
	Py_RETURN_NONE;
}

static PyObject* API_Input_setTarget(PyObject* self, PyObject* args) {
	PyObject *positionObject;
	if (!PyArg_ParseTuple(args, "O", &positionObject)) {
		return NULL;
	}
	if (!PyDict_Check(positionObject)) {
		PyErr_SetString(PyExc_TypeError, "parameter must be a dictionary.");
		return NULL;
	}

	PyObject *xObject = PyDict_GetItemString(positionObject, "x");
	PyObject *yObject = PyDict_GetItemString(positionObject, "y");

	if (xObject == NULL || yObject == NULL) {
		PyErr_SetString(PyExc_KeyError, "key not found in dictionary");
		return NULL;
	}
	int x = PyLong_AsLong(xObject);
	int y = PyLong_AsLong(yObject);

	PythonAPI_GameClient->pythonController.inputs[g_Config.m_ClDummy].m_TargetX = x;
	PythonAPI_GameClient->pythonController.inputs[g_Config.m_ClDummy].m_TargetY = y;

	Py_RETURN_NONE;
}

static PyMethodDef API_InputMethods[] = {
	{"move", API_Input_move, METH_VARARGS, "Move tee"},
	{"jump", API_Input_jump, METH_VARARGS, "Jump tee"},
	{"hook", API_Input_hook, METH_VARARGS, "Hook tee"},
	{"fire", API_Input_fire, METH_VARARGS, "Fire tee"},
	{"setTarget", API_Input_setTarget, METH_VARARGS, "Set Target position(arg: {'x': int, 'y': int}"},
	{"setBlockUserInput", API_Input_setBlockUserInput, METH_VARARGS, "Block user input"},
	{NULL, NULL, 0, NULL}
};

static struct PyModuleDef API_InputModule = {
	PyModuleDef_HEAD_INIT,
	"API.Input",
	NULL,
	-1,
	API_InputMethods
};

PyMODINIT_FUNC PyInit_API_Input(void) {
	PyObject* module = PyModule_Create(&API_InputModule);

	PyObject* direction_enum = Py_BuildValue(
		"{s:i, s:i, s:i}",
		"Left", DIRECTION_LEFT,
		"Right", DIRECTION_RIGHT,
		"None", DIRECTION_NONE
	);
	PyModule_AddObject(module, "Direction", direction_enum);

	return module;
}
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
	PyModule_AddObject(APIModule, "Console", PyInit_API_Console());
	PyModule_AddObject(APIModule, "Input", PyInit_API_Input());
	return APIModule;
}