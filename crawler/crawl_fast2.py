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
from bs4 import BeautifulSoup

logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(levelname)s] %(message)s")
log = logging.getLogger("fast2")

HEADERS = {
    "User-Agent": "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/121.0.0.0 Safari/537.36"
}
CONCURRENCY = 100
SOURCE_NAME = "movieweb"
SITEMAP_URL = "https://movieweb.com/sitemap.xml"
DOMAIN = "movieweb.com"
TARGET = 6000


def extract_text_bs4(html, url):
    soup = BeautifulSoup(html, "lxml")

    title_tag = soup.find("title")
    title = title_tag.get_text(strip=True) if title_tag else ""

    for tag in soup(["script", "style", "nav", "header", "footer", "aside", "figure", "figcaption", "noscript", "iframe"]):
        tag.decompose()

    article = soup.find("article")
    if article:
        paragraphs = article.find_all("p")
    else:
        paragraphs = soup.find_all("p")

    text = "\n".join(p.get_text(strip=True) for p in paragraphs if len(p.get_text(strip=True)) > 20)

    return title, text


async def fetch_bytes(session, url):
    try:
        async with session.get(url, timeout=aiohttp.ClientTimeout(total=10)) as resp:
            if resp.status == 200:
                return await resp.read()
    except Exception:
        pass
    return None


async def fetch_text(session, url):
    try:
        async with session.get(url, timeout=aiohttp.ClientTimeout(total=10)) as resp:
            if resp.status == 200:
                return await resp.text(errors="replace")
    except Exception:
        pass
    return None


async def fetch_all_sitemap_urls(session):
    urls = []
    log.info("Fetching main sitemap from %s", SITEMAP_URL)
    data = await fetch_bytes(session, SITEMAP_URL)
    if not data:
        log.error("Failed main sitemap")
        return urls

    root = ET.fromstring(data)
    ns = {"s": "http://www.sitemaps.org/schemas/sitemap/0.9"}

    for loc in root.findall("s:url/s:loc", ns):
        urls.append(loc.text.strip())

    sub_sitemaps = [s.text.strip() for s in root.findall("s:sitemap/s:loc", ns)]
    log.info("Main: %d direct, %d sub-sitemaps", len(urls), len(sub_sitemaps))

    sem = asyncio.Semaphore(20)

    async def fetch_sub(sm_url):
        async with sem:
            return await fetch_bytes(session, sm_url)

    tasks = [fetch_sub(sm) for sm in sub_sitemaps[:60]]
    results = await asyncio.gather(*tasks)

    for sm_data in results:
        if sm_data:
            try:
                sm_root = ET.fromstring(sm_data)
                for loc in sm_root.findall("s:url/s:loc", ns):
                    urls.append(loc.text.strip())
            except Exception:
                pass
        if len(urls) > TARGET * 3:
            break

    log.info("Total sitemap URLs: %d", len(urls))
    return urls


async def crawl_one(session, url, db, sem, stats):
    async with sem:
        html = await fetch_text(session, url)
        if not html:
            stats["err"] += 1
            return

        title, text = extract_text_bs4(html, url)
        if len(text) < 200:
            stats["err"] += 1
            return

        doc = {
            "url": url,
            "raw_html": html,
            "title": title,
            "text": text,
            "source": SOURCE_NAME,
            "crawl_timestamp": int(time.time()),
            "content_hash": hashlib.md5(html.encode("utf-8", errors="ignore")).hexdigest()
        }
        try:
            db.documents.update_one({"url": url}, {"$set": doc}, upsert=True)
            stats["ok"] += 1
        except pymongo.errors.DuplicateKeyError:
            pass

        if stats["ok"] % 500 == 0 and stats["ok"] > 0:
            log.info("crawled=%d errors=%d", stats["ok"], stats["err"])


async def main():
    if len(sys.argv) < 2:
        print("Usage: python3 crawl_fast2.py <config.yaml>")
        sys.exit(1)

    with open(sys.argv[1]) as f:
        cfg = yaml.safe_load(f)

    client = pymongo.MongoClient(cfg["db"]["host"], cfg["db"]["port"])
    db = client[cfg["db"]["name"]]
    db.documents.create_index("url", unique=True)

    existing = set()
    for doc in db.documents.find({"source": SOURCE_NAME}, {"url": 1}):
        existing.add(doc["url"])
    log.info("Existing %s docs: %d", SOURCE_NAME, len(existing))

    conn = aiohttp.TCPConnector(limit=CONCURRENCY, limit_per_host=CONCURRENCY, ttl_dns_cache=300)
    async with aiohttp.ClientSession(headers=HEADERS, connector=conn) as session:
        all_urls = await fetch_all_sitemap_urls(session)

        new_urls = [u for u in all_urls
                    if u not in existing
                    and DOMAIN in u
                    and not u.endswith((".jpg", ".png", ".xml", ".gz", ".pdf"))]

        import random
        random.shuffle(new_urls)
        new_urls = new_urls[:TARGET]
        log.info("Will crawl %d new URLs", len(new_urls))

        sem = asyncio.Semaphore(CONCURRENCY)
        stats = {"ok": 0, "err": 0}

        batch_sz = 1000
        for i in range(0, len(new_urls), batch_sz):
            batch = new_urls[i:i+batch_sz]
            tasks = [crawl_one(session, u, db, sem, stats) for u in batch]
            await asyncio.gather(*tasks)
            log.info("Batch %d-%d: ok=%d err=%d", i, i+len(batch), stats["ok"], stats["err"])
            if stats["ok"] >= TARGET:
                break

    total = db.documents.estimated_document_count()
    log.info("=== DONE === %s: %d new, DB total: ~%d", SOURCE_NAME, stats["ok"], total)


if __name__ == "__main__":
    asyncio.run(main())
