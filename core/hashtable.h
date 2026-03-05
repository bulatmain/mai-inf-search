#ifndef HASHTABLE_H
#define HASHTABLE_H

#include "mystring.h"
#include "dynarray.h"

struct HTEntry {
    char *key;
    IntArray postings;
    HTEntry *next;
};

struct HashTable {
    HTEntry **buckets;
    int num_buckets;
    int num_entries;
};

inline unsigned int ht_hash(const char *key, int num_buckets) {
    unsigned int hash = 5381;
    while (*key) {
        hash = hash * 33 + (unsigned char)(*key);
        key++;
    }
    return hash % num_buckets;
}

inline void ht_init(HashTable *ht, int num_buckets) {
    ht->num_buckets = num_buckets;
    ht->num_entries = 0;
    ht->buckets = new HTEntry*[num_buckets];
    for (int i = 0; i < num_buckets; i++) ht->buckets[i] = NULL;
}

inline void ht_free(HashTable *ht) {
    for (int i = 0; i < ht->num_buckets; i++) {
        HTEntry *e = ht->buckets[i];
        while (e) {
            HTEntry *next = e->next;
            delete[] e->key;
            intarray_free(&e->postings);
            delete e;
            e = next;
        }
    }
    delete[] ht->buckets;
    ht->buckets = NULL;
    ht->num_buckets = 0;
    ht->num_entries = 0;
}

inline HTEntry *ht_find(HashTable *ht, const char *key) {
    unsigned int idx = ht_hash(key, ht->num_buckets);
    HTEntry *e = ht->buckets[idx];
    while (e) {
        if (my_strcmp(e->key, key) == 0) return e;
        e = e->next;
    }
    return NULL;
}

inline HTEntry *ht_insert(HashTable *ht, const char *key) {
    HTEntry *existing = ht_find(ht, key);
    if (existing) return existing;

    unsigned int idx = ht_hash(key, ht->num_buckets);
    HTEntry *e = new HTEntry;
    e->key = my_strdup(key);
    intarray_init(&e->postings);
    e->next = ht->buckets[idx];
    ht->buckets[idx] = e;
    ht->num_entries++;
    return e;
}

inline void ht_add_posting(HashTable *ht, const char *term, int doc_id) {
    HTEntry *e = ht_insert(ht, term);
    if (e->postings.size == 0 || e->postings.data[e->postings.size - 1] != doc_id) {
        intarray_push(&e->postings, doc_id);
    }
}

struct HTIterator {
    HashTable *ht;
    int bucket_idx;
    HTEntry *current;
};

inline void ht_iter_init(HTIterator *it, HashTable *ht) {
    it->ht = ht;
    it->bucket_idx = 0;
    it->current = NULL;
    for (int i = 0; i < ht->num_buckets; i++) {
        if (ht->buckets[i]) {
            it->bucket_idx = i;
            it->current = ht->buckets[i];
            return;
        }
    }
}

inline HTEntry *ht_iter_next(HTIterator *it) {
    if (!it->current) return NULL;
    HTEntry *result = it->current;
    if (it->current->next) {
        it->current = it->current->next;
    } else {
        it->current = NULL;
        for (int i = it->bucket_idx + 1; i < it->ht->num_buckets; i++) {
            if (it->ht->buckets[i]) {
                it->bucket_idx = i;
                it->current = it->ht->buckets[i];
                break;
            }
        }
    }
    return result;
}

#endif
