#include "PythonController.h"
#include "Python.h"
#include "game/client/python/api/api.h"

PythonController::PythonController()
{
	PyImport_AppendInittab("API", &PyInit_API);
	Py_Initialize();
}

void PythonController::AutoloadAdd(PythonScript *pythonScript)
{
	if(this->isScriptAutoloading(pythonScript))
		return;

	this->autoLoadPythonScripts.push_back(pythonScript);
}

void PythonController::AutoloadRemove(PythonScript *pythonScript)
{
	for(auto i = this->autoLoadPythonScripts.begin(); i != this->autoLoadPythonScripts.end(); i++)
	{
		auto autoLoadScript = *i;
		if(autoLoadScript->filepath == pythonScript->filepath)
		{
			this->autoLoadPythonScripts.erase(i);
			return;
		}
	}
}

bool PythonController::isScriptAutoloading(PythonScript *pythonScript)
{
	for(auto PythonScript : this->autoLoadPythonScripts)
	{
		if(PythonScript->filepath == pythonScript->filepath)
			return true;
	}
	return false;
}

void PythonController::StartExecuteScript(PythonScript* pythonScript)
{
	ResetInput();

	pythonScript->init();

	if (!pythonScript->isInitialized() || this->isExecutedScript(pythonScript)) {
		return;
	}

	PyObject* function = nullptr;

	if (PyObject_HasAttrString(pythonScript->module, "onScriptStarted")) {
		function = PyObject_GetAttrString(pythonScript->module, "onScriptStarted");
	} else {
		pythonScript->updateExceptions();
	}

	if (function != nullptr && PyCallable_Check(function)) {
		PyObject* args = PyTuple_Pack(0);
		PyObject_CallObject(function, args);
		Py_XDECREF(args);
	} else {
		PyErr_Clear();
	}

	pythonScript->updateExceptions();

	this->executedPythonScripts.push_back(pythonScript);
}

void PythonController::StopExecuteScript(PythonScript* pythonScript)
{
	pythonScript->fileExceptions = vector<string>(0);

	for (auto iterator = this->executedPythonScripts.begin(); iterator != this->executedPythonScripts.end(); iterator++) {
		auto executedPythonScript = *iterator;

		if (executedPythonScript->filepath == pythonScript->filepath) {
			this->executedPythonScripts.erase(iterator);

			PyObject* function = nullptr;

			if (PyObject_HasAttrString(executedPythonScript->module, "onScriptStopped")) {
				function = PyObject_GetAttrString(pythonScript->module, "onScriptStopped");
			} else {
				executedPythonScript->updateExceptions();
			}

			if (function != nullptr && PyCallable_Check(function)) {
				PyObject* args = PyTuple_Pack(0);
				PyObject_CallObject(function, args);
				PyOS_InterruptOccurred();
				Py_XDECREF(args);
			} else {
				PyErr_Clear();
			}

			pythonScript->updateExceptions();
			ResetInput();

			return;
		}
	}
}

bool PythonController::OnInput(const IInput::CEvent &Event)
{
	std::string keyName = this->m_pClient->Input()->KeyName(Event.m_Key);

	if (keyName == "l") {
		vec2 playerPos = this->m_pClient->m_aClients[this->m_pClient->m_aLocalIDs[g_Config.m_ClDummy]].m_Predicted.m_Pos;
		vec2 pos = this->m_pClient->m_Controls.m_aMousePos[g_Config.m_ClDummy] * GameClient()->m_Camera.m_Zoom + playerPos;
		this->m_pClient->movementAgent.moveTo(pos);
	}

	bool needBrakeInput = false;

	for (auto executedPythonScript : this->executedPythonScripts) {
		if (!PyObject_HasAttrString(executedPythonScript->module, "onInput")) {
			executedPythonScript->updateExceptions();
			continue;
		}

		PyObject* function = PyObject_GetAttrString(executedPythonScript->module, "onInput");

		if (function != nullptr && PyCallable_Check(function)) {
			PyObject* keyCodeObject = PyLong_FromLong(Event.m_Key);
			PyObject* keyFlagsObject = PyLong_FromLong(Event.m_Flags);
			PyObject* keyNameObject = PyUnicode_DecodeUTF8(keyName.c_str(), keyName.size(), "strict");
			PyObject* args = PyTuple_Pack(3, keyCodeObject, keyFlagsObject, keyNameObject);
			PyObject* result = PyObject_CallObject(function, args);

			if (result != nullptr && PyObject_IsTrue(result)) {
				needBrakeInput = true;
			}

			PyOS_InterruptOccurred();
			Py_XDECREF(args);
			Py_XDECREF(keyCodeObject);
			Py_XDECREF(keyFlagsObject);
			Py_XDECREF(keyNameObject);
		} else {
			PyErr_Clear();
		}

		executedPythonScript->updateExceptions();
	}

	return needBrakeInput;
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
	if (inputId != g_Config.m_ClDummy) {
		return enableDummyControl;
	}

	return this->inputs[inputId].m_Direction != 0 ||
	       this->inputs[inputId].m_Jump != 0 ||
	       this->inputs[inputId].m_Hook != 0 ||
	       this->inputs[inputId].m_Fire != 0 ||
	       this->inputs[inputId].m_TargetX != 0 ||
	       this->inputs[inputId].m_TargetY != 0 ||
	       this->inputs[inputId].m_WantedWeapon != -1 ||
		blockUserInput;
}

int PythonController::SnapInput(int *pData, int inputId)
{
	int sizeData;
	CNetObj_PlayerInput input;

	if (inputId == g_Config.m_ClDummy) {
		sizeData = this->m_pClient->m_Controls.SnapInput(pData);
		mem_copy(&input, pData, sizeData);
	} else {
		input.m_Direction = 0;
		input.m_TargetX = 0;
		input.m_TargetY = 0;
		input.m_Jump = 0;
		input.m_Fire = 0;
		input.m_Hook = 0;
		input.m_PlayerFlags = 0;
		input.m_WantedWeapon = 0;
		input.m_NextWeapon = 0;
		input.m_PrevWeapon = 0;
		sizeData = sizeof(input);
	}

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

	int hookState = this->m_pClient->m_aClients[this->m_pClient->m_aLocalIDs[inputId]].m_Predicted.m_HookState;

	if (hookState == -1) {
		this->inputs[inputId].m_Hook = 0;
	}

	if (this->inputs[inputId].m_Fire > 0 || this->blockUserInput) {
		input.m_Fire = this->inputs[inputId].m_Fire;
		if (this->inputs[inputId].m_Fire > 0 && !this->blockUserInput) {
			this->m_pClient->m_Controls.m_aInputData[inputId].m_Fire = (this->m_pClient->m_Controls.m_aInputData[inputId].m_Fire + 1) % 64;
		}
		this->inputs[inputId].m_Fire = 0;
//		this->m_pClient->m_Chat.AddLine(inputId, 0, ("On snap input" + std::to_string(input.m_Fire)).c_str());
	}

	if (this->inputs[inputId].m_TargetX != 0 || this->inputs[inputId].m_TargetY != 0) {
		input.m_TargetX = this->inputs[inputId].m_TargetX;
		input.m_TargetY = this->inputs[inputId].m_TargetY;
	}

	if (this->inputs[inputId].m_WantedWeapon != -1) {
		input.m_WantedWeapon = this->inputs[inputId].m_WantedWeapon;
		this->m_pClient->m_Controls.m_aInputData[inputId].m_WantedWeapon = input.m_WantedWeapon;
	}

	mem_copy(pData, &input, sizeof(input));
	if (this->inputs[inputId].m_Jump > 0) {
		this->inputs[inputId].m_Jump--;
	}


	return sizeData;
}

void PythonController::InputFire(int id)
{
	if (GameClient()->m_Controls.m_aInputData[id].m_Fire % 2 == 1) {
		return;
	}

	GameClient()->m_Controls.m_aInputData[id].m_Fire = (GameClient()->m_Controls.m_aInputData[id].m_Fire + 1) % 64;
	GameClient()->pythonController.inputs[id].m_Fire = GameClient()->m_Controls.m_aInputData[id].m_Fire;
}

int fps = 60;

void PythonController::OnUpdate()
{
	for (auto executedPythonScript : this->executedPythonScripts) {
		executedPythonScript->updateExceptions();
		if (!PyModule_Check(executedPythonScript->module) || !PyObject_HasAttrString(executedPythonScript->module, "onUpdate")) {
			executedPythonScript->updateExceptions();
			continue;
		}

		PyObject* function = PyObject_GetAttrString(executedPythonScript->module, "onUpdate");

		if (function != nullptr && PyCallable_Check(function)) {
			PyObject* args = PyTuple_Pack(0);
			PyObject* result = PyObject_CallObject(function, args);
			executedPythonScript->updateExceptions();
			Py_XDECREF(args);
			Py_XDECREF(result);
		}
		else {
			PyErr_Clear();
		}
		Py_XDECREF(function);
	}
}

void PythonController::ResetInput(int id)
{
	if (id == -1) {
		for (int i = 0; i < 2; i++) {
			this->inputs[i].m_Fire = 0;
			this->inputs[i].m_Direction = 0;
			this->inputs[i].m_Jump = 0;
			this->inputs[i].m_Hook = 0;
			this->inputs[i].m_TargetX = 0;
			this->inputs[i].m_TargetY = 0;
			this->inputs[i].m_WantedWeapon = -1;
			GameClient()->m_Controls.m_aInputData[i].m_Fire = 0;
		}

		GameClient()->humanLikeMouse.removeMoving();
	} else {
		this->inputs[id].m_Fire = 0;
		this->inputs[id].m_Direction = 0;
		this->inputs[id].m_Jump = 0;
		this->inputs[id].m_Hook = 0;
		this->inputs[id].m_TargetX = 0;
		this->inputs[id].m_TargetY = 0;
		this->inputs[id].m_WantedWeapon = -1;
		GameClient()->m_Controls.m_aInputData[id].m_Fire = 0;

		if (id == g_Config.m_ClDummy) {
			GameClient()->humanLikeMouse.removeMoving();
		}
	}
}
