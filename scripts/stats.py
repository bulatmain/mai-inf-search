#!/usr/bin/env python3
import sys
import os


def main():
    if len(sys.argv) < 2:
        print("Usage: python3 stats.py <corpus_dir>")
        sys.exit(1)

    corpus_dir = sys.argv[1]
    total_docs = 0
    total_bytes = 0
    total_text_bytes = 0
    min_size = float('inf')
    max_size = 0

    for fname in os.listdir(corpus_dir):
        if not fname.endswith(".txt") or fname == "stats.txt":
            continue

        fpath = os.path.join(corpus_dir, fname)
        with open(fpath, "r", encoding="utf-8") as f:
            lines = f.readlines()

        if len(lines) < 3:
            continue

        text = "".join(lines[2:])
        size = len(text.encode("utf-8"))
        total_text_bytes += size
        total_bytes += os.path.getsize(fpath)
        total_docs += 1
        if size < min_size:
            min_size = size
        if size > max_size:
            max_size = size

    print("=== Corpus Statistics ===")
    print(f"Total documents: {total_docs}")
    print(f"Total file size: {total_bytes / (1024*1024):.1f} MB")
    print(f"Total text size: {total_text_bytes / (1024*1024):.1f} MB")
    if total_docs > 0:
        avg = total_text_bytes / total_docs
        print(f"Average text per document: {avg:.0f} bytes ({avg/1024:.1f} KB)")
        print(f"Min document text: {min_size} bytes")
        print(f"Max document text: {max_size} bytes")


if __name__ == "__main__":
    main()
