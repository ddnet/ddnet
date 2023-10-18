//
// Created by danii on 18.09.2023.
//

#ifndef DDNET_API_INPUT_H
#define DDNET_API_INPUT_H

#include <functional>
#include "api.h"
#include "api_vector2.h"

// ============ API.Input Module ============ //
enum API_Input_Direction {
	DIRECTION_LEFT = -1,
	DIRECTION_RIGHT = 1,
	DIRECTION_NONE = 0,
};

static PyObject* API_Input_move(PyObject* self, PyObject* args)
{
	API_Input_Direction direction;
	PyArg_ParseTuple(args, "i", &direction);

	PythonAPI_GameClient->pythonController.inputs[g_Config.m_ClDummy].m_Direction = direction;
	Py_RETURN_NONE;
}

static PyObject* API_Input_jump(PyObject* self, PyObject* args)
{
	PythonAPI_GameClient->pythonController.inputs[g_Config.m_ClDummy].m_Jump = 1;
	Py_RETURN_NONE;
}

static PyObject* API_Input_hook(PyObject* self, PyObject* args)
{
	bool hook;
	PyArg_ParseTuple(args, "b", &hook);
	PythonAPI_GameClient->pythonController.inputs[g_Config.m_ClDummy].m_Hook = hook;
	Py_RETURN_NONE;
}

static PyObject* API_Input_reset(PyObject* self, PyObject* args)
{
	PythonAPI_GameClient->pythonController.ResetInput(g_Config.m_ClDummy);
	Py_RETURN_NONE;
}

static PyObject* API_Input_fire(PyObject* self, PyObject* args)
{
	PythonAPI_GameClient->pythonController.InputFire(g_Config.m_ClDummy);
	Py_RETURN_NONE;
}

static PyObject* API_Input_setBlockUserInput(PyObject* self, PyObject* args)
{
	bool block;
	PyArg_ParseTuple(args, "b", &block);

	PythonAPI_GameClient->pythonController.blockUserInput = block;
	Py_RETURN_NONE;
}

static PyObject* API_Input_setTarget(PyObject* self, PyObject* args)
{
	Vector2 *position;

	if (!PyArg_ParseTuple(args, "O!", &Vector2Type, &position))
		return NULL;

	PythonAPI_GameClient->pythonController.inputs[g_Config.m_ClDummy].m_TargetX = position->x;
	PythonAPI_GameClient->pythonController.inputs[g_Config.m_ClDummy].m_TargetY = position->y;

	Py_RETURN_NONE;
}

PyObject* global_onArrivalPythonFunction = nullptr;

static PyObject* wrap_onArrival() {
	if (global_onArrivalPythonFunction) {
		PyObject_CallObject(global_onArrivalPythonFunction, NULL);
		Py_DECREF(global_onArrivalPythonFunction);
		global_onArrivalPythonFunction = nullptr;
	}
	Py_RETURN_NONE;
}

static PyObject* API_Input_setTargetHumanLike(PyObject* self, PyObject* args)
{
	Vector2 *position;
	PyObject *pyMoveTime = nullptr;
	PyObject *pyFunction = nullptr;

	if (!PyArg_ParseTuple(args, "O!OO:Input_setTargetHumanLike", &Vector2Type, &position, &pyMoveTime, &pyFunction))
		return NULL;

	if (pyMoveTime != nullptr && !PyFloat_Check(pyMoveTime)) {
		PyErr_SetString(PyExc_TypeError, "`moveTime` must be a float or None");
		return NULL;
	}

	if (pyFunction != nullptr && !PyCallable_Check(pyFunction)) {
		PyErr_SetString(PyExc_TypeError, "`onArrived` must be a callable function or None");
		return NULL;
	}

	float moveTime = 0.02;
	if (pyMoveTime != nullptr) {
		moveTime = PyFloat_AsDouble(pyMoveTime);
	}

	Point point;
	point.x = (int) position->x;
	point.y = (int) position->y;

	std::function<void()> onArrival;

	if (global_onArrivalPythonFunction) {
		Py_DECREF(global_onArrivalPythonFunction);
	}

	global_onArrivalPythonFunction = pyFunction;

	if (global_onArrivalPythonFunction) {
		Py_INCREF(global_onArrivalPythonFunction);
		onArrival = [] {
			wrap_onArrival();
		};
	} else {
		onArrival = []() {};
	}

	PythonAPI_GameClient->humanLikeMouse.moveToPoint(&point, moveTime, onArrival);

	Py_RETURN_NONE;
}

static PyObject* API_Input_moveMouseToPlayer(PyObject* self, PyObject* args)
{
	int playerId;
	PyObject *pyMoveTime = nullptr;
	PyObject *pyFunction = nullptr;

	if (!PyArg_ParseTuple(args, "iOO:Input_setTargetHumanLike", &playerId, &pyMoveTime, &pyFunction))
		return NULL;


	if (pyMoveTime != nullptr && !PyFloat_Check(pyMoveTime)) {
		PyErr_SetString(PyExc_TypeError, "`moveTime` must be a float or None");
		return NULL;
	}

	if (pyFunction != nullptr && !PyCallable_Check(pyFunction)) {
		PyErr_SetString(PyExc_TypeError, "`onArrived` must be a callable function");
		return NULL;
	}

	float moveTime = 0.02;
	if (pyMoveTime != nullptr) {
		moveTime = PyFloat_AsDouble(pyMoveTime);
	}

	std::function<void()> onArrival;

	if (global_onArrivalPythonFunction) {
		Py_DECREF(global_onArrivalPythonFunction);
	}

	global_onArrivalPythonFunction = pyFunction;

	if (global_onArrivalPythonFunction) {
		Py_INCREF(global_onArrivalPythonFunction);
		onArrival = [] {
			wrap_onArrival();
		};
	} else {
		onArrival = []() {};
	}

	PythonAPI_GameClient->humanLikeMouse.moveToPlayer(playerId, moveTime, onArrival);

	Py_RETURN_NONE;
}

static PyObject* API_Input_getMousePosition(PyObject* self, PyObject* args)
{
	vec2 mousePos = PythonAPI_GameClient->m_Controls.m_aMousePos[g_Config.m_ClDummy];

	Vector2* mousePosObject = (Vector2*) PyObject_New(Vector2, &Vector2Type);
	mousePosObject->x = mousePos.x;
	mousePosObject->y = mousePos.y;

	return (PyObject*) mousePosObject;
}

static PyObject* API_Input_getTargetPosition(PyObject* self, PyObject* args)
{
	Vector2* mousePosObject = (Vector2*) PyObject_New(Vector2, &Vector2Type);
	mousePosObject->x = PythonAPI_GameClient->pythonController.inputs[g_Config.m_ClDummy].m_TargetX;
	mousePosObject->y = PythonAPI_GameClient->pythonController.inputs[g_Config.m_ClDummy].m_TargetY;

	return (PyObject*) mousePosObject;
}

static PyObject* API_Input_isHumanLikeMoveEnded(PyObject* self, PyObject* args)
{
	bool moveEnded = PythonAPI_GameClient->humanLikeMouse.isMoveEnded();

	return PyBool_FromLong(moveEnded ? 1 : 0);
}

static PyObject* API_Input_removeMoving(PyObject* self, PyObject* args)
{
	PythonAPI_GameClient->humanLikeMouse.removeMoving();

	Py_RETURN_NONE;
}

static PyObject* API_Input_setWantedWeapon(PyObject* self, PyObject* args)
{
	int wantedWeapon;

	if (!PyArg_ParseTuple(args, "i", &wantedWeapon))
		return NULL;


	PythonAPI_GameClient->pythonController.inputs[g_Config.m_ClDummy].m_WantedWeapon = wantedWeapon;

	Py_RETURN_NONE;
}

static PyMethodDef API_InputMethods[] = {
	{"move", API_Input_move, METH_VARARGS, "Move tee. Takes a value from (API.Direction['Left'], API.Direction['None'], API.Direction['Right'])."},
	{"jump", API_Input_jump, METH_NOARGS, "Jump tee"},
	{"hook", API_Input_hook, METH_VARARGS, "Hook tee"},
	{"fire", API_Input_fire, METH_NOARGS, "Fire tee"},
	{"setTarget", API_Input_setTarget, METH_VARARGS, "Set Target position(arg: {'x': int, 'y': int})"},
	{"setTargetHumanLike", API_Input_setTargetHumanLike, METH_VARARGS, "Set Target Human Like position(arg: {'x': int, 'y': int}"},
	{"moveMouseToPlayer", API_Input_moveMouseToPlayer, METH_VARARGS, "Move mouse to player human Like(arg: playerId"},
	{"setBlockUserInput", API_Input_setBlockUserInput, METH_VARARGS, "Block user input"},
	{"isHumanLikeMoveEnded", API_Input_isHumanLikeMoveEnded, METH_NOARGS, "Return true, if human like moving is ended, else false"},
	{"getMousePosition", API_Input_getMousePosition, METH_NOARGS, "Return mouse position of current player"},
	{"getTargetPosition", API_Input_getTargetPosition, METH_NOARGS, "Return Python target position of current player"},
	{"removeMoving", API_Input_removeMoving, METH_NOARGS, "Remove human like moving"},
	{"setWantedWeapon", API_Input_setWantedWeapon, METH_VARARGS, "Set wanted weapon for current player"},
	{"reset", API_Input_reset, METH_VARARGS, "Reset input for current player"},
	{NULL, NULL, 0, NULL}
};

static struct PyModuleDef API_InputModule = {
	PyModuleDef_HEAD_INIT,
	"API.Input",
	NULL,
	-1,
	API_InputMethods
};

PyMODINIT_FUNC PyInit_API_Input(void)
{
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

#endif // DDNET_API_INPUT_H
