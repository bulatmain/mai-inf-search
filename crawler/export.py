#!/usr/bin/env python3
import sys
import os
import csv
import yaml
import pymongo


def main():
    if len(sys.argv) < 3:
        print("Usage: python3 export.py <config.yaml> <output_dir>")
        sys.exit(1)

    with open(sys.argv[1]) as f:
        cfg = yaml.safe_load(f)

    output_dir = sys.argv[2]
    os.makedirs(output_dir, exist_ok=True)

    client = pymongo.MongoClient(cfg["db"]["host"], cfg["db"]["port"])
    db = client[cfg["db"]["name"]]

    total_docs = 0
    total_raw_bytes = 0
    total_text_bytes = 0

    pipeline = [
        {"$project": {"url": 1, "title": 1, "text": 1, "source": 1,
                       "raw_len": {"$strLenBytes": {"$ifNull": ["$raw_html", ""]}}}}
    ]

    meta_path = os.path.join(output_dir, "metadata.csv")
    with open(meta_path, "w", newline="", encoding="utf-8") as mf:
        writer = csv.writer(mf)
        writer.writerow(["id", "url", "title", "source", "text_bytes", "raw_bytes"])

        for doc in db.documents.aggregate(pipeline, allowDiskUse=True, batchSize=500):
            text = doc.get("text", "")
            title = doc.get("title", "")
            url = doc.get("url", "")
            source = doc.get("source", "")

            if len(text) < 200:
                continue

            filepath = os.path.join(output_dir, f"{total_docs}.txt")
            with open(filepath, "w", encoding="utf-8") as f:
                f.write(title + "\n")
                f.write(url + "\n")
                f.write(text + "\n")

            raw_size = doc.get("raw_len", 0)
            text_size = len(text.encode("utf-8"))
            total_raw_bytes += raw_size
            total_text_bytes += text_size

            writer.writerow([total_docs, url, title, source, text_size, raw_size])
            total_docs += 1

            if total_docs % 1000 == 0:
                print(f"Exported {total_docs} documents...", flush=True)

    print(f"\n=== Export Statistics ===")
    print(f"Total documents exported: {total_docs}")
    print(f"Total raw HTML size: {total_raw_bytes / (1024*1024):.1f} MB")
    print(f"Total text size: {total_text_bytes / (1024*1024):.1f} MB")
    if total_docs > 0:
        print(f"Average raw doc size: {total_raw_bytes / total_docs:.0f} bytes")
        print(f"Average text per doc: {total_text_bytes / total_docs:.0f} bytes")

    stats_path = os.path.join(output_dir, "stats.txt")
    with open(stats_path, "w") as sf:
        sf.write(f"total_documents={total_docs}\n")
        sf.write(f"total_raw_bytes={total_raw_bytes}\n")
        sf.write(f"total_text_bytes={total_text_bytes}\n")
        if total_docs > 0:
            sf.write(f"avg_raw_doc_bytes={total_raw_bytes // total_docs}\n")
            sf.write(f"avg_text_doc_bytes={total_text_bytes // total_docs}\n")

    print(f"Stats written to {stats_path}")
    print(f"Metadata written to {meta_path}")


if __name__ == "__main__":
    main()
