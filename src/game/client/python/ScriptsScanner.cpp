#include "ScriptsScanner.h"
#include "base/system.h"
#include <filesystem>
#include <queue>

namespace fs = filesystem;

vector<PythonScript*> ScriptsScanner::scan()
{
	vector<PythonScript*> scripts(0);
	queue<string> directories;
	directories.push(this->directoryForScanning);

	while (!directories.empty()) {
		string directory = directories.front();
		directories.pop();

		for (const auto & entry : fs::directory_iterator(directory))
		{
			string path = entry.path().string().substr(strlen(this->directoryForScanning) + 1);

			if (path.length() >= 11 && path.substr(path.length() - 11) == "__pycache__") {
				continue;
			}

			if (path.length() >= 6 && path.substr(path.length() - 6) == "API.py") {
				continue;
			}

			if (entry.is_directory()) {
				directories.push(directory + "\\" + path);
				continue;
			}

			if (path.substr(path.length() - 3) != ".py") {
				continue;
			}

			string scriptPath = string(this->directoryForScanning) + "." + path.substr(0, path.length() - 3);
			replace(scriptPath.begin(), scriptPath.end(), '\\', '.');
			scripts.emplace_back(new PythonScript(scriptPath));
		}
	}

	return scripts;
}