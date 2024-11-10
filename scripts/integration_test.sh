#!/bin/bash

arg_verbose=0
arg_valgrind_memcheck=0

for arg in "$@"; do
	if [ "$arg" == "-h" ] || [ "$arg" == "--help" ]; then
		echo "usage: $(basename "$0") [OPTION..]"
		echo "description:"
		echo "  Runs a simple integration test of the client and server"
		echo "  binaries from the current build directory."
		echo "options:"
		echo "  --help|-h             show this help"
		echo "  --verbose|-v          verbose output"
		echo "  --valgrind-memcheck   use valgrind's memcheck to run server and client"
		exit 0
	elif [ "$arg" == "-v" ] || [ "$arg" == "--verbose" ]; then
		arg_verbose=1
	elif [ "$arg" == "--valgrind-memcheck" ]; then
		arg_valgrind_memcheck=1
	else
		echo "Error: unknown argument '$arg'"
		exit 1
	fi
done

if [ ! -f DDNet ]; then
	echo "[-] Error: client binary 'DDNet' not found"
	exit 1
fi
if [ ! -f DDNet-Server ]; then
	echo "[-] Error: server binary 'DDNet-Server' not found"
	exit 1
fi

echo "[*] Setup"
got_killed=0

function kill_all() {
	# needed to fix hang fifo with additional ctrl+c
	if [ "$got_killed" == "1" ]; then
		exit
	fi
	got_killed=1

	if [ "$arg_verbose" == "1" ]; then
		echo "[*] Shutting down test clients and server"
	fi
	sleep 1

	if [[ ! -f fail_server.txt ]]; then
		echo "[*] Shutting down server"
		if ! timeout 3 sh -c "echo shutdown > server.fifo"; then
			echo "[-] shutdown server timed out"
		fi
	fi
	sleep 1

	local i
	for ((i = 1; i < 3; i++)); do
		if [[ ! -f fail_client$i.txt ]]; then
			echo "[*] Shutting down client$i"
			if ! timeout 3 sh -c "echo quit > \"client$i.fifo\""; then
				echo "[-] shutdown client $i timed out"
			fi
		fi
	done
	sleep 1
}

function cleanup() {
	kill_all
}

trap cleanup EXIT

function fail() {
	sleep 1
	tail -n2 "$1".log > fail_"$1".txt
	echo "$1 exited with code $2" >> fail_"$1".txt
	echo "[-] $1 exited with code $2"
}

# Get unused port from the system by binding to port 0 and immediately closing the socket again
port=$(python3 -c 'import socket; s=socket.socket(); s.bind(("", 0)); print(s.getsockname()[1]); s.close()')

if [[ $OSTYPE == 'darwin'* ]]; then
	DETECT_LEAKS=0
else
	DETECT_LEAKS=1
fi

export UBSAN_OPTIONS=suppressions=../ubsan.supp:log_path=./SAN:print_stacktrace=1:halt_on_errors=0
export ASAN_OPTIONS=log_path=./SAN:print_stacktrace=1:check_initialization_order=1:detect_leaks=$DETECT_LEAKS:halt_on_errors=0
export LSAN_OPTIONS=suppressions=../lsan.supp:print_suppressions=0

function check_asan_and_valgrind_results() {
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
		if test -n "$(find . -maxdepth 1 -name 'SAN.*' -print -quit)"; then
			echo "[-] Error: ASAN has detected the following errors:"
			cat SAN.*
			return 1
		fi
	fi
	return 0
}

print_results() {
	for logfile in client1.log client2.log server.log; do
		if [ "$arg_valgrind_memcheck" == "1" ]; then
			break
		fi
		if [ ! -f "$logfile" ]; then
			echo "[-] Error: logfile '$logfile' not found"
			touch fail_logs.txt
			continue
		fi
		logdiff="$(diff -u <(grep -v "console: .* access for .* is now .*abled" "$logfile" | sort) <(sort "stdout_$(basename "$logfile" .log).txt"))"
		if [ "$logdiff" != "" ]; then
			echo "[-] Error: logfile '$logfile' differs from stdout"
			echo "$logdiff"
			echo "[-] Error: logfile '$logfile' differs from stdout" >> fail_logs.txt
			echo "$logdiff" >> fail_logs.txt
		fi
	done

	for stderr in ./stderr_*.txt; do
		if [ ! -f "$stderr" ]; then
			continue
		fi
		if [ "$(cat "$stderr")" == "" ]; then
			continue
		fi
		echo "[!] Warning: $stderr"
		cat "$stderr"
	done

	if test -n "$(find . -maxdepth 1 -name 'fail_*' -print -quit)"; then
		for fail in fail_*; do
			cat "$fail"
		done
		check_asan_and_valgrind_results
		echo "[-] Test failed. See errors above"
		exit 1
	fi

	echo "[*] All tests passed"
	check_asan_and_valgrind_results || exit 1
}

function fifo() {
	local cmd="$1"
	local fifo_file="$2"
	if [ -f fail_fifo_timeout.txt ]; then
		echo "[fifo] skipping because of timeout cmd: $cmd"
		return
	fi
	if [ "$arg_verbose" == "1" ]; then
		echo "[fifo] $cmd >> $fifo_file"
	fi
	if printf '%s' "$cmd" | grep -q '[`'"'"']'; then
		echo "[-] fifo commands can not contain backticks or single quotes"
		echo "[-] invalid fifo command: $cmd"
		exit 1
	fi
	if ! timeout 3 sh -c "printf '%s\n' '$cmd' >> \"$fifo_file\""; then
		fifo_error="[-] fifo command timeout: $cmd >> $fifo_file"
		printf '%s\n' "$fifo_error"
		printf '%s\n' "$fifo_error" >> fail_fifo_timeout.txt
		kill_all
		print_results
		exit 1
	fi
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
	while [[ ! -p "$fifo" ]]; do
		fails="$((fails + 1))"
		if [ "$arg_verbose" == "1" ]; then
			echo "[!] Note: $fifo not found (attempts $fails/$tries)"
		fi
		if [ "$fails" -gt "$tries" ]; then
			echo "[-] Error: $(basename "$fifo" .fifo) possibly crashed on launch"
			kill_all
			print_results
			exit 1
		fi
		sleep 0.1
	done
}

function wait_for_launch() {
	local fifo="$1"
	local baseDuration="$2"
	if [ "$arg_valgrind_memcheck" == "1" ]; then
		wait_for_fifo "$fifo" $((400 * baseDuration))
		sleep $((8 * baseDuration))
	else
		wait_for_fifo "$fifo" $((100 * baseDuration))
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

wait_for_launch client1.fifo 3

echo "[*] Start demo recording"
fifo "record server" server.fifo
fifo "record client1" client1.fifo

echo "[*] Launch client 2"
$tool ../DDNet \
	"cl_input_fifo client2.fifo;
	player_name client2;
	logfile client2.log;
	$client_args
	connect localhost:$port" > stdout_client2.txt 2> stderr_client2.txt || fail client2 "$?" &

wait_for_launch client2.fifo 5

# wait for tees to finish
sleep 15

echo "[*] Test chat and chat commands"
fifo "say hello world" client1.fifo
fifo "rcon_auth rcon" client1.fifo
sleep 1
fifo "$(
	tr -d '\n' << EOF
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
)" client1.fifo

sleep 10

echo "[*] Test rcon commands"
fifo "$(
	tr -d '\n' << EOF
rcon say hello from admin;
rcon broadcast test;
rcon status;
rcon echo test;
muteid 1 900 spam;
unban_all;
EOF
)" client1.fifo
sleep 5

echo "[*] Stop demo recording"
fifo "stoprecord" server.fifo
fifo "stoprecord" client1.fifo
sleep 1

echo "[*] Test map change"
fifo "rcon sv_map Tutorial" client1.fifo
if [ "$arg_valgrind_memcheck" == "1" ]; then
	sleep 60
else
	sleep 15
fi

echo "[*] Play demos"
fifo "play demos/server.demo" client1.fifo
fifo "play demos/client1.demo" client2.fifo
if [ "$arg_valgrind_memcheck" == "1" ]; then
	sleep 40
else
	sleep 10
fi

# Kill all processes first so all outputs are fully written
kill_all

if ! grep -qE '^[0-9]{4}-[0-9]{2}-[0-9]{2} ([0-9]{2}:){2}[0-9]{2} I chat: 0:-2:client1: hello world$' server.log; then
	touch fail_chat.txt
	echo "[-] Error: chat message not found in server log"
fi

if ! grep -q 'cmdlist' client1.log ||
	! grep -q 'pause' client1.log ||
	! grep -q 'rank' client1.log ||
	! grep -q 'points' client1.log; then
	touch fail_chatcommand.txt
	echo "[-] Error: did not find output of /cmdlist command"
fi

if ! grep -q "hello from admin" server.log; then
	touch fail_rcon.txt
	echo "[-] Error: admin message not found in server log"
fi

if ! grep -q "demo_player: Stopped playback" client1.log; then
	touch fail_demo_server.txt
	echo "[-] Error: demo playback of server demo in client 1 was not started/finished"
fi
if ! grep -q "demo_player: Stopped playback" client2.log; then
	touch fail_demo_client.txt
	echo "[-] Error: demo playback of client demo in client 2 was not started/finished"
fi

ranks="$(sqlite3 -init /dev/null -cmd '.timeout 10000' ddnet-server.sqlite < <(echo "select * from record_race;"))"
rank_time="$(echo "$ranks" | awk -F '|' '{ print "player:", $2, "time:", $4, "cps:", $6,$7,$8,$9,$10,$11,$12,$13,$14,$15,$16,$17,$18,$19,$20,$21,$22,$23,$24,$25,$26,$27,$28 }')"
expected_times="\
player: client2 time: 1020.98 cps: 0.02 0.1 0.2 0.26 0.32 600.36 600.42 600.46 600.5 1020.54 1020.58 1020.6 1020.64 1020.66 1020.7 1020.72 1020.76 1020.78 1020.8 1020.84 1020.86 1020.88 1020.9
player: client2 time: 1020.38 cps: 1021.34 0.02 0.04 0.04 0.06 600.08 600.1 600.12 600.12 1020.14 1020.16 1020.18 1020.2 1020.2 1020.22 1020.24 1020.26 1020.26 1020.28 1020.3 1020.3 1020.32 1020.34
player: client1 time: 6248.56 cps: 0.42 0.5 0.0 0.66 0.92 0.02 300.18 300.46 300.76 300.88 300.98 301.02 301.04 301.06 301.08 301.18 301.38 301.66 307.34 308.08 308.1 308.14 308.44
player: client1 time: 168300.5 cps: 0.02 0.06 0.12 15300.14 15300.18 30600.2 30600.22 45900.24 45900.26 61200.28 61200.3 76500.32 76500.34 91800.36 91800.36 107100.38 107100.4 122400.42 122400.42 137700.44 137700.45 137700.45 153000.48
player: client2 time: 302.02 cps: 0.42 0.5 0.0 0.66 0.92 0.02 300.18 300.46 300.76 300.88 300.98 301.16 301.24 301.28 301.3 301.86 301.96 0.0 0.0 0.0 0.0 0.0 0.0"

# require at least one rank in all cases. Exact finishes only with valgrind disabled
if [ "$ranks" == "" ]; then
	touch fail_ranks.txt
	echo "[-] Error: no ranks found in database"
elif [ "$arg_valgrind_memcheck" != "1" ] && [ "$rank_time" != "$expected_times" ]; then
	touch fail_ranks.txt
	echo "[-] Error: unexpected finish time"
	echo "  expected: $expected_times"
	echo "  got: $rank_time"
fi

print_results
