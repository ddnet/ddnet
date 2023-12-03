#!/bin/bash

arg_verbose=0
arg_valgrind_memcheck=0

for arg in "$@"
do
	if [ "$arg" == "-h" ] || [ "$arg" == "--help" ]
	then
		echo "usage: $(basename "$0") [OPTION..]"
		echo "description:"
		echo "  Runs a simple integration test of the client and server"
		echo "  binaries from the current build directory."
		echo "options:"
		echo "  --help|-h             show this help"
		echo "  --verbose|-v          verbose output"
		echo "  --valgrind-memcheck   use valgrind's memcheck to run server and client"
		exit 0
	elif [ "$arg" == "-v" ] || [ "$arg" == "--verbose" ]
	then
		arg_verbose=1
	elif [ "$arg" == "--valgrind-memcheck" ]
	then
		arg_valgrind_memcheck=1
	else
		echo "Error: unknown argument '$arg'"
		exit 1
	fi
done

if [ ! -f DDNet ]
then
	echo "[-] Error: client binary 'DDNet' not found"
	exit 1
fi
if [ ! -f DDNet-Server ]
then
	echo "[-] Error: server binary 'DDNet-Server' not found"
	exit 1
fi

echo "[*] Setup"
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
		echo "[*] Shutting down test clients and server"
	fi
	sleep 1

	if [[ ! -f fail_server.txt ]]
	then
		echo "[*] Shutting down server"
		echo "shutdown" > server.fifo
	fi
	sleep 1

	local i
	for ((i=1;i<3;i++))
	do
		if [[ ! -f fail_client$i.txt ]]
		then
			echo "[*] Shutting down client$i"
			echo "quit" > "client$i.fifo"
		fi
	done
	sleep 1
}

function cleanup() {
	kill_all
}

trap cleanup EXIT

function fail()
{
	sleep 1
	tail -n2 "$1".log > fail_"$1".txt
	echo "$1 exited with code $2" >> fail_"$1".txt
	echo "[-] $1 exited with code $2"
}

# Get unused port from the system by binding to port 0 and immediately closing the socket again
port=$(python3 -c 'import socket; s=socket.socket(); s.bind(("", 0)); print(s.getsockname()[1]); s.close()');

if [[ $OSTYPE == 'darwin'* ]]; then
	DETECT_LEAKS=0
else
	DETECT_LEAKS=1
fi

export UBSAN_OPTIONS=suppressions=../ubsan.supp:log_path=./SAN:print_stacktrace=1:halt_on_errors=0
export ASAN_OPTIONS=log_path=./SAN:print_stacktrace=1:check_initialization_order=1:detect_leaks=$DETECT_LEAKS:halt_on_errors=0
export LSAN_OPTIONS=suppressions=../lsan.supp:print_suppressions=0

function print_results() {
	if [ "$arg_valgrind_memcheck" == "1" ]; then
		# Wait to ensure that the error summary was written to the stderr files because valgrind takes some time
		# TODO: Instead wait for all started processes to finish
		sleep 20
		if grep "== ERROR SUMMARY: " stderr_server.txt stderr_client1.txt stderr_client2.txt | grep -q -v "ERROR SUMMARY: 0"; then
			echo "[-] Error: Valgrind has detected the following errors:"
			grep "^==" stderr_server.txt stderr_client1.txt stderr_client2.txt
			return 1
		fi
	else
		if test -n "$(find . -maxdepth 1 -name 'SAN.*' -print -quit)"
		then
			echo "[-] Error: ASAN has detected the following errors:"
			cat SAN.*
			return 1
		fi
	fi
	return 0
}

rm -rf integration_test
mkdir -p integration_test/data/maps
cp data/maps/coverage.map integration_test/data/maps
cp data/maps/Tutorial.map integration_test/data/maps
cd integration_test || exit 1

{
	echo $'add_path $CURRENTDIR'
	echo $'add_path $USERDIR'
	echo $'add_path $DATADIR'
	echo $'add_path ../data'
} > storage.cfg

tool=""
client_args="cl_download_skins 0;
	gfx_fullscreen 0;
	snd_enable 0;"

if [ "$arg_valgrind_memcheck" == "1" ]; then
	tool="valgrind --tool=memcheck --gen-suppressions=all --suppressions=../memcheck.supp --track-origins=yes"
	client_args="$client_args cl_menu_map \"\";"
fi

function wait_for_fifo() {
	local fifo="$1"
	local tries="$2"
	local fails=0
	# give the server/client time to launch and create the fifo file
	# but assume after X secs that the server/client crashed before
	# being able to create the file
	while [[ ! -p "$fifo" ]]
	do
		fails="$((fails+1))"
		if [ "$arg_verbose" == "1" ]
		then
			echo "[!] Note: $fifo not found (attempts $fails/$tries)"
		fi
		if [ "$fails" -gt "$tries" ]
		then
			echo "[-] Error: $(basename "$fifo" .fifo) possibly crashed on launch"
			kill_all
			print_results
			exit 1
		fi
		sleep 1
	done
}

function wait_for_launch() {
	local fifo="$1"
	local baseDuration="$2"
	if [ "$arg_valgrind_memcheck" == "1" ]; then
		wait_for_fifo "$fifo" $((40 * baseDuration))
		sleep $((8 * baseDuration))
	else
		wait_for_fifo "$fifo" $((10 * baseDuration))
		sleep "$baseDuration"
	fi
}

echo "[*] Launch server"
$tool ../DDNet-Server \
	"sv_input_fifo server.fifo;
	sv_rcon_password rcon;
	sv_map coverage;
	sv_sqlite_file ddnet-server.sqlite;
	logfile server.log;
	sv_register 0;
	sv_port $port" > stdout_server.txt 2> stderr_server.txt || fail server "$?" &

wait_for_launch server.fifo 1

echo "[*] Launch client 1"
$tool ../DDNet \
	"cl_input_fifo client1.fifo;
	player_name client1;
	logfile client1.log;
	$client_args
	connect localhost:$port" > stdout_client1.txt 2> stderr_client1.txt || fail client1 "$?" &

wait_for_launch client1.fifo 5

echo "[*] Start demo recording"
echo "record server" > server.fifo
echo "record client1" > client1.fifo
sleep 1

echo "[*] Launch client 2"
$tool ../DDNet \
	"cl_input_fifo client2.fifo;
	player_name client2;
	logfile client2.log;
	$client_args
	connect localhost:$port" > stdout_client2.txt 2> stderr_client2.txt || fail client2 "$?" &

wait_for_launch client2.fifo 5

echo "[*] Test chat and chat commands"
echo "say hello world" > client1.fifo
echo "rcon_auth rcon" > client1.fifo
sleep 1
tr -d '\n' > client1.fifo << EOF
say "/mc
;top5
;rank
;team 512
;emote happy -999
;pause
;points
;mapinfo
;list
;whisper client2 hi
;kill
;settings cheats
;timeout 123
;timer broadcast
;cmdlist
;saytime"
EOF
sleep 1

echo "[*] Test rcon commands"
tr -d '\n' > client1.fifo << EOF
rcon say hello from admin;
rcon broadcast test;
rcon status;
rcon echo test;
muteid 1 900 spam;
unban_all;
EOF
sleep 1

echo "[*] Stop demo recording"
echo "stoprecord" > server.fifo
echo "stoprecord" > client1.fifo
sleep 1

echo "[*] Test map change"
echo "rcon sv_map Tutorial" > client1.fifo
if [ "$arg_valgrind_memcheck" == "1" ]; then
	sleep 60
else
	sleep 15
fi

echo "[*] Play demos"
echo "play demos/server.demo" > client1.fifo
echo "play demos/client1.demo" > client2.fifo
if [ "$arg_valgrind_memcheck" == "1" ]; then
	sleep 40
else
	sleep 10
fi

# Kill all processes first so all outputs are fully written
kill_all

if ! grep -qE '^[0-9]{4}-[0-9]{2}-[0-9]{2} ([0-9]{2}:){2}[0-9]{2} I chat: 0:-2:client1: hello world$' server.log
then
	touch fail_chat.txt
	echo "[-] Error: chat message not found in server log"
fi

if ! grep -q 'cmdlist' client1.log || \
	! grep -q 'pause' client1.log || \
	! grep -q 'rank' client1.log || \
	! grep -q 'points' client1.log
then
	touch fail_chatcommand.txt
	echo "[-] Error: did not find output of /cmdlist command"
fi

if ! grep -q "hello from admin" server.log
then
	touch fail_rcon.txt
	echo "[-] Error: admin message not found in server log"
fi

if ! grep -q "demo_player: Stopped playback" client1.log
then
	touch fail_demo_server.txt
	echo "[-] Error: demo playback of server demo in client 1 was not started/finished"
fi
if ! grep -q "demo_player: Stopped playback" client2.log
then
	touch fail_demo_client.txt
	echo "[-] Error: demo playback of client demo in client 2 was not started/finished"
fi

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

for logfile in client1.log client2.log server.log
do
	if [ "$arg_valgrind_memcheck" == "1" ]
	then
		break
	fi
	if [ ! -f "$logfile" ]
	then
		echo "[-] Error: logfile '$logfile' not found"
		touch fail_logs.txt
		continue
	fi
	logdiff="$(diff -u <(grep -v "console: .* access for .* is now .*abled" "$logfile" | sort) <(sort "stdout_$(basename "$logfile" .log).txt"))"
	if [ "$logdiff" != "" ]
	then
		echo "[-] Error: logfile '$logfile' differs from stdout"
		echo "$logdiff"
		echo "[-] Error: logfile '$logfile' differs from stdout" >> fail_logs.txt
		echo "$logdiff" >> fail_logs.txt
	fi
done

for stderr in ./stderr_*.txt
do
	if [ ! -f "$stderr" ]
	then
		continue
	fi
	if [ "$(cat "$stderr")" == "" ]
	then
		continue
	fi
	echo "[!] Warning: $stderr"
	cat "$stderr"
done

if test -n "$(find . -maxdepth 1 -name 'fail_*' -print -quit)"
then
	for fail in fail_*
	do
		cat "$fail"
	done
	print_results
	echo "[-] Test failed. See errors above"
	exit 1
fi

echo "[*] All tests passed"
print_results || exit 1
