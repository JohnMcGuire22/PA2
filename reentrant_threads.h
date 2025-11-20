#ifndef REENTRANT_THREADS_H
#define REENTRANT_THREADS_H

#include "parsing.h"
#include <stddef.h>

/* Initialize and run threads for parsed commands.
 * - cmds: array of cmd_t of length cmd_count
 * - cmd_count: number of commands
 * - on_finish: optional callback invoked after all threads complete (can be NULL)
 * Returns 0 on success, non-zero on error.
 */
int start_command_threads(cmd_t *cmds, size_t cmd_count);

#endif
