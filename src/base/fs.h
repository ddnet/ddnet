#ifndef BASE_FS_H
#define BASE_FS_H

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

#endif
