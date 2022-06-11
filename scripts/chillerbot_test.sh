#!/bin/bash

if [ ! -f ./scripts/chillerbot_test.sh ] || [ ! -f CMakeLists.txt ]
then
	echo "Error: make sure your are in the root of the repo"
	exit 1
fi

function is_cmd() {
    [ -x "$(command -v "$1")" ] && return 0
}

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
./chillerbot-* \
	"cl_input_fifo client1.fifo;
	cl_chat_spam_filter 0;
	player_name client1;
	cl_tabbed_out_msg 0;
	connect localhost:17822" > client1.log &
# shellcheck disable=SC2211
./chillerbot-* \
	'cl_input_fifo client2.fifo;
	cl_chat_spam_filter 0;
	player_name client2;
	cl_tabbed_out_msg 0;
	connect localhost:17822;
	player_clan "Chilli.*";
	inp_mousesens 1000' > client2.log &

while [[ ! -p client1.fifo ]]
do
	sleep 1
done

ins=()
outs=()
ins+=('client2: ?');outs+=('client1 has war because: bullied me in school')
ins+=('client2: who do you war?');outs+=('client1 1 of my 1 enemies are online: client1')
ins+=('client2: do you even have any friends?');outs+=('client1 currently 0 of my 0 friends are connected')
ins+=('client2: why foo war?');outs+=('client1: foo has war because: bullied me in school')
ins+=('client2: warum hat       foo war?');outs+=('client1: foo has war because: bullied me in school')
ins+=('client2 hast du eig war mit samplename300 ?');outs+=("client1: 'samplename300' is not on my warlist.")
ins+=('client2 do you war foo?');outs+=('client1: foo has war because: bullied me in school')
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
ins+=('how do you chat so fast?? client2');outs+=('client1 I bound the chillerbot-ux command "reply_to_last_ping" to automate chat')
ins+=('client2: how do always drop money?');outs+=('client1 I auto drop money using "auto_drop_money" in chillerbot-ux')
ins+=('client2 why is my name red');outs+=('client1 has war because: bullied me in school')
ins+=('client2 ah nice where can i download chillerbot?');outs+=('client1 I use chillerbot-ux ( https://chillerbot.github.io )')
ins+=('client2 me is join your clan? yes?');outs+=('client1 Chilli.* is a fun clan everybody that uses the skin greensward can join')
ins+=('client2: i is enemi?');outs+=('client1 you have war because: bullied me in school')

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
			if [ "$srv_log" == "" ]
			then
				tail server.log
			fi
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

