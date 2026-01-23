/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#ifndef BASE_FS_H
#define BASE_FS_H

#include "types.h"

#include <cstddef> // size_t
#include <ctime> // time_t

/**
 * Utilities for accessing the file system.
 *
 * @defgroup Filesystem Filesystem
 */

/**
 * Creates a directory.
 *
 * @ingroup Filesystem
 *
 * @param path Directory to create.
 *
 * @return `0` on success. Negative value on failure.
 *
 * @remark Does not create several directories if needed. "a/b/c" will
 *         result in a failure if b or a does not exist.
 *
 * @remark The strings are treated as null-terminated strings.
 */
int fs_makedir(const char *path);

/**
 * Recursively creates parent directories for a file or directory.
 *
 * @ingroup Filesystem
 *
 * @param path File or directory for which to create parent directories.
 *
 * @return `0` on success. Negative value on failure.
 *
 * @remark The strings are treated as null-terminated strings.
 */
int fs_makedir_rec_for(const char *path);

/**
 * Removes a directory.
 *
 * @ingroup Filesystem
 *
 * @param path Directory to remove.
 *
 * @return `0` on success. Negative value on failure.
 *
 * @remark Cannot remove a non-empty directory.
 *
 * @remark The strings are treated as null-terminated strings.
 */
int fs_removedir(const char *path);

/**
 * Lists the files and folders in a directory.
 *
 * @ingroup Filesystem
 *
 * @param dir Directory to list.
 * @param cb Callback function to call for each entry.
 * @param type Type of the directory.
 * @param user Pointer to give to the callback.
 *
 * @remark The strings are treated as null-terminated strings.
 */
void fs_listdir(const char *dir, FS_LISTDIR_CALLBACK cb, int type, void *user);

/**
 * Lists the files and folders in a directory and gets additional file information.
 *
 * @ingroup Filesystem
 *
 * @param dir Directory to list.
 * @param cb Callback function to call for each entry.
 * @param type Type of the directory.
 * @param user Pointer to give to the callback.
 *
 * @remark The strings are treated as null-terminated strings.
 */
void fs_listdir_fileinfo(const char *dir, FS_LISTDIR_CALLBACK_FILEINFO cb, int type, void *user);

/**
 * Changes the current working directory.
 *
 * @ingroup Filesystem
 *
 * @param path New working directory path.
 *
 * @return `0` on success. `1` on failure.
 *
 * @remark The strings are treated as null-terminated strings.
 */
int fs_chdir(const char *path);

/**
 * Gets the current working directory.
 *
 * @ingroup Filesystem
 *
 * @param buffer Buffer that will receive the current working directory.
 * @param buffer_size Size of the buffer.
 *
 * @return Pointer to the buffer on success, `nullptr` on failure.
 *
 * @remark The strings are treated as null-terminated strings.
 */
char *fs_getcwd(char *buffer, int buffer_size);

/**
 * Fetches per user configuration directory.
 *
 * @ingroup Filesystem
 *
 * @param appname Name of the application.
 * @param path Buffer that will receive the storage path.
 * @param max Size of the buffer.
 *
 * @return `0` on success. Negative value on failure.
 *
 * @remark Returns ~/.appname on UNIX based systems.
 * @remark Returns ~/Library/Applications Support/appname on macOS.
 * @remark Returns %APPDATA%/Appname on Windows based systems.
 *
 * @remark The strings are treated as null-terminated strings.
 */
int fs_storage_path(const char *appname, char *path, int max);

/**
 * Gets the absolute path to the executable.
 *
 * @ingroup Filesystem
 *
 * @param buffer Buffer that will receive the path of the executable.
 * @param buffer_size Size of the buffer.
 *
 * @return `0` on success. Negative value on failure.
 *
 * @remark The executable name is included in the result.
 *
 * @remark The strings are treated as null-terminated strings.
 */
int fs_executable_path(char *buffer, int buffer_size);

/**
 * Checks if a file exists.
 *
 * @ingroup Filesystem
 *
 * @param path The path to check.
 *
 * @return `1` if a file with the given path exists.
 * @return `0` on failure or if the file does not exist.
 *
 * @remark The strings are treated as null-terminated strings.
 */
int fs_is_file(const char *path);

/**
 * Checks if a folder exists.
 *
 * @ingroup Filesystem
 *
 * @param path The path to check.
 *
 * @return `1` if a folder with the given path exists.
 * @return `0` on failure or if the folder does not exist.
 *
 * @remark The strings are treated as null-terminated strings.
 */
int fs_is_dir(const char *path);

/**
 * Checks whether a given path is relative or absolute.
 *
 * @ingroup Filesystem
 *
 * @param path Path to check.
 *
 * @return `1` if relative, `0` if absolute.
 *
 * @remark The strings are treated as null-terminated strings.
 */
int fs_is_relative_path(const char *path);

/**
 * Gets the name of a file or folder specified by a path,
 * i.e. the last segment of the path.
 *
 * @ingroup Filesystem
 *
 * @param path Path from which to retrieve the filename.
 *
 * @return Filename of the path.
 *
 * @remark Supports forward and backward slashes as path segment separator.
 * @remark No distinction between files and folders is being made.
 * @remark The strings are treated as null-terminated strings.
 */
const char *fs_filename(const char *path);

/**
 * Splits a filename into name (without extension) and file extension.
 *
 * @ingroup Filesystem
 *
 * @param filename The filename to split.
 * @param name Buffer that will receive the name without extension, may be nullptr.
 * @param name_size Size of the name buffer (ignored if name is nullptr).
 * @param extension Buffer that will receive the extension, may be nullptr.
 * @param extension_size Size of the extension buffer (ignored if extension is nullptr).
 *
 * @remark Does NOT handle forward and backward slashes.
 * @remark No distinction between files and folders is being made.
 * @remark The strings are treated as null-terminated strings.
 */
void fs_split_file_extension(const char *filename, char *name, size_t name_size, char *extension = nullptr, size_t extension_size = 0);

/**
 * Get the parent directory of a directory.
 *
 * @ingroup Filesystem
 *
 * @param path Path of the directory. The parent will be store in this buffer as well.
 *
 * @return `0` on success. `1` on failure.
 *
 * @remark The strings are treated as null-terminated strings.
 */
int fs_parent_dir(char *path);

/**
 * Normalizes the given path: replaces backslashes with regular slashes
 * and removes trailing slashes.
 *
 * @ingroup Filesystem
 *
 * @param path Path to normalize.
 *
 * @remark The strings are treated as null-terminated strings.
 */
void fs_normalize_path(char *path);

/**
 * Deletes a file.
 *
 * @ingroup Filesystem
 *
 * @param filename Path of the file to delete.
 *
 * @return `0` on success. `1` on failure.
 *
 * @remark The strings are treated as null-terminated strings.
 * @remark Returns an error if the path specifies a directory name.
 */
int fs_remove(const char *filename);

/**
 * Renames the file or directory. If the paths differ the file will be moved.
 *
 * @ingroup Filesystem
 *
 * @param oldname The current path of a file or directory.
 * @param newname The new path for the file or directory.
 *
 * @return `0` on success. `1` on failure.
 *
 * @remark The strings are treated as null-terminated strings.
 */
int fs_rename(const char *oldname, const char *newname);

/**
 * Gets the creation and the last modification date of a file or directory.
 *
 * @ingroup Filesystem
 *
 * @param name Path of a file or directory.
 * @param created Pointer where the creation time will be stored.
 * @param modified Pointer where the modification time will be stored.
 *
 * @return `0` on success. non-zero on failure.
 *
 * @remark The strings are treated as null-terminated strings.
 * @remark Returned time is in seconds since UNIX Epoch.
 */
int fs_file_time(const char *name, time_t *created, time_t *modified);

#endif
