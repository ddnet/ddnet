#include "PythonScript.h"
#include "Python.h"

PythonScript::PythonScript(string filepath)
{
	this->filepath = filepath;
	init();
}

PythonScript::~PythonScript()
{
	Py_XDECREF(this->errStream);
	Py_XDECREF(this->sysModule);
	Py_XDECREF(this->module);
}

string PythonScript::getError()
{
	PyErr_Print();
	PyObject* getvalue = PyObject_GetAttrString(errStream, "getvalue");
	PyObject* errorObject = PyObject_CallObject(getvalue, nullptr);
	PyObject_SetAttrString(sysModule, "stderr", PySys_GetObject((char*)"__stderr__"));

	return PyUnicode_AsUTF8(errorObject);
}

void PythonScript::init()
{
	this->name = this->filepath;
	this->fileExceptions = vector<string>(0);

	if (isInitialized()) {
		Py_XDECREF(this->errStream);
		Py_XDECREF(this->sysModule);
		Py_XDECREF(this->module);
		this->initialized = false;
	}

	PyObject* ioModule = PyImport_ImportModule("io");
	PyObject* StringIO = PyObject_GetAttrString(ioModule, "StringIO");

	errStream = PyObject_CallObject(StringIO, nullptr);

	sysModule = PyImport_ImportModule("sys");
	PyObject_SetAttrString(sysModule, "stderr", errStream);

	this->module = PyImport_ImportModule(this->filepath.c_str());
	this->updateExceptions();

	this->module = PyImport_ReloadModule(this->module);
	this->updateExceptions();

	if (this->module == nullptr) {
		this->fileExceptions.push_back(getError());
		return;
	}


	PyObject* getScriptNameFunction = PyObject_GetAttrString(this->module, "getScriptName");

	if (getScriptNameFunction && PyCallable_Check(getScriptNameFunction)) {
		PyObject* args = PyTuple_Pack(0);
		PyObject* getScriptNameFunctionReturn = PyObject_CallObject(getScriptNameFunction, args);
		Py_XDECREF(args);
		this->name = PyUnicode_AsUTF8(getScriptNameFunctionReturn);
	} else {
		PyErr_Clear();
	}

	initialized = true;
}

bool PythonScript::isInitialized()
{
	return initialized;
}

void PythonScript::updateExceptions()
{
	if (!PyOS_InterruptOccurred()) {
		string error = getError();

		if (error.size() > 0) {
			this->fileExceptions.push_back(error);
		}

		PyErr_Clear();
	}
}
