#!/bin/bash
#set -x
result=

dil_path=$1

result=$(find "$2" -iname '*.png' -print0 | while IFS= read -r -d $'\0' file; do
	new_file=$(mktemp --tmpdir "$(basename "$file" .png).XXX.png")
	cp "$file" "$new_file"
	convert "$new_file" "${new_file}_old.bmp" > /dev/null
	"${dil_path}"/dilate "$new_file" > /dev/null
	convert "$new_file" "${new_file}_new.bmp" > /dev/null
	orig_hash=$(identify -quiet -format "%#" "${new_file}_old.bmp")
	new_hash=$(identify -quiet -format "%#" "${new_file}_new.bmp")
	rm "$new_file"
	rm "${new_file}_old.bmp"
	rm "${new_file}_new.bmp"
	if [ "$orig_hash" != "$new_hash" ]; then
		echo "$file is not dilated"
	fi
done)

if [[ "$result" != "" ]]; then
	echo -n "$result"
	exit 1
fi

exit 0
