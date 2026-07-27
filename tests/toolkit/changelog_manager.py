"""
PocketDoctor Mark 4 — Test Suite Changelog Toolkit Manager [v1.1.0-beta]

CLI tool & Python library to manage, append, search, and export test suite changelogs.

Usage:
  python tests/toolkit/changelog_manager.py list
  python tests/toolkit/changelog_manager.py search <query>
  python tests/toolkit/changelog_manager.py export
  python tests/toolkit/changelog_manager.py add --version <ver> --desc <desc>
"""

import json
import os
import sys
import argparse
from datetime import datetime

TOOLKIT_DIR = os.path.dirname(os.path.abspath(__file__))
TESTS_DIR = os.path.dirname(TOOLKIT_DIR)
JSON_FILE = os.path.join(TOOLKIT_DIR, "test_changelog_history.json")
MD_FILE = os.path.join(TESTS_DIR, "CHANGELOG_BETA.md")

INITIAL_DATA = [
    {
        "version": "v1.0.0-beta",
        "date": "2026-07-27",
        "author": "Vaidik Khurana",
        "title": "Initial Test Suite & UI Template Verification",
        "changes": [
            "Added test_ui_templates.py to verify DASHBOARD_HTML, WIFI_HTML, PROFILES_HTML, PROFILE_EDIT_HTML",
            "Added test_profile_validation.py to test EEPROM profile boundaries (name, age 1-120, height 50-250cm, weight 20-300kg, max 3 capacity)",
            "Added run_all_tests.py master runner"
        ]
    },
    {
        "version": "v1.1.0-beta",
        "date": "2026-07-27",
        "author": "Vaidik Khurana",
        "title": "Prescription & Clinical Triage Report Revamp",
        "changes": [
            "Revamped createComprehensiveHTMLReport into a state-of-the-art Rx Prescription layout",
            "Integrated Outfit & Plus Jakarta Sans Google Fonts typography and Rx badge styling",
            "Added Rx Administration Schedule table, Side Effect Surveillance checklist, and Official AI Testation Signature block",
            "Added test_report_generation_function and test_modern_css_features tests",
            "Created tests/toolkit CLI for automated test suite changelog management"
        ]
    }
]

def load_history():
    if not os.path.exists(JSON_FILE):
        save_history(INITIAL_DATA)
        return INITIAL_DATA
    with open(JSON_FILE, "r", encoding="utf-8") as f:
        return json.load(f)

def save_history(history):
    with open(JSON_FILE, "w", encoding="utf-8") as f:
        json.dump(history, f, indent=2)

def export_markdown():
    history = load_history()
    md = "# 🩺 PocketDoctor Mark 4 — Test Suite Beta Changelog\n\n"
    md += "> **Status**: Active Beta  \n"
    md += "> **Toolkit Version**: 1.1.0-beta  \n"
    md += "> **Last Updated**: " + datetime.now().strftime("%Y-%m-%d") + "\n\n"
    md += "---\n\n"
    for entry in reversed(history):
        md += f"## 🚀 {entry['version']} — {entry['title']}\n"
        md += f"**Date**: `{entry['date']}` | **Author**: `{entry['author']}`\n\n"
        md += "### Key Changes & Assertions:\n"
        for change in entry['changes']:
            md += f"- {change}\n"
        md += "\n---\n\n"
    with open(MD_FILE, "w", encoding="utf-8") as f:
        f.write(md)
    print(f"[SUCCESS] Exported test suite changelog to {MD_FILE}")

def list_entries():
    history = load_history()
    print("\n--- Test Suite Changelog History ---")
    for entry in reversed(history):
        print(f"[{entry['version']}] {entry['date']} - {entry['title']} (by {entry['author']})")
        for c in entry['changes']:
            print(f"  - {c}")
        print()

def search_entries(query):
    history = load_history()
    query_lower = query.lower()
    results = []
    for entry in history:
        match = False
        if query_lower in entry['version'].lower() or query_lower in entry['title'].lower():
            match = True
        for c in entry['changes']:
            if query_lower in c.lower():
                match = True
        if match:
            results.append(entry)
    print(f"\n--- Search Results for '{query}' ({len(results)} found) ---")
    for entry in results:
        print(f"[{entry['version']}] {entry['title']}")
        for c in entry['changes']:
            print(f"  - {c}")
        print()

def add_entry(version, title, changes, author="Vaidik Khurana"):
    history = load_history()
    entry = {
        "version": version,
        "date": datetime.now().strftime("%Y-%m-%d"),
        "author": author,
        "title": title,
        "changes": changes
    }
    history.append(entry)
    save_history(history)
    export_markdown()
    print(f"[SUCCESS] Added entry for {version}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="PocketDoctor Test Suite Changelog Toolkit")
    subparsers = parser.add_subparsers(dest="command")

    subparsers.add_parser("list", help="List all test changelogs")
    subparsers.add_parser("export", help="Export Markdown CHANGELOG_BETA.md")
    
    search_p = subparsers.add_parser("search", help="Search changelog history")
    search_p.add_argument("query", type=str, help="Search query string")

    add_p = subparsers.add_parser("add", help="Add new test changelog entry")
    add_p.add_argument("--version", required=True, help="Release version (e.g. v1.2.0-beta)")
    add_p.add_argument("--title", required=True, help="Entry title")
    add_p.add_argument("--changes", nargs="+", required=True, help="List of change strings")
    add_p.add_argument("--author", default="Vaidik Khurana", help="Author name")

    args = parser.parse_args()

    if args.command == "list":
        list_entries()
    elif args.command == "export":
        export_markdown()
    elif args.command == "search":
        search_entries(args.query)
    elif args.command == "add":
        add_entry(args.version, args.title, args.changes, args.author)
    else:
        export_markdown()
        list_entries()
