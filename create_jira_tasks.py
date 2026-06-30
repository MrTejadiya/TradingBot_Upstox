#!/usr/bin/env python3
"""
Create Jira issues from jira_import.csv using Jira Cloud REST API v3.

Required environment variables:
  JIRA_BASE_URL      Example: https://your-domain.atlassian.net
  JIRA_EMAIL         Atlassian account email
  JIRA_API_TOKEN     Jira API token
  JIRA_PROJECT_KEY   Target Jira project key, for example UPSTOX

Optional environment variables:
  JIRA_DRY_RUN       Set to 1 to print planned actions without creating issues
  JIRA_USE_COMPONENTS
                     Set to 1 only if Jira components already exist in the project

Alternatively, create jira_settings.txt next to this script with either:
  JIRA_BASE_URL=https://your-domain.atlassian.net
or PowerShell lines like:
  [Environment]::SetEnvironmentVariable("JIRA_BASE_URL", "...", "User")
"""

from __future__ import annotations

import base64
import csv
import json
import os
import re
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path


CSV_PATH = Path(__file__).with_name("jira_import.csv")
SETTINGS_PATH = Path(__file__).with_name("jira_settings.txt")


class JiraError(RuntimeError):
    pass


def load_settings_file() -> dict[str, str]:
    if not SETTINGS_PATH.exists():
        return {}

    settings = {}
    pattern = re.compile(
        r"\[Environment\]::SetEnvironmentVariable\(\s*['\"]([^'\"]+)['\"]\s*,\s*['\"]([^'\"]*)['\"]",
        re.IGNORECASE,
    )
    for raw_line in SETTINGS_PATH.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#") or line.startswith(";"):
            continue

        match = pattern.search(line)
        if match:
            settings[match.group(1)] = match.group(2)
            continue

        if "=" in line:
            key, value = line.split("=", 1)
            key = key.strip()
            value = value.strip().strip("'\"")
            if key:
                settings[key] = value

    return settings


SETTINGS = load_settings_file()


def env(name: str) -> str:
    value = os.environ.get(name, "").strip() or SETTINGS.get(name, "").strip()
    if not value:
        raise JiraError(f"Missing required environment variable: {name}")
    return value


def adf(text: str) -> dict:
    lines = [line.rstrip() for line in text.splitlines()]
    content = []
    for line in lines:
        if not line:
            content.append({"type": "paragraph"})
        else:
            content.append(
                {
                    "type": "paragraph",
                    "content": [{"type": "text", "text": line}],
                }
            )
    return {"type": "doc", "version": 1, "content": content or [{"type": "paragraph"}]}


def issue_description(row: dict) -> dict:
    parts = []
    description = row.get("Description", "").strip()
    acceptance = row.get("Acceptance Criteria", "").strip()
    components = row.get("Components", "").strip()
    labels = row.get("Labels", "").strip()
    points = row.get("Story Points", "").strip()

    if description:
        parts.append(description)
    if acceptance:
        parts.append("Acceptance Criteria:\n" + acceptance)
    if components:
        parts.append("Components: " + components)
    if labels:
        parts.append("Labels: " + labels)
    if points:
        parts.append("Story Points: " + points)

    return adf("\n\n".join(parts))


class JiraClient:
    def __init__(self, base_url: str, email: str, token: str):
        self.base_url = base_url.rstrip("/")
        auth = base64.b64encode(f"{email}:{token}".encode("utf-8")).decode("ascii")
        self.headers = {
            "Authorization": f"Basic {auth}",
            "Accept": "application/json",
            "Content-Type": "application/json",
        }

    def request(self, method: str, path: str, body: dict | None = None) -> dict:
        url = self.base_url + path
        data = json.dumps(body).encode("utf-8") if body is not None else None
        req = urllib.request.Request(url, data=data, headers=self.headers, method=method)
        try:
            with urllib.request.urlopen(req, timeout=60) as response:
                raw = response.read().decode("utf-8")
                return json.loads(raw) if raw else {}
        except urllib.error.HTTPError as exc:
            raw = exc.read().decode("utf-8", errors="replace")
            raise JiraError(f"Jira API {method} {path} failed: HTTP {exc.code} {raw}") from exc
        except urllib.error.URLError as exc:
            raise JiraError(f"Jira API {method} {path} failed: {exc}") from exc

    def get_issue_types(self, project_key: str) -> set[str]:
        data = self.request("GET", f"/rest/api/3/issue/createmeta/{project_key}/issuetypes")
        return {item["name"] for item in data.get("issueTypes", [])}

    def create_issue(self, fields: dict) -> dict:
        return self.request("POST", "/rest/api/3/issue", {"fields": fields})


def split_values(value: str) -> list[str]:
    return [part.strip() for part in value.split(";") if part.strip()]


def normalize_issue_type(issue_type: str, available: set[str]) -> str:
    if issue_type in available:
        return issue_type
    fallbacks = {
        "Story": ["Story", "Task"],
        "Task": ["Task", "Story"],
        "Epic": ["Epic"],
    }
    for candidate in fallbacks.get(issue_type, []):
        if candidate in available:
            return candidate
    raise JiraError(f"Issue type {issue_type!r} is not available in the target project")


def create_fields(
    row: dict, project_key: str, issue_type: str, parent_key: str | None, use_components: bool
) -> dict:
    fields = {
        "project": {"key": project_key},
        "summary": row["Summary"].strip(),
        "issuetype": {"name": issue_type},
        "description": issue_description(row),
    }

    priority = row.get("Priority", "").strip()
    if priority:
        fields["priority"] = {"name": priority}

    components = split_values(row.get("Components", ""))
    if use_components and components:
        fields["components"] = [{"name": name} for name in components]

    labels = split_values(row.get("Labels", ""))
    if labels:
        fields["labels"] = [label.replace(" ", "-") for label in labels]

    if parent_key:
        fields["parent"] = {"key": parent_key}

    return fields


def main() -> int:
    base_url = env("JIRA_BASE_URL")
    email = env("JIRA_EMAIL")
    token = env("JIRA_API_TOKEN")
    project_key = env("JIRA_PROJECT_KEY")
    dry_run = os.environ.get("JIRA_DRY_RUN", "").strip() == "1"
    use_components = os.environ.get("JIRA_USE_COMPONENTS", "").strip() == "1"

    if not CSV_PATH.exists():
        raise JiraError(f"Cannot find {CSV_PATH}")

    with CSV_PATH.open(newline="", encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle))

    client = JiraClient(base_url, email, token)
    available_types = client.get_issue_types(project_key)
    epic_keys: dict[str, str] = {}
    created = []

    print(f"Loaded {len(rows)} rows from {CSV_PATH.name}")
    print(f"Target project: {project_key}")
    print(f"Available issue types: {', '.join(sorted(available_types))}")

    for index, row in enumerate(rows, start=1):
        source_type = row["Issue Type"].strip()
        issue_type = normalize_issue_type(source_type, available_types)
        epic_link = row.get("Epic Link", "").strip()
        parent_key = epic_keys.get(epic_link) if epic_link else None
        fields = create_fields(row, project_key, issue_type, parent_key, use_components)

        if dry_run:
            parent = f" parent={parent_key}" if parent_key else ""
            print(f"[dry-run] {index:02d} {issue_type}: {row['Summary']}{parent}")
            if source_type == "Epic":
                epic_keys[row["Epic Name"].strip()] = f"DRY-{index}"
            continue

        result = client.create_issue(fields)
        key = result["key"]
        created.append(key)
        print(f"Created {key}: {row['Summary']}")

        if source_type == "Epic":
            epic_keys[row["Epic Name"].strip()] = key

        time.sleep(0.2)

    print(f"Done. Created {len(created)} Jira issues.")
    if created:
        print("Issue keys: " + ", ".join(created))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except JiraError as exc:
        print(str(exc), file=sys.stderr)
        raise SystemExit(1)
