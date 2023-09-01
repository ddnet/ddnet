#include "PythonScript.h"
#include "Python.h"

PythonScript::PythonScript(const char* filepath)
{
	this->filepath = filepath;
	this->module = PyImport_ImportModule(filepath);

	if (this->module == nullptr) {
		PyErr_Print();
		return;
	}

	PyObject* getScriptNameFunction = PyObject_GetAttrString(this->module, "getScriptName");

	if (getScriptNameFunction && PyCallable_Check(getScriptNameFunction)) {
		PyObject* args = PyTuple_Pack(0);
		PyObject* getScriptNameFunctionReturn = PyObject_CallObject(getScriptNameFunction, args);
		Py_XDECREF(args);
		this->name = PyUnicode_AsUTF8(getScriptNameFunctionReturn);
	} else {
		PyErr_Print();
		this->name = "Python Script";
	}
}