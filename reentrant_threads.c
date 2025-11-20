#define _GNU_SOURCE
#include "reentrant_threads.h"
#include "hash.h"
#include "rwlog.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>

/* We'll reuse the same rwlog / hash implementations provided earlier.
 * This module:
 * - assigns priorities for commands with priority == -1 (updates)
 * - creates a thread per command
 * - enforces strict ascending-priority turn ordering
 * - each thread logs WAITING/AWAKENED messages directly to hash.log
 */

/* turn management */
static pthread_mutex_t turn_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t turn_cv = PTHREAD_COND_INITIALIZER;
static int next_priority = -1; /* first priority to run */

/* helper to find min priority among cmds where priority >= 0 */
static int find_min_priority(cmd_t *cmds, size_t n) {
    int min = INT_MAX;
    for (size_t i = 0; i < n; ++i) {
        if (cmds[i].priority >= 0 && cmds[i].priority < min) min = cmds[i].priority;
    }
    if (min == INT_MAX) return 0;
    return min;
}

/* worker passes */
typedef struct {
    cmd_t *cmd;
    /* nothing else; the hash API and rwlog are global singletons */
} worker_arg_t;

/* forward declare: we'll use hash_* functions from hash.c and rwlog logging inside those */
extern uint32_t jenkins_one_at_a_time_hash(const char *key);

static long long current_timestamp() {
    struct timeval te;
    gettimeofday(&te, NULL);
    return (long long)te.tv_sec * 1000000LL + te.tv_usec;
}

static void append_log_line(const char *line) {
    FILE *f = fopen("hash.log", "a");
    if (!f) return;
    fprintf(f, "%s\n", line);
    fflush(f);
    fclose(f);
}

static void *worker(void *varg) {
    worker_arg_t *wa = (worker_arg_t*)varg;
    cmd_t *c = wa->cmd;

    /* Log command execution header line */
    long long ts = current_timestamp();
    FILE *lf = fopen("hash.log","a");
    if (lf) {
        if (strcmp(c->command, "insert") == 0) {
            uint32_t h = jenkins_one_at_a_time_hash(c->name);
            fprintf(lf, "%lld: THREAD %d INSERT,%u,%s,%u\n", ts, c->priority, h, c->name, c->salary);
        } else if (strcmp(c->command, "delete") == 0) {
            fprintf(lf, "%lld: THREAD %d DELETE,%s\n", ts, c->priority, c->name);
        } else if (strcmp(c->command, "update") == 0) {
            fprintf(lf, "%lld: THREAD %d UPDATE,%s,%u\n", ts, c->priority, c->name, c->salary);
        } else if (strcmp(c->command, "search") == 0) {
            fprintf(lf, "%lld: THREAD %d SEARCH,%s\n", ts, c->priority, c->name);
        } else if (strcmp(c->command, "print") == 0) {
            fprintf(lf, "%lld: THREAD %d PRINT\n", ts, c->priority);
        }
        fclose(lf);
    }

    /* WAIT for my turn */
    ts = current_timestamp();
    char linebuf[256];
    snprintf(linebuf, sizeof(linebuf), "%lld: THREAD %d WAITING FOR MY TURN", ts, c->priority);
    append_log_line(linebuf);

    pthread_mutex_lock(&turn_mutex);
    while (c->priority != next_priority) {
        pthread_cond_wait(&turn_cv, &turn_mutex);
    }
    pthread_mutex_unlock(&turn_mutex);

    /* AWAKENED */
    ts = current_timestamp();
    snprintf(linebuf, sizeof(linebuf), "%lld: THREAD %d AWAKENED FOR WORK", ts, c->priority);
    append_log_line(linebuf);

    /* Execute the command by calling hash_* functions (these handle locking and stdout) */
    if (strcmp(c->command, "insert") == 0) {
        hash_insert(c->name, c->salary, NULL, c->priority);
    } else if (strcmp(c->command, "delete") == 0) {
        hash_delete(c->name, NULL, c->priority);
    } else if (strcmp(c->command, "update") == 0) {
        hash_update(c->name, c->salary, NULL, c->priority);
    } else if (strcmp(c->command, "search") == 0) {
        hashRecord *r = hash_search(c->name, NULL, c->priority);
        if (r) {
            printf("Found: %u,%s,%u\n", r->hash, r->name, r->salary);
            free(r);
        } else {
            printf("Not Found: %s not found.\n", c->name);
        }
    } else if (strcmp(c->command, "print") == 0) {
        hash_print_all(stdout);
    }

    /* advance turn */
    pthread_mutex_lock(&turn_mutex);
    next_priority++; /* strict ascending order */
    pthread_cond_broadcast(&turn_cv);
    pthread_mutex_unlock(&turn_mutex);

    free(wa);
    return NULL;
}

int start_command_threads(cmd_t *cmds, size_t cmd_count) {
    if (!cmds || cmd_count == 0) return 0;

    /* assign priorities for commands with -1 (updates) sequentially after max existing priority */
    int maxprio = INT_MIN;
    for (size_t i = 0; i < cmd_count; ++i) {
        if (cmds[i].priority >= 0 && cmds[i].priority > maxprio) maxprio = cmds[i].priority;
    }
    if (maxprio == INT_MIN) maxprio = 0;
    int next_assign = maxprio + 1;
    for (size_t i = 0; i < cmd_count; ++i) {
        if (cmds[i].priority < 0) {
            cmds[i].priority = next_assign++;
        }
    }

    /* determine starting priority (min priority present) */
    int minprio = INT_MAX;
    for (size_t i = 0; i < cmd_count; ++i) {
        if (cmds[i].priority < minprio) minprio = cmds[i].priority;
    }
    if (minprio == INT_MAX) minprio = 0;
    next_priority = minprio;

    /* spawn threads */
    pthread_t *tids = malloc(sizeof(pthread_t) * cmd_count);
    if (!tids) { perror("malloc tids"); return -1; }

    for (size_t i = 0; i < cmd_count; ++i) {
        worker_arg_t *wa = malloc(sizeof(worker_arg_t));
        if (!wa) { perror("malloc wa"); free(tids); return -1; }
        wa->cmd = &cmds[i];
        if (pthread_create(&tids[i], NULL, worker, wa) != 0) {
            perror("pthread_create");
            free(wa);
            free(tids);
            return -1;
        }
    }

    /* Let threads run and join */
    for (size_t i = 0; i < cmd_count; ++i) {
        pthread_join(tids[i], NULL);
    }

    free(tids);
    return 0;
}
