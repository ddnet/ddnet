#ifndef ENGINE_SHARED_CHILLERBOT_LANGPARSER_H
#define ENGINE_SHARED_CHILLERBOT_LANGPARSER_H

class CLangParser
{
public:
	/*
		StrFindOrder

		Provide a haystay to search in and the amount of needles
		followed by needle strings as const char pointers

		Will return a pointer to the last found needle
		if all needles were found in order

		returns NULL otherwise
	*/
	const char *StrFindOrder(const char *pHaystack, int NumNeedles, ...);

	/*
		StrFindIndex

		Returns index of needle start in haystack if found
		Returns -1 if needle not found
	*/
	int StrFindIndex(const char *pHaystack, const char *pNeedle);

	bool IsAskToAsk(const char *pMessage, const char *pMessageAuthor = 0, char *pResponse = 0, int SizeOfResponse = 0);
	bool IsAskToAskGerman(const char *pMessage, const char *pMessageAuthor = 0, char *pResponse = 0, int SizeOfResponse = 0);

	bool IsGreeting(const char *pMsg);
	bool IsBye(const char *pMsg);
	bool IsInsult(const char *pMsg);
	bool IsQuestionWhy(const char *pMsg);
	bool IsQuestionHow(const char *pMsg);
	bool IsQuestionWhichWhat(const char *pMsg);
	bool IsQuestionWhoWhichWhat(const char *pMsg);
};

#endif
