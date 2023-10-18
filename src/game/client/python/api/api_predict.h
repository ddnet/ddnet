#ifndef DDNET_API_PREDICT_H
#define DDNET_API_PREDICT_H

#include "api.h"
#include "api_vector2.h"

static PyObject* API_Predict_predictLaser(PyObject* self, PyObject* args)
{
	Player* shooterPlayer;
	Vector2* shootPosition;

	if (args == nullptr || !PyArg_ParseTuple(args, "O!O!", &PlayerType, &shooterPlayer, &Vector2Type, &shootPosition)) {
		return NULL;
	}

	int shooterId = g_Config.m_ClDummy;

	if (shooterPlayer != nullptr) {
		shooterId = shooterPlayer->tee.id;
	}

	int playerIdTakeLaser = -1;
	vector<Line> lines = PythonAPI_GameClient->aimHelper.predictLaserShoot(shooterId, shootPosition->toVec2(), playerIdTakeLaser);
	vector<Vector2*> points(0);

	bool firstSkipped = false;

	for (auto line : lines) {
		if (!firstSkipped) {
			Vector2* firstPoint = (Vector2*) PyObject_New(Vector2, &Vector2Type);
			Vector2* secondPoint = (Vector2*) PyObject_New(Vector2, &Vector2Type);

			firstPoint->x = line.startPoint.x;
			firstPoint->y = line.startPoint.y;
			secondPoint->x = line.endPoint.x;
			secondPoint->y = line.endPoint.y;

			points.push_back(firstPoint);
			points.push_back(secondPoint);

			firstSkipped = true;
			continue;
		}

		Vector2* point = (Vector2*) PyObject_New(Vector2, &Vector2Type);

		point->x = line.endPoint.x;
		point->y = line.endPoint.y;
		points.push_back(point);
	}

	PyObject* pointsList = PyList_New(points.size());

	for (size_t i = 0; i < points.size(); i++) {
		PyList_SET_ITEM(pointsList, i, (PyObject*) points[i]);
	}

	PyObject* playerTakeLaserObject;

	if (playerIdTakeLaser != -1) {
		Player* player = (Player*) PyObject_New(Player, &PlayerType);
		Player_init(player, Py_BuildValue("(i)", playerIdTakeLaser), NULL);

		playerTakeLaserObject = (PyObject*) player;
	} else {
		playerTakeLaserObject = Py_None;
		Py_INCREF(playerTakeLaserObject);
	}

	return Py_BuildValue("(OO)", pointsList, playerTakeLaserObject);
}

static PyMethodDef API_PredictMethods[] = {
	{"predictLaser", API_Predict_predictLaser, METH_VARARGS, "Return Client->LocalPredict(). 1 unit it's 1 second"},
	{NULL, NULL, 0, NULL}
};

static struct PyModuleDef API_PredictModule = {
	PyModuleDef_HEAD_INIT,
	"API.Predict",
	NULL,
	-1,
	API_PredictMethods
};

PyMODINIT_FUNC PyInit_API_Predict(void)
{
	return PyModule_Create(&API_PredictModule);
}

#endif // DDNET_API_PREDICT_H
