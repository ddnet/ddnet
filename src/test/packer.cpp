#include "test.h"
#include <gtest/gtest.h>

#include <base/system.h>
#include <engine/shared/packer.h>

// pExpected is NULL if an error is expected
static void ExpectAddString5(const char *pString, int Limit, const char *pExpected)
{
	static char ZEROS[CPacker::PACKER_BUFFER_SIZE] = {0};
	static const int OFFSET = CPacker::PACKER_BUFFER_SIZE - 5;
	CPacker Packer;
	Packer.Reset();
	Packer.AddRaw(ZEROS, OFFSET);
	Packer.AddString(pString, Limit);

	EXPECT_EQ(pExpected == 0, Packer.Error());
	if(pExpected)
	{
		// Include null termination.
		int ExpectedLength = str_length(pExpected) + 1;
		EXPECT_EQ(ExpectedLength, Packer.Size() - OFFSET);
		if(ExpectedLength == Packer.Size() - OFFSET)
		{
			EXPECT_STREQ(pExpected, (const char *)Packer.Data() + OFFSET);
		}
	}
}

static void ExpectAddInt(int Input, int Expected)
{
	CPacker Packer;
	Packer.Reset();
	Packer.AddInt(Input);
	EXPECT_EQ(Packer.Error(), false);
	ASSERT_EQ(Packer.Size(), 1);
	EXPECT_EQ(Packer.Data()[0], Expected);
}

static void ExpectAddExtendedInt(int Input, unsigned char *pExpected, int Size)
{
	CPacker Packer;
	Packer.Reset();
	Packer.AddInt(Input);
	EXPECT_EQ(Packer.Error(), false);
	ASSERT_EQ(Packer.Size(), Size);
	EXPECT_EQ(mem_comp(Packer.Data(), pExpected, Size), 0);
}

TEST(Packer, AddInt)
{
	ExpectAddInt(1, 0b0000'0001);
	ExpectAddInt(2, 0b0000'0010);
	//                ^^^     ^
	//                ||\     /
	//     Not extended| \   /
	//                 |  \ /
	//                 |00'0010 => 2
	//                 |
	//                 Positive

	// there is no -0 so 00 0000
	// plus the negative bit is -1
	ExpectAddInt(-1, 0b0100'0000);
	//                 ^^^     ^
	//                 ||\     /
	//      Not extended| \   /
	//                  |  \ /
	//                  |00'0000 => 0
	//                  |
	//                  Negative
	ExpectAddInt(-2, 0b0100'0001);

	// 0-63 packed ints
	// are represented the same way
	// C++ represents ints
	// All other numbers differ due to packing shinanigans
	for(int i = 0; i < 63; i++)
		ExpectAddInt(i, i);
}

TEST(Packer, AddExtendedInt)
{
	unsigned char pExpected64[] = {
		0b1000'0000, 0b0000'0001};
	ExpectAddExtendedInt(64, pExpected64, 2);
	unsigned char pExpected65[] = {
		0b1000'0001, 0b0000'0001
		/*^^^      ^   ^^      ^
		  ||\     /    | \    /
		  || \   /     |  \  /
		  ||  \ /    Not   \/
		  ||   \  Extended /
		  ||    \         /
		  ||     \       /
		  ||      \     /
		  ||       \   /
		  ||        \ /
		  ||         X
		  ||        / \
		  ||       /   \
		  ||      /     \
		  ||     /       \
		  ||  000'0001  00'0001 => 1000001 => 65
		  ||
		  |0 Not negative
		  |
		  1 Extended (one more byte following)
		*/
	};
	ExpectAddExtendedInt(65, pExpected65, 2);

	unsigned char pExpected66[] = {
		0b1000'0010, 0b0000'0001
		/*^^^      ^   ^^      ^
		  ||\     /    | \    /
		  || \   /     |  \  /
		  ||  \ /    Not   \/
		  ||   \  Extended /
		  ||    \         /
		  ||     \       /
		  ||      \     /
		  ||       \   /
		  ||        \ /
		  ||         X
		  ||        / \
		  ||       /   \
		  ||      /     \
		  ||     /       \
		  ||  000'0001  00'0010 => 1000010 => 66
		  ||
		  |0 Not negative
		  |
		  1 Extended (one more byte following)
		*/
	};
	ExpectAddExtendedInt(66, pExpected66, 2);

	// there is no -0 so all negative numbers
	// are one smaller
	// So 64 + negative bit is -65
	unsigned char pExpectedNegative65[] = {
		0b1100'0000, 0b0000'0001
		/*^^^      ^   ^^      ^
		  ||\     /    | \    /
		  || \   /     |  \  /
		  ||  \ /    Not   \/
		  ||   \  Extended /
		  ||    \         /
		  ||     \       /
		  ||      \     /
		  ||       \   /
		  ||        \ /
		  ||         X
		  ||        / \
		  ||       /   \
		  ||      /     \
		  ||     /       \
		  ||  000'0001  00'0000 => 1000000 => 64
		  ||
		  |1 Negative
		  |
		  1 Extended (one more byte following)
		*/
	};
	ExpectAddExtendedInt(-65, pExpectedNegative65, 2);

	unsigned char pExpectedNegative66[] = {
		0b1100'0001, 0b0000'0001
		/*^^^      ^   ^^      ^
		  ||\     /    | \    /
		  || \   /     |  \  /
		  ||  \ /    Not   \/
		  ||   \  Extended /
		  ||    \         /
		  ||     \       /
		  ||      \     /
		  ||       \   /
		  ||        \ /
		  ||         X
		  ||        / \
		  ||       /   \
		  ||      /     \
		  ||     /       \
		  ||  000'0001  00'0001 => 1000001 => 65
		  ||
		  |1 Negative
		  |
		  1 Extended (one more byte following)
		*/
	};
	ExpectAddExtendedInt(-66, pExpectedNegative66, 2);

	unsigned char pExpectedNegative67[] = {
		0b1100'0010, 0b0000'0001};
	ExpectAddExtendedInt(-67, pExpectedNegative67, 2);
	unsigned char pExpectedNegative68[] = {
		0b1100'0011, 0b0000'0001};
	ExpectAddExtendedInt(-68, pExpectedNegative68, 2);
	unsigned char pExpectedNegative69[] = {
		0b1100'0100, 0b0000'0001};
	ExpectAddExtendedInt(-69, pExpectedNegative69, 2);
	unsigned char pExpectedNegative70[] = {
		0b1100'0101, 0b0000'0001};
	ExpectAddExtendedInt(-70, pExpectedNegative70, 2);
}

TEST(Packer, AddString)
{
	ExpectAddString5("", 0, "");
	ExpectAddString5("a", 0, "a");
	ExpectAddString5("abcd", 0, "abcd");
	ExpectAddString5("abcde", 0, 0);
}

TEST(Packer, AddStringLimit)
{
	ExpectAddString5("", 1, "");
	ExpectAddString5("a", 1, "a");
	ExpectAddString5("aa", 1, "a");
	ExpectAddString5("ä", 1, "");

	ExpectAddString5("", 10, "");
	ExpectAddString5("a", 10, "a");
	ExpectAddString5("abcd", 10, "abcd");
	ExpectAddString5("abcde", 10, 0);

	ExpectAddString5("äöü", 5, "äö");
	ExpectAddString5("äöü", 6, 0);
}

TEST(Packer, AddStringBroken)
{
	ExpectAddString5("\x80", 0, "�");
	ExpectAddString5("\x80\x80", 0, 0);
	ExpectAddString5("a\x80", 0, "a�");
	ExpectAddString5("\x80"
			 "a",
		0, "�a");
	ExpectAddString5("\x80", 1, "");
	ExpectAddString5("\x80\x80", 3, "�");
	ExpectAddString5("\x80\x80", 5, "�");
	ExpectAddString5("\x80\x80", 6, 0);
}

TEST(Packer, Error)
{
	char aData[CPacker::PACKER_BUFFER_SIZE];
	mem_zero(aData, sizeof(aData));

	{
		CPacker Packer;
		Packer.Reset();
		EXPECT_EQ(Packer.Error(), false);
		Packer.AddRaw(aData, sizeof(aData) - 1);
		EXPECT_EQ(Packer.Error(), false);
		EXPECT_EQ(Packer.Size(), sizeof(aData) - 1);
		Packer.AddInt(1);
		EXPECT_EQ(Packer.Error(), false);
		EXPECT_EQ(Packer.Size(), sizeof(aData));
		Packer.AddInt(2);
		EXPECT_EQ(Packer.Error(), true);
		Packer.AddInt(3);
		EXPECT_EQ(Packer.Error(), true);
	}

	{
		CPacker Packer;
		Packer.Reset();
		EXPECT_EQ(Packer.Error(), false);
		Packer.AddRaw(aData, sizeof(aData) - 1);
		EXPECT_EQ(Packer.Error(), false);
		EXPECT_EQ(Packer.Size(), sizeof(aData) - 1);
		Packer.AddRaw(aData, 1);
		EXPECT_EQ(Packer.Error(), false);
		EXPECT_EQ(Packer.Size(), sizeof(aData));
		Packer.AddRaw(aData, 1);
		EXPECT_EQ(Packer.Error(), true);
		Packer.AddRaw(aData, 1);
		EXPECT_EQ(Packer.Error(), true);
	}

	{
		CPacker Packer;
		Packer.Reset();
		EXPECT_EQ(Packer.Error(), false);
		Packer.AddRaw(aData, sizeof(aData) - 5);
		EXPECT_EQ(Packer.Error(), false);
		EXPECT_EQ(Packer.Size(), sizeof(aData) - 5);
		Packer.AddString("test");
		EXPECT_EQ(Packer.Error(), false);
		EXPECT_EQ(Packer.Size(), sizeof(aData));
		Packer.AddString("test");
		EXPECT_EQ(Packer.Error(), true);
		Packer.AddString("test");
		EXPECT_EQ(Packer.Error(), true);
	}
}
