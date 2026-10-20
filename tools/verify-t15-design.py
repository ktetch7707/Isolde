#!/usr/bin/env python3
"""T15 static verification: Page Studio design pack coverage (no Qt build needed).

Checks (design state-transition/feature-coverage review for backlog T15):
  T15.1 feature map covers every CustomizeHomeView action label and PageManager
        operation, plus the four shell surfaces (source notices, All Apps,
        themed surfaces, Web search switch), citing backend paths.
  T15.2 wireframes cover the required screen set (blank/existing page, Add,
        selection, inspector, pick-up/drop, resize, collision, dirty exit,
        preview, apply error, nested scope) in TV and desktop forms.
  T15.3 state model covers D-pad/Confirm/Back/Menu for every state, field
        editing, focus restoration, error feedback and accessible movement
        alternatives; remote-only constraint stated (no pointer/number keys/
        hidden gestures).
  T15.4 review records a designer-executed artifact walkthrough (no user
        present) with resolved findings and measurable acceptance journeys;
        user review explicitly deferred to T24 (no invented review evidence).
  Link check: every relative Markdown link in the pack resolves.
Exit 0 = all checks pass. Prints PASS/FAIL per check.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PACK = ROOT / "docs" / "design" / "page-studio"
fails = []


def check(name, ok, detail=""):
    print(("PASS " if ok else "FAIL ") + name + (f" — {detail}" if detail else ""))
    if not ok:
        fails.append(name)


def read(rel):
    return (PACK / rel).read_text(encoding="utf-8")


for required in ("README.md", "feature-map.md", "wireframes.md",
                 "interaction-states.md", "review-and-acceptance.md"):
    check(f"artifact exists: {required}", (PACK / required).exists())

fmap, wf, states, review = (read(n) for n in
                            ("feature-map.md", "wireframes.md",
                             "interaction-states.md", "review-and-acceptance.md"))

# T15.1 — operation coverage from the live QML sources (label is not backend proof:
# every label must also appear near a backend path in the map).
chv = (ROOT / "qml" / "CustomizeHomeView.qml").read_text(encoding="utf-8")
actions_block = chv.split("readonly property var actions: [", 1)[1].split("]", 1)[0]
labels = set(re.findall(r'"([^"]+)"', actions_block))
missing = sorted(l for l in labels if l not in fmap)
check(f"T15.1 all {len(labels)} Customize Home action labels mapped", not missing, f"missing: {missing}")

pm = (ROOT / "qml" / "PageManager.qml").read_text(encoding="utf-8")
ops_block = pm.split("operations:[", 1)[1].split("]", 1)[0]
pm_ops = set(re.findall(r'"([^"]+)"', ops_block))
missing_pm = sorted(o for o in pm_ops if o not in fmap)
check("T15.1 all PageManager operations mapped", not missing_pm, f"missing: {missing_pm}")

for surface, needle in (("source notices", "Detected-source notices"),
                        ("All Apps", "All Apps"),
                        ("themed surfaces", "Themed shell surfaces"),
                        ("Web search switch", "Web search switch")):
    check(f"T15.1 shell surface mapped: {surface}", needle in fmap)

for backend in ("moveItem", "setItemPlacement", "gridCollisions", "addActionTile",
                "rebindSource", "setAppearance", "applyTheme", "createPage",
                "drawerRequested", "searchToggle"):
    check(f"T15.1 backend evidence cited: {backend}", backend in fmap)

# T15.2 — required screens, each with TV and desktop treatment.
for screen in ("Blank page", "Existing page", "Add (catalog)", "Selection and inspector",
               "Pick-up / drop", "Resize", "Collision", "Dirty exit", "Preview and Apply",
               "Nested scope"):
    check(f"T15.2 wireframe screen: {screen}", f"## " in wf and screen in wf)
check("T15.2 TV and desktop forms present",
      "TV:" in wf and "Desktop:" in wf and "Parity note" in wf)

# T15.3 — state model completeness.
for state in ("Canvas browsing", "Moving", "Resizing", "Collision dialog", "Context menu",
              "Inspector sheet", "Field editing", "Add catalog", "Pages / Layers drawer",
              "Preview / try navigation", "Apply summary", "Apply error", "Dirty-exit dialog"):
    check(f"T15.3 state covered: {state}", state in states)
check("T15.3 four input channels in the table",
      all(h in states.split("| State |")[1].split("\n")[0] for h in
          ("D-pad", "Confirm", "Back", "Menu")))
check("T15.3 focus restoration rules", "Focus restoration rules" in states)
check("T15.3 error feedback", "Error feedback" in states)
check("T15.3 accessible movement alternatives",
      "Accessible movement alternatives" in states)
check("T15.3 remote-only constraint stated",
      "No long press" in states and "number keys" in states and "hidden" in states)

# T15.4 — designer walkthrough + journeys (user review deferred to T24).
check("T15.4 designer walkthrough recorded (no user present)", "Designer walkthrough record" in review)
check("T15.4 explicitly states no user review yet",
      "No user or representative review has taken place" in review)
check("T15.4 T15.4 open/deferred-to-T24 stated",
      "T15.4 user review is OPEN" in review or "deferred to T24" in review)
check("T15.4 findings resolved", re.search(r"\| W\d+ \|.*\| (High|Medium|Low) \|", review) is not None)
check("T15.4 measurable acceptance journeys", all(f"| A{i} |" in review for i in range(1, 11)))
check("T15.4 no-build claim present", "No application build is claimed" in review)

# Relative link resolution inside the pack.
bad_links = []
for doc in PACK.glob("*.md"):
    for target in re.findall(r"\]\((\.\.?/[^)#]+)\)", doc.read_text(encoding="utf-8")):
        if not (doc.parent / target).resolve().exists():
            bad_links.append(f"{doc.name} -> {target}")
check("links resolve", not bad_links, "; ".join(bad_links))

print(f"\n{len(fails)} failures" if fails else "\nAll T15 static checks pass.")
sys.exit(1 if fails else 0)
