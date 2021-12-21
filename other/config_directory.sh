#!/bin/sh
case "$(uname -s)" in
	CYGWIN*|MINGW*|MSYS*)
		if [ -d "$APPDATA/DDNet/" ]; then
			explorer "$APPDATA/DDNet/"
		else
			explorer "$APPDATA/Teeworlds/"
		fi;;
	Darwin*)
		if [ -d "$HOME/Library/Application Support/DDNet/" ]; then
			open "$HOME/Library/Application Support/DDNet/"
		else
			open "$HOME/Library/Application Support/Teeworlds/"
		fi;;
	*)
		if [ -d "$HOME/.ddnet/" ]; then
			xdg-open "$HOME/.ddnet/"
		else
			xdg-open "$HOME/.teeworlds/"
		fi;;
esac
