#ifndef DYNARRAY_H
#define DYNARRAY_H

#include <cstdlib>
#include <cstring>

struct IntArray {
    int *data;
    int size;
    int capacity;
};

inline void intarray_init(IntArray *a) {
    a->data = NULL;
    a->size = 0;
    a->capacity = 0;
}

inline void intarray_free(IntArray *a) {
    delete[] a->data;
    a->data = NULL;
    a->size = 0;
    a->capacity = 0;
}

inline void intarray_push(IntArray *a, int val) {
    if (a->size >= a->capacity) {
        int new_cap = a->capacity == 0 ? 16 : a->capacity * 2;
        int *new_data = new int[new_cap];
        for (int i = 0; i < a->size; i++) new_data[i] = a->data[i];
        delete[] a->data;
        a->data = new_data;
        a->capacity = new_cap;
    }
    a->data[a->size++] = val;
}

inline int intarray_contains(IntArray *a, int val) {
    for (int i = 0; i < a->size; i++) {
        if (a->data[i] == val) return 1;
    }
    return 0;
}

struct StrArray {
    char **data;
    int size;
    int capacity;
};

inline void strarray_init(StrArray *a) {
    a->data = NULL;
    a->size = 0;
    a->capacity = 0;
}

inline void strarray_free(StrArray *a) {
    for (int i = 0; i < a->size; i++) delete[] a->data[i];
    delete[] a->data;
    a->data = NULL;
    a->size = 0;
    a->capacity = 0;
}

inline void strarray_push(StrArray *a, const char *s) {
    if (a->size >= a->capacity) {
        int new_cap = a->capacity == 0 ? 16 : a->capacity * 2;
        char **new_data = new char*[new_cap];
        for (int i = 0; i < a->size; i++) new_data[i] = a->data[i];
        delete[] a->data;
        a->data = new_data;
        a->capacity = new_cap;
    }
    int len = 0;
    while (s[len]) len++;
    char *copy = new char[len + 1];
    for (int i = 0; i <= len; i++) copy[i] = s[i];
    a->data[a->size++] = copy;
}

inline void intarray_swap(int *a, int *b) {
    int tmp = *a; *a = *b; *b = tmp;
}

inline void intarray_sort_range(int *arr, int lo, int hi) {
    if (lo >= hi) return;
    int pivot = arr[(lo + hi) / 2];
    int i = lo, j = hi;
    while (i <= j) {
        while (arr[i] < pivot) i++;
        while (arr[j] > pivot) j--;
        if (i <= j) {
            intarray_swap(&arr[i], &arr[j]);
            i++; j--;
        }
    }
    if (lo < j) intarray_sort_range(arr, lo, j);
    if (i < hi) intarray_sort_range(arr, i, hi);
}

inline void intarray_sort(IntArray *a) {
    if (a->size > 1) intarray_sort_range(a->data, 0, a->size - 1);
}

inline void intarray_unique(IntArray *a) {
    if (a->size <= 1) return;
    int w = 1;
    for (int r = 1; r < a->size; r++) {
        if (a->data[r] != a->data[w - 1]) {
            a->data[w++] = a->data[r];
        }
    }
    a->size = w;
}

inline IntArray intarray_intersect(IntArray *a, IntArray *b) {
    IntArray result;
    intarray_init(&result);
    int i = 0, j = 0;
    while (i < a->size && j < b->size) {
        if (a->data[i] == b->data[j]) {
            intarray_push(&result, a->data[i]);
            i++; j++;
        } else if (a->data[i] < b->data[j]) {
            i++;
        } else {
            j++;
        }
    }
    return result;
}

inline IntArray intarray_union(IntArray *a, IntArray *b) {
    IntArray result;
    intarray_init(&result);
    int i = 0, j = 0;
    while (i < a->size && j < b->size) {
        if (a->data[i] == b->data[j]) {
            intarray_push(&result, a->data[i]);
            i++; j++;
        } else if (a->data[i] < b->data[j]) {
            intarray_push(&result, a->data[i]);
            i++;
        } else {
            intarray_push(&result, b->data[j]);
            j++;
        }
    }
    while (i < a->size) intarray_push(&result, a->data[i++]);
    while (j < b->size) intarray_push(&result, b->data[j++]);
    return result;
}

inline IntArray intarray_difference(IntArray *all_docs, IntArray *exclude) {
    IntArray result;
    intarray_init(&result);
    int i = 0, j = 0;
    while (i < all_docs->size && j < exclude->size) {
        if (all_docs->data[i] == exclude->data[j]) {
            i++; j++;
        } else if (all_docs->data[i] < exclude->data[j]) {
            intarray_push(&result, all_docs->data[i]);
            i++;
        } else {
            j++;
        }
    }
    while (i < all_docs->size) intarray_push(&result, all_docs->data[i++]);
    return result;
}

#endif
