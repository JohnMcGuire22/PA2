#ifndef HASH_H
#define HASH_H

#include <stdint.h>
#include <pthread.h>

typedef struct hash_struct {
    uint32_t hash;
    char name[50];
    uint32_t salary;
    struct hash_struct *next;
} hashRecord;

/* Initialize the table */
void hash_init(void);

/* Operations */
int hash_insert(const char *name, uint32_t salary, FILE *logf, int thread_prio);
int hash_delete(const char *name, FILE *logf, int thread_prio);
int hash_update(const char *name, uint32_t new_salary, FILE *logf, int thread_prio);
hashRecord* hash_search(const char *name, FILE *logf, int thread_prio);
void hash_print_all(FILE *out);

/* Jenkins hash */
uint32_t jenkins_one_at_a_time_hash(const char *key);

#endif
