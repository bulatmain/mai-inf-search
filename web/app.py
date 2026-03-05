#!/usr/bin/env python3
import os
import json
import subprocess

from fastapi import FastAPI, Request, Query
from fastapi.responses import HTMLResponse
from fastapi.templating import Jinja2Templates

app = FastAPI(title="Movie Reviews Search Engine")

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
templates = Jinja2Templates(directory=os.path.join(BASE_DIR, "templates"))

SEARCH_CLI = os.path.join(BASE_DIR, "..", "core", "search_cli")
INDEX_PATH = os.path.join(BASE_DIR, "..", "index.bin")


def run_search(query_text, limit=50):
    try:
        proc = subprocess.run(
            [SEARCH_CLI, INDEX_PATH, "--json", "--limit", str(limit)],
            input=query_text + "\n",
            capture_output=True,
            text=True,
            timeout=10
        )
        if proc.returncode != 0:
            return {"query": query_text, "total": 0, "time_ms": 0, "results": [], "error": proc.stderr}

        for line in proc.stdout.strip().split("\n"):
            if line.strip():
                return json.loads(line)
    except Exception as e:
        return {"query": query_text, "total": 0, "time_ms": 0, "results": [], "error": str(e)}

    return {"query": query_text, "total": 0, "time_ms": 0, "results": []}


@app.get("/", response_class=HTMLResponse)
async def home(request: Request):
    return templates.TemplateResponse("search.html", {"request": request})


@app.get("/search", response_class=HTMLResponse)
async def search(request: Request,
                 q: str = Query(""),
                 page: int = Query(1, ge=1)):
    if not q.strip():
        return templates.TemplateResponse("search.html", {"request": request})

    per_page = 50
    data = run_search(q.strip(), limit=per_page * page)

    results = data.get("results", [])
    total = data.get("total", 0)
    time_ms = data.get("time_ms", 0)

    start = (page - 1) * per_page
    page_results = results[start:start + per_page] if len(results) > start else results

    has_next = total > page * per_page

    return templates.TemplateResponse("results.html", {
        "request": request,
        "query": q,
        "results": page_results,
        "total": total,
        "time_ms": time_ms,
        "page": page,
        "has_next": has_next,
        "per_page": per_page,
    })
