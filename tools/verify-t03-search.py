#!/usr/bin/env python3
"""T03 static verification: Kodi title-search hardening + Web toggle model.

Checks the tree (no Qt build needed) for:
  T03.1: debounce timer, min-length gate, cancel/invalidate on close-or-short-query,
         three per-type clients (movies/shows/episodes), explicit unavailable state.
  T03.2: persisted SearchSettings webEnabled default OFF, CommandCenter toggle entry,
         Web ON path stubbed (no external endpoint wired).
Exit 0 = all checks pass. Prints PASS/FAIL per check.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
fails = []


def check(name, ok, detail=""):
    print(("PASS " if ok else "FAIL ") + name + (f" — {detail}" if detail else ""))
    if not ok:
        fails.append(name)


def read(rel):
    return (ROOT / rel).read_text(encoding="utf-8")


cc = read("qml/CommandCenter.qml")
ss_h = read("src/settings/SearchSettings.h")
ss_cpp = read("src/settings/SearchSettings.cpp")

# T03.1 — debounce (250ms single-shot timer restarting on each keystroke)
check("T03.1 debounce timer",
      bool(re.search(r"Timer\s*\{\s*id:\s*mediaSearchTimer;\s*interval:\s*250;\s*repeat:\s*false", cc)))
check("T03.1 keystroke restarts debounce",
      "mediaSearchTimer.restart()" in cc)
# min-length gate + cancel path
check("T03.1 min-length gate + invalidate on short/closed",
      "text.length < 2" in cc and "invalidateMediaSearch()" in cc)
check("T03.1 three per-type clients",
      all(s in cc for s in ("movieSearchClient.openView(\"movies\", \"search\"",
                            "showSearchClient.openView(\"shows\", \"search\"",
                            "episodeSearchClient.openView(\"episodes\", \"search\"")))
check("T03.1 explicit Kodi-unavailable state",
      "unavailable" in cc and "mediaStatus" in cc)
# T03.2 — persisted toggle, default OFF, stubbed ON path
check("T03.2 persisted webEnabled default false",
      "m_webEnabled = false" in ss_h and 'settings.value(QStringLiteral("webEnabled"), false)' in ss_cpp)
check("T03.2 toggle entry in Command Center",
      "toggleWebEnabled()" in cc and "searchToggle" in cc)
check("T03.2 Web ON stubbed, no external endpoint",
      "web-discovery-unavailable" in cc
      and not re.search(r"XMLHttpRequest|fetch\(|WebSocket|TcpSocket|NetworkAccessManager", cc)
      and bool(re.search(r"Web ON path is a future", ss_h)))

print(f"\n{len(fails)} failures" if fails else "\nAll T03 static checks pass.")
sys.exit(1 if fails else 0)
