"""Validate first-product backlog bookkeeping without a build or VM."""
from pathlib import Path
import argparse
import json
import re

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--evidence', action='store_true', help='Also inspect retained local VM checkpoint reports')
args = parser.parse_args()
text = (ROOT / 'docs/agent-backlog.md').read_text(encoding='utf-8')
active = text.split('## Hardware qualification', 1)[0]
expected = {'T01', 'T03', 'T04', 'T05'} | {f'T{i:02}' for i in range(9, 32)}
parts = re.split(r'(?=^## T\d+ )', active, flags=re.M)
tasks = {re.match(r'## (T\d+) ', p).group(1): p for p in parts if re.match(r'## (T\d+) ', p)}
assert set(tasks) == expected
rows = re.findall(r'^\| (T\d+) \| (\d+) \| [^|]+ \| (\d+)% \| [^|]+ \|$', active, re.M)
assert len(rows) == len(expected) == 27
assert [int(r[1]) for r in rows] == list(range(1, 28))
assert [r[0] for r in rows] == list(tasks), 'Table/scope order differs'
round_pct = lambda n, d: (200*n+d)//(2*d)
counts = {}
for task, _, pct in rows:
    scope = re.split(r'^Scope[^\n]*:\s*', tasks[task], maxsplit=1, flags=re.M)[1].split('Done when:', 1)[0]
    boxes = re.findall(r'^- \[([ x])\] ('+task+r'\.\d+)\b', scope, re.M)
    assert boxes and [int(b[1].split('.')[1]) for b in boxes] == list(range(1, len(boxes)+1)), task
    checked = sum(b[0] == 'x' for b in boxes)
    assert round_pct(checked, len(boxes)) == int(pct), task
    assert 'Verify:' in tasks[task] and re.search(r'\bevidence:', tasks[task], re.I), task
    counts[task] = {'checked': checked, 'total': len(boxes), 'percent': int(pct)}
total = sum(r['percent'] for r in counts.values())
overall = (2*total+len(counts))//(2*len(counts))
overall_row = re.search(r'^\| \*\*Overall\*\*[^\n]*\| \*\*(\d+)%\*\*', active, re.M)
assert overall_row and int(overall_row[1]) == overall
assert f'Sum **{total}**; Overall **{overall}%**' in active
assert '27 ACTIVE software tasks' in (ROOT/'AGENTS.md').read_text(encoding='utf-8')
assert '**27 ACTIVE software tasks**' in (ROOT/'docs/implementation-order.md').read_text(encoding='utf-8')
hardware = text.split('## Hardware qualification', 1)[1].split('## Archive:', 1)[0]
assert re.findall(r'^\| (H\d+) \|', hardware, re.M) == [f'H{i:02}' for i in range(1, 6)]
deps = {}
for task, part in tasks.items():
    raw = part.splitlines()[0].split('depends:', 1)[1].split(')', 1)[0]
    expanded = set()
    for m in re.finditer(r'T(\d+)(?:-T?(\d+))?', raw):
        expanded.update(f'T{i:02}' for i in range(int(m[1]), int(m[2] or m[1])+1))
    deps[task] = expected-{task} if task == 'T31' else expanded
    assert deps[task] <= expected and task not in deps[task], (task, deps[task])
def visit(task, path=()):
    assert task not in path, ('Dependency cycle', path+(task,))
    for dep in deps[task]:
        visit(dep, path+(task,))
for task in deps:
    visit(task)
for name in ['docs/first-product-software-scope.md', 'docs/implementation-order.md',
             'docs/agent-backlog.md', 'docs/agent-backlog-history-2026-10-25.md']:
    path = ROOT/name
    for target in re.findall(r'\]\(([^)]+)\)', path.read_text(encoding='utf-8')):
        if '://' not in target and not target.startswith('#'):
            assert (path.parent/target.split('#')[0]).exists(), (name, target)
evidence = ['build/vm-oracle/android-apps-20261025-b/report/ctest.log',
            'build/vm-oracle/android-apps-20261025-b/visual/androidapps/attempt-20261025-182448-750',
            'build/vm-oracle/store-20261025-g/report/status.json',
            'build/vm-oracle/store-20261025-g/report/ctest.log',
            'build/vm-oracle/store-online-20261025-c/report/status.json']
if args.evidence:
    for name in evidence:
        assert (ROOT/name).exists(), name
    store = json.loads((ROOT/evidence[2]).read_text(encoding='utf-8'))
    assert store['visualPassed'] and store['semanticAcceptance'] == 'not_verified'
    assert store['normalLayoutsBefore'] == store['normalLayoutsAfter']
    online = json.loads((ROOT/evidence[4]).read_text(encoding='utf-8'))
    assert online['state'] == 'passed' and online['tests'] == 3 and not online['fullSuite']
print(json.dumps({'result': 'PASS', 'activeTasks': len(counts), 'sumPercentages': total,
                  'overallPercent': overall, 'physicalGates': 5,
                  'checks': ['scope/table/order', 'checkbox math', 'entrypoint synchronization',
                             'dependency graph', 'local document links'],
                  'retainedEvidenceChecked': args.evidence, 'tasks': counts}, indent=2))
