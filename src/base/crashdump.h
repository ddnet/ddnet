#ifndef BASE_CRASHDUMP_H
#define BASE_CRASHDUMP_H

/**
 * @defgroup Crash-Dumping Crash Dumping
 */

/**
 * Initializes the crash dumper and sets the filename to write the crash dump
 * to, if support for crash logging was compiled in. Otherwise does nothing.
 *
 * @ingroup Crash-Dumping
 *
 * @param log_file_path Absolute path to which crash log file should be written.
 */
void crashdump_init_if_available(const char *log_file_path);

#endif
