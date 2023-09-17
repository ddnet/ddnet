#ifndef DDNET_API_H
#define DDNET_API_H

#include "Python.h"
#include "game/client/gameclient.h"

extern CGameClient* PythonAPI_GameClient;

PyMODINIT_FUNC PyInit_API(void);
#endif // DDNET_API_H
