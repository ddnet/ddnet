/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "huffman.h"

#include <base/system.h>

#include <algorithm>

const unsigned CHuffman::ms_aFreqTable[HUFFMAN_MAX_SYMBOLS] = {
	1 << 30, 4545, 2657, 431, 1950, 919, 444, 482, 2244, 617, 838, 542, 715, 1814, 304, 240, 754, 212, 647, 186,
	283, 131, 146, 166, 543, 164, 167, 136, 179, 859, 363, 113, 157, 154, 204, 108, 137, 180, 202, 176,
	872, 404, 168, 134, 151, 111, 113, 109, 120, 126, 129, 100, 41, 20, 16, 22, 18, 18, 17, 19,
	16, 37, 13, 21, 362, 166, 99, 78, 95, 88, 81, 70, 83, 284, 91, 187, 77, 68, 52, 68,
	59, 66, 61, 638, 71, 157, 50, 46, 69, 43, 11, 24, 13, 19, 10, 12, 12, 20, 14, 9,
	20, 20, 10, 10, 15, 15, 12, 12, 7, 19, 15, 14, 13, 18, 35, 19, 17, 14, 8, 5,
	15, 17, 9, 15, 14, 18, 8, 10, 2173, 134, 157, 68, 188, 60, 170, 60, 194, 62, 175, 71,
	148, 67, 167, 78, 211, 67, 156, 69, 1674, 90, 174, 53, 147, 89, 181, 51, 174, 63, 163, 80,
	167, 94, 128, 122, 223, 153, 218, 77, 200, 110, 190, 73, 174, 69, 145, 66, 277, 143, 141, 60,
	136, 53, 180, 57, 142, 57, 158, 61, 166, 112, 152, 92, 26, 22, 21, 28, 20, 26, 30, 21,
	32, 27, 20, 17, 23, 21, 30, 22, 22, 21, 27, 25, 17, 27, 23, 18, 39, 26, 15, 21,
	12, 18, 18, 27, 20, 18, 15, 19, 11, 17, 33, 12, 18, 15, 19, 18, 16, 26, 17, 18,
	9, 10, 25, 22, 22, 17, 20, 16, 6, 16, 15, 20, 14, 18, 24, 335, 1517};

struct CHuffmanConstructNode
{
	unsigned short m_NodeId;
	int m_Frequency;
};

static bool CompareNodesByFrequencyDesc(const CHuffmanConstructNode *pNode1, const CHuffmanConstructNode *pNode2)
{
	return pNode2->m_Frequency < pNode1->m_Frequency;
}

void CHuffman::Setbits_r(CNode *pNode, int Bits, unsigned Depth)
{
	if(pNode->m_aLeaves[1] != 0xffff)
		Setbits_r(&m_aNodes[pNode->m_aLeaves[1]], Bits | (1 << Depth), Depth + 1);
	if(pNode->m_aLeaves[0] != 0xffff)
		Setbits_r(&m_aNodes[pNode->m_aLeaves[0]], Bits, Depth + 1);

	if(pNode->m_NumBits)
	{
		pNode->m_Bits = Bits;
		pNode->m_NumBits = Depth;
	}
}

void CHuffman::ConstructTree(const unsigned *pFrequencies)
{
	CHuffmanConstructNode aNodesLeftStorage[HUFFMAN_MAX_SYMBOLS];
	CHuffmanConstructNode *apNodesLeft[HUFFMAN_MAX_SYMBOLS];
	int NumNodesLeft = HUFFMAN_MAX_SYMBOLS;

	// add the symbols
	for(int i = 0; i < HUFFMAN_MAX_SYMBOLS; i++)
	{
		m_aNodes[i].m_NumBits = 0xFFFFFFFF;
		m_aNodes[i].m_Symbol = i;
		m_aNodes[i].m_aLeaves[0] = 0xffff;
		m_aNodes[i].m_aLeaves[1] = 0xffff;

		if(i == HUFFMAN_EOF_SYMBOL)
			aNodesLeftStorage[i].m_Frequency = 1;
		else
			aNodesLeftStorage[i].m_Frequency = pFrequencies[i];
		aNodesLeftStorage[i].m_NodeId = i;
		apNodesLeft[i] = &aNodesLeftStorage[i];
	}

	m_NumNodes = HUFFMAN_MAX_SYMBOLS;

	// construct the table
	while(NumNodesLeft > 1)
	{
		std::stable_sort(apNodesLeft, apNodesLeft + NumNodesLeft, CompareNodesByFrequencyDesc);

		m_aNodes[m_NumNodes].m_NumBits = 0;
		m_aNodes[m_NumNodes].m_aLeaves[0] = apNodesLeft[NumNodesLeft - 1]->m_NodeId;
		m_aNodes[m_NumNodes].m_aLeaves[1] = apNodesLeft[NumNodesLeft - 2]->m_NodeId;
		apNodesLeft[NumNodesLeft - 2]->m_NodeId = m_NumNodes;
		apNodesLeft[NumNodesLeft - 2]->m_Frequency = apNodesLeft[NumNodesLeft - 1]->m_Frequency + apNodesLeft[NumNodesLeft - 2]->m_Frequency;

		m_NumNodes++;
		NumNodesLeft--;
	}

	// set start node
	m_pStartNode = &m_aNodes[m_NumNodes - 1];

	// build symbol bits
	Setbits_r(m_pStartNode, 0, 0);
}

void CHuffman::Init(const unsigned *pFrequencies)
{
	// make sure to cleanout every thing
	mem_zero(m_aNodes, sizeof(m_aNodes));
	mem_zero(m_apDecodeLut, sizeof(m_apDecodeLut));
	m_pStartNode = nullptr;
	m_NumNodes = 0;

	// construct the tree
	ConstructTree(pFrequencies);

	// build decode LUT
	for(int i = 0; i < HUFFMAN_LUTSIZE; i++)
	{
		unsigned Bits = i;
		int k;
		CNode *pNode = m_pStartNode;
		for(k = 0; k < HUFFMAN_LUTBITS; k++)
		{
			pNode = &m_aNodes[pNode->m_aLeaves[Bits & 1]];
			Bits >>= 1;

			if(!pNode)
				break;

			if(pNode->m_NumBits)
			{
				m_apDecodeLut[i] = pNode;
				break;
			}
		}

		if(k == HUFFMAN_LUTBITS)
			m_apDecodeLut[i] = pNode;
	}
}

//***************************************************************
int CHuffman::Compress(const void *pInput, int InputSize, void *pOutput, int OutputSize) const
{
	// this macro loads a symbol for a byte into bits and bitcount
#define HUFFMAN_MACRO_LOADSYMBOL(Sym) \
	do \
	{ \
		Bits |= m_aNodes[Sym].m_Bits << Bitcount; \
		Bitcount += m_aNodes[Sym].m_NumBits; \
	} while(0)

	// this macro writes the symbol stored in bits and bitcount to the dst pointer
#define HUFFMAN_MACRO_WRITE() \
	do \
	{ \
		while(Bitcount >= 8) \
		{ \
			*pDst++ = (unsigned char)(Bits & 0xff); \
			if(pDst == pDstEnd) \
				return -1; \
			Bits >>= 8; \
			Bitcount -= 8; \
		} \
	} while(0)

	// setup buffer pointers
	const unsigned char *pSrc = (const unsigned char *)pInput;
	const unsigned char *pSrcEnd = pSrc + InputSize;
	unsigned char *pDst = (unsigned char *)pOutput;
	unsigned char *pDstEnd = pDst + OutputSize;

	// symbol variables
	unsigned Bits = 0;
	unsigned Bitcount = 0;

	// make sure that we have data that we want to compress
	if(InputSize)
	{
		// {A} load the first symbol
		int Symbol = *pSrc++;

		while(pSrc != pSrcEnd)
		{
			// {B} load the symbol
			HUFFMAN_MACRO_LOADSYMBOL(Symbol);

			// {C} fetch next symbol, this is done here because it will reduce dependency in the code
			Symbol = *pSrc++;

			// {B} write the symbol loaded at
			HUFFMAN_MACRO_WRITE();
		}

		// write the last symbol loaded from {C} or {A} in the case of only 1 byte input buffer
		HUFFMAN_MACRO_LOADSYMBOL(Symbol);
		HUFFMAN_MACRO_WRITE();
	}

	// write EOF symbol
	HUFFMAN_MACRO_LOADSYMBOL(HUFFMAN_EOF_SYMBOL);
	HUFFMAN_MACRO_WRITE();

	// write out the last bits
	*pDst++ = Bits;

	// return the size of the output
	return (int)(pDst - (const unsigned char *)pOutput);

	// remove macros
#undef HUFFMAN_MACRO_LOADSYMBOL
#undef HUFFMAN_MACRO_WRITE
}

//***************************************************************
int CHuffman::Decompress(const void *pInput, int InputSize, void *pOutput, int OutputSize) const
{
	// setup buffer pointers
	unsigned char *pDst = (unsigned char *)pOutput;
	unsigned char *pSrc = (unsigned char *)pInput;
	unsigned char *pDstEnd = pDst + OutputSize;
	unsigned char *pSrcEnd = pSrc + InputSize;

	unsigned Bits = 0;
	unsigned Bitcount = 0;

	const CNode *pEof = &m_aNodes[HUFFMAN_EOF_SYMBOL];

	while(true)
	{
		// {A} try to load a node now, this will reduce dependency at location {D}
		const CNode *pNode = nullptr;
		if(Bitcount >= HUFFMAN_LUTBITS)
			pNode = m_apDecodeLut[Bits & HUFFMAN_LUTMASK];

		// {B} fill with new bits
		while(Bitcount < 24 && pSrc != pSrcEnd)
		{
			Bits |= (*pSrc++) << Bitcount;
			Bitcount += 8;
		}

		// {C} load symbol now if we didn't that earlier at location {A}
		if(!pNode)
			pNode = m_apDecodeLut[Bits & HUFFMAN_LUTMASK];

		if(!pNode)
			return -1;

		// {D} check if we hit a symbol already
		if(pNode->m_NumBits)
		{
			// remove the bits for that symbol
			Bits >>= pNode->m_NumBits;
			Bitcount -= pNode->m_NumBits;
		}
		else
		{
			// remove the bits that the lut checked up for us
			Bits >>= HUFFMAN_LUTBITS;
			Bitcount -= HUFFMAN_LUTBITS;

			// walk the tree bit by bit
			while(true)
			{
				// traverse tree
				pNode = &m_aNodes[pNode->m_aLeaves[Bits & 1]];

				// remove bit
				Bitcount--;
				Bits >>= 1;

				// check if we hit a symbol
				if(pNode->m_NumBits)
					break;

				// no more bits, decoding error
				if(Bitcount == 0)
					return -1;
			}
		}

		// check for eof
		if(pNode == pEof)
			break;

		// output character
		if(pDst == pDstEnd)
			return -1;
		*pDst++ = pNode->m_Symbol;
	}

	// return the size of the decompressed buffer
	return (int)(pDst - (const unsigned char *)pOutput);
}
