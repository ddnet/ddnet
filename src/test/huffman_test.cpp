#include <base/mem.h>

#include <engine/shared/huffman.h>

#include <gtest/gtest.h>

TEST(Huffman, CompressionShouldNotChangeData)
{
	CHuffman Huffman;
	Huffman.Init();

	unsigned char aInput[64];
	unsigned char aCompressed[2048];
	unsigned char aDecompressed[2048];

	for(int InputMod = 0x00; InputMod <= 0xFFFF; ++InputMod)
	{
		mem_zero(aInput, sizeof(aInput));
		mem_zero(aCompressed, sizeof(aCompressed));
		mem_zero(aDecompressed, sizeof(aDecompressed));
		aInput[0] = InputMod & 0xFF;
		aInput[1] = (InputMod >> 8) & 0xFF;

		const int CompressedSize = Huffman.Compress(aInput, sizeof(aInput), aCompressed, sizeof(aCompressed));
		const int MaxSize = InputMod <= 0xFF ? 12 : 14;
		ASSERT_GE(CompressedSize, 10);
		ASSERT_LE(CompressedSize, MaxSize);

		const int UncompressedSize = Huffman.Decompress(aCompressed, CompressedSize, aDecompressed, sizeof(aDecompressed));
		ASSERT_EQ(UncompressedSize, (int)sizeof(aInput));
		EXPECT_EQ(mem_comp(aInput, aDecompressed, UncompressedSize), 0);
	}
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

	const int Size = Huffman.Compress(aInput, sizeof(aInput), aCompressed, sizeof(aCompressed));

	const unsigned char aExpected[] = {0x51, 0x58, 0x78, 0x76, 0x1B, 0xB7, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F, 0xc5, 0x0D};

	ASSERT_EQ(Size, (int)sizeof(aExpected)) << "The compression is not compatible with older/other implementations anymore";
	EXPECT_EQ(mem_comp(aCompressed, aExpected, Size), 0) << "The compression is not compatible with older/other implementations anymore";
}
