//
// Created by danii on 18.09.2023.
//

#ifndef DDNET_API_DUMMY_DummyInput_H
#define DDNET_API_DUMMY_DummyInput_H

#include <functional>
#include "api.h"
#include "api_vector2.h"

// ============ API.DummyInput Module ============ //
static PyObject* API_DummyInput_move(PyObject* self, PyObject* args)
{
	API_Input_Direction direction;
	PyArg_ParseTuple(args, "i", &direction);

	PythonAPI_GameClient->pythonController.inputs[(g_Config.m_ClDummy + 1) % 2].m_Direction = direction;
	Py_RETURN_NONE;
}

static PyObject* API_DummyInput_jump(PyObject* self, PyObject* args)
{
	PythonAPI_GameClient->pythonController.inputs[(g_Config.m_ClDummy + 1) % 2].m_Jump = 1;
	Py_RETURN_NONE;
}

static PyObject* API_DummyInput_Hook(PyObject* self, PyObject* args)
{
	bool hook;
	PyArg_ParseTuple(args, "b", &hook);
	PythonAPI_GameClient->pythonController.inputs[(g_Config.m_ClDummy + 1) % 2].m_Hook = hook;
	Py_RETURN_NONE;
}

static PyObject* API_DummyInput_fire(PyObject* self, PyObject* args)
{
	PythonAPI_GameClient->pythonController.InputFire((g_Config.m_ClDummy + 1) % 2);
	Py_RETURN_NONE;
}

static PyObject* API_DummyInput_setTarget(PyObject* self, PyObject* args)
{
	Vector2 *position;

	if (!PyArg_ParseTuple(args, "O!", &Vector2Type, &position))
		return NULL;

	PythonAPI_GameClient->pythonController.inputs[(g_Config.m_ClDummy + 1) % 2].m_TargetX = position->x;
	PythonAPI_GameClient->pythonController.inputs[(g_Config.m_ClDummy + 1) % 2].m_TargetY = position->y;

	Py_RETURN_NONE;
}

static PyObject* API_DummyInput_getMousePosition(PyObject* self, PyObject* args)
{
	vec2 mousePos = PythonAPI_GameClient->m_Controls.m_aMousePos[(g_Config.m_ClDummy + 1) % 2];

	Vector2* mousePosObject = (Vector2*) PyObject_New(Vector2, &Vector2Type);
	mousePosObject->x = mousePos.x;
	mousePosObject->y = mousePos.y;

	return (PyObject*) mousePosObject;
}

static PyObject* API_DummyInput_getTargetPosition(PyObject* self, PyObject* args)
{
	Vector2* mousePosObject = (Vector2*) PyObject_New(Vector2, &Vector2Type);
	mousePosObject->x = PythonAPI_GameClient->pythonController.inputs[(g_Config.m_ClDummy + 1) % 2].m_TargetX;
	mousePosObject->y = PythonAPI_GameClient->pythonController.inputs[(g_Config.m_ClDummy + 1) % 2].m_TargetY;

	return (PyObject*) mousePosObject;
}

static PyObject* API_DummyInput_setWantedWeapon(PyObject* self, PyObject* args)
{
	int wantedWeapon;

	if (!PyArg_ParseTuple(args, "i", &wantedWeapon))
		return NULL;


	PythonAPI_GameClient->pythonController.inputs[(g_Config.m_ClDummy + 1) % 2].m_WantedWeapon = wantedWeapon;

	Py_RETURN_NONE;
}

static PyObject* API_DummyInput_enableControl(PyObject* self, PyObject* args)
{
	bool enable;
	PyArg_ParseTuple(args, "b", &enable);
	PythonAPI_GameClient->pythonController.enableDummyControl = enable;
	Py_RETURN_NONE;
}

static PyMethodDef API_DummyInputMethods[] = {
	{"move", API_DummyInput_move, METH_VARARGS, "Move tee. Takes a value from (API.Direction['Left'], API.Direction['None'], API.Direction['Right'])."},
	{"jump", API_DummyInput_jump, METH_NOARGS, "Jump tee"},
	{"hook", API_DummyInput_Hook, METH_VARARGS, "Hook tee"},
	{"fire", API_DummyInput_fire, METH_NOARGS, "Fire tee"},
	{"setTarget", API_DummyInput_setTarget, METH_VARARGS, "Set Target position(arg: {'x': int, 'y': int})"},
	{"getMousePosition", API_DummyInput_getMousePosition, METH_NOARGS, "Return mouse position of current player"},
	{"getTargetPosition", API_DummyInput_getTargetPosition, METH_NOARGS, "Return Python target position of current player"},
	{"setWantedWeapon", API_DummyInput_setWantedWeapon, METH_VARARGS, "Set wanted weapon for current player"},
	{"enableControl", API_DummyInput_enableControl, METH_VARARGS, "Enable/Disable control of dummy"},
	{NULL, NULL, 0, NULL}
};

static struct PyModuleDef API_DummyInputModule = {
	PyModuleDef_HEAD_INIT,
	"API.DummyInput",
	NULL,
	-1,
	API_DummyInputMethods
};

PyMODINIT_FUNC PyInit_API_DummyInput(void)
{
	return PyModule_Create(&API_DummyInputModule);
}

#endif // DDNET_API_DUMMY_DummyInput_H
