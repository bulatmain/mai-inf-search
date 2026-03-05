#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <dirent.h>

#include "mystring.h"
#include "dynarray.h"
#include "hashtable.h"
#include "stemmer.h"

#define MAX_LINE 65536
#define MAX_WORD 256
#define HT_BUCKETS 100003

struct DocInfo {
    char *title;
    char *url;
};

struct DocArray {
    DocInfo *data;
    int size;
    int capacity;
};

void docarray_init(DocArray *a) {
    a->data = NULL;
    a->size = 0;
    a->capacity = 0;
}

void docarray_push(DocArray *a, const char *title, const char *url) {
    if (a->size >= a->capacity) {
        int new_cap = a->capacity == 0 ? 256 : a->capacity * 2;
        DocInfo *nd = new DocInfo[new_cap];
        for (int i = 0; i < a->size; i++) nd[i] = a->data[i];
        delete[] a->data;
        a->data = nd;
        a->capacity = new_cap;
    }
    a->data[a->size].title = my_strdup(title);
    a->data[a->size].url = my_strdup(url);
    a->size++;
}

void docarray_free(DocArray *a) {
    for (int i = 0; i < a->size; i++) {
        delete[] a->data[i].title;
        delete[] a->data[i].url;
    }
    delete[] a->data;
}

static int is_token_char(char c) {
    return my_is_alpha(c) || my_is_digit(c) || c == '\'';
}

static int is_purely_digit(const char *s) {
    for (int i = 0; s[i]; i++) {
        if (!my_is_digit(s[i])) return 0;
    }
    return 1;
}

void tokenize_and_index(const char *text, int doc_id, HashTable *ht) {
    char word[MAX_WORD];
    int wi = 0;
    const char *p = text;

    while (*p) {
        if (is_token_char(*p)) {
            if (wi < MAX_WORD - 1) {
                word[wi++] = my_tolower(*p);
            }
        } else {
            if (wi >= 2) {
                word[wi] = '\0';
                while (wi > 0 && word[wi-1] == '\'') word[--wi] = '\0';
                char *start = word;
                while (*start == '\'') start++;
                int slen = my_strlen(start);
                if (slen >= 2 && !is_purely_digit(start)) {
                    stem_english(start);
                    if (my_strlen(start) >= 2) {
                        ht_add_posting(ht, start, doc_id);
                    }
                }
            }
            wi = 0;
        }
        p++;
    }
    if (wi >= 2) {
        word[wi] = '\0';
        while (wi > 0 && word[wi-1] == '\'') word[--wi] = '\0';
        char *start = word;
        while (*start == '\'') start++;
        int slen = my_strlen(start);
        if (slen >= 2 && !is_purely_digit(start)) {
            stem_english(start);
            if (my_strlen(start) >= 2) {
                ht_add_posting(ht, start, doc_id);
            }
        }
    }
}

void write_u16(FILE *f, unsigned short v) { fwrite(&v, 2, 1, f); }
void write_u32(FILE *f, unsigned int v) { fwrite(&v, 4, 1, f); }
void write_u64(FILE *f, unsigned long long v) { fwrite(&v, 8, 1, f); }

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <corpus_dir> <output_index>\n", argv[0]);
        return 1;
    }

    const char *corpus_dir = argv[1];
    const char *output_path = argv[2];

    clock_t start_time = clock();

    HashTable ht;
    ht_init(&ht, HT_BUCKETS);
    DocArray docs;
    docarray_init(&docs);

    DIR *dir = opendir(corpus_dir);
    if (!dir) {
        fprintf(stderr, "Cannot open directory: %s\n", corpus_dir);
        return 1;
    }

    char line[MAX_LINE];
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
        int nlen = my_strlen(name);
        if (nlen < 5) continue;
        if (name[nlen-4] != '.' || name[nlen-3] != 't' || name[nlen-2] != 'x' || name[nlen-1] != 't') continue;

        char filepath[4096];
        snprintf(filepath, sizeof(filepath), "%s/%s", corpus_dir, name);

        FILE *f = fopen(filepath, "r");
        if (!f) continue;

        char title_buf[MAX_LINE] = "";
        char url_buf[MAX_LINE] = "";
        if (fgets(title_buf, MAX_LINE, f)) {
            int tl = my_strlen(title_buf);
            if (tl > 0 && title_buf[tl-1] == '\n') title_buf[tl-1] = '\0';
        }
        if (fgets(url_buf, MAX_LINE, f)) {
            int ul = my_strlen(url_buf);
            if (ul > 0 && url_buf[ul-1] == '\n') url_buf[ul-1] = '\0';
        }

        int doc_id = docs.size;
        docarray_push(&docs, title_buf, url_buf);

        tokenize_and_index(title_buf, doc_id, &ht);

        while (fgets(line, MAX_LINE, f)) {
            tokenize_and_index(line, doc_id, &ht);
        }
        fclose(f);

        if (docs.size % 1000 == 0) {
            fprintf(stderr, "Indexed %d documents, %d terms...\n", docs.size, ht.num_entries);
        }
    }
    closedir(dir);

    fprintf(stderr, "Indexing complete: %d docs, %d terms\n", docs.size, ht.num_entries);

    HTIterator iter;
    ht_iter_init(&iter, &ht);
    HTEntry *e;
    while ((e = ht_iter_next(&iter)) != NULL) {
        intarray_sort(&e->postings);
        intarray_unique(&e->postings);
    }

    char **all_terms = new char*[ht.num_entries];
    IntArray *all_postings = new IntArray[ht.num_entries];
    int term_count = 0;

    ht_iter_init(&iter, &ht);
    while ((e = ht_iter_next(&iter)) != NULL) {
        all_terms[term_count] = e->key;
        all_postings[term_count] = e->postings;
        term_count++;
    }

    struct SortHelper {
        static void qsort_terms(char **terms, IntArray *posts, int lo, int hi) {
            if (lo >= hi) return;
            int i = lo, j = hi;
            char *pivot = terms[(lo + hi) / 2];
            while (i <= j) {
                while (my_strcmp(terms[i], pivot) < 0) i++;
                while (my_strcmp(terms[j], pivot) > 0) j--;
                if (i <= j) {
                    char *tmp = terms[i]; terms[i] = terms[j]; terms[j] = tmp;
                    IntArray ta = posts[i]; posts[i] = posts[j]; posts[j] = ta;
                    i++; j--;
                }
            }
            if (lo < j) qsort_terms(terms, posts, lo, j);
            if (i < hi) qsort_terms(terms, posts, i, hi);
        }
    };
    if (term_count > 1) {
        SortHelper::qsort_terms(all_terms, all_postings, 0, term_count - 1);
    }

    FILE *out = fopen(output_path, "wb");
    if (!out) {
        fprintf(stderr, "Cannot open output file: %s\n", output_path);
        return 1;
    }

    unsigned char header[32];
    memset(header, 0, 32);
    header[0] = 'B'; header[1] = 'I'; header[2] = 'D'; header[3] = 'X';
    fwrite(header, 1, 32, out);

    for (int i = 0; i < docs.size; i++) {
        unsigned short url_len = my_strlen(docs.data[i].url);
        unsigned short title_len = my_strlen(docs.data[i].title);
        write_u16(out, url_len);
        fwrite(docs.data[i].url, 1, url_len, out);
        write_u16(out, title_len);
        fwrite(docs.data[i].title, 1, title_len, out);
    }

    unsigned long long dict_offset = ftell(out);

    long long *posting_offset_positions = new long long[term_count];
    for (int i = 0; i < term_count; i++) {
        unsigned char tlen = my_strlen(all_terms[i]);
        fwrite(&tlen, 1, 1, out);
        fwrite(all_terms[i], 1, tlen, out);
        posting_offset_positions[i] = ftell(out);
        unsigned long long poff = 0;
        fwrite(&poff, 8, 1, out);
        unsigned int pcount = all_postings[i].size;
        fwrite(&pcount, 4, 1, out);
    }

    unsigned long long postings_offset = ftell(out);
    for (int i = 0; i < term_count; i++) {
        unsigned long long cur_off = ftell(out);
        fseek(out, posting_offset_positions[i], SEEK_SET);
        fwrite(&cur_off, 8, 1, out);
        fseek(out, cur_off, SEEK_SET);

        for (int j = 0; j < all_postings[i].size; j++) {
            write_u32(out, all_postings[i].data[j]);
        }
    }

    fseek(out, 0, SEEK_SET);
    header[0] = 'B'; header[1] = 'I'; header[2] = 'D'; header[3] = 'X';
    unsigned int version = 1;
    memcpy(header + 4, &version, 4);
    unsigned int num_terms = term_count;
    memcpy(header + 8, &num_terms, 4);
    unsigned int num_docs = docs.size;
    memcpy(header + 12, &num_docs, 4);
    memcpy(header + 16, &dict_offset, 8);
    memcpy(header + 24, &postings_offset, 8);
    fwrite(header, 1, 32, out);
    fclose(out);

    clock_t end_time = clock();
    double elapsed = (double)(end_time - start_time) / CLOCKS_PER_SEC;

    long long total_postings = 0;
    long long total_term_len = 0;
    for (int i = 0; i < term_count; i++) {
        total_postings += all_postings[i].size;
        total_term_len += my_strlen(all_terms[i]);
    }

    printf("=== Index Statistics ===\n");
    printf("Documents: %d\n", docs.size);
    printf("Terms: %d\n", term_count);
    printf("Total postings: %lld\n", total_postings);
    if (term_count > 0) {
        printf("Average term length: %.2f\n", (double)total_term_len / term_count);
        printf("Average postings per term: %.2f\n", (double)total_postings / term_count);
    }
    printf("Index build time: %.3f seconds\n", elapsed);
    if (docs.size > 0) {
        printf("Time per document: %.4f ms\n", (elapsed * 1000.0) / docs.size);
    }

    delete[] all_terms;
    delete[] all_postings;
    delete[] posting_offset_positions;
    docarray_free(&docs);

    return 0;
}
