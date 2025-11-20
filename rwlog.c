#define _GNU_SOURCE
#include "rwlog.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>

long long current_timestamp() {
    struct timeval te;
    gettimeofday(&te, NULL);
    long long microseconds = (te.tv_sec * 1000000LL) + te.tv_usec;
    return microseconds;
}

static void do_log(rwlog_t *rw, const char *fmt, ...) {
    pthread_mutex_lock(&rw->log_mutex);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(rw->logf, fmt, ap);
    fprintf(rw->logf, "\n");
    fflush(rw->logf);
    va_end(ap);
    pthread_mutex_unlock(&rw->log_mutex);
}

void rwlog_init(rwlog_t *rw, const char *logfile) {
    pthread_rwlock_init(&rw->lock, NULL);
    pthread_mutex_init(&rw->log_mutex, NULL);
    rw->log_filename = strdup(logfile);
    rw->logf = fopen(logfile, "w");
    if (!rw->logf) {
        perror("fopen log");
        exit(1);
    }
}

void rwlog_destroy(rwlog_t *rw) {
    if (rw->logf) fclose(rw->logf);
    free(rw->log_filename);
    pthread_rwlock_destroy(&rw->lock);
    pthread_mutex_destroy(&rw->log_mutex);
}

void rwlog_rdlock(rwlog_t *rw, int thread_prio) {
    long long ts = current_timestamp();
    if (thread_prio >= 0)
        do_log(rw, "%lld: THREAD %d READ LOCK ACQUIRE_REQUEST", ts, thread_prio);
    pthread_rwlock_rdlock(&rw->lock);
    ts = current_timestamp();
    if (thread_prio >= 0)
        do_log(rw, "%lld: THREAD %d READ LOCK ACQUIRED", ts, thread_prio);
    else
        do_log(rw, "%lld: READ LOCK ACQUIRED", ts);
}

void rwlog_rdunlock(rwlog_t *rw, int thread_prio) {
    pthread_rwlock_unlock(&rw->lock);
    long long ts = current_timestamp();
    if (thread_prio >= 0)
        do_log(rw, "%lld: THREAD %d READ LOCK RELEASED", ts, thread_prio);
    else
        do_log(rw, "%lld: READ LOCK RELEASED", ts);
}

void rwlog_wrlock(rwlog_t *rw, int thread_prio) {
    long long ts = current_timestamp();
    if (thread_prio >= 0)
        do_log(rw, "%lld: THREAD %d WRITE LOCK ACQUIRE_REQUEST", ts, thread_prio);
    pthread_rwlock_wrlock(&rw->lock);
    ts = current_timestamp();
    if (thread_prio >= 0)
        do_log(rw, "%lld: THREAD %d WRITE LOCK ACQUIRED", ts, thread_prio);
    else
        do_log(rw, "%lld: WRITE LOCK ACQUIRED", ts);
}

void rwlog_wrunlock(rwlog_t *rw, int thread_prio) {
    pthread_rwlock_unlock(&rw->lock);
    long long ts = current_timestamp();
    if (thread_prio >= 0)
        do_log(rw, "%lld: THREAD %d WRITE LOCK RELEASED", ts, thread_prio);
    else
        do_log(rw, "%lld: WRITE LOCK RELEASED", ts);
}
