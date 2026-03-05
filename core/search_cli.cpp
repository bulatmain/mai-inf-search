#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "searcher.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <index_file> [--limit N] [--json]\n", argv[0]);
        return 1;
    }

    const char *index_path = argv[1];
    int limit = 50;
    int json_mode = 0;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--limit") == 0 && i + 1 < argc) {
            limit = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--json") == 0) {
            json_mode = 1;
        }
    }

    SearchIndex idx;
    int rc = index_load(&idx, index_path);
    if (rc != 0) {
        fprintf(stderr, "Failed to load index: %s (error %d)\n", index_path, rc);
        return 1;
    }

    fprintf(stderr, "Index loaded: %d terms, %d documents\n", idx.num_terms, idx.num_docs);

    char query[MAX_QUERY];
    while (fgets(query, MAX_QUERY, stdin)) {
        int qlen = strlen(query);
        while (qlen > 0 && (query[qlen-1] == '\n' || query[qlen-1] == '\r'))
            query[--qlen] = '\0';
        if (qlen == 0) continue;

        clock_t t0 = clock();
        IntArray results = search_query(&idx, query);
        clock_t t1 = clock();
        double elapsed_ms = (double)(t1 - t0) / CLOCKS_PER_SEC * 1000.0;

        int show = results.size < limit ? results.size : limit;

        if (json_mode) {
            printf("{\"query\":\"");
            for (const char *c = query; *c; c++) {
                if (*c == '"') printf("\\\"");
                else if (*c == '\\') printf("\\\\");
                else putchar(*c);
            }
            printf("\",\"total\":%d,\"time_ms\":%.2f,\"results\":[",
                   results.size, elapsed_ms);
            for (int i = 0; i < show; i++) {
                int did = results.data[i];
                if (i > 0) printf(",");
                printf("{\"id\":%d,\"title\":\"", did);
                for (const char *c = idx.docs[did].title; *c; c++) {
                    if (*c == '"') printf("\\\"");
                    else if (*c == '\\') printf("\\\\");
                    else putchar(*c);
                }
                printf("\",\"url\":\"");
                for (const char *c = idx.docs[did].url; *c; c++) {
                    if (*c == '"') printf("\\\"");
                    else if (*c == '\\') printf("\\\\");
                    else putchar(*c);
                }
                printf("\"}");
            }
            printf("]}\n");
        } else {
            printf("Query: %s\n", query);
            printf("Results: %d (%.2f ms)\n", results.size, elapsed_ms);
            for (int i = 0; i < show; i++) {
                int did = results.data[i];
                printf("  [%d] %s\n       %s\n", did, idx.docs[did].title, idx.docs[did].url);
            }
            printf("\n");
        }
        fflush(stdout);
        intarray_free(&results);
    }

    index_close(&idx);
    return 0;
}
