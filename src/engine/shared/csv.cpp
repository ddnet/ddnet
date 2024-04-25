#include "csv.h"

#include <base/system.h>

void CsvWrite(IOHANDLE File, int NumColumns, const char *const *ppColumns)
{
	for(int i = 0; i < NumColumns; i++)
	{
		if(i != 0)
		{
			io_write(File, ",", 1);
		}
		const char *pColumn = ppColumns[i];
		int ColumnLength = str_length(pColumn);
		if(!str_find(pColumn, "\"") && !str_find(pColumn, ","))
		{
			io_write(File, pColumn, ColumnLength);
			continue;
		}

		int Start = 0;
		io_write(File, "\"", 1);
		for(int j = 0; j < ColumnLength; j++)
		{
			if(pColumn[j] == '"')
			{
				if(Start != j)
				{
					io_write(File, pColumn + Start, j - Start);
				}
				Start = j + 1;
				io_write(File, "\"\"", 2);
			}
		}
		if(Start != ColumnLength)
		{
			io_write(File, pColumn + Start, ColumnLength - Start);
		}
		io_write(File, "\"", 1);
	}
	io_write_newline(File);
}
