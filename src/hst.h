#ifndef HASH_TABLE
#define HASH_TABLE

// #include <sysdir.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>
#include <readline/readline.h>
#include <readline/history.h>

/* The modulo-based slot lookup requires a non-zero capacity. */
#define INITIAL_CAPACITY 16
#define FNV_OFFSET 14695981039346656037UL
#define FNV_PRIME 1099511628211UL

// Hash table entry (slot may be filled or empty).
typedef struct
{
    const char *key; /* NULL marks an unused slot. */
    void *value;
} HT_entry;

// Hash table structure: create with HT_create, free with HT_destroy.
typedef struct HT
{
    HT_entry *entries; /* Hash slots, addressed by hash modulo capacity. */
    size_t capacity;   /* Allocated slot count. */
    size_t length;     /* Number of occupied slots. */
} HT;

// Hash table iterator: create with HT_iterator, iterate with HT_next.
typedef struct
{
    const char *key; // current key
    void *value;     // current value

    // Don't use these fields directly.
    HT *_table;    // reference to hash table being iterated
    size_t _index; // current index into HT._entries
} HTi;

HT *ht_create(void);
void ht_destroy(HT *table);
uint64_t hash_key(const char *key);
void *ht_get(HT *table, const char *key);
uint8_t ht_expand(HT *table);
const char *ht_set(HT *table, const char *key, void *value);
size_t ht_length(HT *table);
HTi ht_iterator(HT *table);
int ht_next(HTi *it);

#endif
