#!/usr/bin/env python3
"""
Create the Jira project used by create_jira_tasks.py.

Reads credentials from environment variables or jira_settings.txt.
Optional settings:
  JIRA_PROJECT_KEY       Defaults to RK9311
  JIRA_PROJECT_NAME      Defaults to TradingBot Upstox
  JIRA_PROJECT_TEMPLATE  Defaults to Scrum software project template
"""

from __future__ import annotations

import json
import importlib.util
import sys
import urllib.error
import urllib.request
from pathlib import Path


TASKS_SCRIPT = Path(__file__).with_name("create_jira_tasks.py")
SPEC = importlib.util.spec_from_file_location("create_jira_tasks", TASKS_SCRIPT)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError(f"Cannot load {TASKS_SCRIPT}")
TASKS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(TASKS)

JiraClient = TASKS.JiraClient
JiraError = TASKS.JiraError
SETTINGS = TASKS.SETTINGS
env = TASKS.env


def setting(name: str, default: str) -> str:
    return SETTINGS.get(name, "").strip() or default


def main() -> int:
    base_url = env("JIRA_BASE_URL")
    email = env("JIRA_EMAIL")
    token = env("JIRA_API_TOKEN")
    key = setting("JIRA_PROJECT_KEY", "RK9311")
    name = setting("JIRA_PROJECT_NAME", "TradingBot Upstox")
    template = setting("JIRA_PROJECT_TEMPLATE", "com.pyxis.greenhopper.jira:gh-simplified-scrum-classic")

    client = JiraClient(base_url, email, token)
    myself = client.request("GET", "/rest/api/3/myself")
    account_id = myself["accountId"]

    existing = client.request("GET", "/rest/api/3/project/search?maxResults=100")
    for project in existing.get("values", []):
        if project.get("key") == key:
            print(f"Project already exists: {project['key']} {project.get('name', '')}")
            return 0

    payload = {
        "key": key,
        "name": name,
        "projectTypeKey": "software",
        "projectTemplateKey": template,
        "leadAccountId": account_id,
        "assigneeType": "UNASSIGNED",
        "description": "C++ Upstox Delivery Trading Bot backlog from SRS.",
    }

    try:
        result = client.request("POST", "/rest/api/3/project", payload)
    except JiraError as exc:
        message = str(exc)
        if "projectTemplateKey" in message or "template" in message.lower():
            fallback = dict(payload)
            fallback["projectTemplateKey"] = "com.pyxis.greenhopper.jira:gh-simplified-kanban-classic"
            result = client.request("POST", "/rest/api/3/project", fallback)
        else:
            raise

    print(json.dumps({"id": result.get("id"), "key": key, "name": name}, indent=2))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except JiraError as exc:
        print(str(exc), file=sys.stderr)
        raise SystemExit(1)
