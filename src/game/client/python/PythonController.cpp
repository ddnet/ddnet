#include "PythonController.h"
#include "Python.h"
#include "api.h"

PythonController::PythonController()
{
	PyImport_AppendInittab("API", &PyInit_API);
	Py_Initialize();
}

void PythonController::StartExecuteScript(PythonScript* pythonScript)
{
	this->executedPythonScripts[this->countExecutedPythonScripts++] = *pythonScript;

}

void PythonController::StopExecuteScript(PythonScript* pythonScript)
{

}