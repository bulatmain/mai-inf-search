#ifndef STEMMER_H
#define STEMMER_H

#include "mystring.h"

static int is_vowel_at(const char *w, int i) {
    char c = w[i];
    if (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u') return 1;
    if (c == 'y' && i > 0) return 1;
    return 0;
}

static int measure(const char *w, int len) {
    int m = 0;
    int i = 0;
    while (i < len && is_vowel_at(w, i)) i++;
    while (i < len) {
        while (i < len && !is_vowel_at(w, i)) i++;
        if (i >= len) break;
        m++;
        while (i < len && is_vowel_at(w, i)) i++;
    }
    return m;
}

static int has_vowel(const char *w, int len) {
    for (int i = 0; i < len; i++) {
        if (is_vowel_at(w, i)) return 1;
    }
    return 0;
}

static int ends_with_double_consonant(const char *w, int len) {
    if (len < 2) return 0;
    if (w[len - 1] != w[len - 2]) return 0;
    return !is_vowel_at(w, len - 1);
}

static int ends_cvc(const char *w, int len) {
    if (len < 3) return 0;
    if (is_vowel_at(w, len - 1)) return 0;
    if (!is_vowel_at(w, len - 2)) return 0;
    if (is_vowel_at(w, len - 3)) return 0;
    char c = w[len - 1];
    if (c == 'w' || c == 'x' || c == 'y') return 0;
    return 1;
}

static int endswith(const char *w, int len, const char *suffix) {
    int slen = my_strlen(suffix);
    if (len < slen) return 0;
    for (int i = 0; i < slen; i++) {
        if (w[len - slen + i] != suffix[i]) return 0;
    }
    return 1;
}

static int replace_suffix(char *w, int *len, const char *old_suf, const char *new_suf) {
    int olen = my_strlen(old_suf);
    int nlen = my_strlen(new_suf);
    if (!endswith(w, *len, old_suf)) return 0;
    int base = *len - olen;
    for (int i = 0; i < nlen; i++) w[base + i] = new_suf[i];
    w[base + nlen] = '\0';
    *len = base + nlen;
    return 1;
}

inline void stem_english(char *w) {
    int len = my_strlen(w);
    if (len <= 2) return;

    if (endswith(w, len, "sses")) {
        replace_suffix(w, &len, "sses", "ss");
    } else if (endswith(w, len, "ies")) {
        replace_suffix(w, &len, "ies", "i");
    } else if (!endswith(w, len, "ss") && endswith(w, len, "s")) {
        w[--len] = '\0';
    }

    int step1b_extra = 0;
    if (endswith(w, len, "eed")) {
        int base = len - 3;
        if (measure(w, base) > 0) {
            replace_suffix(w, &len, "eed", "ee");
        }
    } else if (endswith(w, len, "ed")) {
        int base = len - 2;
        if (has_vowel(w, base)) {
            w[base] = '\0'; len = base;
            step1b_extra = 1;
        }
    } else if (endswith(w, len, "ing")) {
        int base = len - 3;
        if (has_vowel(w, base)) {
            w[base] = '\0'; len = base;
            step1b_extra = 1;
        }
    }

    if (step1b_extra) {
        if (endswith(w, len, "at")) {
            replace_suffix(w, &len, "at", "ate");
        } else if (endswith(w, len, "bl")) {
            replace_suffix(w, &len, "bl", "ble");
        } else if (endswith(w, len, "iz")) {
            replace_suffix(w, &len, "iz", "ize");
        } else if (ends_with_double_consonant(w, len)
                   && w[len-1] != 'l' && w[len-1] != 's' && w[len-1] != 'z') {
            w[--len] = '\0';
        } else if (measure(w, len) == 1 && ends_cvc(w, len)) {
            w[len] = 'e'; w[++len] = '\0';
        }
    }

    if (len > 2 && w[len - 1] == 'y' && has_vowel(w, len - 1)) {
        w[len - 1] = 'i';
    }

    if (len > 1) {
        int m;
        if (endswith(w, len, "ational")) { m = measure(w, len - 7); if (m > 0) replace_suffix(w, &len, "ational", "ate"); }
        else if (endswith(w, len, "tional"))  { m = measure(w, len - 6); if (m > 0) replace_suffix(w, &len, "tional", "tion"); }
        else if (endswith(w, len, "enci"))    { m = measure(w, len - 4); if (m > 0) replace_suffix(w, &len, "enci", "ence"); }
        else if (endswith(w, len, "anci"))    { m = measure(w, len - 4); if (m > 0) replace_suffix(w, &len, "anci", "ance"); }
        else if (endswith(w, len, "izer"))    { m = measure(w, len - 4); if (m > 0) replace_suffix(w, &len, "izer", "ize"); }
        else if (endswith(w, len, "abli"))    { m = measure(w, len - 4); if (m > 0) replace_suffix(w, &len, "abli", "able"); }
        else if (endswith(w, len, "alli"))    { m = measure(w, len - 4); if (m > 0) replace_suffix(w, &len, "alli", "al"); }
        else if (endswith(w, len, "entli"))   { m = measure(w, len - 5); if (m > 0) replace_suffix(w, &len, "entli", "ent"); }
        else if (endswith(w, len, "eli"))     { m = measure(w, len - 3); if (m > 0) replace_suffix(w, &len, "eli", "e"); }
        else if (endswith(w, len, "ousli"))   { m = measure(w, len - 5); if (m > 0) replace_suffix(w, &len, "ousli", "ous"); }
        else if (endswith(w, len, "ization")) { m = measure(w, len - 7); if (m > 0) replace_suffix(w, &len, "ization", "ize"); }
        else if (endswith(w, len, "ation"))   { m = measure(w, len - 5); if (m > 0) replace_suffix(w, &len, "ation", "ate"); }
        else if (endswith(w, len, "ator"))    { m = measure(w, len - 4); if (m > 0) replace_suffix(w, &len, "ator", "ate"); }
        else if (endswith(w, len, "alism"))   { m = measure(w, len - 5); if (m > 0) replace_suffix(w, &len, "alism", "al"); }
        else if (endswith(w, len, "iveness")) { m = measure(w, len - 7); if (m > 0) replace_suffix(w, &len, "iveness", "ive"); }
        else if (endswith(w, len, "fulness")) { m = measure(w, len - 7); if (m > 0) replace_suffix(w, &len, "fulness", "ful"); }
        else if (endswith(w, len, "ousnes"))  { m = measure(w, len - 6); if (m > 0) replace_suffix(w, &len, "ousnes", "ous"); }
        else if (endswith(w, len, "aliti"))   { m = measure(w, len - 5); if (m > 0) replace_suffix(w, &len, "aliti", "al"); }
        else if (endswith(w, len, "iviti"))   { m = measure(w, len - 5); if (m > 0) replace_suffix(w, &len, "iviti", "ive"); }
        else if (endswith(w, len, "biliti"))  { m = measure(w, len - 6); if (m > 0) replace_suffix(w, &len, "biliti", "ble"); }
    }

    if (len > 1) {
        int m;
        if (endswith(w, len, "icate"))  { m = measure(w, len - 5); if (m > 0) replace_suffix(w, &len, "icate", "ic"); }
        else if (endswith(w, len, "ative"))  { m = measure(w, len - 5); if (m > 0) { w[len - 5] = '\0'; len -= 5; } }
        else if (endswith(w, len, "alize"))  { m = measure(w, len - 5); if (m > 0) replace_suffix(w, &len, "alize", "al"); }
        else if (endswith(w, len, "iciti"))  { m = measure(w, len - 5); if (m > 0) replace_suffix(w, &len, "iciti", "ic"); }
        else if (endswith(w, len, "ical"))   { m = measure(w, len - 4); if (m > 0) replace_suffix(w, &len, "ical", "ic"); }
        else if (endswith(w, len, "ful"))    { m = measure(w, len - 3); if (m > 0) { w[len - 3] = '\0'; len -= 3; } }
        else if (endswith(w, len, "ness"))   { m = measure(w, len - 4); if (m > 0) { w[len - 4] = '\0'; len -= 4; } }
    }

    if (len > 1) {
        int m;
        if (endswith(w, len, "al"))      { m = measure(w, len - 2); if (m > 1) { w[len-2]=0; len-=2; } }
        else if (endswith(w, len, "ance"))  { m = measure(w, len - 4); if (m > 1) { w[len-4]=0; len-=4; } }
        else if (endswith(w, len, "ence"))  { m = measure(w, len - 4); if (m > 1) { w[len-4]=0; len-=4; } }
        else if (endswith(w, len, "er"))    { m = measure(w, len - 2); if (m > 1) { w[len-2]=0; len-=2; } }
        else if (endswith(w, len, "ic"))    { m = measure(w, len - 2); if (m > 1) { w[len-2]=0; len-=2; } }
        else if (endswith(w, len, "able"))  { m = measure(w, len - 4); if (m > 1) { w[len-4]=0; len-=4; } }
        else if (endswith(w, len, "ible"))  { m = measure(w, len - 4); if (m > 1) { w[len-4]=0; len-=4; } }
        else if (endswith(w, len, "ant"))   { m = measure(w, len - 3); if (m > 1) { w[len-3]=0; len-=3; } }
        else if (endswith(w, len, "ement")) { m = measure(w, len - 5); if (m > 1) { w[len-5]=0; len-=5; } }
        else if (endswith(w, len, "ment"))  { m = measure(w, len - 4); if (m > 1) { w[len-4]=0; len-=4; } }
        else if (endswith(w, len, "ent"))   { m = measure(w, len - 3); if (m > 1) { w[len-3]=0; len-=3; } }
        else if (endswith(w, len, "ion"))   {
            m = measure(w, len - 3);
            if (m > 1 && len >= 4 && (w[len-4] == 's' || w[len-4] == 't')) { w[len-3]=0; len-=3; }
        }
        else if (endswith(w, len, "ou"))    { m = measure(w, len - 2); if (m > 1) { w[len-2]=0; len-=2; } }
        else if (endswith(w, len, "ism"))   { m = measure(w, len - 3); if (m > 1) { w[len-3]=0; len-=3; } }
        else if (endswith(w, len, "ate"))   { m = measure(w, len - 3); if (m > 1) { w[len-3]=0; len-=3; } }
        else if (endswith(w, len, "iti"))   { m = measure(w, len - 3); if (m > 1) { w[len-3]=0; len-=3; } }
        else if (endswith(w, len, "ous"))   { m = measure(w, len - 3); if (m > 1) { w[len-3]=0; len-=3; } }
        else if (endswith(w, len, "ive"))   { m = measure(w, len - 3); if (m > 1) { w[len-3]=0; len-=3; } }
        else if (endswith(w, len, "ize"))   { m = measure(w, len - 3); if (m > 1) { w[len-3]=0; len-=3; } }
    }

    if (w[len - 1] == 'e') {
        int m = measure(w, len - 1);
        if (m > 1 || (m == 1 && !ends_cvc(w, len - 1))) {
            w[--len] = '\0';
        }
    }

    if (ends_with_double_consonant(w, len) && w[len - 1] == 'l') {
        if (measure(w, len) > 1) {
            w[--len] = '\0';
        }
    }
}

#endif
