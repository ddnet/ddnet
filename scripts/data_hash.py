from pathlib import Path
import io
from hashlib import sha256

files = []
for p in Path("data/").rglob('*'):
	if p.is_dir():
		continue

	h = sha256()
	with p.open('rb') as f:
		while True:
			chunk = f.read(io.DEFAULT_BUFFER_SIZE)
			if not chunk:
				break
			h.update(chunk)
	digeststr = '{' + ','.join(["0x{:02x}".format(k) for k in h.digest()]) + '}'
	files += [(str(p.relative_to("data/")), digeststr)]

files.sort(key=lambda x: x[0])
pathlen = len(max(files, key=lambda x: len(x[0]))[0])

print("""\
#ifndef ENGINE_GENERATED_DATA_HASH_H
#define ENGINE_GENERATED_DATA_HASH_H

#include <base/tl/range.h>
#include <base/tl/algorithm.h>
#include <base/hash.h>
#include <base/system.h>

struct SHashEntry {{
	char m_aPath[{0}];
	SHA256_DIGEST m_Hash;

	bool operator<(const SHashEntry &Other) const {{ return str_comp(m_aPath, Other.m_aPath); }}
	bool operator<(const char *pOther) const {{ return str_comp(m_aPath, pOther); }}
	bool operator==(const char *pOther) const {{ return !str_comp(m_aPath, pOther); }}
}};

const int DATA_FILE_COUNT = {1};
const SHashEntry g_aDataHashes[{1}] = {{\
""".format(pathlen + 1, len(files)))

for f in files:
	print('\t{{"{}",{}}},'.format(f[0].replace("\\", "\\\\"), f[1]))

print("};\n")
print("""\
inline int GetCheckIndex(const char *pFilename)
{{
	auto all = plain_range_sorted<const SHashEntry>(&g_aDataHashes[0], &g_aDataHashes[{}]);
	auto r = ::find_binary(all, pFilename);

	if(r.empty())
		return -1;

	return &r.front() - &g_aDataHashes[0];
}}
#endif //ENGINE_GENERATED_DATA_HASH_H
""".format(len(files)))
