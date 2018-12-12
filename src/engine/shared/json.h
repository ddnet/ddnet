#ifndef ENGINE_SHARED_JSON_H
#define ENGINE_SHARED_JSON_H

char *EscapeJson(char *pBuffer, int BufferSize, const char *pString);
const char *JsonBool(bool Bool);

#endif // ENGINE_SHARED_JSON_H
