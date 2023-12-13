#!/bin/bash

arg_is_dry=0

if [ "$1" == "--dry-run" ]
then
	arg_is_dry=1
fi

tmp() {
	mkdir -p scripts/
	echo "scripts/tmp.swp"
}

gen_configs() {
	local cfg
	local desc
	local cmd
	# shellcheck disable=SC2016
	echo '+ `sv_gametype` Game type (gctf, ictf)'
	while read -r cfg
	do
		desc="$(echo "$cfg" | cut -d',' -f7- | cut -d'"' -f2-)"
		desc="${desc::-2}"
		cmd="$(echo "$cfg" | cut -d',' -f2 | xargs)"
		echo "+ \`$cmd\` $desc"
	done < <(grep '^MACRO_CONFIG_INT' src/engine/shared/variables_insta.h)
}

insert_at() {
	# insert_at [from_pattern] [to_pattern] [new content] [filename]
	local from_pattern="$1"
	local to_pattern="$2"
	local content="$3"
	local filename="$4"
	local from_ln
	local to_ln
	from_ln="$(grep -n "$from_pattern" "$filename" | cut -d':' -f1 | head -n1)"
	from_ln="$((from_ln+1))"
	to_ln="$(tail -n +"$from_ln" "$filename" | grep -n "$to_pattern" | cut -d':' -f1 | head -n1)"
	to_ln="$((from_ln+to_ln-2))"

	{
		head -n "$from_ln" "$filename"
		echo "$content"
		tail -n +"$to_ln" "$filename"
	} > "$(tmp)"
	if [ "$arg_is_dry" == "1" ]
	then
		if [ "$(cat "$(tmp)")" != "$(cat "$filename")" ]
		then
			echo "Error: missing docs for $filename"
			echo "       run ./scripts/gendocs_instagib.sh"
			git diff --no-index --color "$(tmp)" "$filename"
			exit 1
		fi
	else
		mv "$(tmp)" "$filename"
	fi
}

insert_at '^# Configs$' '^# ' "$(gen_configs)" README.md

[[ -f "$(tmp)" ]] && rm "$(tmp)"
exit 0
