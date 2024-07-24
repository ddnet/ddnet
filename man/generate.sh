#!/usr/bin/env bash

command -v a2x > /dev/null 2>&1 || {
	echo >&2 "You need asciidoc installed"
	echo >&2 "Debian/Ubuntu: sudo apt install asciidoc"
	echo >&2 "http://asciidoc.org/"
	exit 1
}

set -ex

# To generate all possible formats uncomment this and be sure to have all the needed tools:
: << 'EOF'
for FORMAT in 'epub' 'htmlhelp' 'manpage' 'pdf' 'text' 'xhtml' 'dvi' 'ps' 'tex' 'docbook'
do
    a2x --doctype manpage --format $FORMAT DDNet.adoc
    a2x --doctype manpage --format $FORMAT DDNetServer.adoc
done
EOF

a2x --doctype manpage --format manpage DDNet.adoc
a2x --doctype manpage --format manpage DDNetServer.adoc
