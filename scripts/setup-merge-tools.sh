#!/bin/bash

function file_check() {
	if [ ! -f "$1" ]
	then
		echo "Error: file $1 not found"
		echo "       script must be run from the root of the repo"
		exit 1
	fi
}

file_check ./scripts/setup-merge-tools.sh
file_check CMakeLists.txt
file_check .git/config

ABS_PATH="$(cd "$(dirname "$0" || exit 1)"/ && pwd -P)"

read -r -d '' driver << EOF
[merge "ddnet-cmake"]
	name = Support for file conflicts in CMakeLists.txt
	driver = $ABS_PATH/ddnet-cmake-merge-tool.sh %O %A %B
EOF

if ! grep -qF '[merge "ddnet-cmake"]' .git/config
then
	echo -e "\n$driver" >> .git/config
fi

