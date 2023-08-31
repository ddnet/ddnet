#ifndef DDNET_PYTHONCONTROLLER_H
#define DDNET_PYTHONCONTROLLER_H

#include "game/client/python/PythonScript.h"

class PythonController
{
public:
	PythonController();
	void StartExecuteScript(PythonScript* pythonScript);
	void StopExecuteScript(PythonScript* pythonScript);
protected:
	PythonScript executedPythonScripts[32];
	int countExecutedPythonScripts = 0;
};

#endif // DDNET_PYTHONCONTROLLER_H
