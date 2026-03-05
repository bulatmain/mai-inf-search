#!/usr/bin/env python3
import sys
import os
import time
import hashlib
import logging
import yaml
import requests
import xml.etree.ElementTree as ET
from concurrent.futures import ThreadPoolExecutor, as_completed
from urllib.parse import urljoin, urlparse

import pymongo
from newspaper import Article

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s"
)
log = logging.getLogger("crawler")


def load_config(path):
    with open(path, "r") as f:
        return yaml.safe_load(f)


def connect_db(cfg):
    client = pymongo.MongoClient(cfg["db"]["host"], cfg["db"]["port"])
    db = client[cfg["db"]["name"]]
    db.documents.create_index("url", unique=True)
    return db


def fetch_sitemap_urls(sitemap_url, max_depth=3):
    urls = []
    if max_depth <= 0:
        return urls
    try:
        resp = requests.get(sitemap_url, timeout=15, headers={
            "User-Agent": "Mozilla/5.0 (compatible; StudentBot/1.0)"
        })
        if resp.status_code != 200:
            return urls
        root = ET.fromstring(resp.content)
        ns = {"s": "http://www.sitemaps.org/schemas/sitemap/0.9"}
        for sitemap in root.findall("s:sitemap/s:loc", ns):
            urls.extend(fetch_sitemap_urls(sitemap.text.strip(), max_depth - 1))
        for url_el in root.findall("s:url/s:loc", ns):
            urls.append(url_el.text.strip())
    except Exception as e:
        log.warning("Failed to parse sitemap %s: %s", sitemap_url, e)
    return urls


def discover_urls_from_source(source_cfg):
    base = source_cfg["url"].rstrip("/")
    all_urls = set()

    for sitemap_path in ["/sitemap.xml", "/sitemap_index.xml", "/news-sitemap.xml",
                         "/post-sitemap.xml", "/post-sitemap1.xml", "/post-sitemap2.xml",
                         "/post-sitemap3.xml", "/post-sitemap4.xml", "/post-sitemap5.xml",
                         "/post-sitemap6.xml", "/post-sitemap7.xml", "/post-sitemap8.xml",
                         "/post-sitemap9.xml", "/post-sitemap10.xml"]:
        sitemap_url = base + sitemap_path
        log.info("Trying sitemap: %s", sitemap_url)
        found = fetch_sitemap_urls(sitemap_url)
        log.info("  Found %d URLs from %s", len(found), sitemap_url)
        all_urls.update(found)

    domain = urlparse(base).netloc
    filtered = []
    for u in all_urls:
        parsed = urlparse(u)
        if parsed.netloc == domain or parsed.netloc == "www." + domain:
            path = parsed.path.lower()
            if any(seg in path for seg in ["/movie", "/film", "/review", "/news", "/feature",
                                           "/trailer", "/box-office", "/cast", "/director",
                                           "/oscar", "/actor", "/actress", "/cinema",
                                           "/streaming", "/tv/", "/show"]):
                filtered.append(u)
            elif len(path.split("/")) >= 3:
                filtered.append(u)

    log.info("Source %s: %d total URLs, %d after filtering", source_cfg["name"],
             len(all_urls), len(filtered))
    return filtered


def normalize_url(url):
    parsed = urlparse(url)
    path = parsed.path.rstrip("/")
    return f"{parsed.scheme}://{parsed.netloc}{path}"


def fetch_article(url, source_name):
    try:
        resp = requests.get(url, timeout=15, headers={
            "User-Agent": "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36"
        })
        if resp.status_code != 200:
            return None
        html = resp.text
        article = Article(url)
        article.set_html(html)
        article.parse()
        if not article.text or len(article.text) < 200:
            return None
        content_hash = hashlib.md5(html.encode("utf-8", errors="ignore")).hexdigest()
        return {
            "url": normalize_url(url),
            "raw_html": html,
            "title": article.title or "",
            "text": article.text,
            "source": source_name,
            "crawl_timestamp": int(time.time()),
            "content_hash": content_hash
        }
    except Exception as e:
        log.debug("Failed to fetch %s: %s", url, e)
        return None


def crawl_source(db, source_cfg, delay, max_pages, threads):
    source_name = source_cfg["name"]
    log.info("=== Starting crawl of %s ===", source_name)

    urls = discover_urls_from_source(source_cfg)
    if not urls:
        log.warning("No URLs found for %s", source_name)
        return 0

    existing = set()
    for doc in db.documents.find({"source": source_name}, {"url": 1}):
        existing.add(doc["url"])
    log.info("Already have %d documents from %s", len(existing), source_name)

    new_urls = [u for u in urls if normalize_url(u) not in existing]
    new_urls = new_urls[:max_pages]
    log.info("Will crawl %d new URLs from %s", len(new_urls), source_name)

    crawled = 0
    errors = 0

    def process_url(url):
        time.sleep(delay)
        return fetch_article(url, source_name)

    with ThreadPoolExecutor(max_workers=threads) as pool:
        futures = {pool.submit(process_url, u): u for u in new_urls}
        for future in as_completed(futures):
            url = futures[future]
            try:
                doc = future.result()
                if doc is None:
                    errors += 1
                    continue
                try:
                    db.documents.update_one(
                        {"url": doc["url"]},
                        {"$set": doc},
                        upsert=True
                    )
                    crawled += 1
                    if crawled % 100 == 0:
                        log.info("[%s] Crawled %d / %d (errors: %d)",
                                 source_name, crawled, len(new_urls), errors)
                except pymongo.errors.DuplicateKeyError:
                    pass
            except Exception as e:
                errors += 1
                log.debug("Error processing %s: %s", url, e)

    log.info("=== %s done: %d crawled, %d errors ===", source_name, crawled, errors)
    return crawled


def recrawl_changed(db, source_cfg, delay):
    source_name = source_cfg["name"]
    old_threshold = int(time.time()) - 86400 * 7
    stale = db.documents.find({
        "source": source_name,
        "crawl_timestamp": {"$lt": old_threshold}
    }).limit(500)

    updated = 0
    for old_doc in stale:
        time.sleep(delay)
        new_doc = fetch_article(old_doc["url"], source_name)
        if new_doc and new_doc["content_hash"] != old_doc.get("content_hash"):
            db.documents.update_one({"_id": old_doc["_id"]}, {"$set": new_doc})
            updated += 1

    log.info("[%s] Re-crawled: %d updated", source_name, updated)
    return updated


def print_stats(db):
    total = db.documents.count_documents({})
    sources = db.documents.distinct("source")
    log.info("=== Corpus Statistics ===")
    log.info("Total documents: %d", total)
    for src in sources:
        cnt = db.documents.count_documents({"source": src})
        log.info("  %s: %d", src, cnt)


def main():
    if len(sys.argv) < 2:
        print("Usage: python3 crawler.py <config.yaml>")
        sys.exit(1)

    cfg = load_config(sys.argv[1])
    db = connect_db(cfg)
    delay = cfg["logic"]["delay"]
    max_pages = cfg["logic"]["max_pages"]
    threads = cfg["logic"]["threads"]

    for source in cfg["sources"]:
        crawl_source(db, source, delay, max_pages, threads)
        recrawl_changed(db, source, delay)

    print_stats(db)


if __name__ == "__main__":
    main()
