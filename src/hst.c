#include "hst.h"

// Create hash table and return pointer to it, or NULL if out of memory.
HT *ht_create(void)
{
    // Allocate space for hash table struct.
    HT *hashtable = (HT *)malloc(sizeof(HT));
    if (hashtable == NULL)
    {
        return NULL;
    }

    // Allocate (zero'd) space for entry buckets.
    HT_entry *entries = (HT_entry *)calloc(INITIAL_CAPACITY, sizeof(HT_entry));
    if (entries == NULL)
    {
        free(hashtable);
        return NULL;
    }

    hashtable->entries = entries;
    hashtable->capacity = INITIAL_CAPACITY;
    hashtable->length = 0;
    return hashtable;
}

// Free memory allocated for hash table, including allocated keys.
void ht_destroy(HT *table)
{
    for (int i = 0; i < table->capacity; i++)
    {
        free((void *)table->entries[i].key);
    }
    free(table->entries);
    free(table);
}

// Return 64-bit FNV-1a hash for key (NUL-terminated). See description:
// https://en.wikipedia.org/wiki/Fowler–Noll–Vo_hash_function
uint64_t hash_key(const char *key)
{
    /* FNV-1a folds each byte into a 64-bit accumulator. */
    uint64_t hash = FNV_OFFSET;
    for (const char *p = key; *p; p++)
    {
        hash ^= (uint64_t)(unsigned char)(*p);
        hash *= FNV_PRIME;
    }
    return hash;
}

// Get item with given key (NUL-terminated) from hash table. Return
// value (which was set with ht_set), or NULL if key not found.
void *ht_get(HT *table, const char *key)
{
    uint64_t hash = hash_key(key);
    size_t index = hash % table->capacity;

    /* Linear probing searches subsequent occupied slots after a collision. */
    while (index < table->capacity && table->entries[index].key != NULL && strcmp(key, table->entries[index].key))
    {
        index++;
    }

    if (index == table->capacity || table->entries[index].key == NULL)
    {
        return NULL;
    }

    return table->entries[index].value;
}

// Set item with given key (NUL-terminated) to value (which must not
// be NULL). If not already present in table, key is copied to newly
// allocated memory (keys are freed automatically when ht_destroy is
// called). Return address of copied key, or NULL if out of memory.
const char *ht_set(HT *table, const char *key, void *value)
{
    // Check for resize
    if (table->length >= table->capacity * (3.0 / 4.0))
    {
        ht_expand(table);
    }

    uint64_t hash = hash_key(key);
    size_t index = hash % table->capacity;

    /* The table owns its key copy, but only borrows the caller's value. */
    char *newKey = strdup(key);
    if (newKey == NULL)
    {
        return NULL;
    }

    if (table->entries[index].key == NULL)
    {
        table->entries[index].key = newKey;
        table->entries[index].value = value;
        table->length++;
        return newKey;
    }

    while (table->entries[index].key != NULL)
    {
        // Check for duplicates
        if (!strcmp(table->entries[index++].key, key))
        {
            free(newKey);
            return NULL;
        }
    }

    if (index == table->capacity)
    {
        free(newKey);
        return NULL;
    }

    table->entries[index].key = newKey;
    table->entries[index].value = value;
    table->length++;
    return newKey;
}

uint8_t ht_expand(HT *table)
{
    /* Capacity growth is intended to keep the probing load factor bounded. */
    uint16_t capacity = table->capacity * 2;
    HT_entry *entries = (HT_entry *)realloc(table->entries, sizeof(HT) * capacity);
    if (entries == NULL)
    {
        return false;
    }
    else
    {
        printf("Error happens inside EXPAND\n");
        table->entries = entries;
        table->capacity = capacity;
        return true;
    }
}

// Return number of items in hash table.
size_t ht_length(HT *table)
{
    if (table == NULL)
    {
        return 0;
    }
    return table->length;
}
