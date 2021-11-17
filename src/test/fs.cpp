#include "test.h"
#include <gtest/gtest.h>

#include <base/system.h>

TEST(Filesystem, CreateCloseDelete)
{
	CTestInfo Info;

	IOHANDLE File = io_open(Info.m_aFilename, IOFLAG_WRITE);
	ASSERT_TRUE(File);
	EXPECT_FALSE(io_close(File));
	EXPECT_FALSE(fs_remove(Info.m_aFilename));
}

TEST(Filesystem, CreateDeleteDirectory)
{
	CTestInfo Info;
	char aFilename[IO_MAX_PATH_LENGTH];
	str_format(aFilename, sizeof(aFilename), "%s/test.txt", Info.m_aFilename);

	EXPECT_FALSE(fs_makedir(Info.m_aFilename));
	IOHANDLE File = io_open(aFilename, IOFLAG_WRITE);
	ASSERT_TRUE(File);
	EXPECT_FALSE(io_close(File));

	// Directory removal fails if there are any files left in the directory.
	EXPECT_TRUE(fs_removedir(Info.m_aFilename));

	EXPECT_FALSE(fs_remove(aFilename));
	EXPECT_FALSE(fs_removedir(Info.m_aFilename));
}

TEST(Filesystem, CantDeleteDirectoryWithRemove)
{
	CTestInfo Info;
	EXPECT_FALSE(fs_makedir(Info.m_aFilename));
	EXPECT_TRUE(fs_remove(Info.m_aFilename)); // Cannot remove directory with file removal function.
	EXPECT_FALSE(fs_removedir(Info.m_aFilename));
}

TEST(Filesystem, CantDeleteFileWithRemoveDirectory)
{
	CTestInfo Info;
	IOHANDLE File = io_open(Info.m_aFilename, IOFLAG_WRITE);
	ASSERT_TRUE(File);
	EXPECT_FALSE(io_close(File));
	EXPECT_TRUE(fs_removedir(Info.m_aFilename)); // Cannot remove file with directory removal function.
	EXPECT_FALSE(fs_remove(Info.m_aFilename));
}
