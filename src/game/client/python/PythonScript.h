#ifndef DDNET_PYTHONSCRIPT_H
#define DDNET_PYTHONSCRIPT_H
#include <vector>
#include <string>
#include "Python.h"

using namespace std;

class PythonScript
{
public:
	PythonScript() : filepath(nullptr) {}
	PythonScript(string filepath);

	void init();
	bool isInitialized();
	void updateExceptions();

	~PythonScript();

	string filepath;
	string name;
	vector<string> fileExceptions;

	PyObject* module;
protected:
	bool initialized = false;
	PyObject* errStream;
	PyObject* sysModule;

	string getError();
};

#endif // DDNET_PYTHONSCRIPT_H
