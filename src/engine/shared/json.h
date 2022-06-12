#ifndef ENGINE_SHARED_JSON_H
#define ENGINE_SHARED_JSON_H

#include <engine/external/json-parser/json.h>

const struct _json_value *json_object_get(const json_value *object, const char *index);
const struct _json_value *json_array_get(const json_value *array, int index);
int json_array_length(const json_value *array);
const char *json_string_get(const json_value *string);
int json_int_get(const json_value *integer);
int json_boolean_get(const json_value *boolean);

char *EscapeJson(char *pBuffer, int BufferSize, const char *pString);
const char *JsonBool(bool Bool);

#endif // ENGINE_SHARED_JSON_H
