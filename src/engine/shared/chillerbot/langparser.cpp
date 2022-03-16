// Langugage parser by ChillerDragon

#include <cstdarg>
#include <cstdio>

#include <base/system.h>

#include "langparser.h"

const char *CLangParser::StrFindOrder(const char *pHaystack, int NumNeedles, ...)
{
	va_list args;
	va_start(args, NumNeedles);
	// dbg_msg("langparser", "strfindorder called with %d needles", NumNeedles);
	const char *pSearch = pHaystack;
	bool Found = true;
	for(int i = 0; i < NumNeedles; i++)
	{
		const char *pNeedle = va_arg(args, const char *);
		// dbg_msg("needle", "%s", pNeedle);
		if(!(pSearch = str_find_nocase(pSearch, pNeedle)))
		{
			Found = false;
			break;
		}
	}
	va_end(args);
	return Found ? pSearch : NULL;
}

bool CLangParser::IsAskToAskGerman(const char *pMessage, const char *pMessageAuthor, char *pResponse, int SizeOfResponse)
{
	if(pResponse)
		pResponse[0] = '\0';
	// kann ich dich etwas
	// kan i di was
	const char *pCanSomething = StrFindOrder(pMessage, 2, "kan", "was");
	if(!pCanSomething)
		return false;
	if(str_find_nocase(pCanSomething, "frag"))
	{
		if(pResponse)
			str_format(pResponse, SizeOfResponse, "%s frag! Aber es kann sein, dass ich nicht antworte.", pMessageAuthor ? pMessageAuthor : "");
		return true;
	}
	return false;
}

bool CLangParser::IsAskToAsk(const char *pMessage, const char *pMessageAuthor, char *pResponse, int SizeOfResponse)
{
	if(pResponse)
		pResponse[0] = '\0';
	const char *pCanAsk = StrFindOrder(pMessage, 2, "can", "ask");
	if(!pCanAsk)
		return IsAskToAskGerman(pMessage, pMessageAuthor, pResponse, SizeOfResponse);
	if(str_find_nocase(pCanAsk, "smt") ||
		str_find_nocase(pCanAsk, "sume") ||
		str_find_nocase(pCanAsk, "some") ||
		str_find_nocase(pCanAsk, "thing") ||
		str_find_nocase(pCanAsk, "question"))
	{
		if(pResponse)
			str_format(pResponse, SizeOfResponse, "%s yes but I might not answer", pMessageAuthor ? pMessageAuthor : "");
		return true;
	}
	return false;
}

bool CLangParser::IsGreeting(const char *pMsg)
{
	const char aGreetings[][128] = {
		"hi",
		"hay",
		"hey",
		"heey",
		"heeey",
		"heeeey",
		"haay",
		"haaay",
		"haaaay",
		"haaaaay",
		"henlo",
		"helo",
		"hello",
		"hallo",
		"hellu",
		"hallu",
		"helu",
		"henlu",
		"hemnlo",
		"herro",
		"ahoi",
		"moin",
		"servus",
		"guten tag",
		"good morning",
		"priviet",
		"ola",
		"ay",
		"ayy",
		"ayyy",
		"ayyyy",
		"aayyy",
		"aaay",
		"aaaay",
		"yo",
		"yoyo",
		"yoyoyo",
		"yoo",
		"yooo",
		"salut",
		"slt",
		"sup",
		"szia",
		"selam"};
	for(const auto &aGreeting : aGreetings)
	{
		const char *pHL = str_find_nocase(pMsg, aGreeting);
		while(pHL)
		{
			int Length = str_length(aGreeting);

			if((pMsg == pHL || pHL[-1] == ' ') && (pHL[Length] == 0 || pHL[Length] == ' ' || pHL[Length] == '.' || pHL[Length] == '!' || pHL[Length] == ',' || pHL[Length] == '1' || pHL[Length] == pHL[Length - 1]))
				return true;
			pHL = str_find_nocase(pHL + 1, aGreeting);
		}
	}
	return false;
}

bool CLangParser::IsBye(const char *pMsg)
{
	const char aByes[][128] = {
		"bb",
		"see you",
		"leaving",
		"have a nice day",
		"have an nice day",
		"quit",
		"bye"};
	for(const auto &aBye : aByes)
	{
		const char *pHL = str_find_nocase(pMsg, aBye);
		while(pHL)
		{
			int Length = str_length(aBye);

			if((pMsg == pHL || pHL[-1] == ' ') && (pHL[Length] == 0 || pHL[Length] == ' ' || pHL[Length] == '.' || pHL[Length] == '!' || pHL[Length] == ',' || pHL[Length] == '1' || pHL[Length] == pHL[Length - 1]))
				return true;
			pHL = str_find_nocase(pHL + 1, aBye);
		}
	}
	return false;
}

bool CLangParser::IsInsult(const char *pMsg)
{
	const char aByes[][128] = {
		"DELETE THE GAME",
		"GAYASS",
		"NIGGER",
		"NIGGA",
		"GAYNIGGER",
		"GAYNIGGA",
		"your mother",
		"ur mom",
		"fuck your",
		"fucking idiot",
		"piece of shit"};
	for(const auto &aBye : aByes)
	{
		const char *pHL = str_find_nocase(pMsg, aBye);
		while(pHL)
		{
			int Length = str_length(aBye);

			if((pMsg == pHL || pHL[-1] == ' ') && (pHL[Length] == 0 || pHL[Length] == ' ' || pHL[Length] == '.' || pHL[Length] == '!' || pHL[Length] == ',' || pHL[Length] == '1' || pHL[Length] == pHL[Length - 1]))
				return true;
			pHL = str_find_nocase(pHL + 1, aBye);
		}
	}
	// /me
	if(str_startswith(pMsg, "### '"))
	{
		if(str_endswith(pMsg, "' DELETED"))
			return true;
		if(str_endswith(pMsg, "' RRRRREEEEEEEEEEEEEEEEEEEEEEEEE"))
			return true;
	}
	return false;
}

bool CLangParser::IsQuestionWhy(const char *pMsg)
{
	const char aWhys[][128] = {
		"warum",
		"whyy",
		"whyyy",
		"whyyyy",
		"w hyyyy",
		"whhy",
		"whhyy",
		"whhyyy",
		"wtf?",
		"why"};
	for(const auto &pWhy : aWhys)
	{
		const char *pHL = str_find_nocase(pMsg, pWhy);
		while(pHL)
		{
			int Length = str_length(pWhy);

			if((pMsg == pHL || pHL[-1] == ' ') && (pHL[Length] == 0 || pHL[Length] == ' ' || pHL[Length] == '.' || pHL[Length] == '!' || pHL[Length] == ',' || pHL[Length] == '?' || pHL[Length] == pHL[Length - 1]))
				return true;
			pHL = str_find_nocase(pHL + 1, pWhy);
		}
	}
	return false;
}
