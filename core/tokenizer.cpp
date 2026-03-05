#include <iostream>
#include <fstream>
#include <string>
#include <dirent.h>
#include <cstring>
#include <ctime>
#include <cstdio>

static bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

static bool is_token_char(char c) {
    return is_alpha(c) || is_digit(c) || c == '\'';
}

static char to_lower(char c) {
    if (c >= 'A' && c <= 'Z') return c + 32;
    return c;
}

static bool is_purely_numeric(const std::string &s) {
    for (size_t i = 0; i < s.size(); i++) {
        if (!is_digit(s[i])) return false;
    }
    return true;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <corpus_dir> <output_file>\n", argv[0]);
        return 1;
    }

    const char *corpus_dir = argv[1];
    const char *output_file = argv[2];

    DIR *dir = opendir(corpus_dir);
    if (!dir) {
        fprintf(stderr, "Cannot open directory: %s\n", corpus_dir);
        return 1;
    }

    FILE *out = fopen(output_file, "w");
    if (!out) {
        fprintf(stderr, "Cannot open output file: %s\n", output_file);
        closedir(dir);
        return 1;
    }

    long long total_tokens = 0;
    long long total_token_len = 0;
    long long total_bytes = 0;
    int num_files = 0;

    clock_t start = clock();

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
        int nlen = strlen(name);
        if (nlen < 5 || strcmp(name + nlen - 4, ".txt") != 0) continue;

        char filepath[4096];
        snprintf(filepath, sizeof(filepath), "%s/%s", corpus_dir, name);

        std::ifstream ifs(filepath);
        if (!ifs.is_open()) continue;

        std::string line;
        int line_num = 0;
        while (std::getline(ifs, line)) {
            line_num++;
            if (line_num <= 2) continue;

            total_bytes += line.size();
            int i = 0;
            int len = line.size();

            while (i < len) {
                while (i < len && !is_token_char(line[i])) i++;
                if (i >= len) break;

                std::string token;
                while (i < len && is_token_char(line[i])) {
                    token += to_lower(line[i]);
                    i++;
                }

                while (!token.empty() && token.back() == '\'') token.pop_back();
                while (!token.empty() && token.front() == '\'') token.erase(token.begin());

                if (token.size() >= 2 && !is_purely_numeric(token)) {
                    fprintf(out, "%s\n", token.c_str());
                    total_tokens++;
                    total_token_len += token.size();
                }
            }
        }
        ifs.close();
        num_files++;
        if (num_files % 1000 == 0) {
            fprintf(stderr, "Tokenized %d files...\n", num_files);
        }
    }

    closedir(dir);
    fclose(out);

    clock_t end = clock();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;

    printf("=== Tokenization Statistics ===\n");
    printf("Files processed: %d\n", num_files);
    printf("Total tokens: %lld\n", total_tokens);
    printf("Total input bytes: %lld\n", total_bytes);
    if (total_tokens > 0) {
        printf("Average token length: %.2f\n", (double)total_token_len / total_tokens);
    }
    printf("Time: %.3f seconds\n", elapsed);
    if (total_bytes > 0) {
        printf("Speed: %.1f KB/s\n", (total_bytes / 1024.0) / elapsed);
    }

    return 0;
}
