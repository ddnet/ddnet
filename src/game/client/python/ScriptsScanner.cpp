#include "ScriptsScanner.h"
#include "base/system.h"
#include <filesystem>
namespace fs = std::filesystem;

std::vector<PythonScript*> ScriptsScanner::scan()
{
	std::vector<PythonScript*> scripts(0);

	for (const auto & entry : fs::directory_iterator(this->directoryForScanning))
	{
		if (entry.is_directory()) {
			// Add recursive scan
			continue;
		}

		std::string path = entry.path().string().substr(strlen(this->directoryForScanning) + 1);

		if (path.substr(path.length() - 3) != ".py") {
			continue;
		}

		std::string scriptPath = std::string(this->directoryForScanning) + "." + path.substr(0, path.length() - 3);
		scripts.emplace_back(new PythonScript(scriptPath));
	}

	return scripts;
}