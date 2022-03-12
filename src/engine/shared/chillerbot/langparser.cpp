// Langugage parser by ChillerDragon

#include <base/system.h>

#include "langparser.h"

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

bool CLangParser::IsInsult(int ClientID, const char *pMsg, int MsgLen, int NameLen)
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
