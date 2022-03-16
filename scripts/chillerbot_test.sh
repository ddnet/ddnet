#!/bin/bash

if [ ! -f ./scripts/chillerbot_test.sh ] || [ ! -f CMakeLists.txt ]
then
	echo "Error: make sure your are in the root of the repo"
	exit 1
fi

function get_cores() {
    local cores
    if is_cmd nproc
    then
        cores="$(nproc)"
    elif is_cmd sysctl
    then
        cores="$(sysctl -n hw.ncpu)"
    fi
    if [ "$cores" -lt "1" ]
    then
        cores=1
    fi
    echo "$cores"
}

mkdir -p test-chillerbot

cd test-chillerbot || exit 1
cmake .. -DHEADLESS_CLIENT=ON || exit 1
make -j"$(get_cores)" || exit 1

function cleanup() {
	echo "shutting down test clients and server ..."
	sleep 1
	echo "shutdown" > server.fifo
	echo "quit" > client1.fifo
	echo "quit" > client2.fifo
}

trap cleanup EXIT

{
	echo $'add_path $CURRENTDIR'
	echo $'add_path $USERDIR'
	echo $'add_path $DATADIR'
} > storage.cfg

mkdir -p chillerbot/warlist/war/foo
echo foo > chillerbot/warlist/war/foo/names.txt
echo fooslongalt >> chillerbot/warlist/war/foo/names.txt
echo client1 >> chillerbot/warlist/war/foo/names.txt
echo "bullied me in school" > chillerbot/warlist/war/foo/reason.txt

./DDNet-Server "sv_input_fifo server.fifo;sv_port 17822;sv_spamprotection 0;sv_spam_mute_duration 0" > server.log &

# support chillerbot-zx
# shellcheck disable=SC2211
./chillerbot-* "cl_input_fifo client1.fifo;player_name client1;connect localhost:17822" > client1.log &
# shellcheck disable=SC2211
./chillerbot-* \
	"cl_input_fifo client2.fifo;
	player_name client2;
	connect localhost:17822;
	inp_mousesens 1000" > client2.log &

while [[ ! -p client1.fifo ]]
do
	sleep 1
done

ins=()
outs=()
ins+=('hi client2');outs+=('hi client2')
ins+=('client2 how are you?');outs+=('client1 good, and you? :)')
ins+=('wats ur inp_mousesens? client2');outs+=('client1 my current inp_mousesens is 1000')
ins+=('client2: why?');outs+=('client1 has war because: bullied me in school')
ins+=('client2: why kill my friend foo');outs+=('client1: foo has war because: bullied me in school')
ins+=('why do you war foo client2');outs+=('client1: foo has war because: bullied me in school')
# TODO: add str_endswith_nocase() https://github.com/chillerbot/chillerbot-ux/issues/58
# ins+=('why do you war fooslongalt CLIENT2');outs+=('client1 fooslongalt has war because: bullied me in school')

function run_tests() {
	local i
	local in_msg
	local out_msg
	local srv_log
	local line
	sleep 1
	for i in "${!ins[@]}"
	do
		in_msg="${ins[$i]}"
		out_msg="${outs[$i]}"
		echo "say $in_msg" > client1.fifo
		sleep 0.5
		echo "reply_to_last_ping" > client2.fifo
		sleep 0.5
		srv_log="$(tail server.log)"
		if ! echo "$srv_log" | grep -q "$out_msg"
		then
			echo ""
			echo "Error: missing expected message in server log"
			echo ""
			echo "Expected:"
			echo "  $out_msg"
			echo "Got:"
			while read -r line
			do
				echo "  $line"
			done < <(echo "$srv_log")
			echo ""
			exit 1
		else
			printf '.'
		fi
	done
}

run_tests

echo ""
echo "All tests passed :)"

