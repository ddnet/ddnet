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

	fs_split_file_extension("no_dot", aName, sizeof(aName)); // extension parameter is optional
	EXPECT_STREQ(aName, "no_dot");

	fs_split_file_extension("no_dot", nullptr, 0, aExt, sizeof(aExt)); // name parameter is optional
	EXPECT_STREQ(aExt, "");

	fs_split_file_extension(".dot_first", aName, sizeof(aName), aExt, sizeof(aExt));
	EXPECT_STREQ(aName, ".dot_first");
	EXPECT_STREQ(aExt, "");

	fs_split_file_extension(".dot_first", aName, sizeof(aName)); // extension parameter is optional
	EXPECT_STREQ(aName, ".dot_first");

	fs_split_file_extension(".dot_first", nullptr, 0, aExt, sizeof(aExt)); // name parameter is optional
	EXPECT_STREQ(aExt, "");
}

TEST(Filesystem, StoragePath)
{
	char aStoragePath[IO_MAX_PATH_LENGTH];
	ASSERT_FALSE(fs_storage_path("TestAppName", aStoragePath, sizeof(aStoragePath)));
	EXPECT_FALSE(fs_is_relative_path(aStoragePath));
	EXPECT_TRUE(str_endswith_nocase(aStoragePath, "/TestAppName"));
}

TEST(Filesystem, CreateCloseDelete)
{
	CTestInfo Info;

	EXPECT_FALSE(fs_is_file(Info.m_aFilename));
	IOHANDLE File = io_open(Info.m_aFilename, IOFLAG_WRITE);
	ASSERT_TRUE(File);
	EXPECT_FALSE(io_close(File));
	EXPECT_TRUE(fs_is_file(Info.m_aFilename));
	EXPECT_FALSE(fs_remove(Info.m_aFilename));
	EXPECT_FALSE(fs_is_file(Info.m_aFilename));
}

TEST(Filesystem, CreateDeleteDirectory)
{
	CTestInfo Info;
	char aFilename[IO_MAX_PATH_LENGTH];
	str_format(aFilename, sizeof(aFilename), "%s/test.txt", Info.m_aFilename);

	EXPECT_FALSE(fs_is_dir(Info.m_aFilename));
	EXPECT_FALSE(fs_makedir(Info.m_aFilename));
	EXPECT_TRUE(fs_is_dir(Info.m_aFilename));

	IOHANDLE File = io_open(aFilename, IOFLAG_WRITE);
	ASSERT_TRUE(File);
	EXPECT_FALSE(io_close(File));

	// Directory removal fails if there are any files left in the directory.
	EXPECT_TRUE(fs_removedir(Info.m_aFilename));
	EXPECT_TRUE(fs_is_dir(Info.m_aFilename));

	EXPECT_FALSE(fs_remove(aFilename));
	EXPECT_FALSE(fs_removedir(Info.m_aFilename));
	EXPECT_FALSE(fs_is_dir(Info.m_aFilename));
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

TEST(Filesystem, DeleteNonexistentFile)
{
	CTestInfo Info;

	EXPECT_FALSE(fs_is_file(Info.m_aFilename));
	EXPECT_FALSE(fs_remove(Info.m_aFilename)); // Can delete a file that does not exist.
}

TEST(Filesystem, DeleteNonexistentDirectory)
{
	CTestInfo Info;

	EXPECT_FALSE(fs_is_dir(Info.m_aFilename));
	EXPECT_FALSE(fs_removedir(Info.m_aFilename)); // Can delete a folder that does not exist.
}

TEST(Filesystem, DeleteOpenFile)
{
	CTestInfo Info;

	IOHANDLE File = io_open(Info.m_aFilename, IOFLAG_WRITE);
	ASSERT_TRUE(File);

	EXPECT_TRUE(fs_is_file(Info.m_aFilename));
	EXPECT_FALSE(fs_remove(Info.m_aFilename)); // Can delete a file that has open handle.
	EXPECT_FALSE(fs_is_file(Info.m_aFilename)); // File should be gone immediately even before the last file handle is closed.

	EXPECT_FALSE(io_close(File));
}

TEST(Filesystem, DeleteOpenFileMultipleHandles)
{
	CTestInfo Info;

	IOHANDLE FileWrite1 = io_open(Info.m_aFilename, IOFLAG_WRITE);
	ASSERT_TRUE(FileWrite1);

	IOHANDLE FileRead1 = io_open(Info.m_aFilename, IOFLAG_READ);
	ASSERT_TRUE(FileRead1);

	IOHANDLE FileWrite2 = io_open(Info.m_aFilename, IOFLAG_WRITE);
	ASSERT_TRUE(FileWrite2);

	IOHANDLE FileRead2 = io_open(Info.m_aFilename, IOFLAG_READ);
	ASSERT_TRUE(FileRead2);

	EXPECT_TRUE(fs_is_file(Info.m_aFilename));
	EXPECT_FALSE(fs_remove(Info.m_aFilename)); // Can delete a file that has multiple open handles.
	EXPECT_FALSE(fs_is_file(Info.m_aFilename)); // File should be gone immediately even before the last file handle is closed.

	EXPECT_FALSE(io_close(FileWrite1));
	EXPECT_FALSE(io_close(FileRead1));
	EXPECT_FALSE(io_close(FileWrite2));
	EXPECT_FALSE(io_close(FileRead2));
}

TEST(Filesystem, RenameFile)
{
	char aNewFilename[IO_MAX_PATH_LENGTH];
	CTestInfo Info;
	Info.Filename(aNewFilename, sizeof(aNewFilename), ".renamed");

	IOHANDLE FileWrite = io_open(Info.m_aFilename, IOFLAG_WRITE);
	ASSERT_TRUE(FileWrite);
	EXPECT_FALSE(io_close(FileWrite));

	EXPECT_TRUE(fs_is_file(Info.m_aFilename));
	EXPECT_FALSE(fs_rename(Info.m_aFilename, aNewFilename));
	EXPECT_FALSE(fs_is_file(Info.m_aFilename));
	EXPECT_TRUE(fs_is_file(aNewFilename));

	EXPECT_FALSE(fs_remove(aNewFilename));
}

TEST(Filesystem, RenameFolder)
{
	char aNewFilename[IO_MAX_PATH_LENGTH];
	CTestInfo Info;
	Info.Filename(aNewFilename, sizeof(aNewFilename), ".renamed");

	EXPECT_FALSE(fs_makedir(Info.m_aFilename));

	EXPECT_TRUE(fs_is_dir(Info.m_aFilename));
	EXPECT_FALSE(fs_rename(Info.m_aFilename, aNewFilename));
	EXPECT_FALSE(fs_is_dir(Info.m_aFilename));
	EXPECT_TRUE(fs_is_dir(aNewFilename));

	EXPECT_FALSE(fs_removedir(aNewFilename));
}

TEST(Filesystem, RenameOpenFileSource)
{
	char aNewFilename[IO_MAX_PATH_LENGTH];
	CTestInfo Info;
	Info.Filename(aNewFilename, sizeof(aNewFilename), ".renamed");

	IOHANDLE FileWrite = io_open(Info.m_aFilename, IOFLAG_WRITE);
	ASSERT_TRUE(FileWrite);

	EXPECT_TRUE(fs_is_file(Info.m_aFilename));
	EXPECT_FALSE(fs_rename(Info.m_aFilename, aNewFilename)); // Can rename a file that has open handle.
	EXPECT_FALSE(fs_is_file(Info.m_aFilename)); // Rename should take effect immediately.
	EXPECT_TRUE(fs_is_file(aNewFilename));

	EXPECT_FALSE(io_close(FileWrite));

	EXPECT_FALSE(fs_remove(aNewFilename));
}

TEST(Filesystem, RenameOpenFileSourceMultipleHandles)
{
	char aNewFilename[IO_MAX_PATH_LENGTH];
	CTestInfo Info;
	Info.Filename(aNewFilename, sizeof(aNewFilename), ".renamed");

	IOHANDLE FileWrite1 = io_open(Info.m_aFilename, IOFLAG_WRITE);
	ASSERT_TRUE(FileWrite1);

	IOHANDLE FileRead1 = io_open(Info.m_aFilename, IOFLAG_READ);
	ASSERT_TRUE(FileRead1);

	IOHANDLE FileWrite2 = io_open(Info.m_aFilename, IOFLAG_WRITE);
	ASSERT_TRUE(FileWrite2);

	IOHANDLE FileRead2 = io_open(Info.m_aFilename, IOFLAG_READ);
	ASSERT_TRUE(FileRead2);

	EXPECT_TRUE(fs_is_file(Info.m_aFilename));
	EXPECT_FALSE(fs_rename(Info.m_aFilename, aNewFilename)); // Can rename a file that has multiple open handles.
	EXPECT_FALSE(fs_is_file(Info.m_aFilename)); // Rename should take effect immediately.
	EXPECT_TRUE(fs_is_file(aNewFilename));

	EXPECT_FALSE(io_close(FileWrite1));
	EXPECT_FALSE(io_close(FileRead1));
	EXPECT_FALSE(io_close(FileWrite2));
	EXPECT_FALSE(io_close(FileRead2));

	EXPECT_FALSE(fs_remove(aNewFilename));
}

TEST(Filesystem, RenameTargetFileExists)
{
	char aNewFilename[IO_MAX_PATH_LENGTH];
	CTestInfo Info;
	Info.Filename(aNewFilename, sizeof(aNewFilename), ".renamed");

	IOHANDLE FileWrite1 = io_open(Info.m_aFilename, IOFLAG_WRITE);
	ASSERT_TRUE(FileWrite1);
	EXPECT_FALSE(io_close(FileWrite1));

	IOHANDLE FileWrite2 = io_open(aNewFilename, IOFLAG_WRITE);
	ASSERT_TRUE(FileWrite2);
	EXPECT_FALSE(io_close(FileWrite2));

	EXPECT_TRUE(fs_is_file(Info.m_aFilename));
	EXPECT_TRUE(fs_is_file(aNewFilename));
	EXPECT_FALSE(fs_rename(Info.m_aFilename, aNewFilename)); // Renaming can overwrite the existing target file if it has no open handles.
	EXPECT_FALSE(fs_is_file(Info.m_aFilename));
	EXPECT_TRUE(fs_is_file(aNewFilename));

	EXPECT_FALSE(fs_remove(aNewFilename));
}

TEST(Filesystem, RenameOpenFileDeleteTarget)
{
	char aNewFilename[IO_MAX_PATH_LENGTH];
	CTestInfo Info;
	Info.Filename(aNewFilename, sizeof(aNewFilename), ".renamed");

	IOHANDLE FileWrite1 = io_open(Info.m_aFilename, IOFLAG_WRITE);
	ASSERT_TRUE(FileWrite1);
	EXPECT_FALSE(io_close(FileWrite1));

	IOHANDLE FileWrite2 = io_open(aNewFilename, IOFLAG_WRITE);
	ASSERT_TRUE(FileWrite2);
	EXPECT_FALSE(io_close(FileWrite2));

	IOHANDLE FileRead = io_open(aNewFilename, IOFLAG_READ);
	ASSERT_TRUE(FileRead);

	EXPECT_TRUE(fs_is_file(Info.m_aFilename));
	EXPECT_TRUE(fs_is_file(aNewFilename));
	EXPECT_FALSE(fs_remove(aNewFilename)); // Target file must be deleted else rename fails on Windows when target file has open handle.
	EXPECT_FALSE(fs_rename(Info.m_aFilename, aNewFilename));
	EXPECT_FALSE(fs_is_file(Info.m_aFilename));
	EXPECT_TRUE(fs_is_file(aNewFilename));

	EXPECT_FALSE(io_close(FileRead));

	EXPECT_FALSE(fs_remove(aNewFilename));
}
