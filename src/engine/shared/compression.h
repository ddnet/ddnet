/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SHARED_COMPRESSION_H
#define ENGINE_SHARED_COMPRESSION_H

// variable int packing
class CVariableInt
{
public:
	enum
	{
		MAX_BYTES_PACKED = 5, // maximum number of bytes in a packed int
	};

	// Format: ESDDDDDD EDDDDDDD EDD... Extended, Data, Sign
	// Defined here so that callers packing many ints in a row do not pay a call per int.
	static unsigned char *Pack(unsigned char *pDst, int i, int DstSize)
	{
		if(DstSize <= 0)
			return nullptr;

		DstSize--;
		*pDst = 0;
		if(i < 0)
		{
			*pDst |= 0x40; // set sign bit
			i = ~i;
		}

		*pDst |= i & 0x3F; // pack 6bit into dst
		i >>= 6; // discard 6 bits
		while(i)
		{
			if(DstSize <= 0)
				return nullptr;
			*pDst |= 0x80; // set extend bit
			DstSize--;
			pDst++;
			*pDst = i & 0x7F; // pack 7bit
			i >>= 7; // discard 7 bits
		}

		pDst++;
		return pDst;
	}

	static const unsigned char *Unpack(const unsigned char *pSrc, int *pInOut, int SrcSize);

	static long Compress(const void *pSrc, int SrcSize, void *pDst, int DstSize);
	static long Decompress(const void *pSrc, int SrcSize, void *pDst, int DstSize);
};

#endif
