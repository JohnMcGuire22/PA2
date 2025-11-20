#ifndef RWLOG_H
#define RWLOG_H
#include <pthread.h>

typedef struct {
    pthread_rwlock_t lock;
    pthread_mutex_t log_mutex;
    FILE *logf;
    char *log_filename;
} rwlog_t;

void rwlog_init(rwlog_t *rw, const char *logfile);
void rwlog_destroy(rwlog_t *rw);

/* wrappers: thread_prio used for logging; -1 allowed */
void rwlog_rdlock(rwlog_t *rw, int thread_prio);
void rwlog_rdunlock(rwlog_t *rw, int thread_prio);
void rwlog_wrlock(rwlog_t *rw, int thread_prio);
void rwlog_wrunlock(rwlog_t *rw, int thread_prio);

long long current_timestamp();

#endif
