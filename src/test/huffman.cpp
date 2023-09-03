#include <gtest/gtest.h>

#include <base/system.h>
#include <engine/shared/huffman.h>

TEST(Huffman, CompressionShouldNotChangeData)
{
	CHuffman Huffman;
	Huffman.Init();

	unsigned char aInput[64];
	unsigned char aCompressed[2048];

	mem_zero(aInput, sizeof(aInput));
	mem_zero(aCompressed, sizeof(aCompressed));

	int Size = Huffman.Compress(aInput, sizeof(aInput), aCompressed, sizeof(aCompressed));

	unsigned char aDecompressed[2048];
	Huffman.Decompress(aCompressed, sizeof(aCompressed), aDecompressed, sizeof(aDecompressed));

	int match = mem_comp(aInput, aDecompressed, Size);
	EXPECT_EQ(match, 0);
}

TEST(Huffman, CompressionCompatible)
{
	CHuffman Huffman;
	Huffman.Init();

	unsigned char aInput[64];
	unsigned char aCompressed[2048];

	mem_zero(aInput, sizeof(aInput));
	mem_zero(aCompressed, sizeof(aCompressed));

	// compress 1-7 followed by a bunch of nullbytes
	for(int i = 0; i < 8; i++)
		aInput[i] = i;

	int Size = Huffman.Compress(aInput, sizeof(aInput), aCompressed, sizeof(aCompressed));

	unsigned char aExpected[] = {0x51, 0x58, 0x78, 0x76, 0x1B, 0xB7, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F, 0xc5, 0x0D};

	int match = mem_comp(aCompressed, aExpected, Size);
	EXPECT_EQ(match, 0) << "The compression is not compatible with older/other implementations anymore";
	EXPECT_EQ(Size, 15);
}
