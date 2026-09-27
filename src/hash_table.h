#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define HASH_TABLE_INITIAL_CAPACITY 16U

/* A NULL key identifies an unused slot. */
typedef struct {
    const char *key;
    void *value;
} HashTableEntry;

/* Open-addressed table; entries are located by hash and linear probing. */
typedef struct {
    HashTableEntry *entries;
    size_t capacity;
    size_t length;
} HashTable;

/* The table owns copied keys but only borrows values supplied to set(). */
HashTable *hash_table_create(void);
void hash_table_destroy(HashTable *table);
void *hash_table_get(const HashTable *table, const char *key);
bool hash_table_set(HashTable *table, const char *key, void *value);
size_t hash_table_length(const HashTable *table);

#endif
