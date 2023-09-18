//
// Created by danii on 18.09.2023.
//

#ifndef DDNET_API_INPUT_H
#define DDNET_API_INPUT_H

#include "api.h"
#include "api_vector2.h"

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
	PythonAPI_GameClient->pythonController.inputs[g_Config.m_ClDummy].m_Jump = 1;
	Py_RETURN_NONE;
}

static PyObject* API_Input_hook(PyObject* self, PyObject* args) {
	bool hook;
	PyArg_ParseTuple(args, "b", &hook);
	PythonAPI_GameClient->pythonController.inputs[g_Config.m_ClDummy].m_Hook = hook;
	Py_RETURN_NONE;
}

static PyObject* API_Input_fire(PyObject* self, PyObject* args) {
	PythonAPI_GameClient->pythonController.InputFire();
	Py_RETURN_NONE;
}

static PyObject* API_Input_setBlockUserInput(PyObject* self, PyObject* args) {
	bool block;
	PyArg_ParseTuple(args, "b", &block);

	PythonAPI_GameClient->pythonController.blockUserInput = block;
	Py_RETURN_NONE;
}

static PyObject* API_Input_setTarget(PyObject* self, PyObject* args) {
	Vector2 *position;

	if (!PyArg_ParseTuple(args, "O!", &Vector2Type, &position))
		return NULL;

	PythonAPI_GameClient->pythonController.inputs[g_Config.m_ClDummy].m_TargetX = position->x;
	PythonAPI_GameClient->pythonController.inputs[g_Config.m_ClDummy].m_TargetY = position->y;

	Py_RETURN_NONE;
}

static PyObject* API_Input_setTargetHumanLike(PyObject* self, PyObject* args) {
	Vector2 *position;

	if (!PyArg_ParseTuple(args, "O!", &Vector2Type, &position))
		return NULL;

	Point point;

	point.x = (int) position->x;
	point.y = (int) position->y;

	PythonAPI_GameClient->humanLikeMouse.moveToPoint(&point);

	Py_RETURN_NONE;
}

static PyObject* API_Input_moveMouseToPlayer(PyObject* self, PyObject* args) {
	int playerId;

	if (!PyArg_ParseTuple(args, "i", &playerId)) {
		return NULL;
	}

	PythonAPI_GameClient->humanLikeMouse.moveToPlayer(playerId);

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

#endif // DDNET_API_INPUT_H
