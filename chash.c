#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>

#include "parsing.h"
#include "reentrant_threads.h"
#include "hash.h"
#include "rwlog.h"

/* main */
int main(int argc, char **argv) {
    (void)argc; (void)argv;
    const char *commands_file = "commands.txt";

    /* parse commands */
    size_t cmd_count = 0;
    cmd_t *cmds = parse_commands(commands_file, &cmd_count);
    if (!cmds || cmd_count == 0) {
        fprintf(stderr, "No commands parsed from %s\n", commands_file);
        free(cmds);
        return 1;
    }

    /* init hash table (this will also init rwlog inside hash_init) */
    hash_init();

    /* start threads and process commands (blocks until done) */
    int rc = start_command_threads(cmds, cmd_count);
    if (rc != 0) {
        fprintf(stderr, "Error running threads\n");
    }

    /* final print (assignment requires final print) */
    hash_print_all(stdout);

    free(cmds);
    return rc;
}
