#ifndef DDNET_API_TIME_H
#define DDNET_API_TIME_H

#include "api.h"

static PyObject* API_Time_getLocalTime(PyObject* self, PyObject* args)
{
	return PyFloat_FromDouble((double) PythonAPI_GameClient->Client()->LocalTime());
}

static PyObject* API_Time_getGameTick(PyObject* self, PyObject* args)
{
	PyObject* localPlayerIdObject;
	int localPlayerId = g_Config.m_ClDummy;

	if (args != nullptr && PyArg_ParseTuple(args, "O", &localPlayerIdObject) && localPlayerIdObject != nullptr && PyLong_Check(localPlayerIdObject)) {
		localPlayerId = PyLong_AsLong(localPlayerIdObject);
	}

	return PyLong_FromLong(PythonAPI_GameClient->Client()->GameTick(localPlayerId));
}

static PyMethodDef API_TimeMethods[] = {
	{"getLocalTime", API_Time_getLocalTime, METH_VARARGS, "Return Client->LocalTime(). 1 unit it's 1 second"},
	{"getGameTick", API_Time_getGameTick, METH_NOARGS, "Jump tee"},
	{NULL, NULL, 0, NULL}
};

static struct PyModuleDef API_TimeModule = {
	PyModuleDef_HEAD_INIT,
	"API.Time",
	NULL,
	-1,
	API_TimeMethods
};

PyMODINIT_FUNC PyInit_API_Time(void)
{
	return PyModule_Create(&API_TimeModule);
}

#endif // DDNET_API_TIME_H
