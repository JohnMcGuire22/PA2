#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "hash.h"
#include "rwlog.h"

#define LINE_BUF 256
typedef struct {
    char command[16];
    char name[64];
    uint32_t salary;
    int priority;
    int has_salary;
} cmd_t;

/* Priority turn management */
static pthread_mutex_t turn_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  turn_cv = PTHREAD_COND_INITIALIZER;
static int next_priority = -1; /* will be set to min priority found */

/* global log file pointer (for console commands we use stdout) */
static FILE *logf_global = NULL;

/* Thread worker */
void *worker(void *arg) {
    cmd_t *cmd = (cmd_t*)arg;
    long long ts;

    /* log thread command execution */
    ts = current_timestamp();
    /* log to hash.log: "<timestamp>: THREAD <priority>,<command and parameters>" */
    FILE *logf = fopen("hash.log", "a");
    if (logf) {
        if (strcmp(cmd->command, "insert")==0) {
            fprintf(logf, "%lld: THREAD %d %s,%u,%s,%d\n", ts, cmd->priority, "INSERT", jenkins_one_at_a_time_hash(cmd->name), cmd->name, cmd->salary);
        } else if (strcmp(cmd->command, "delete")==0) {
            fprintf(logf, "%lld: THREAD %d %s,%s\n", ts, cmd->priority, "DELETE", cmd->name);
        } else if (strcmp(cmd->command, "update")==0) {
            fprintf(logf, "%lld: THREAD %d %s,%s,%u\n", ts, cmd->priority, "UPDATE", cmd->name, cmd->salary);
        } else if (strcmp(cmd->command, "search")==0) {
            fprintf(logf, "%lld: THREAD %d %s,%s\n", ts, cmd->priority, "SEARCH", cmd->name);
        } else if (strcmp(cmd->command, "print")==0) {
            fprintf(logf, "%lld: THREAD %d %s\n", ts, cmd->priority, "PRINT");
        }
        fclose(logf);
    }

    /* Wait for my turn (priority ordering) */
    pthread_mutex_lock(&turn_mutex);
    ts = current_timestamp();
    /* WAITING FOR MY TURN */
    logf = fopen("hash.log", "a");
    if (logf) { fprintf(logf, "%lld: THREAD %d WAITING FOR MY TURN\n", ts, cmd->priority); fclose(logf); }
    while (cmd->priority != next_priority) {
        pthread_cond_wait(&turn_cv, &turn_mutex);
    }
    ts = current_timestamp();
    logf = fopen("hash.log", "a");
    if (logf) { fprintf(logf, "%lld: THREAD %d AWAKENED FOR WORK\n", ts, cmd->priority); fclose(logf); }
    pthread_mutex_unlock(&turn_mutex);

    /* Execute the command */
    if (strcmp(cmd->command, "insert")==0) {
        hash_insert(cmd->name, cmd->salary, NULL, cmd->priority);
    } else if (strcmp(cmd->command, "delete")==0) {
        hash_delete(cmd->name, NULL, cmd->priority);
    } else if (strcmp(cmd->command, "update")==0) {
        hash_update(cmd->name, cmd->salary, NULL, cmd->priority);
    } else if (strcmp(cmd->command, "search")==0) {
        hashRecord *r = hash_search(cmd->name, NULL, cmd->priority);
        if (r) {
            printf("Found: %u,%s,%u\n", r->hash, r->name, r->salary);
            free(r);
        } else {
            printf("Not Found: %s not found.\n", cmd->name);
        }
    } else if (strcmp(cmd->command, "print")==0) {
        hash_print_all(stdout);
    }

    /* advance turn */
    pthread_mutex_lock(&turn_mutex);
    next_priority++;
    pthread_cond_broadcast(&turn_cv);
    pthread_mutex_unlock(&turn_mutex);

    free(cmd);
    return NULL;
}

int main() {
    hash_init();
    /* read commands.txt */
    FILE *f = fopen("commands.txt", "r");
    if (!f) {
        perror("commands.txt");
        return 1;
    }

    /* parse all commands into an array (to determine min priority) */
    char line[LINE_BUF];
    cmd_t **cmds = NULL;
    size_t cmd_count = 0;
    int min_prio = INT_MAX;
    while (fgets(line, sizeof(line), f)) {
        /* trim newline */
        size_t L = strlen(line);
        while (L && (line[L-1] == '\n' || line[L-1]=='\r')) { line[--L] = '\0'; }
        if (L==0) continue;
        /* split by commas */
        char *parts[4];
        int p = 0;
        char *s = line;
        while (p<4 && s) {
            parts[p++] = strsep(&s, ",");
        }
        if (p==0) continue;
        cmd_t *c = calloc(1, sizeof(cmd_t));
        c->has_salary = 0;
        if (strcasecmp(parts[0], "insert")==0 && p >= 4) {
            strcpy(c->command, "insert");
            strncpy(c->name, parts[1], sizeof(c->name)-1);
            c->salary = (uint32_t)atoi(parts[2]);
            c->priority = atoi(parts[3]);
            c->has_salary = 1;
        } else if (strcasecmp(parts[0], "delete")==0 && p >= 3) {
            strcpy(c->command, "delete");
            strncpy(c->name, parts[1], sizeof(c->name)-1);
            c->priority = atoi(parts[2]);
        } else if (strcasecmp(parts[0], "update")==0 && p >= 3) {
            strcpy(c->command, "update");
            strncpy(c->name, parts[1], sizeof(c->name)-1);
            c->salary = (uint32_t)atoi(parts[2]);
            c->priority = -1; /* no priority field in update per assignment sample - run immediately? */
            /* to make things consistent, treat update as having priority equal to min available */
            /* but we will treat update with a priority of min_prio later */
        } else if (strcasecmp(parts[0], "search")==0 && p >= 3) {
            strcpy(c->command, "search");
            strncpy(c->name, parts[1], sizeof(c->name)-1);
            c->priority = atoi(parts[2]);
        } else if (strcasecmp(parts[0], "print")==0 && p >= 2) {
            strcpy(c->command, "print");
            c->priority = atoi(parts[1]);
        } else {
            /* unknown or malformed: skip */
            free(c);
            continue;
        }
        /* store */
        cmds = realloc(cmds, sizeof(cmd_t*) * (cmd_count+1));
        cmds[cmd_count++] = c;
        if (c->priority >= 0 && c->priority < min_prio) min_prio = c->priority;
    }
    fclose(f);

    if (cmd_count == 0) {
        fprintf(stderr, "No commands found.\n");
        return 1;
    }

    /* set next_priority to min_prio */
    if (min_prio==INT_MAX) min_prio = 0;
    next_priority = min_prio;

    /* create threads for each command that has a priority. For updates with -1, set priority to increasing values after min */
    pthread_t *tids = calloc(cmd_count, sizeof(pthread_t));
    for (size_t i=0;i<cmd_count;i++) {
        cmd_t *c = cmds[i];
        /* if update had -1, assign sequential priorities after max existing to ensure they get processed */
        if (c->priority < 0) {
            c->priority = next_priority + (int)i + 1;
        }
        pthread_create(&tids[i], NULL, worker, c);
    }

    for (size_t i=0;i<cmd_count;i++) pthread_join(tids[i], NULL);

    /* final print as required */
    hash_print_all(stdout);

    return 0;
}
