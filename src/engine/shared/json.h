#ifndef ENGINE_SHARED_JSON_H
#define ENGINE_SHARED_JSON_H

#include <engine/external/json-parser/json.h>

const struct _json_value *json_object_get(const json_value *pObject, const char *pIndex);
const struct _json_value *json_array_get(const json_value *pArray, int Index);
int json_array_length(const json_value *pArray);
const char *json_string_get(const json_value *pString);
int json_int_get(const json_value *pInteger);
int json_boolean_get(const json_value *pBoolean);

char *EscapeJson(char *pBuffer, int BufferSize, const char *pString);
const char *JsonBool(bool Bool);

#endif // ENGINE_SHARED_JSON_H
