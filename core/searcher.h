#ifndef SEARCHER_H
#define SEARCHER_H

#include <cstdio>
#include <cstring>
#include "mystring.h"
#include "dynarray.h"
#include "stemmer.h"

#define MAX_QUERY 4096

struct TermEntry {
    char *term;
    unsigned long long postings_offset;
    unsigned int postings_count;
};

struct DocEntry {
    char *url;
    char *title;
};

struct SearchIndex {
    FILE *fp;
    int num_terms;
    int num_docs;
    unsigned long long dict_offset;
    unsigned long long postings_offset;
    TermEntry *terms;
    DocEntry *docs;
};

inline void read_u16(FILE *f, unsigned short *v) { fread(v, 2, 1, f); }
inline void read_u32(FILE *f, unsigned int *v) { fread(v, 4, 1, f); }
inline void read_u64(FILE *f, unsigned long long *v) { fread(v, 8, 1, f); }

int index_load(SearchIndex *idx, const char *path) {
    idx->fp = fopen(path, "rb");
    if (!idx->fp) return -1;

    char magic[4];
    fread(magic, 1, 4, idx->fp);
    if (magic[0] != 'B' || magic[1] != 'I' || magic[2] != 'D' || magic[3] != 'X') {
        fclose(idx->fp);
        return -2;
    }

    unsigned int version;
    read_u32(idx->fp, &version);

    unsigned int nt, nd;
    read_u32(idx->fp, &nt); idx->num_terms = nt;
    read_u32(idx->fp, &nd); idx->num_docs = nd;
    read_u64(idx->fp, &idx->dict_offset);
    read_u64(idx->fp, &idx->postings_offset);

    idx->docs = new DocEntry[nd];
    for (unsigned int i = 0; i < nd; i++) {
        unsigned short url_len, title_len;
        read_u16(idx->fp, &url_len);
        idx->docs[i].url = new char[url_len + 1];
        fread(idx->docs[i].url, 1, url_len, idx->fp);
        idx->docs[i].url[url_len] = '\0';

        read_u16(idx->fp, &title_len);
        idx->docs[i].title = new char[title_len + 1];
        fread(idx->docs[i].title, 1, title_len, idx->fp);
        idx->docs[i].title[title_len] = '\0';
    }

    fseek(idx->fp, idx->dict_offset, SEEK_SET);
    idx->terms = new TermEntry[nt];
    for (unsigned int i = 0; i < nt; i++) {
        unsigned char tlen;
        fread(&tlen, 1, 1, idx->fp);
        idx->terms[i].term = new char[tlen + 1];
        fread(idx->terms[i].term, 1, tlen, idx->fp);
        idx->terms[i].term[tlen] = '\0';
        read_u64(idx->fp, &idx->terms[i].postings_offset);
        read_u32(idx->fp, &idx->terms[i].postings_count);
    }

    return 0;
}

void index_close(SearchIndex *idx) {
    for (int i = 0; i < idx->num_terms; i++) delete[] idx->terms[i].term;
    delete[] idx->terms;
    for (int i = 0; i < idx->num_docs; i++) {
        delete[] idx->docs[i].url;
        delete[] idx->docs[i].title;
    }
    delete[] idx->docs;
    if (idx->fp) fclose(idx->fp);
}

int index_find_term(SearchIndex *idx, const char *term) {
    int lo = 0, hi = idx->num_terms - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        int cmp = my_strcmp(idx->terms[mid].term, term);
        if (cmp == 0) return mid;
        if (cmp < 0) lo = mid + 1;
        else hi = mid - 1;
    }
    return -1;
}

IntArray index_get_postings(SearchIndex *idx, int term_idx) {
    IntArray result;
    intarray_init(&result);
    if (term_idx < 0 || term_idx >= idx->num_terms) return result;

    TermEntry *te = &idx->terms[term_idx];
    fseek(idx->fp, te->postings_offset, SEEK_SET);
    for (unsigned int i = 0; i < te->postings_count; i++) {
        unsigned int doc_id;
        read_u32(idx->fp, &doc_id);
        intarray_push(&result, (int)doc_id);
    }
    return result;
}

IntArray index_get_all_doc_ids(SearchIndex *idx) {
    IntArray all;
    intarray_init(&all);
    for (int i = 0; i < idx->num_docs; i++) {
        intarray_push(&all, i);
    }
    return all;
}

struct QueryParser {
    const char *input;
    int pos;
    SearchIndex *idx;
};

static void qp_skip_spaces(QueryParser *qp) {
    while (qp->input[qp->pos] == ' ' || qp->input[qp->pos] == '\t')
        qp->pos++;
}

static IntArray qp_or_expr(QueryParser *qp);
static IntArray qp_and_expr(QueryParser *qp);
static IntArray qp_not_expr(QueryParser *qp);
static IntArray qp_atom(QueryParser *qp);

static IntArray qp_atom(QueryParser *qp) {
    qp_skip_spaces(qp);
    IntArray result;
    intarray_init(&result);

    if (qp->input[qp->pos] == '(') {
        qp->pos++;
        result = qp_or_expr(qp);
        qp_skip_spaces(qp);
        if (qp->input[qp->pos] == ')') qp->pos++;
        return result;
    }

    char word[MAX_QUERY];
    int wi = 0;
    while (qp->input[qp->pos] &&
           qp->input[qp->pos] != ' ' &&
           qp->input[qp->pos] != '\t' &&
           qp->input[qp->pos] != '(' &&
           qp->input[qp->pos] != ')' &&
           qp->input[qp->pos] != '!' &&
           !(qp->input[qp->pos] == '&' && qp->input[qp->pos+1] == '&') &&
           !(qp->input[qp->pos] == '|' && qp->input[qp->pos+1] == '|')) {
        if (wi < MAX_QUERY - 1) word[wi++] = my_tolower(qp->input[qp->pos]);
        qp->pos++;
    }
    word[wi] = '\0';

    if (wi == 0) return result;

    stem_english(word);
    int tidx = index_find_term(qp->idx, word);
    if (tidx >= 0) {
        result = index_get_postings(qp->idx, tidx);
    }
    return result;
}

static IntArray qp_not_expr(QueryParser *qp) {
    qp_skip_spaces(qp);
    if (qp->input[qp->pos] == '!') {
        qp->pos++;
        IntArray inner = qp_not_expr(qp);
        IntArray all = index_get_all_doc_ids(qp->idx);
        IntArray result = intarray_difference(&all, &inner);
        intarray_free(&inner);
        intarray_free(&all);
        return result;
    }
    return qp_atom(qp);
}

static IntArray qp_and_expr(QueryParser *qp) {
    IntArray left = qp_not_expr(qp);
    while (1) {
        qp_skip_spaces(qp);
        char c = qp->input[qp->pos];
        if (c == '\0' || c == ')' || (c == '|' && qp->input[qp->pos+1] == '|')) break;

        if (c == '&' && qp->input[qp->pos+1] == '&') {
            qp->pos += 2;
        }
        IntArray right = qp_not_expr(qp);
        IntArray merged = intarray_intersect(&left, &right);
        intarray_free(&left);
        intarray_free(&right);
        left = merged;
    }
    return left;
}

static IntArray qp_or_expr(QueryParser *qp) {
    IntArray left = qp_and_expr(qp);
    while (1) {
        qp_skip_spaces(qp);
        if (qp->input[qp->pos] == '|' && qp->input[qp->pos+1] == '|') {
            qp->pos += 2;
            IntArray right = qp_and_expr(qp);
            IntArray merged = intarray_union(&left, &right);
            intarray_free(&left);
            intarray_free(&right);
            left = merged;
        } else {
            break;
        }
    }
    return left;
}

IntArray search_query(SearchIndex *idx, const char *query) {
    QueryParser qp;
    qp.input = query;
    qp.pos = 0;
    qp.idx = idx;
    return qp_or_expr(&qp);
}

#endif
