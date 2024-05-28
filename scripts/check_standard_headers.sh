#!/usr/bin/env bash

# List of C headers and their corresponding C++ headers
c_headers=(assert complex ctype errno fenv float inttypes iso646 limits locale math setjmp signal stdarg stdbool stddef stdint stdio stdlib string tgmath time wchar wctype)
c_headers_map=(cassert complex cctype cerrno cfenv cfloat cinttypes ciso646 climits clocale cmath csetjmp csignal cstdarg cstdbool cstddef cstdint cstdio cstdlib cstring ctgmath ctime cwchar cwctype)

# Create regex dynamically from the array to match any C header
c_headers_regex=$(printf "%s" "${c_headers[*]}" | tr ' ' '|')

# Find all C++ source and header files
files=$(find ./src -type f \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' \) ! -path "./src/engine/external/*")

error_found=0

# Check each source file for C headers
for file in $files; do
	# First check if the file includes any C headers for more efficiency when no C header is used
	if grep -E "#include\s+<($c_headers_regex)\.h>" "$file" > /dev/null; then
		# Check each C header individually to print an error message with the appropriate replacement C++ header
		for ((i = 0; i < ${#c_headers[@]}; i++)); do
			if grep -E "#include\s+<${c_headers[i]}\.h>" "$file" > /dev/null; then
				echo "Error: '$file' includes C header '${c_headers[i]}.h'. Include the C++ header '${c_headers_map[i]}' instead."
			fi
		done
		error_found=1
	fi
done

if [ $error_found -eq 1 ]; then
	exit 1
fi

echo "Success: No standard C headers are used."
