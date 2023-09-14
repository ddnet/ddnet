#include "api.h"
#include "base/system.h"
#include "game/client/gameclient.h"

// ============ API.Collision Module ============ //
static PyObject* API_Collision_debug(PyObject* self, PyObject* args) {
	char* message;
//	PyArg_ParseTuple(args, "s", &message);

//	PythonAPI_GameClient->Collision().
	Py_RETURN_NONE;
}

static PyMethodDef API_CollisionMethods[] = {
	{"debug", API_Collision_debug, METH_VARARGS, "Prints a debug message"},
	{NULL, NULL, 0, NULL}
};

static struct PyModuleDef API_CollisionModule = {
	PyModuleDef_HEAD_INIT,
	"API.Collision",
	NULL,
	-1,
	API_CollisionMethods
};

PyMODINIT_FUNC PyInit_API_Collision(void) {
	PyObject* module = PyModule_Create(&API_CollisionModule);

	return module;
}

// ============ API.Player(id) Module ============ //
PyObject* CreatePlayerObject(CGameClient::CClientData playerClient)
{
	return Py_BuildValue(
		"{s:b, s:b, s:d, s:i, s:b, s:b, s:i, s:i, s:i, s:b, s:i, s:b, s:d, s:i, s:b, s:b, s:b, s:b, s:b, s:b, s:b, s:b, s:b, s:b, s:b, s:b, s:b, s:b, s:b, s:b, s:b, s:i, s:b, s:b, s:b, s:b, s:i, s:i, s:s, s:s, s:s, s:{s:i, s:b, s:b, s:b, s:b, s:b, s:b, s:b, s:b, s:b, s:b, s:b, s:i, s:b, s:b, s:b, s:b, s:i, s:{s:d, s:d}, s:{s:d, s:d}, s:i, s:i, s:i, s:i, s:{s:d, s:d}, s:{s:d, s:d}, s:{s:d, s:d}, s:i, s:i, s:i, s:b, s:i, s:i, s:i, s:{s:d, s:d}, s:b, s:b, s:b, s:i}, s:{s:i, s:b, s:b, s:b, s:b, s:b, s:b, s:b, s:b, s:b, s:b, s:b, s:i, s:b, s:b, s:b, s:b, s:i, s:{s:d, s:d}, s:{s:d, s:d}, s:i, s:i, s:i, s:i, s:{s:d, s:d}, s:{s:d, s:d}, s:{s:d, s:d}, s:i, s:i, s:i, s:b, s:i, s:i, s:i, s:{s:d, s:d}, s:b, s:b, s:b, s:i}}",
		"isActive", playerClient.m_Active,
		"isAfk", playerClient.m_Afk,
		"angle", (double) playerClient.m_Angle,
		"authLevel", playerClient.m_AuthLevel,
		"isChatIgnore", playerClient.m_ChatIgnore,
		"isCollisionDisabled", playerClient.m_CollisionDisabled,
		"colorBody", playerClient.m_ColorBody,
		"colorFeet", playerClient.m_ColorFeet,
		"county", playerClient.m_Country,
		"isDeepFrozen", playerClient.m_DeepFrozen,
		"emoticon", playerClient.m_Emoticon,
		"isEmoticonIgnore", playerClient.m_EmoticonIgnore,
		"emoticonStartFraction", (double) playerClient.m_EmoticonStartFraction,
		"emoticonStartTick", playerClient.m_EmoticonStartTick,
		"isEndlessHook", playerClient.m_EndlessHook,
		"isEndlessJump", playerClient.m_EndlessJump,
		"isFoe", playerClient.m_Foe,
		"isFriend", playerClient.m_Friend,
		"isGrenadeHitDisabled", playerClient.m_GrenadeHitDisabled,
		"isHammerHitDisabled", playerClient.m_HammerHitDisabled,
		"hasTelegunGrenade", playerClient.m_HasTelegunGrenade,
		"hasTelegunGun", playerClient.m_HasTelegunGun,
		"hasTelegunLaser", playerClient.m_HasTelegunLaser,
		"isHookHitDisabled", playerClient.m_HookHitDisabled,
		"isPredicted", playerClient.m_IsPredicted,
		"isPredictedLocal", playerClient.m_IsPredictedLocal,
		"isJetpack", playerClient.m_Jetpack,
		"isLaserHitDisabled", playerClient.m_LaserHitDisabled,
		"isLiveFrozen", playerClient.m_LiveFrozen,
		"isPaused", playerClient.m_Paused,
		"isShotgunHitDisabled", playerClient.m_ShotgunHitDisabled,
		"skinColor", playerClient.m_SkinColor,
		"isSolo", playerClient.m_Solo,
		"isSpec", playerClient.m_Spec,
		"isSpecCharPresent", playerClient.m_SpecCharPresent,
		"isSuper", playerClient.m_Super,
		"team", playerClient.m_Team,
		"useCustomColor", playerClient.m_UseCustomColor,
		"clan", playerClient.m_aClan,
		"name", playerClient.m_aName,
		"skinName", playerClient.m_aSkinName,
		"predicted",
		"colliding", playerClient.m_Predicted.m_Colliding,
		"isSuper", playerClient.m_Predicted.m_Super,
		"isSolo", playerClient.m_Predicted.m_Solo,
		"isShotgunHitDisabled", playerClient.m_Predicted.m_ShotgunHitDisabled,
		"isLiveFrozen", playerClient.m_Predicted.m_LiveFrozen,
		"isLaserHitDisabled", playerClient.m_Predicted.m_LaserHitDisabled,
		"isJetpack", playerClient.m_Predicted.m_Jetpack,
		"isHookHitDisabled", playerClient.m_Predicted.m_HookHitDisabled,
		"isHasTelegunGun", playerClient.m_Predicted.m_HasTelegunGun,
		"isHasTelegunGrenade", playerClient.m_Predicted.m_HasTelegunGrenade,
		"isHammerHitDisabled", playerClient.m_Predicted.m_HammerHitDisabled,
		"isGrenadeHitDisabled", playerClient.m_Predicted.m_GrenadeHitDisabled,
		"freezeEnd", playerClient.m_Predicted.m_FreezeEnd,
		"isEndlessJump", playerClient.m_Predicted.m_EndlessJump,
		"isEndlessHook", playerClient.m_Predicted.m_EndlessHook,
		"isDeepFrozen", playerClient.m_Predicted.m_DeepFrozen,
		"isCollisionDisabled", playerClient.m_Predicted.m_CollisionDisabled,
		"angle", playerClient.m_Predicted.m_Angle,
		"pos",
		"x", (double) playerClient.m_Predicted.m_Pos.x,
		"y", (double) playerClient.m_Predicted.m_Pos.y,
		"vel",
		"x", (double) playerClient.m_Predicted.m_Vel.x,
		"y", (double) playerClient.m_Predicted.m_Vel.y,
		"hookState", playerClient.m_Predicted.m_HookState,
		"direction", playerClient.m_Predicted.m_Direction,
		"activeWeapon", playerClient.m_Predicted.m_ActiveWeapon,
		"freezeStart", playerClient.m_Predicted.m_FreezeStart,
		"hookDir",
		"x", (double) playerClient.m_Predicted.m_HookDir.x,
		"y", (double) playerClient.m_Predicted.m_HookDir.y,
		"hookPos",
		"x", (double) playerClient.m_Predicted.m_HookPos.x,
		"y", (double) playerClient.m_Predicted.m_HookPos.y,
		"hookTeleBase",//
		"x", (double) playerClient.m_Predicted.m_HookTeleBase.x,
		"y", (double) playerClient.m_Predicted.m_HookTeleBase.y,
		"hookTick", playerClient.m_Predicted.m_HookTick,
		"hookedPlayer", playerClient.m_Predicted.m_HookedPlayer,
		"id", playerClient.m_Predicted.m_Id,
		"isInFreeze", playerClient.m_Predicted.m_IsInFreeze,
		"jumped", playerClient.m_Predicted.m_Jumped,
		"jumpedTotal", playerClient.m_Predicted.m_JumpedTotal,
		"jumps", playerClient.m_Predicted.m_Jumps,
		"lastVel",
		"x", (double) playerClient.m_Predicted.m_LastVel.x,
		"y", (double) playerClient.m_Predicted.m_LastVel.y,
		"isLeftWall", playerClient.m_Predicted.m_LeftWall,
		"isNewHook", playerClient.m_Predicted.m_NewHook,
		"isReset", playerClient.m_Predicted.m_Reset,
		"triggeredEvents", playerClient.m_Predicted.m_TriggeredEvents,
		"prevPredicted",
		"colliding", playerClient.m_PrevPredicted.m_Colliding,
		"isSuper", playerClient.m_PrevPredicted.m_Super,
		"isSolo", playerClient.m_PrevPredicted.m_Solo,
		"isShotgunHitDisabled", playerClient.m_PrevPredicted.m_ShotgunHitDisabled,
		"isLiveFrozen", playerClient.m_PrevPredicted.m_LiveFrozen,
		"isLaserHitDisabled", playerClient.m_PrevPredicted.m_LaserHitDisabled,
		"isJetpack", playerClient.m_PrevPredicted.m_Jetpack,
		"isHookHitDisabled", playerClient.m_PrevPredicted.m_HookHitDisabled,
		"isHasTelegunGun", playerClient.m_PrevPredicted.m_HasTelegunGun,
		"isHasTelegunGrenade", playerClient.m_PrevPredicted.m_HasTelegunGrenade,
		"isHammerHitDisabled", playerClient.m_PrevPredicted.m_HammerHitDisabled,
		"isGrenadeHitDisabled", playerClient.m_PrevPredicted.m_GrenadeHitDisabled,
		"freezeEnd", playerClient.m_PrevPredicted.m_FreezeEnd,
		"isEndlessJump", playerClient.m_PrevPredicted.m_EndlessJump,
		"isEndlessHook", playerClient.m_PrevPredicted.m_EndlessHook,
		"isDeepFrozen", playerClient.m_PrevPredicted.m_DeepFrozen,
		"isCollisionDisabled", playerClient.m_PrevPredicted.m_CollisionDisabled,
		"angle", playerClient.m_PrevPredicted.m_Angle,
		"pos",
		"x", (double) playerClient.m_PrevPredicted.m_Pos.x,
		"y", (double) playerClient.m_PrevPredicted.m_Pos.y,
		"vel",
		"x", (double) playerClient.m_PrevPredicted.m_Vel.x,
		"y", (double) playerClient.m_PrevPredicted.m_Vel.y,
		"hookState", playerClient.m_PrevPredicted.m_HookState,
		"direction", playerClient.m_PrevPredicted.m_Direction,
		"activeWeapon", playerClient.m_PrevPredicted.m_ActiveWeapon,
		"freezeStart", playerClient.m_PrevPredicted.m_FreezeStart,
		"hookDir",
		"x", (double) playerClient.m_PrevPredicted.m_HookDir.x,
		"y", (double) playerClient.m_PrevPredicted.m_HookDir.y,
		"hookPos",
		"x", (double) playerClient.m_PrevPredicted.m_HookPos.x,
		"y", (double) playerClient.m_PrevPredicted.m_HookPos.y,
		"hookTeleBase",//
		"x", (double) playerClient.m_PrevPredicted.m_HookTeleBase.x,
		"y", (double) playerClient.m_PrevPredicted.m_HookTeleBase.y,
		"hookTick", playerClient.m_PrevPredicted.m_HookTick,
		"hookedPlayer", playerClient.m_PrevPredicted.m_HookedPlayer,
		"id", playerClient.m_PrevPredicted.m_Id,
		"isInFreeze", playerClient.m_PrevPredicted.m_IsInFreeze,
		"jumped", playerClient.m_PrevPredicted.m_Jumped,
		"jumpedTotal", playerClient.m_PrevPredicted.m_JumpedTotal,
		"jumps", playerClient.m_PrevPredicted.m_Jumps,
		"lastVel",
		"x", (double) playerClient.m_PrevPredicted.m_LastVel.x,
		"y", (double) playerClient.m_PrevPredicted.m_LastVel.y,
		"isLeftWall", playerClient.m_PrevPredicted.m_LeftWall,
		"isNewHook", playerClient.m_PrevPredicted.m_NewHook,
		"isReset", playerClient.m_PrevPredicted.m_Reset,
		"triggeredEvents", playerClient.m_PrevPredicted.m_TriggeredEvents
	);
}

static PyObject* API_Player(PyObject* self, PyObject* args) {
	int playerId;
	PyArg_ParseTuple(args, "i", &playerId);

	auto playerClientData = PythonAPI_GameClient->m_aClients[playerId];

	PyObject* player = CreatePlayerObject(playerClientData);

	return player;
}

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

static PyObject* API_Input_setTargetHumanLike(PyObject* self, PyObject* args) {
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

	Point point;

	point.x = x;
	point.y = y;

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
	{"move", API_Input_move, METH_VARARGS, "Move tee"},
	{"jump", API_Input_jump, METH_VARARGS, "Jump tee"},
	{"hook", API_Input_hook, METH_VARARGS, "Hook tee"},
	{"fire", API_Input_fire, METH_VARARGS, "Fire tee"},
	{"setTarget", API_Input_setTarget, METH_VARARGS, "Set Target position(arg: {'x': int, 'y': int}"},
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
	{"Player", API_Player, METH_VARARGS, "Get Player Data"},
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
	PyModule_AddObject(APIModule, "Collision", PyInit_API_Collision());
	return APIModule;
}