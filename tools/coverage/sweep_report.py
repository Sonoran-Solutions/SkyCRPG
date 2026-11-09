#!/usr/bin/env python3
"""Conservative reproducibility record; geometric observations are not an oracle."""
import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def aggregate(manifest, reports, events, threshold=None, oracle=None):
    bybatch = {r.get('route', {}).get('name', r.get('name')): r for r in reports}
    bytag = defaultdict(list)
    for e in events:
        if e.get('ev') == 'view_sample' and e.get('requested') is True:
            bytag[e.get('tag')].append(e)
    oracle = oracle or {}
    rows = []
    for c in manifest['cells']:
        batch = c['batch']
        r = bybatch.get(batch) if batch else None
        status, reason = 'NOT_RUN', c.get('reason') or 'batch not run'
        if r:
            samples = [bytag.get(t, []) for t in c['tags']]
            if any(len(s) != 1 for s in samples):
                reason = 'missing or ambiguous requested samples'
            elif any(s[0].get('available') is not True for s in samples):
                status, reason = 'BLOCKED', 'sample unavailable'
            elif any(not isinstance(s[0].get('points'), list) or len(s[0]['points']) != 5 or
                     any(p.get('on_screen') is not True or p.get('ray', {}).get('status') not in ('clear', 'faded_only', 'blocked') for p in s[0]['points']) or
                     not isinstance(s[0].get('ring'), list) or not s[0]['ring'] or
                     any(p.get('on_screen') is not True or p.get('ray', {}).get('status') not in ('clear', 'faded_only', 'blocked') for p in s[0]['ring']) for s in samples):
                reason = 'missing, off-screen or indeterminate points or ring'
            else:
                status, reason = 'NOT_RUN', 'rendered-visibility oracle, walk and threshold not validated'
                evidence = oracle.get('cells', {}).get(c['cell'], {})
                measured = evidence.get('visible_fraction')
                if (oracle.get('validated') is True and threshold is not None and
                    evidence.get('walk_verified') is True and evidence.get('rig_ok') in (True, False) and
                    evidence.get('room_drawn') in (True, False) and
                    isinstance(measured, (int, float)) and not isinstance(measured, bool) and 0 <= measured <= 1):
                    passed = measured >= threshold and evidence['rig_ok'] and evidence['room_drawn']
                    status, reason = ('PASS', 'validated oracle and configured threshold') if passed else ('FAIL', 'validated oracle or rig/room criterion failed')
        rows.append(dict(**c, attempted=bool(r and any(bytag.get(t) for t in c['tags'])), status=status, result_reason=reason))
    totals = Counter(r['status'] for r in rows)
    types = defaultdict(Counter)
    for r in rows:
        types[r['classification']][r['status']] += 1
    attempted = sum(r['attempted'] for r in rows)
    return dict(manifest_sha256=None, census_sha256=manifest['census_sha256'], generator_sha256=manifest['generator_sha256'], report_tool_sha256=sha(Path(__file__)), candidate_hashes=[r.get('hashes', {}).get('installed_dlls', {}) for r in reports], threshold=threshold, eligible=sum(r['eligible'] for r in rows), attempted=attempted, totals=dict(totals), per_type={k:dict(v) for k,v in types.items()}, pass_rate_attempted=(totals['PASS']/attempted if attempted else None), cells=rows)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('manifest', type=Path)
    p.add_argument('--report', type=Path, action='append', default=[])
    p.add_argument('--probe', type=Path, action='append', default=[])
    p.add_argument('--threshold', type=float, help='minimum validated rendered-visible fraction')
    p.add_argument('--oracle', type=Path, help='validated per-cell rendered visibility evidence JSON')
    p.add_argument('--out', type=Path, required=True)
    a = p.parse_args()
    reports = [json.loads(x.read_text()) for x in a.report]
    events = [json.loads(line) for x in a.probe for line in x.read_text().splitlines() if line.strip()]
    if a.threshold is not None and not 0 <= a.threshold <= 1:
        p.error('threshold must be between 0 and 1')
    oracle = json.loads(a.oracle.read_text()) if a.oracle else None
    result = aggregate(json.loads(a.manifest.read_text()), reports, events, a.threshold, oracle)
    result['manifest_sha256'] = sha(a.manifest)
    result['inputs'] = {str(x):sha(x) for x in a.report + a.probe + ([a.oracle] if a.oracle else [])}
    a.out.write_text(json.dumps(result, indent=2) + '\n')
    print('attempted:', result['attempted'], 'statuses:', result['totals'])


if __name__ == '__main__':
    main()
