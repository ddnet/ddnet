#ifndef DDNET_SCRIPTSSCANNER_H
#define DDNET_SCRIPTSSCANNER_H

#include <vector>
#include "PythonScript.h"

class ScriptsScanner
{
public:
	ScriptsScanner() : directoryForScanning("python") {}
	ScriptsScanner(char* directoryForScanning)
	{
		this->directoryForScanning = directoryForScanning;
	}

	std::vector<PythonScript*> scan();
protected:
	char* directoryForScanning;
};

#endif // DDNET_SCRIPTSSCANNER_H
