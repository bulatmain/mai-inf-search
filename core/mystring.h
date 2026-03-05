#ifndef MYSTRING_H
#define MYSTRING_H

#include <cstring>
#include <cstdlib>
#include <cstdio>

inline int my_strlen(const char *s) {
    int len = 0;
    while (s[len]) len++;
    return len;
}

inline char *my_strdup(const char *s) {
    int len = my_strlen(s);
    char *copy = new char[len + 1];
    for (int i = 0; i <= len; i++) copy[i] = s[i];
    return copy;
}

inline int my_strcmp(const char *a, const char *b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

inline void my_strcpy(char *dst, const char *src) {
    while ((*dst++ = *src++));
}

inline char my_tolower(char c) {
    if (c >= 'A' && c <= 'Z') return c + 32;
    return c;
}

inline void my_to_lower_inplace(char *s) {
    for (int i = 0; s[i]; i++) {
        s[i] = my_tolower(s[i]);
    }
}

inline char *my_to_lower(const char *s) {
    char *r = my_strdup(s);
    my_to_lower_inplace(r);
    return r;
}

inline int my_is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

inline int my_is_digit(char c) {
    return c >= '0' && c <= '9';
}

inline int my_is_alnum(char c) {
    return my_is_alpha(c) || my_is_digit(c);
}

inline int my_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

inline int my_ends_with(const char *s, int len, const char *suffix, int suf_len) {
    if (len < suf_len) return 0;
    for (int i = 0; i < suf_len; i++) {
        if (s[len - suf_len + i] != suffix[i]) return 0;
    }
    return 1;
}

#endif
