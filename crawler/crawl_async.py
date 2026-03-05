#!/usr/bin/env python3
import sys
import time
import hashlib
import asyncio
import logging
import yaml
import xml.etree.ElementTree as ET

import aiohttp
import pymongo
from newspaper import Article

logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(levelname)s] %(message)s")
log = logging.getLogger("async_crawl")

HEADERS = {
    "User-Agent": "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"
}

TARGET = 5500
CONCURRENCY = 50
SOURCE_NAME = "screenrant"


async def fetch_url(session, url):
    try:
        async with session.get(url, timeout=aiohttp.ClientTimeout(total=12)) as resp:
            if resp.status == 200:
                return await resp.text()
    except Exception:
        pass
    return None


async def fetch_sitemap_urls(session):
    urls = []
    log.info("Fetching main sitemap...")
    text = await fetch_url(session, "https://screenrant.com/sitemap.xml")
    if not text:
        log.error("Failed to fetch main sitemap")
        return urls

    root = ET.fromstring(text.encode())
    ns = {"s": "http://www.sitemaps.org/schemas/sitemap/0.9"}

    for loc in root.findall("s:url/s:loc", ns):
        urls.append(loc.text.strip())

    sub_sitemaps = [s.text.strip() for s in root.findall("s:sitemap/s:loc", ns)]
    log.info("Main sitemap: %d direct URLs, %d sub-sitemaps", len(urls), len(sub_sitemaps))

    tasks = []
    for sm_url in sub_sitemaps[:30]:
        tasks.append(fetch_url(session, sm_url))

    results = await asyncio.gather(*tasks)
    for i, sm_text in enumerate(results):
        if sm_text:
            try:
                sm_root = ET.fromstring(sm_text.encode())
                sub_urls = [u.text.strip() for u in sm_root.findall("s:url/s:loc", ns)]
                urls.extend(sub_urls)
                log.info("  Sub-sitemap %d: %d URLs (total: %d)", i+1, len(sub_urls), len(urls))
            except Exception:
                pass
        if len(urls) > TARGET * 3:
            break

    return urls


async def process_url(session, url, db, sem, stats):
    async with sem:
        try:
            html = await fetch_url(session, url)
            if not html:
                stats["errors"] += 1
                return

            article = Article(url)
            article.set_html(html)
            article.parse()

            if not article.text or len(article.text) < 200:
                stats["errors"] += 1
                return

            doc = {
                "url": url,
                "raw_html": html,
                "title": article.title or "",
                "text": article.text,
                "source": SOURCE_NAME,
                "crawl_timestamp": int(time.time()),
                "content_hash": hashlib.md5(html.encode("utf-8", errors="ignore")).hexdigest()
            }
            try:
                db.documents.update_one({"url": doc["url"]}, {"$set": doc}, upsert=True)
                stats["crawled"] += 1
            except pymongo.errors.DuplicateKeyError:
                pass

        except Exception:
            stats["errors"] += 1

        total = stats["crawled"] + stats["errors"]
        if total % 200 == 0 and total > 0:
            log.info("Progress: crawled=%d errors=%d", stats["crawled"], stats["errors"])


async def main():
    if len(sys.argv) < 2:
        print("Usage: python3 crawl_async.py <config.yaml>")
        sys.exit(1)

    with open(sys.argv[1]) as f:
        cfg = yaml.safe_load(f)

    client = pymongo.MongoClient(cfg["db"]["host"], cfg["db"]["port"])
    db = client[cfg["db"]["name"]]
    db.documents.create_index("url", unique=True)

    existing_urls = set()
    cursor = db.documents.find({"source": SOURCE_NAME}, {"url": 1})
    for doc in cursor:
        existing_urls.add(doc["url"])
    log.info("Already have %d %s docs", len(existing_urls), SOURCE_NAME)

    connector = aiohttp.TCPConnector(limit=CONCURRENCY, limit_per_host=CONCURRENCY)
    async with aiohttp.ClientSession(headers=HEADERS, connector=connector) as session:
        all_urls = await fetch_sitemap_urls(session)
        log.info("Total URLs from sitemaps: %d", len(all_urls))

        new_urls = [u for u in all_urls
                    if u not in existing_urls
                    and "screenrant.com/" in u
                    and not u.endswith((".jpg", ".png", ".xml", ".gz", ".pdf"))]
        log.info("New URLs to crawl: %d", len(new_urls))

        import random
        random.shuffle(new_urls)
        new_urls = new_urls[:TARGET]

        sem = asyncio.Semaphore(CONCURRENCY)
        stats = {"crawled": 0, "errors": 0}

        batch_size = 500
        for i in range(0, len(new_urls), batch_size):
            batch = new_urls[i:i + batch_size]
            tasks = [process_url(session, url, db, sem, stats) for url in batch]
            await asyncio.gather(*tasks)
            log.info("Batch %d-%d done. crawled=%d errors=%d",
                     i, i+len(batch), stats["crawled"], stats["errors"])
            if stats["crawled"] >= TARGET:
                break

    total = db.documents.estimated_document_count()
    log.info("=== Done ===")
    log.info("%s crawled: %d", SOURCE_NAME, stats["crawled"])
    log.info("Total estimated in DB: %d", total)


if __name__ == "__main__":
    asyncio.run(main())
