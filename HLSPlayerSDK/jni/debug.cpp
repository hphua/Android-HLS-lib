#include <debug.h>
#include <stdlib.h>

void LogBytes(const char* header, const char* footer, char* bytes, int size)
{
	int rowLen = 16;
	int rowCount = size / rowLen;
	int extraRow = size % rowLen;
	int o = 0;

	LOGE("%s: size = %d", header, size);

	for (int i = 0; i < rowCount; ++i)
	{
		o = i * rowLen;
		LOGE("%x: %x %x %x %x %x %x %x %x  %x %x %x %x %x %x %x %x", o ,*(bytes + (0 + o)),*(bytes + (1 + o)),*(bytes + (2 + o)),*(bytes + (3 + o)),
																		*(bytes + (4 + o)),*(bytes + (5 + o)),*(bytes + (6 + o)),*(bytes + (7 + o)),
																		*(bytes + (8 + o)),*(bytes + (9 + o)),*(bytes + (10 + o)),*(bytes + (11 + o)),
																		*(bytes + (12 + o)),*(bytes + (13 + o)),*(bytes + (14 + o)),*(bytes + (15 + o))
		);
	}

	if (extraRow > 0)
	{
		if (rowCount > 0) o += 16;
		char xb[rowLen];
		memset(xb, 0, rowLen);
		memcpy(xb, bytes + o, extraRow);
		LOGE("%x: %x %x %x %x %x %x %x %x  %x %x %x %x %x %x %x %x", o, *(xb + 0),*(xb + 1 ),*(xb + 2 ),*(xb + 3),
																		*(xb + 4),*(xb + 5),*(xb + 6),*(xb + 7),
																		*(xb + 8),*(xb + 9 ),*(xb + 10 ),*(xb + 11),
																		*(xb + 12),*(xb + 13),*(xb + 14),*(xb + 15));
	}

	LOGE("%s", footer);
}
