#define _GNU_SOURCE
#include "hash.h"
#include "rwlog.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static hashRecord *head = NULL;
static rwlog_t table_lock;

/* helpers */
static void insert_sorted_node(hashRecord *node) {
    if (!head || node->hash < head->hash) {
        node->next = head;
        head = node;
        return;
    }
    hashRecord *cur = head;
    while (cur->next && cur->next->hash < node->hash) cur = cur->next;
    node->next = cur->next;
    cur->next = node;
}

void hash_init(void) {
    head = NULL;
    rwlog_init(&table_lock, "hash.log"); /* initialize with file logging inside rwlog */
}

uint32_t jenkins_one_at_a_time_hash(const char *key) {
    size_t i = 0;
    uint32_t hash = 0;
    while (key[i]) {
        hash += (unsigned char)key[i++];
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return hash;
}

int hash_insert(const char *name, uint32_t salary, FILE *logf, int thread_prio) {
    uint32_t h = jenkins_one_at_a_time_hash(name);

    rwlog_wrlock(&table_lock, thread_prio); /* logs write lock acquired */
    /* check for duplicate */
    hashRecord *cur = head;
    while (cur) {
        if (cur->hash == h && strcmp(cur->name, name) == 0) {
            rwlog_wrunlock(&table_lock, thread_prio);
            fprintf(stdout, "Insert failed. Entry %u is a duplicate.\n", h);
            return -1;
        }
        cur = cur->next;
    }

    hashRecord *node = malloc(sizeof(hashRecord));
    node->hash = h;
    strncpy(node->name, name, sizeof(node->name)-1);
    node->name[sizeof(node->name)-1] = '\0';
    node->salary = salary;
    node->next = NULL;

    insert_sorted_node(node);

    rwlog_wrunlock(&table_lock, thread_prio);
    fprintf(stdout, "Inserted %u,%s,%u\n", h, name, salary);
    return 0;
}

int hash_delete(const char *name, FILE *logf, int thread_prio) {
    uint32_t h = jenkins_one_at_a_time_hash(name);
    rwlog_wrlock(&table_lock, thread_prio);
    hashRecord *cur = head, *prev = NULL;
    while (cur) {
        if (cur->hash == h && strcmp(cur->name, name) == 0) {
            if (prev) prev->next = cur->next;
            else head = cur->next;
            fprintf(stdout, "Deleted record for %u,%s,%u\n", cur->hash, cur->name, cur->salary);
            free(cur);
            rwlog_wrunlock(&table_lock, thread_prio);
            return 0;
        }
        prev = cur;
        cur = cur->next;
    }
    rwlog_wrunlock(&table_lock, thread_prio);
    fprintf(stdout, "Entry %u not deleted. Not in database.\n", h);
    return -1;
}

int hash_update(const char *name, uint32_t new_salary, FILE *logf, int thread_prio) {
    uint32_t h = jenkins_one_at_a_time_hash(name);
    rwlog_wrlock(&table_lock, thread_prio);
    hashRecord *cur = head;
    while (cur) {
        if (cur->hash == h && strcmp(cur->name, name) == 0) {
            uint32_t old = cur->salary;
            cur->salary = new_salary;
            rwlog_wrunlock(&table_lock, thread_prio);
            fprintf(stdout, "Updated record %u,%s from %u to %u\n", h, name, old, new_salary);
            return 0;
        }
        cur = cur->next;
    }
    rwlog_wrunlock(&table_lock, thread_prio);
    fprintf(stdout, "Updated failed. Entry %u not found.\n", h);
    return -1;
}

hashRecord* hash_search(const char *name, FILE *logf, int thread_prio) {
    uint32_t h = jenkins_one_at_a_time_hash(name);
    rwlog_rdlock(&table_lock, thread_prio);
    hashRecord *cur = head;
    while (cur) {
        if (cur->hash == h && strcmp(cur->name, name) == 0) {
            /* copy node to return (caller should not free) - return pointer to heap copy */
            hashRecord *res = malloc(sizeof(hashRecord));
            *res = *cur;
            rwlog_rdunlock(&table_lock, thread_prio);
            return res;
        }
        cur = cur->next;
    }
    rwlog_rdunlock(&table_lock, thread_prio);
    return NULL;
}

void hash_print_all(FILE *out) {
    rwlog_rdlock(&table_lock, -1); /* -1 indicates print (no thread_prio), still logs */
    fprintf(out, "Current Database:\n");
    hashRecord *cur = head;
    while (cur) {
        fprintf(out, "%u,%s,%u\n", cur->hash, cur->name, cur->salary);
        cur = cur->next;
    }
    rwlog_rdunlock(&table_lock, -1);
}
