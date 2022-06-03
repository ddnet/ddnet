#!/bin/bash

if [ ! -f scripts/integration_test.sh ] || [ ! -f CMakeLists.txt ]
then
	echo "Error: make sure your are in the root of the repo"
	exit 1
fi

arg_build_dir="build"
arg_end_args=0
arg_verbose=0
arg_valgrind_memcheck=0

for arg in "$@"
do
	if [[ "${arg::1}" == "-" ]] && [[ "$arg_end_args" == "0" ]] 
	then
		if [ "$arg" == "-h" ] || [ "$arg" == "--help" ]
		then
			echo "usage: $(basename "$0") [OPTION..] [build dir]"
			echo "description:"
			echo "  Runs a simple integration test of the client and server"
			echo "  binaries from the given build dir"
			echo "options:"
			echo "  --help|-h     show this help"
			echo "	--verbose|-v  verbose output"
		elif [ "$arg" == "-v" ] || [ "$arg" == "--verbose" ]
		then
			arg_verbose=1
		elif [ "$arg" == "--valgrind-memcheck" ]
		then
			arg_valgrind_memcheck=1
		elif [ "$arg" == "--" ]
		then
			arg_end_args=1
		else
			echo "Error: unknown arg '$arg'"
		fi
	else
		arg_build_dir="$arg"
	fi
done

if [ ! -d "$arg_build_dir" ]
then
	echo "Error: build directory '$arg_build_dir' not found"
	exit 1
fi
if [ ! -f "$arg_build_dir"/DDNet ]
then
	echo "Error: client binary not found '$arg_build_dir/DDNet' not found"
	exit 1
fi
if [ ! -f "$arg_build_dir"/DDNet-Server ]
then
	echo "Error: server binary not found '$arg_build_dir/DDNet-Server' not found"
	exit 1
fi

mkdir -p integration_test
cp "$arg_build_dir"/DDNet* integration_test

cd integration_test || exit 1

got_killed=0

function kill_all() {
	# needed to fix hang fifo with additional ctrl+c
	if [ "$got_killed" == "1" ]
	then
		exit
	fi
	got_killed=1

	if [ "$arg_verbose" == "1" ]
	then
		echo "[*] shutting down test clients and server ..."
	fi

	sleep 1
	echo "shutdown" > server.fifo
	echo "quit" > client1.fifo
	echo "quit" > client2.fifo
}

function cleanup() {
	kill_all
}

trap cleanup EXIT

{
	echo $'add_path $CURRENTDIR'
	echo $'add_path $USERDIR'
	echo $'add_path $DATADIR'
	echo $'add_path ../data'
} > storage.cfg

function fail()
{
	sleep 1
	tail -n2 "$1".log > fail_"$1".txt
	echo "$1 exited with code $2" >> fail_"$1".txt
	echo "[-] $1 exited with code $2"
}

if test -n "$(find . -maxdepth 1 -name '*.fifo' -print -quit)"
then
	rm ./*.fifo
fi
if test -n "$(find . -maxdepth 1 -name 'SAN.*' -print -quit)"
then
	rm SAN.*
fi
if test -n "$(find . -maxdepth 1 -name 'fail_*' -print -quit)"
then
	rm fail_*
fi
if [ -f ddnet-server.sqlite ]
then
	rm ddnet-server.sqlite
fi

# TODO: check for open ports instead
port=17822

cp ../ubsan.supp .
cp ../memcheck.supp .

if [[ $OSTYPE == 'darwin'* ]]; then
	DETECT_LEAKS=0
else
	DETECT_LEAKS=1
fi

export UBSAN_OPTIONS=suppressions=./ubsan.supp:log_path=./SAN:print_stacktrace=1:halt_on_errors=0
export ASAN_OPTIONS=log_path=./SAN:print_stacktrace=1:check_initialization_order=1:detect_leaks=$DETECT_LEAKS:halt_on_errors=0

function print_results() {
	if [ "$arg_valgrind_memcheck" == "1" ]; then
		if grep "ERROR SUMMARY" server.log client1.log client2.log | grep -q -v "ERROR SUMMARY: 0"; then
			grep "^==" server.log client1.log client2.log
			return 1
		fi
	else
		if test -n "$(find . -maxdepth 1 -name 'SAN.*' -print -quit)"
		then
			cat SAN.*
			return 1
		fi
	fi
	return 0
}

if [ "$arg_valgrind_memcheck" == "1" ]; then
	tool="valgrind --tool=memcheck --gen-suppressions=all --suppressions=memcheck.supp"
	client_args="cl_menu_map \"\";"
else
	tool=""
	client_args=""
fi

$tool ./DDNet-Server \
	"sv_input_fifo server.fifo;
	sv_map coverage;
	sv_sqlite_file ddnet-server.sqlite;
	sv_port $port" &> server.log || fail server "$?" &

$tool ./DDNet \
	"cl_input_fifo client1.fifo;
	player_name client1;
	cl_download_skins 0;
	gfx_fullscreen 0;
	$client_args
	connect localhost:$port" &> client1.log || fail client1 "$?" &

if [ "$arg_valgrind_memcheck" == "1" ]; then
  sleep 10
else
  sleep 1
fi

$tool ./DDNet \
	"cl_input_fifo client2.fifo;
	player_name client2;
	cl_download_skins 0;
	gfx_fullscreen 0;
	$client_args
	connect localhost:$port" &> client2.log || fail client2 "$?" &

fails=0
if [ "$arg_valgrind_memcheck" == "1" ]; then
	tries=120
else
	tries=2
fi
# give the client time to launch and create the fifo file
# but assume after X secs that the client crashed before
# being able to create the file
while [[ ! -p client1.fifo || ! -p client2.fifo ]]
do
	fails="$((fails+1))"
	if [ "$arg_verbose" == "1" ]
	then
		echo "[!] client fifos not found (attempts $fails/$tries)"
	fi
	if [ "$fails" -gt "$tries" ]
	then
		print_results
		echo "[-] Error: client possibly crashed on launch"
		exit 1
	fi
	sleep 1
done

if [ "$arg_valgrind_memcheck" == "1" ]; then
  sleep 20
else
  sleep 2
fi

kill_all
wait

sleep 1

ranks="$(sqlite3 ddnet-server.sqlite < <(echo "select * from record_race;"))"
num_ranks="$(echo "$ranks" | wc -l | xargs)"
if [ "$ranks" == "" ]
then
	touch fail_ranks.txt
	echo "[-] Error: no ranks found in database"
elif [ "$num_ranks" != "1" ]
then
	touch fail_ranks.txt
	echo "[-] Error: expected 1 rank got $num_ranks"
elif ! echo "$ranks" | grep -q client1
then
	touch fail_ranks.txt
	echo "[-] Error: expected a rank from client1 instead got:"
	echo "  $ranks"
fi

if test -n "$(find . -maxdepth 1 -name 'fail_*' -print -quit)"
then
	if [ "$arg_verbose" == "1" ]
	then
		for fail in fail_*
		do
			cat "$fail"
		done
	fi
	print_results
	echo "[-] Test failed. See errors above."
	exit 1
else
	echo "[*] all tests passed"
fi

print_results || exit 1
