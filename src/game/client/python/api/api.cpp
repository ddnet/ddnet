#include "api.h"
#include "api_player.h"
#include "api_vector2.h"
#include "api_tee.h"
#include "api_collision.h"
#include "api_console.h"
#include "api_input.h"
#include "api_dummy_input.h"
#include "api_time.h"
#include "api_predict.h"
#include "api_character.h"
#include "game/client/gameclient.h"
#include "structmember.h"

// ============ API Module ============ //

static PyObject* API_LocalID(PyObject* self, PyObject* args) {
	int clientId;
	PyArg_ParseTuple(args, "i", &clientId);

	if(clientId == -1)
		clientId = g_Config.m_ClDummy;

	auto localId = PythonAPI_GameClient->m_aLocalIDs[clientId];

	PyObject* localIdObject = PyLong_FromLong(localId);

	return localIdObject;
}

static PyMethodDef APIMethods[] = {
	{"LocalID", API_LocalID, METH_VARARGS, "Get Local ID"},
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
	PyModule_AddObject(APIModule, "DummyInput", PyInit_API_DummyInput());
	PyModule_AddObject(APIModule, "Collision", PyInit_API_Collision());
	PyModule_AddObject(APIModule, "Time", PyInit_API_Time());
	PyModule_AddObject(APIModule, "Predict", PyInit_API_Predict());

	while (PyType_Ready(&Vector2Type) < 0 || PyType_Ready(&PlayerType) < 0 || PyType_Ready(&TeeType) < 0 || PyType_Ready(&CharacterType) < 0)
	{
	}

	PyModule_AddObject(APIModule, "Vector2", (PyObject *)&Vector2Type);
	PyModule_AddObject(APIModule, "Player", (PyObject *)&PlayerType);
	PyModule_AddObject(APIModule, "Tee", (PyObject *)&TeeType);
	PyModule_AddObject(APIModule, "Character", (PyObject *)&CharacterType);

	return APIModule;
}