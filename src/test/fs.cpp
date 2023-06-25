#include "test.h"
#include <gtest/gtest.h>

#include <base/system.h>

TEST(Filesystem, Filename)
{
	EXPECT_STREQ(fs_filename(""), "");
	EXPECT_STREQ(fs_filename("a"), "a");
	EXPECT_STREQ(fs_filename("abc"), "abc");
	EXPECT_STREQ(fs_filename("a/b"), "b");
	EXPECT_STREQ(fs_filename("a/b/c"), "c");
	EXPECT_STREQ(fs_filename("aaaaa/bbbb/ccc"), "ccc");
	EXPECT_STREQ(fs_filename("aaaaa\\bbbb\\ccc"), "ccc");
	EXPECT_STREQ(fs_filename("aaaaa/bbbb\\ccc"), "ccc");
	EXPECT_STREQ(fs_filename("aaaaa\\bbbb/ccc"), "ccc");
}

TEST(Filesystem, SplitFileExtension)
{
	char aName[IO_MAX_PATH_LENGTH];
	char aExt[IO_MAX_PATH_LENGTH];

	fs_split_file_extension("", aName, sizeof(aName), aExt, sizeof(aExt));
	EXPECT_STREQ(aName, "");
	EXPECT_STREQ(aExt, "");

	fs_split_file_extension("name.ext", aName, sizeof(aName), aExt, sizeof(aExt));
	EXPECT_STREQ(aName, "name");
	EXPECT_STREQ(aExt, "ext");

	fs_split_file_extension("name.ext", aName, sizeof(aName)); // extension parameter is optional
	EXPECT_STREQ(aName, "name");

	fs_split_file_extension("name.ext", nullptr, 0, aExt, sizeof(aExt)); // name parameter is optional
	EXPECT_STREQ(aExt, "ext");

	fs_split_file_extension("archive.tar.gz", aName, sizeof(aName), aExt, sizeof(aExt));
	EXPECT_STREQ(aName, "archive.tar");
	EXPECT_STREQ(aExt, "gz");

	fs_split_file_extension("no_dot", aName, sizeof(aName), aExt, sizeof(aExt));
	EXPECT_STREQ(aName, "no_dot");
	EXPECT_STREQ(aExt, "");

	fs_split_file_extension(".dot_first", aName, sizeof(aName), aExt, sizeof(aExt));
	EXPECT_STREQ(aName, ".dot_first");
	EXPECT_STREQ(aExt, "");
}

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
