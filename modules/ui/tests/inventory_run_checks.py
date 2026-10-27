"""Bounded DYL-163 jobs; logs and binaries stay outside the repository."""
import datetime
import json
import os
from pathlib import Path
import signal
import sys
import subprocess
import time

repo = Path(__file__).resolve().parents[3]
log = Path('/srv/storage/Projects/SkyCRPG/session-p3/logs/dyl-163')
node = '/home/dq/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/bin/node'
provider = '/srv/storage/Projects/SkyCRPG/session-20261026/ui-skyui-v6.11'
commands = [(['python3', '-I', 'modules/ui/tests/inventory_prepare_contract.py', str(log)], 'prepare-contract')]
for test in ('inventory_policy', 'favorite_slots', 'menu_focus', 'resource_snapshot', 'hud_settings'):
    binary = str(log / (test + '_test'))
    commands += [(['g++', '-std=c++20', '-Wall', '-Wextra', '-Werror', '-pedantic', '-Imodules/ui/src',
                  f'modules/ui/tests/{test}_test.cpp', '-o', binary], test + '-build'), ([binary], test)]
commands += [([node, 'modules/ui/tests/inventory_actions_contract.js', 'modules/ui', str(log / 'InventoryMenu.as')], 'inventory-as'),
             ([node, 'modules/ui/tests/hud_contract.js', 'modules/ui', provider], 'hud-as'),
             ([node, 'modules/ui/tests/navigation_contract.js', 'modules/ui', provider], 'navigation-as'),
             (['python3', '-I', '-m', 'unittest', 'discover', '-s', 'modules/ui/tests', '-p', 'test_scaleform_contract.py', '-v'], 'scaleform-contract'),
             (['git', 'apply', '--check', '/srv/storage/Projects/SkyCRPG/session-p3/integration/dyl-163.patch'], 'integration-patch')]
jobs = [dict(purpose=name, command=['timeout', '600', *command], owner='DYL-163 / Codex', timeout_seconds=600,
             expected='exit 0; assertions pass / portable binary / patch applies', completion='process exit and log',
             log=str(log / (name + '.log')), cleanup='terminate only owned process group on timeout; reap child', status='NOT_RUN')
        for command, name in commands]
record = log / 'jobs.json'
if '--resume' in sys.argv:
    previous = json.loads(record.read_text())
    jobs = [prior if prior['status'] == 'PASS' else job for prior, job in zip(previous, jobs)]
record.write_text(json.dumps(jobs, indent=2) + '\n')
env = dict(os.environ, SKYCRPG_SKYUI_SOURCE=provider)
failed = False
active = None

def cleanup(process):
    if process is None:
        return
    try:
        os.killpg(process.pid, signal.SIGTERM)
    except ProcessLookupError:
        pass
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        os.killpg(process.pid, signal.SIGKILL)
        process.wait(timeout=5)
    # A completed command can still have descendants in its owned group.
    try:
        os.killpg(process.pid, signal.SIGKILL)
    except ProcessLookupError:
        pass

def interrupted(signum, _frame):
    cleanup(active)
    raise SystemExit(128 + signum)

signal.signal(signal.SIGTERM, interrupted)
signal.signal(signal.SIGINT, interrupted)
for job in jobs:
    if job['status'] == 'PASS':
        continue
    started = time.monotonic()
    job['started_utc'] = datetime.datetime.now(datetime.timezone.utc).isoformat()
    job['deadline_utc'] = (datetime.datetime.now(datetime.timezone.utc) + datetime.timedelta(seconds=600)).isoformat()
    with Path(job['log']).open('w') as stream:
        process = subprocess.Popen(job['command'], cwd=repo, env=env, stdout=stream, stderr=subprocess.STDOUT, start_new_session=True)
        active = process
        job['pid'] = process.pid
        record.write_text(json.dumps(jobs, indent=2) + '\n')
        try:
            code = process.wait(timeout=605)
        except subprocess.TimeoutExpired:
            cleanup(process)
            code = 124
        active = None
        job.update(exit_code=code, elapsed_seconds=round(time.monotonic() - started, 3),
                   status='PASS' if code == 0 else 'FAIL', reaped=process.poll() is not None)
    record.write_text(json.dumps(jobs, indent=2) + '\n')
    print(job['purpose'], job['status'], flush=True)
    if code:
        print(Path(job['log']).read_text(), flush=True)
        failed = True
        break
raise SystemExit(1 if failed else 0)
