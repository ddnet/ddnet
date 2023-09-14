#ifndef DDNET_PYTHONSCRIPT_H
#define DDNET_PYTHONSCRIPT_H
#include <vector>
#include <string>
#include "Python.h"

class PythonScript
{
public:
	PythonScript() : filepath(nullptr) {}
	PythonScript(const char* filepath);

	~PythonScript();

	const char* filepath;
	const char* name;
	std::vector<std::string> fileExceptions;

	PyObject* module;
protected:
};

#endif // DDNET_PYTHONSCRIPT_H
