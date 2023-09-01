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
	if (this->isExecutedScript(pythonScript)) {
		return;
	}

	this->executedPythonScripts.push_back(pythonScript);
}

void PythonController::StopExecuteScript(PythonScript* pythonScript)
{
	for (auto iterator = this->executedPythonScripts.begin(); iterator != this->executedPythonScripts.end(); iterator++) {
		auto executedPythonScript = *iterator;

		if (executedPythonScript->filepath == pythonScript->filepath) {
			this->executedPythonScripts.erase(iterator);

			return;
		}
	}
}

bool PythonController::OnInput(const IInput::CEvent &Event)
{
	for (auto executedPythonScript : this->executedPythonScripts) {
		PyObject* function = PyObject_GetAttrString(executedPythonScript->module, "onInput");

		if (function != nullptr && PyCallable_Check(function)) {
			const char* keyName = this->m_pClient->Input()->KeyName(Event.m_Key);
			PyObject* keyCodeObject = PyLong_FromLong(Event.m_Key);
			PyObject* keyFlagsObject = PyLong_FromLong(Event.m_Flags);
			PyObject* keyNameObject = PyUnicode_DecodeUTF8(keyName, strlen(keyName), "strict");
			PyObject* args = PyTuple_Pack(3, keyCodeObject, keyFlagsObject, keyNameObject);
			PyObject_CallObject(function, args);
			Py_XDECREF(args);
			Py_XDECREF(keyCodeObject);
			Py_XDECREF(keyFlagsObject);
			Py_XDECREF(keyNameObject);
		}
	}

	return false;
}

bool PythonController::isExecutedScript(PythonScript *pythonScript)
{
	for (auto executedPythonScript : this->executedPythonScripts) {
		if (executedPythonScript->filepath == pythonScript->filepath) {
			return true;
		}
	}

	return false;
}

bool PythonController::needForceInput(int inputId)
{
	return this->inputs[inputId].m_Direction != 0 ||
	       this->inputs[inputId].m_Jump != 0 ||
	       this->inputs[inputId].m_Hook != 0 ||
	       this->inputs[inputId].m_Fire != 0 ||
		blockUserInput;
}

int PythonController::SnapInput(int *pData, int inputId)
{
	int sizeData = this->m_pClient->m_Controls.SnapInput(pData);
	CNetObj_PlayerInput input;
	mem_copy(&input, pData, sizeData);

	if (input.m_Direction == 0 || this->blockUserInput) {
		input.m_Direction = this->inputs[inputId].m_Direction;
	}

	if (input.m_Jump == 0 || this->blockUserInput) {
		if (this->inputs[inputId].m_Jump > 0) {
			input.m_Jump = 1;
		}
	}

	if (input.m_Hook == 0 || this->blockUserInput) {
		input.m_Hook = this->inputs[inputId].m_Hook;
	}

	int hookState = this->m_pClient->m_aClients[this->m_pClient->m_aLocalIDs[g_Config.m_ClDummy]].m_Predicted.m_HookState;

	if (hookState == -1) {
		this->inputs[inputId].m_Hook = 0;
	}


	if (this->inputs[inputId].m_Fire > 0 || this->blockUserInput) {
		input.m_Fire = this->inputs[inputId].m_Fire;
		if (this->inputs[inputId].m_Fire > 0 && !this->blockUserInput) {
			this->m_pClient->m_Controls.m_aInputData[inputId].m_Fire = (this->m_pClient->m_Controls.m_aInputData[inputId].m_Fire + 1) % 64;
		}
		this->inputs[inputId].m_Fire = 0;
		this->m_pClient->m_Chat.AddLine(g_Config.m_ClDummy, 0, ("On snap input" + std::to_string(input.m_Fire)).c_str());
	}

	mem_copy(pData, &input, sizeof(input));
	if (this->inputs[inputId].m_Jump > 0) {
		this->inputs[inputId].m_Jump--;
	}

	return sizeData;
}
