#ifndef ENGINE_SHARED_CSV_H
#define ENGINE_SHARED_CSV_H

#include <base/system.h>

void CsvWrite(IOHANDLE File, int NumColumns, const char *const *ppColumns);

#endif // ENGINE_SHARED_CSV_H
