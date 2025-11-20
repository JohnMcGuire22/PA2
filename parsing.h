#ifndef PARSING_H
#define PARSING_H

#include <stdint.h>

/* cmd_t structure used across program */
typedef struct {
    char command[16];     /* "insert","delete","update","search","print" */
    char name[64];
    uint32_t salary;      /* only used for insert/update */
    int priority;         /* -1 if not provided (update may be -1) */
} cmd_t;

/* Parse commands.txt into dynamically allocated array of cmd_t.
 * Returns pointer to array and sets out_count. Caller must free the array.
 */
cmd_t *parse_commands(const char *filename, size_t *out_count);

#endif
