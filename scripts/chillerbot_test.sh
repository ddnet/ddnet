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
ins+=('hi client2');outs+=('hi client1')
ins+=('client2 how are you?');outs+=('client1 good, and you? :)')
ins+=('wats ur inp_mousesens? client2');outs+=('client1 my current inp_mousesens is 1000')
ins+=('client2: why?');outs+=('client1 has war because: bullied me in school')
ins+=('client2: why kill my friend foo');outs+=('client1: foo has war because: bullied me in school')
ins+=('can i ask you smtin client2');outs+=('client1 yes but I might not answer')
ins+=('can i ask you something client2');outs+=('client1 yes but I might not answer')
ins+=('why do you war foo client2');outs+=('client1: foo has war because: bullied me in school')
ins+=('kann ich dich was fragen client2?');outs+=('client1 frag! Aber es kann sein, dass ich nicht antworte.')
ins+=('can me ask question client2');outs+=('client1 yes but I might not answer')
ins+=('kan i di was frage client2?');outs+=('client1 frag! Aber es kann sein, dass ich nicht antworte.')
ins+=('u have nade client2?');outs+=('client1 No I got those weapons: hammer, gun')
ins+=('client2 hast du hammer?');outs+=('client1 Yes I got those weapons: hammer, gun')
# not sure if that should be covered (seen it 2 times in game now -,-)
# ins+=('I HAVE A QUESTION client2');outs+=('client1 okay')
ins+=('why do you war fooslongalt CLIENT2');outs+=('client1: fooslongalt has war because: bullied me in school')
ins+=('why do you war fooslongalt cLIEnT2???????');outs+=('client1: fooslongalt has war because: bullied me in school')

function run_tests() {
	local i
	local in_msg
	local out_msg
	local srv_log
	local line
	local attempts
	sleep 1
	for i in "${!ins[@]}"
	do
		in_msg="${ins[$i]}"
		out_msg="${outs[$i]}"
		echo "say $in_msg" > client1.fifo
		sleep 0.5
		srv_log="$(tail server.log)"
		while ! echo "$srv_log" | grep -qF "$in_msg"
		do
			# error sending message
			# resend
			printf '!'
			echo "say $in_msg" > client1.fifo
			sleep 0.5
			srv_log="$(tail server.log)"
		done
		echo "reply_to_last_ping" > client2.fifo
		sleep 0.5
		attempts=0
		while grep -F '[chat]' server.log | grep -v NOREPLY | tail -n 1 | grep -q '[0-9]:client1: '
		do
			if [ "$attempts" -gt "0" ]
			then
				break
			fi
			attempts="$((attempts+1))"
			# last message is from client1
			# resend
			# also resend the input message because reply to last ping got wiped
			echo "say $in_msg" > client1.fifo
			sleep 0.5
			printf '!'
			echo "reply_to_last_ping" > client2.fifo
			sleep 0.5
		done
		sleep 0.5
		srv_log="$(grep -F '[chat]' server.log | tail -n2)"
		if ! echo "$srv_log" | grep -qF "$out_msg"
		then
			echo ""
			echo "Error: missing expected message in server log"
			echo ""
			echo "Sent:"
			echo "  $in_msg"
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

