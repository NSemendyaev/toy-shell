#include "hash_table.h"

#include <stdlib.h>
#include <string.h>

#define FNV_OFFSET_BASIS 14695981039346656037ULL
#define FNV_PRIME 1099511628211ULL

/* The table owns copied keys, so values may safely point to static data. */
static char *duplicate_string(const char *source)
{
    size_t length = strlen(source) + 1U;
    char *copy = malloc(length);

    if (copy != NULL) {
        memcpy(copy, source, length);
    }

    return copy;
}

static uint64_t hash_table_key_hash(const char *key)
{
    /* FNV-1a is a compact non-cryptographic hash suitable for table indexing. */
    uint64_t hash = FNV_OFFSET_BASIS;

    for (const unsigned char *character = (const unsigned char *)key;
         *character != '\0'; ++character) {
        hash ^= *character;
        hash *= FNV_PRIME;
    }

    return hash;
}

static bool hash_table_resize(HashTable *table, size_t new_capacity)
{
    /* Slot positions depend on capacity, so every occupied entry must be rehashed. */
    HashTableEntry *old_entries = table->entries;
    size_t old_capacity = table->capacity;
    HashTableEntry *new_entries = calloc(new_capacity, sizeof(*new_entries));

    if (new_entries == NULL) {
        return false;
    }

    table->entries = new_entries;
    table->capacity = new_capacity;
    table->length = 0;

    for (size_t entry_index = 0; entry_index < old_capacity; ++entry_index) {
        HashTableEntry *entry = &old_entries[entry_index];
        if (entry->key == NULL) {
            continue;
        }

        size_t slot_index = hash_table_key_hash(entry->key) % table->capacity;
        while (table->entries[slot_index].key != NULL) {
            slot_index = (slot_index + 1U) % table->capacity;
        }

        table->entries[slot_index] = *entry;
        ++table->length;
    }

    free(old_entries);
    return true;
}

HashTable *hash_table_create(void)
{
    /* calloc initialises every entry key to NULL, which marks an empty slot. */
    HashTable *table = calloc(1, sizeof(*table));
    if (table == NULL) {
        return NULL;
    }

    table->entries = calloc(HASH_TABLE_INITIAL_CAPACITY, sizeof(*table->entries));
    if (table->entries == NULL) {
        free(table);
        return NULL;
    }

    table->capacity = HASH_TABLE_INITIAL_CAPACITY;
    return table;
}

void hash_table_destroy(HashTable *table)
{
    if (table == NULL) {
        return;
    }

    for (size_t entry_index = 0; entry_index < table->capacity; ++entry_index) {
        free((void *)table->entries[entry_index].key);
    }

    free(table->entries);
    free(table);
}

void *hash_table_get(const HashTable *table, const char *key)
{
    if (table == NULL || key == NULL) {
        return NULL;
    }

    size_t slot_index = hash_table_key_hash(key) % table->capacity;
    /* Linear probing continues past collisions and wraps at the final slot. */
    for (size_t probes = 0; probes < table->capacity; ++probes) {
        HashTableEntry *entry = &table->entries[slot_index];
        if (entry->key == NULL) {
            return NULL;
        }
        if (strcmp(key, entry->key) == 0) {
            return entry->value;
        }
        slot_index = (slot_index + 1U) % table->capacity;
    }

    return NULL;
}

bool hash_table_set(HashTable *table, const char *key, void *value)
{
    if (table == NULL || key == NULL || value == NULL) {
        return false;
    }
    /* Grow before insertion once the table would exceed a 75% load factor. */
    if ((table->length + 1U) * 4U > table->capacity * 3U &&
        !hash_table_resize(table, table->capacity * 2U)) {
        return false;
    }

    size_t slot_index = hash_table_key_hash(key) % table->capacity;
    while (table->entries[slot_index].key != NULL) {
        if (strcmp(key, table->entries[slot_index].key) == 0) {
            table->entries[slot_index].value = value;
            return true;
        }
        slot_index = (slot_index + 1U) % table->capacity;
    }

    char *key_copy = duplicate_string(key);
    if (key_copy == NULL) {
        return false;
    }

    table->entries[slot_index].key = key_copy;
    table->entries[slot_index].value = value;
    ++table->length;
    return true;
}

size_t hash_table_length(const HashTable *table)
{
    return table == NULL ? 0U : table->length;
}
