#ifndef ENGINE_CLIENT_SERVERBROWSER_FILTER_H
#define ENGINE_CLIENT_SERVERBROWSER_FILTER_H

namespace serverbrowser_filter
{

	enum class EFilterField
	{
		ALL,
		NAME,
		PLAYER,
		MAP,
		TYPE,
		ADDR,
	};

	struct SFilterToken
	{
		EFilterField m_Field;
		const char *m_pValue;
		bool m_Exact;
	};

	// Parses a single trimmed filter token. Recognizes a closed set of `key:`
	// mutates pToken.
	SFilterToken ParseFilterToken(char *pToken);

	// Reads the next whitespace-separated token from pStr, respescting double quotes
	// Returns the position past the token, or nullptr when no token remains.
	const char *NextWhitespaceToken(const char *pStr, char *pBuffer, int BufferSize);

	bool TokenMatches(const char *pField, const SFilterToken &Token);

}

#endif
