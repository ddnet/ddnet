#!/usr/bin/env bash

# You need asciidoc installed
# Debian/Ubuntu: sudo apt install asciidoc
# http://asciidoc.org/

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