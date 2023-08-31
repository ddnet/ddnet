#ifndef DDNET_PYTHONSCRIPT_H
#define DDNET_PYTHONSCRIPT_H

#include "Python.h"

class PythonScript
{
public:
	PythonScript() : filepath(nullptr) {}
	PythonScript(const char* filepath);

	const char* filepath;
	const char* name;

	PyObject* module;
protected:
};

#endif // DDNET_PYTHONSCRIPT_H
