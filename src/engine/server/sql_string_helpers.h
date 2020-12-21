#ifndef ENGINE_SERVER_SQL_STRING_HELPERS_H
#define ENGINE_SERVER_SQL_STRING_HELPERS_H

namespace sqlstr {

void FuzzyString(char *pString, int Size);

// written number of added bytes
int EscapeLike(char *pDst, const char *pSrc, int DstSize);

void AgoTimeToString(int AgoTime, char *pAgoString, int Size);

} // namespace sqlstr

#endif
