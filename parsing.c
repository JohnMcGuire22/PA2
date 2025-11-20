#define _GNU_SOURCE
#include "parsing.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define LINE_BUF 512

/* trim helper */
static char *trim(char *s) {
    if (!s) return s;
    while (*s && isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s) - 1;
    while (end >= s && isspace((unsigned char)*end)) *end-- = '\0';
    return s;
}

cmd_t *parse_commands(const char *filename, size_t *out_count) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        perror("parse_commands fopen");
        *out_count = 0;
        return NULL;
    }

    cmd_t *arr = NULL;
    size_t cap = 0, cnt = 0;
    char buf[LINE_BUF];

    while (fgets(buf, sizeof(buf), f)) {
        /* remove trailing newline */
        size_t L = strlen(buf);
        while (L && (buf[L-1] == '\n' || buf[L-1] == '\r')) buf[--L] = '\0';
        if (L == 0) continue;

        /* split into parts by comma (max 4 parts) */
        char *parts[4] = {0};
        char *s = buf;
        int p = 0;
        while (p < 4 && s) {
            parts[p++] = strsep(&s, ",");
        }
        if (p == 0) continue;

        for (int i = 0; i < p; ++i) parts[i] = trim(parts[i]);

        cmd_t c;
        memset(&c, 0, sizeof(c));
        c.salary = 0;
        c.priority = -1;

        if (parts[0] == NULL) continue;
        if (strcasecmp(parts[0], "insert") == 0 && p >= 4 && parts[1] && parts[2] && parts[3]) {
            strncpy(c.command, "insert", sizeof(c.command)-1);
            strncpy(c.name, parts[1], sizeof(c.name)-1);
            c.salary = (uint32_t)atoi(parts[2]);
            c.priority = atoi(parts[3]);
        } else if (strcasecmp(parts[0], "delete") == 0 && p >= 3 && parts[1] && parts[2]) {
            strncpy(c.command, "delete", sizeof(c.command)-1);
            strncpy(c.name, parts[1], sizeof(c.name)-1);
            c.priority = atoi(parts[2]);
        } else if (strcasecmp(parts[0], "update") == 0 && p >= 3 && parts[1] && parts[2]) {
            /* note: assignment sample gave update without priority; treat priority as -1 */
            strncpy(c.command, "update", sizeof(c.command)-1);
            strncpy(c.name, parts[1], sizeof(c.name)-1);
            c.salary = (uint32_t)atoi(parts[2]);
            c.priority = -1;
        } else if (strcasecmp(parts[0], "search") == 0 && p >= 3 && parts[1] && parts[2]) {
            strncpy(c.command, "search", sizeof(c.command)-1);
            strncpy(c.name, parts[1], sizeof(c.name)-1);
            c.priority = atoi(parts[2]);
        } else if (strcasecmp(parts[0], "print") == 0 && p >= 2 && parts[1]) {
            strncpy(c.command, "print", sizeof(c.command)-1);
            c.priority = atoi(parts[1]);
        } else {
            /* malformed line: skip */
            continue;
        }

        if (cnt + 1 > cap) {
            size_t newcap = cap ? cap * 2 : 16;
            cmd_t *tmp = realloc(arr, newcap * sizeof(cmd_t));
            if (!tmp) { perror("realloc"); free(arr); fclose(f); *out_count = 0; return NULL; }
            arr = tmp;
            cap = newcap;
        }
        arr[cnt++] = c;
    }

    fclose(f);
    *out_count = cnt;
    return arr;
}
